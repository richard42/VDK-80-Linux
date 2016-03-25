/**
 @file td3.cpp

 @brief based on TRS-80 Virtual Disk Kit v1.7 for Windows by Miguel Dutra
 Linux port VDK-80-Linux done by Mike Gore, 2016

 @par Tools to Read and Write files inside common TRS-80 emulator images

 @par Copyright &copy; 2016 Miguel Dutra, GPL License
 @par You are free to use this code under the terms of GPL
   please retain a copy of this notice in any code you use it in.

 This is free software: you can redistribute it and/or modify it under the 
 terms of the GNU General Public License as published by the Free Software 
 Foundation, either version 3 of the License, or (at your option) any later version.

 The software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 @par Original Windows Code
  @see http://www.trs-80.com/wordpress/category/contributors/miguel-dutra/
  @see http://www.trs-80.com/wordpress/dsk-and-dmk-image-utilities/
  @see Miguel Dutra www.mdutra.com
*/
//---------------------------------------------------------------------------------
// Operating System Interface for TRSDOS (Model III)
//---------------------------------------------------------------------------------

#include "windows.h"
#include <stdio.h>
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "td4.h"
#include "td3.h"

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CTD3::Load(CVDI* pVDI, DWORD dwFlags)
{

    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // Copy VDI pointer and user flags to member variables
    m_pVDI = pVDI;
    m_dwFlags = dwFlags;

    // Get disk geometry
    m_pVDI->GetDG(m_DG);

    // Calculate the number of disk sides
    if (m_dwFlags & V80_FLAG_SS)
        m_nSides = 1;
    else if (m_dwFlags & V80_FLAG_DS)
        m_nSides = 2;
    else
        m_nSides = 1;   // [PATCH]

    // Calculate the number of sectors per track
    m_nSectorsPerTrack = m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1;

    // Check internal buffer size (just in case)
    if (sizeof(m_Buffer) < m_DG.FT.wSectorSize)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Read boot sector
    if ((dwError = m_pVDI->Read(m_DG.FT.nTrack, m_DG.FT.nFirstSide, m_DG.FT.nFirstSector, m_Buffer, m_DG.FT.wSectorSize)) != NO_ERROR)
        goto Done;

    // Get directory track and reset MSB (set on some DOS to indicate the disk density)
    m_nDirTrack = m_Buffer[1] & 0x7F;   // [PATCH]

    // Calculate number of directory sectors
    m_nDirSectors = m_nSectorsPerTrack * m_nSides;

    // Max Dir Sectors = HIT Size / Entries per Sector
    m_nMaxDirSectors = (m_DG.LT.wSectorSize / (BYTE)(m_DG.LT.wSectorSize / sizeof(TD3_FPDE)));   // [PATCH]

    // If dir sectors exceeds max sectors, limit to max
    if (m_nDirSectors > m_nMaxDirSectors)
        m_nDirSectors = m_nMaxDirSectors;

    // If not first Load, release the previously allocated memory
    if (m_pDir != NULL)
        free(m_pDir);

    // Calculate needed memory to hold the entire directory
    dwBytes = m_nDirSectors * m_DG.LT.wSectorSize + (V80_MEM - (m_nDirSectors * m_DG.LT.wSectorSize) % V80_MEM);

    // Allocate memory for the entire directory
    if ((m_pDir = (BYTE*)calloc(dwBytes,1)) == NULL)
    {
        dwError = ERROR_OUTOFMEMORY;
        goto Done;
    }

    // Load directory into memory
    if ((dwError = DirRW(TD4_DIR_READ)) != NO_ERROR)
        goto Done;

    // Calculate the disk parameters

    m_nSectorsPerGranule = (m_DG.LT.nDensity == VDI_DENSITY_SINGLE ? 2 : 3);        // [PATCH]

    m_nGranulesPerTrack = m_nSectorsPerTrack / m_nSectorsPerGranule;                // [PATCH]

    m_nGranulesPerCylinder = m_nGranulesPerTrack * m_nSides;                        // [PATCH]

    // This division must leave no remainder
    if (m_nSectorsPerTrack % m_nGranulesPerTrack != 0)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // GranulesPerCylinder must be 8 tops
    if (m_nGranulesPerCylinder > 8)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // Check directory structure
    if (!(m_dwFlags & V80_FLAG_CHKDIR))
        if ((dwError = CheckDir()) != NO_ERROR)
            goto Done;

    // Set GAT bits used by System Files    [PATCH]
    if (!(m_dwFlags & V80_FLAG_GATFIX))
        if ((dwError = FixGAT()) != NO_ERROR)
            goto Done;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Create a new file with the indicated name and size
//---------------------------------------------------------------------------------

DWORD CTD3::Create(void** pFile, OSI_FILE& File)
{

    TD4_EXTENT  Extent;
    BYTE        nSectors;
    BYTE        nGranules;
    BYTE        nExtent = 0;
    DWORD       dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Get a new directory entry
    if ((dwError = GetFDE(pFile)) != NO_ERROR)
        goto Abort;

    // Set directory entry as Active
    ((TD3_FPDE*)(*pFile))->nAttributes = TD4_ATTR0_ACTIVE;  // [PATCH]

    // Set file password
    ((TD3_FPDE*)(*pFile))->wOwnerHash = 0x5CEF; // [PATCH]
    ((TD3_FPDE*)(*pFile))->wUserHash = 0x5CEF;  // [PATCH]

    // Set file properties as indicated by the caller
    SetFile(*pFile, File, false);

    // Calculate number of sectors needed [PATCH]
    nSectors = ((TD3_FPDE*)(*pFile))->wERN + (((TD3_FPDE*)(*pFile))->nEOF > 0 ? 1 : 0);

    // Calculate number of granules needed
    nGranules =  nSectors / m_nSectorsPerGranule + (nSectors % m_nSectorsPerGranule > 0 ? 1 : 0);   // [PATCH]

    // Repeat while number of needed granules is greater than zero
    while (nGranules > 0)
    {

        // Create one extent with as many granules as possible, based on the calculated quantity
        if ((dwError = CreateExtent(Extent, nGranules)) != NO_ERROR)
            goto Abort;

        // Increment extent counter
        nExtent++;

        // Copy extent data to the directory entry
        if ((dwError = CopyExtent(*pFile, TD4_EXTENT_SET, nExtent, Extent)) != NO_ERROR)
            goto Abort; // [PATCH]

        // Subtract number of granules allocated in this extent from the total required
        nGranules -= (Extent.nGranules & TD4_GRANULE_COUNT);    // [PATCH]

    }

    // Write updated directory data and exit
    dwError = DirRW(TD4_DIR_WRITE);
    goto Done;

    // Restore previous directory state
    Abort:
    DirRW(TD4_DIR_READ);

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Move the file pointer
//---------------------------------------------------------------------------------

DWORD CTD3::Seek(void* pFile, DWORD dwPos)
{

    TD4_EXTENT  Extent;
    int         nGranuleSectors;
    int         nRemainingSectors;
    int         nExtent = 0;
    DWORD       dwError = NO_ERROR;

    // Calculate number of sectors required to reach the requested file position
    nRemainingSectors = dwPos / m_DG.LT.wSectorSize;

    while (true)
    {

        // Increment extent counter
        nExtent++;

        // Retrieve corresponding extent data
        if ((dwError = CopyExtent(pFile, TD4_EXTENT_GET, nExtent, Extent)) != NO_ERROR)
        {
            m_wSector = 0xFFFF;
            break;
        }

        // Calculate number of contiguous sectors contained in this extent
        nGranuleSectors = (Extent.nGranules & TD4_GRANULE_COUNT) * m_nSectorsPerGranule;    // [PATCH]

        // Check whether it exceeds our need
        if (nGranuleSectors >= nRemainingSectors)
        {   // RelativeSector = (Cylinder * GranulesPerCylinder + InitialGranule) * SectorsPerGranule + RemainingSectors
            m_wSector = ((Extent.nCylinder * m_nGranulesPerCylinder) + ((Extent.nGranules & TD4_GRANULE_INITIAL) >> 5)) * m_nSectorsPerGranule + nRemainingSectors;
            m_dwFilePos = dwPos;
            break;
        }

        // Otherwise, subtract number of extent sectors from the total required
        nRemainingSectors -= nGranuleSectors;

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Get file information
//---------------------------------------------------------------------------------

void CTD3::GetFile(void* pFile, OSI_FILE& File)
{

    // Zero the structure
    memset(&File, 0, sizeof(OSI_FILE));

    // Copy file name and extention
    memcpy(File.szName, ((TD3_FPDE*)pFile)->cName, sizeof(File.szName) - 1);
    memcpy(File.szType, ((TD3_FPDE*)pFile)->cType, sizeof(File.szType) - 1);

    // Get file size
    File.dwSize = GetFileSize(pFile);

    // Get file date
    File.Date.wYear     = ((TD3_FPDE*)pFile)->nYear + (((TD3_FPDE*)pFile)->nYear >= 70 ? 1900 : 2000);  // [PATCH]
    File.Date.nMonth    = ((TD3_FPDE*)pFile)->nMonth;                                                   // [PATCH]
    File.Date.nDay      = 0;    // Not supported in TRSDOS 1.3                                          // [PATCH]

    // If not a valid date, make it all zeroes
    if (File.Date.nMonth < 1 || File.Date.nMonth > 12) // || File.Date.nDay < 1 || File.Date.nDay > 31) [PATCH]
        memset(&File.Date, 0, sizeof(File.Date));

    // Get file attributes
    File.nAccess    = ((TD3_FPDE*)pFile)->nAttributes & TD4_ATTR0_ACCESS;       // [PATCH]
    File.bSystem    = ((TD3_FPDE*)pFile)->nAttributes & TD4_ATTR0_SYSTEM;       // [PATCH]
    File.bInvisible = ((TD3_FPDE*)pFile)->nAttributes & TD4_ATTR0_INVISIBLE;    // [PATCH]
    File.bModified  = false;                                                    // [PATCH]

}

//---------------------------------------------------------------------------------
// Set file information (protected)
//---------------------------------------------------------------------------------

DWORD CTD3::SetFile(void* pFile, OSI_FILE& File, bool bCommit)
{

    // Copy file name and extention
    memcpy(((TD3_FPDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
    memcpy(((TD3_FPDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

    // Set corresponding HIT DEC to the newly calculated name hash
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash((const char *)((TD3_FPDE*)pFile)->cName);

    // Set file size
    ((TD3_FPDE*)pFile)->nEOF = File.dwSize % m_DG.LT.wSectorSize;
    ((TD3_FPDE*)pFile)->wERN = File.dwSize / m_DG.LT.wSectorSize;   // + (((TD3_FPDE*)pFile)->nEOF > 0 ? 1 : 0);   // [PATCH]

    // Set file date
    ((TD3_FPDE*)pFile)->nYear   = File.Date.wYear - (File.Date.wYear != 0 ? 1900 : 0);  // [PATCH]
    ((TD3_FPDE*)pFile)->nMonth  = File.Date.nMonth;                                     // [PATCH]

    // Set file attributes

    ((TD3_FPDE*)pFile)->nAttributes &= ~TD4_ATTR0_ACCESS;                       // [PATCH]
    ((TD3_FPDE*)pFile)->nAttributes |= File.nAccess;                            // [PATCH]

    ((TD3_FPDE*)pFile)->nAttributes &= ~TD4_ATTR0_SYSTEM;                       // [PATCH]
    ((TD3_FPDE*)pFile)->nAttributes |= (TD4_ATTR0_SYSTEM * File.bSystem);       // [PATCH]

    ((TD3_FPDE*)pFile)->nAttributes &= ~TD4_ATTR0_INVISIBLE;                    // [PATCH]
    ((TD3_FPDE*)pFile)->nAttributes |= (TD4_ATTR0_INVISIBLE * File.bInvisible); // [PATCH]

    return (bCommit ? DirRW(TD4_DIR_WRITE) : NO_ERROR);

}

//---------------------------------------------------------------------------------
// Get file size
//---------------------------------------------------------------------------------

DWORD CTD3::GetFileSize(void* pFile)
{
    return (((TD3_FPDE*)pFile)->wERN * m_DG.LT.wSectorSize + ((TD3_FPDE*)pFile)->nEOF); // [PATCH]
}

//---------------------------------------------------------------------------------
// Fix GAT according to HIT System Files
//---------------------------------------------------------------------------------

DWORD CTD3::FixGAT()
{

    TD3_SYS Vector;
    BYTE    nCurrentGranule;
    DWORD   dwError = NO_ERROR;

    // Calculate number of valid cylinders
    int nValidCylinders = m_DG.LT.nTrack - m_DG.FT.nTrack + 1;

    for (BYTE nPair = 16; nPair > 0; nPair--)
    {

        memcpy(&Vector, &m_pDir[m_DG.LT.wSectorSize * 2 - nPair * 2], sizeof(Vector));

        // Skip vectors filled with 0xFFFF
        if (Vector.nCylinder > nValidCylinders)
            continue;

        // Set initial granule
        nCurrentGranule = Vector.nGranule;

        while (Vector.nGranules > 0)
        {

            // Set the granule as used
            m_pDir[Vector.nCylinder] |= (1 << nCurrentGranule);

            // Advance to next granule until all granules are set
            nCurrentGranule++;
            Vector.nGranules--;

            // Check whether we've reached the cylinder's limit
            if (nCurrentGranule == m_nGranulesPerCylinder)
            {

                // Reset granule index and advance to the next cylinder
                nCurrentGranule = 0;
                Vector.nCylinder++;

                // Check whether we've reached the disk's limit
                if (Vector.nCylinder == nValidCylinders)
                {
                    dwError = ERROR_DISK_FULL;
                    goto Done;
                }

            }

        }

    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Scan the Hash Index Table
//---------------------------------------------------------------------------------

DWORD CTD3::ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash)
{

    static int nLastCol = -1;
    static int nLastRow = 0;

    int nCols, nCol;
    int nRows, nRow;
    int nSlot;

    BYTE nEntriesPerSector = m_DG.LT.wSectorSize / sizeof(TD3_FPDE);

    DWORD dwError = NO_ERROR;

    // If mode is any "Find First", reset static variables
    if (nMode != TD4_HIT_FIND_NEXT_USED)
    {
        nLastCol = -1;
        nLastRow = 0;
    }

    // Retrieve last values from static variables
    nCol = nLastCol;
    nRow = nLastRow;

    // Calculates max HIT columns and rows
    nCols = m_nDirSectors - 2;
    nRows = m_DG.LT.wSectorSize / sizeof(TD3_FPDE); // [PATCH]

    while (true)
    {

        // Increment column
        nCol++;

        // If column reaches max, reset it and advance to the next row
        if (nCol >= nCols)
        {
            nCol = 0;
            nRow++;
        }

        // If row reaches max, then we have reached the end of the HIT
        if (nRow >= nRows)
        {
            if (nHash != 0)
                dwError = ERROR_FILE_NOT_FOUND;
            else if (nMode == TD4_HIT_FIND_FIRST_FREE)
                dwError = ERROR_DISK_FULL;
            else
                dwError = ERROR_NO_MORE_FILES;
            break;
        }

        // Get value in HIT[Row * Cols + Col]
        nSlot = m_pDir[m_DG.LT.wSectorSize + (nRow * nCols) + nCol];

        // If this is not what we are looking for, skip to the next
        if ((nMode == TD4_HIT_FIND_FIRST_FREE && nSlot != 0) || (nMode != TD4_HIT_FIND_FIRST_FREE && nSlot == 0))
            continue;

        // If there is a hash to match and it doesn't match, skip to the next
        if (nHash != 0 && nHash != nSlot)
            continue;

        // Return pointer corresponding to the current Directory Entry Code (DEC)
        if ((*pFile = DEC2FDE((((nRow * nCols + nCol) % nEntriesPerSector) << 5) + ((nRow * nCols + nCol) / nEntriesPerSector))) == NULL)   // [PATCH]
//        if ((*pFile = DEC2FDE((nRow << 5) + nCol)) == NULL)
            dwError = ERROR_INVALID_ADDRESS;

        break;

    }

    // Update static variables
    nLastCol = nCol;
    nLastRow = nRow;

    return dwError;

}

//---------------------------------------------------------------------------------
// Allocate disk space
//---------------------------------------------------------------------------------

DWORD CTD3::CreateExtent(TD4_EXTENT& Extent, BYTE nGranules)
{

    BYTE    nValidCylinders;
    BYTE    nInitialCylinder = 0;
    BYTE    nInitialGranule = 0;
    BYTE    nCurrentCylinder = 0;
    BYTE    nCurrentGranule = 0;
    BYTE    nAllocatedGranules = 0;
    DWORD   dwError = NO_ERROR;

    // Requested number of granules must be greater than zero
    if (nGranules == 0)
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto Done;
    }

    // Calculate number of valid cylinders
    nValidCylinders = m_DG.LT.nTrack - m_DG.FT.nTrack + 1;

    // Find first empty slot (0) then continue up to the first non-empty slot (1)
    for (int nExpectedBit = 0; nExpectedBit < 2; nExpectedBit++)
    {

        // Test state of 'CurrentGranule' at GAT[CurrentCylinder]
        while ((m_pDir[nCurrentCylinder] & (1 << nCurrentGranule)) != nExpectedBit)
        {

            // Check if we are in the "continue up to the first non-empty slot" phase
            if (nExpectedBit == 1)
            {

                // Set the granule as reserved
                m_pDir[nCurrentCylinder] |= (1 << nCurrentGranule);

                nAllocatedGranules++;
                nGranules--;

                // Count of allocated granules must fit in 5 bits (so the max is 32)
                // Also exit when number of requested granules have been reached
                if (nAllocatedGranules == 32 || nGranules == 0)
                    goto Stop;

            }

            // Advance to next granule
            nCurrentGranule++;

            // Check whether we've reached the cylinder's limit
            if (nCurrentGranule == m_nGranulesPerCylinder)
            {

                // Reset granule index and advance to the next cylinder
                nCurrentGranule = 0;
                nCurrentCylinder++;

                // Check whether we've reached the disk's limit
                if (nCurrentCylinder == nValidCylinders)
                {
                    dwError = ERROR_DISK_FULL;
                    goto Done;
                }

            }

        }

        // Check whether we are just leaving the "find the first empty slot" phase
        if (nExpectedBit == 0)
        {
            nInitialCylinder = nCurrentCylinder;
            nInitialGranule = nCurrentGranule;
        }

    }

    // Assemble Extent
    Stop:
    Extent.nCylinder = nInitialCylinder;
    Extent.nGranules = (nInitialGranule << 5) + nAllocatedGranules; // 3 MSB: Initial Granule, 5 LSB: Contiguous Granules [PATCH]

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Release disk space
//---------------------------------------------------------------------------------

DWORD CTD3::DeleteExtent(TD4_EXTENT& Extent)
{

    DWORD dwError = NO_ERROR;

    // Get initial granule and count of granules in the extent
    int nIndex = (Extent.nGranules & TD4_GRANULE_INITIAL) >> 5;
    int nCount = (Extent.nGranules & TD4_GRANULE_COUNT);    // [PATCH]

    // Calculate number of valid cylinders
    int nCylinders = m_DG.LT.nTrack - m_DG.FT.nTrack + 1;

    while (true)
    {

        // Reset bit nIndex of GAT[nCylinder]
        m_pDir[Extent.nCylinder] &= ~(1 << nIndex);

        // Repeat until nCount equals zero
        if (--nCount == 0)
            break;

        // If nIndex reaches maximum number of granules per cylinder
        if (++nIndex == m_nGranulesPerCylinder)
        {

            // Reset nIndex and advance to the next cylinder
            nIndex = 0;

            // If have reached the end of the disk, then something is wrong
            if (++Extent.nCylinder == nCylinders)
            {
                dwError = ERROR_FLOPPY_WRONG_CYLINDER;
                break;
            }

        }

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Get or Set extent data
//---------------------------------------------------------------------------------

DWORD CTD3::CopyExtent(void* pFile, TD4_EXT nMode, BYTE nExtent, TD4_EXTENT& Extent)
{

    TD4_EXTENT* pExtent;
    DWORD       dwError = NO_ERROR;

    // Go through the extents table
    for (int x = 1, y = 1; x < 14; x++)  // [PATCH]
    {

        // Point to File.Extent[x-1]
        pExtent = &(((TD3_FPDE*)pFile)->Extent[x - 1]);

        // Check whether we've reached the end of the extents table while trying to GET an extent's data
        if (pExtent->nCylinder == 0xFF && nMode == TD4_EXTENT_GET)
            break;

        // Check whether we've reached the requested extent number (but not a corrupted link)
        if (y == nExtent)   // [PATCH]
        {
            if (nMode == TD4_EXTENT_GET)
            {
                Extent.nCylinder = pExtent->nCylinder;
                Extent.nGranules = pExtent->nGranules;
            }
            else
            {
                pExtent->nCylinder = Extent.nCylinder;
                pExtent->nGranules = Extent.nGranules;
            }
            goto Done;
        }

        // Advance to next extent
        y++;

    }

    dwError = ERROR_NO_MATCH;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to an available File Directory Entry
//---------------------------------------------------------------------------------

DWORD CTD3::GetFDE(void** pFile)
{

    DWORD dwError = NO_ERROR;

    if ((dwError = ScanHIT(pFile, TD4_HIT_FIND_FIRST_FREE)) != NO_ERROR)
        goto Done;

    memset(*pFile, 0, sizeof(TD3_FPDE));    // [PATCH]

    for (int x = 0; x < 13; x++)            // [PATCH]
    {
        ((TD3_FPDE*)(*pFile))->Extent[x].nCylinder = 0xFF;
        ((TD3_FPDE*)(*pFile))->Extent[x].nGranules = 0xFF;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
//---------------------------------------------------------------------------------

void* CTD3::DEC2FDE(BYTE nDEC)
{

    DWORD dwSectorOffset = ((nDEC & TD4_DEC_SECTOR) + 2) * m_DG.LT.wSectorSize;
    DWORD dwEntryOffset = ((nDEC & TD4_DEC_ENTRY) >> 5) * sizeof(TD3_FPDE); // [PATCH]

    return ((nDEC & TD4_DEC_SECTOR) < m_nDirSectors ? &m_pDir[dwSectorOffset + dwEntryOffset] : NULL);

}
/*
void* CTD3::DEC2FDE(BYTE nDEC)
{

    BYTE    nEntriesPerSector = m_DG.LT.wSectorSize / sizeof(TD3_FPDE);             // [PATCH]
    DWORD   dwSectorOffset = (nDEC / nEntriesPerSector + 2) * m_DG.LT.wSectorSize;  // [PATCH]
    DWORD   dwEntryOffset = (nDEC % nEntriesPerSector) * sizeof(TD3_FPDE);          // [PATCH]

    return ((nDEC / nEntriesPerSector) < m_nDirSectors ? &m_pDir[dwSectorOffset + dwEntryOffset] : NULL);   // [PATCH]

}
*/

//---------------------------------------------------------------------------------
// Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
//---------------------------------------------------------------------------------

BYTE CTD3::FDE2DEC(void* pFile)
{

    // Offset = ((pFPDE - pDir) - (GAT + HIT))
    DWORD dwOffset = (((BYTE*)pFile - m_pDir) - (2 * m_DG.LT.wSectorSize)); // [PATCH]

    // Sector =  Offset / SectorSize
    BYTE nSector =  dwOffset / m_DG.LT.wSectorSize;                         // [PATCH]

    // Entry = (Offset % SectorSize) / sizeof(FPDE)
    BYTE nEntry = (dwOffset % m_DG.LT.wSectorSize) / sizeof(TD3_FPDE);      // [PATCH]

    // EntriesPerSector = SectorSize / sizeof(FPDE)
    BYTE nEntriesPerSector = m_DG.LT.wSectorSize / sizeof(TD3_FPDE);        // [PATCH]

    // DEC = (Sector * EntriesPerSector) + Entry
    BYTE nDEC = nSector * nEntriesPerSector + nEntry;                       // [PATCH]

    return nDEC;                                                            // [PATCH]

}
