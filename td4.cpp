/**
 @file td4.cpp

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
// Operating System Interface for TRSDOS (Model 4)
//---------------------------------------------------------------------------------

#include "windows.h"
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "td4.h"

//---------------------------------------------------------------------------------
// Initialize member variables
//---------------------------------------------------------------------------------

CTD4::CTD4()
:   m_pDir(NULL), m_nDirTrack(0), m_nDirSectors(0), m_nMaxDirSectors(0), m_nSides(0), m_nSectorsPerTrack(0),
    m_nGranulesPerTrack(0), m_nGranulesPerCylinder(0), m_nSectorsPerGranule(0), m_dwFilePos(0), m_wSector(0), m_Buffer()
{
}

//---------------------------------------------------------------------------------
// Release allocated memory
//---------------------------------------------------------------------------------

CTD4::~CTD4()
{
    if (m_pDir != NULL)
        free(m_pDir);
}

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CTD4::Load(CVDI* pVDI, DWORD dwFlags)
{

    BYTE    nSides;
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
        m_nSides = m_DG.LT.nLastSide - m_DG.LT.nFirstSide + 1;

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
    m_nDirTrack = m_Buffer[2] & 0x7F;

    // Calculate the number of directory sectors
    m_nDirSectors = m_nSectorsPerTrack * m_nSides;

    // Max Dir Sectors = HIT Size / Entries per Sector
    m_nMaxDirSectors = (m_DG.LT.wSectorSize / (BYTE)(m_DG.LT.wSectorSize / sizeof(TD4_FPDE)));

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

    // Calculate DOS disk parameters

    nSides = ((((TD4_GAT*)m_pDir)->nDiskConfig & TD4_GAT_SIDES) >> 5) + 1;

    if (!(m_dwFlags & (V80_FLAG_SS+V80_FLAG_DS)) && (nSides < m_nSides))
        m_nSides = nSides;

    m_nGranulesPerTrack = (((TD4_GAT*)m_pDir)->nDiskConfig & TD4_GAT_GRANULES) + 1;

    m_nGranulesPerCylinder = m_nGranulesPerTrack * m_nSides;

    m_nSectorsPerGranule = m_nSectorsPerTrack / m_nGranulesPerTrack;

    // GranulesPerCylinder must fit in a 8-bit GAT byte
    if (m_nGranulesPerCylinder < 2 || m_nGranulesPerCylinder > 8)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // This division must leave no remainder
    if (m_nSectorsPerTrack % m_nGranulesPerTrack != 0)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // SectorsPerGranule must be either 5 or 8 for SD disks
    if (m_DG.LT.nDensity == VDI_DENSITY_SINGLE && m_nSectorsPerGranule != 5 && m_nSectorsPerGranule != 8)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // SectorsPerGranule must be either 6 or 10 for DD disks
    if (m_DG.LT.nDensity == VDI_DENSITY_DOUBLE && m_nSectorsPerGranule != 6 && m_nSectorsPerGranule != 10)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // Check directory structure
    if (!(m_dwFlags & V80_FLAG_CHKDIR))
        if ((dwError = CheckDir()) != NO_ERROR)
            goto Done;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the first/next directory entry
//---------------------------------------------------------------------------------

DWORD CTD4::Dir(void** pFile, OSI_DIR nFlag)
{


    TD4_HIT nMode = (nFlag == OSI_DIR_FIND_FIRST ? TD4_HIT_FIND_FIRST_USED : TD4_HIT_FIND_NEXT_USED);

    DWORD dwError = NO_ERROR;

    BYTE x;
    do
    {

        dwError = ScanHIT(pFile, nMode);
        // Return a pointer to the first/next non-empty file entry
        if (dwError != NO_ERROR)
            break;

        // Change the mode to "next" for the remaining searches
        nMode = TD4_HIT_FIND_NEXT_USED;


        x = ((TD4_FPDE*)(*pFile))->nAttributes[0];

    }   // Repeat until we get a pointer to a file entry which is Active and Primary
    while (!(((TD4_FPDE*)(*pFile))->nAttributes[0] & TD4_ATTR0_ACTIVE) || (((TD4_FPDE*)(*pFile))->nAttributes[0] & TD4_ATTR0_EXTENDED));

    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the directory entry matching the file name
//---------------------------------------------------------------------------------

DWORD CTD4::Open(void** pFile, const char cName[11])
{

    TD4_HIT nMode = TD4_HIT_FIND_FIRST_USED;

    BYTE nHash = Hash(cName);

    DWORD dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Repeat while there is no file match
    while (true)
    {

        // Get first/next file whose hash matches the requested name
        if ((dwError = ScanHIT(pFile, nMode, nHash)) != NO_ERROR)
            break;

        // Change mode to "next" for the remaining searches
        nMode = TD4_HIT_FIND_NEXT_USED;

        // If file entry is not Active or Primary, skip to the next
        if (!(((TD4_FPDE*)(*pFile))->nAttributes[0] & TD4_ATTR0_ACTIVE) || (((TD4_FPDE*)(*pFile))->nAttributes[0] & TD4_ATTR0_EXTENDED))
            continue;

        // If entry's filename matches the caller's filename, we are done
        // FIXME if (memcmp(((TD4_FPDE*)(*pFile))->cName, cName, sizeof(cName)) == 0)
        if (memcmp(((TD4_FPDE*)(*pFile))->cName, cName, 11) == 0)
            break;

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Create a new file with the indicated name and size
//---------------------------------------------------------------------------------

DWORD CTD4::Create(void** pFile, OSI_FILE& File)
{

    TD4_EXTENT  Extent;
    void*       pPrevious;
    BYTE        nGranules;
    BYTE        nExtent = 0;
    DWORD       dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Get a new directory entry
    if ((dwError = GetFDE(pFile)) != NO_ERROR)
        goto Abort;

    // Set directory entry as Active
    ((TD4_FPDE*)(*pFile))->nAttributes[0] = TD4_ATTR0_ACTIVE;

    // Set file password
    ((TD4_FPDE*)(*pFile))->wOwnerHash = 0x4296;
    ((TD4_FPDE*)(*pFile))->wUserHash = 0x4296;

    // Set file properties as indicated by the caller
    SetFile(*pFile, File, false);

    // Calculate the number of granules needed

    nGranules = ((TD4_FPDE*)(*pFile))->wERN / m_nSectorsPerGranule + (((TD4_FPDE*)(*pFile))->wERN % m_nSectorsPerGranule > 0 ? 1 : 0);

    if (nGranules == 0 && ((TD4_FPDE*)(*pFile))->nEOF > 0)
        nGranules++;

    // Repeat while the number of needed granules is greater than zero
    while (nGranules > 0)
    {

        // Create one extent with as many granules as possible, based on the calculated quantity
        if ((dwError = CreateExtent(Extent, nGranules)) != NO_ERROR)
            goto Abort;

        // Increment extent counter
        nExtent++;

        // Copy extent data to the directory entry
        if ((dwError = CopyExtent(*pFile, TD4_EXTENT_SET, nExtent, Extent)) != NO_ERROR)
        {

            // Abort if error is other than "extent not found"
            if (dwError != ERROR_NOT_FOUND)
                goto Abort;

            // Preserve current file handle
            pPrevious = *pFile;

            // Get an additional directory entry
            if ((dwError = GetFDE(pFile)) != NO_ERROR)
                goto Abort;

            // Set a forward link to the newly created directory entry
            ((TD4_FPDE*)pPrevious)->Link.nCylinder = 0xFE;
            ((TD4_FPDE*)pPrevious)->Link.nGranules = FDE2DEC(*pFile);

            // Set a backward link from the previously existing directory entry
            ((TD4_FPDE*)(*pFile))->nAttributes[0] = TD4_ATTR0_ACTIVE|TD4_ATTR0_EXTENDED;
            ((TD4_FPDE*)(*pFile))->nAttributes[1] = FDE2DEC(pPrevious);

            // Copy file name and extention to the new directory entry
            memcpy(((TD4_FPDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
            memcpy(((TD4_FPDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

            // Set the corresponding HIT DEC with calculated name hash
            m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash((const char *) ((TD4_FPDE*)pFile)->cName);

            continue;

        }

        // Subtract number of allocated granules in this extent from the total required
        nGranules -= (Extent.nGranules & TD4_GRANULE_COUNT) + 1;

    }

    // Write the updated directory data and exit
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

DWORD CTD4::Seek(void* pFile, DWORD dwPos)
{

    TD4_EXTENT  Extent;
    int         nGranuleSectors;
    int         nRemainingSectors;
    int         nExtent = 0;
    DWORD       dwError = NO_ERROR;

    // Calculate the number of sectors required to reach the requested file position
    nRemainingSectors = dwPos / m_DG.LT.wSectorSize;

    while (true)
    {

        // Increment extent counter
        nExtent++;

        // Retrieve the corresponding extent data
        if ((dwError = CopyExtent(pFile, TD4_EXTENT_GET, nExtent, Extent)) != NO_ERROR)
        {
            m_wSector = 0xFFFF;
            break;
        }

        // Calculate the number of contiguous sectors contained in this extent
        nGranuleSectors = ((Extent.nGranules & TD4_GRANULE_COUNT) + 1) * m_nSectorsPerGranule;

        // Check whether it exceeds our need
        if (nGranuleSectors > nRemainingSectors)
        {   // RelativeSector = (Cylinder * GranulesPerCylinder + InitialGranule) * SectorsPerGranule + RemainingSectors
            m_wSector = ((Extent.nCylinder * m_nGranulesPerCylinder) + ((Extent.nGranules & TD4_GRANULE_INITIAL) >> 5)) * m_nSectorsPerGranule + nRemainingSectors;
            m_dwFilePos = dwPos;
            break;
        }

        // Otherwise, subtract the number of extent sectors from the total required
        nRemainingSectors -= nGranuleSectors;

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Read data from file
//---------------------------------------------------------------------------------

DWORD CTD4::Read(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
{

    VDI_TRACK*  pTrack;
    BYTE        nTrack;
    BYTE        nSide;
    BYTE        nSector;
    DWORD       dwFileSize;
    WORD        wSectorBegin;
    WORD        wSectorEnd;
    WORD        wLength;
    DWORD       dwRead = 0;
    DWORD       dwError = NO_ERROR;

    // Get file size
    dwFileSize = GetFileSize(pFile);

    // Repeat while there is something left to read
    while (dwBytes - dwRead > 0)
    {

        // Check whether the last Seek() was successful
        if (m_wSector == 0xFFFF)
        {
            dwError = ERROR_SEEK;
            break;
        }

        // Check whether the requested position doesn't exceed the file size
        if (m_dwFilePos > dwFileSize)
        {
            dwError = ERROR_READ_FAULT;
            break;
        }

        // Convert relative sector into Track, Side, Sector
        CHS(m_wSector, nTrack, nSide, nSector);

        // Get a pointer to the correct track descriptor
        pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

        // Read sector into internal buffer
        if ((dwError = m_pVDI->Read(nTrack, nSide, nSector, m_Buffer, pTrack->wSectorSize)) != NO_ERROR)
            break;

        // Calculate the starting transfer point
        wSectorBegin = m_dwFilePos % pTrack->wSectorSize;

        // Calculate the ending transfer point
        wSectorEnd = (dwFileSize - m_dwFilePos >= pTrack->wSectorSize ? pTrack->wSectorSize : dwFileSize % pTrack->wSectorSize);

        // Calculate the transfer length
        wLength = ((wSectorEnd - wSectorBegin) <= (dwBytes - dwRead) ? wSectorEnd - wSectorBegin : dwBytes - dwRead);

        // Copy data from internal buffer to the caller's buffer
        memcpy(pBuffer, &m_Buffer[wSectorBegin], wLength);

        // Update bytes count and caller's buffer pointer
        dwRead += wLength;
        pBuffer += wLength;

        // Advance file pointer
        Seek(pFile, m_dwFilePos + wLength);

    }

    dwBytes = dwRead;

    return dwError;

}

//---------------------------------------------------------------------------------
// Save data to file
//---------------------------------------------------------------------------------

DWORD CTD4::Write(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
{

    VDI_TRACK*  pTrack;
    BYTE        nTrack;
    BYTE        nSide;
    BYTE        nSector;
    DWORD       dwFileSize;
    WORD        wSectorBegin;
    WORD        wSectorEnd;
    WORD        wLength;
    DWORD       dwWritten = 0;
    DWORD       dwError = NO_ERROR;

    dwFileSize = GetFileSize(pFile);

    while (dwBytes - dwWritten > 0)
    {

        // Check whether the last Seek() was successful
        if (m_wSector == 0xFFFF)
        {
            dwError = ERROR_SEEK;
            break;
        }

        // Check whether the requested position doesn't exceed the file size
        if (m_dwFilePos > dwFileSize)
        {
            dwError = ERROR_WRITE_FAULT;
            break;
        }

        // Convert relative sector into Track, Side, Sector
        CHS(m_wSector, nTrack, nSide, nSector);

        // Get a pointer to the correct track descriptor
        pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

        // Calculate the starting transfer point
        wSectorBegin = m_dwFilePos % pTrack->wSectorSize;

        // Calculate the ending transfer point
        wSectorEnd = (dwFileSize - m_dwFilePos >= pTrack->wSectorSize ? pTrack->wSectorSize : dwFileSize % pTrack->wSectorSize);

        // Calculate the transfer length
        wLength = ((wSectorEnd - wSectorBegin) <= (dwBytes - dwWritten) ? wSectorEnd - wSectorBegin : dwBytes - dwWritten);

        // If needed, read sector before writing to it
        if (wLength < pTrack->wSectorSize)
        {
            if ((dwError = m_pVDI->Read(nTrack, nSide, nSector, m_Buffer, pTrack->wSectorSize)) != NO_ERROR)
                break;
        }

        // Copy data from caller's buffer to the internal buffer
        memcpy(&m_Buffer[wSectorBegin], pBuffer, wLength);

        // Write sector from internal buffer
        if ((dwError = m_pVDI->Write(nTrack, nSide, nSector, m_Buffer, pTrack->wSectorSize)) != NO_ERROR)
            break;

        // Update bytes count and caller's buffer pointer
        dwWritten += wLength;
        pBuffer += wLength;

        // Advance file pointer
        Seek(pFile, m_dwFilePos + wLength);

    }

    dwBytes = dwWritten;

    return dwError;

}

//---------------------------------------------------------------------------------
// Delete file
//---------------------------------------------------------------------------------

DWORD CTD4::Delete(void* pFile)
{

    TD4_EXTENT  Extent;
    DWORD       dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Inactivate directory entry
    ((TD4_FPDE*)pFile)->nAttributes[0] &= !TD4_ATTR0_ACTIVE;

    // Release corresponding HIT slot
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = 0;

    // Loop through all extents releasing every allocated granule
    for (int x = 1; (dwError = CopyExtent(pFile, TD4_EXTENT_GET, x, Extent)) == NO_ERROR; x++)
    {
        if ((dwError = DeleteExtent(Extent)) != NO_ERROR)
            break;
    }

    // If exited the loop because reached the end of the extents table
    if (dwError == ERROR_NO_MATCH)
        dwError = DirRW(TD4_DIR_WRITE); // Save the directory
    else
        DirRW(TD4_DIR_READ);            // Otherwise, restore its previous state

    return dwError;

}

//---------------------------------------------------------------------------------
// Get DOS information
//---------------------------------------------------------------------------------

void CTD4::GetDOS(OSI_DOS& DOS)
{

    // Zero the structure
    memset(&DOS, 0, sizeof(OSI_DOS));

    // Get DOS version
    DOS.nVersion = ((TD4_GAT*)m_pDir)->nDosVersion;

    // Get disk name
    memcpy(DOS.szName, ((TD4_GAT*)m_pDir)->cDiskName, sizeof(DOS.szName) - 1);

    // Get disk date
    memcpy(&DOS.szDate, ((TD4_GAT*)m_pDir)->cDiskDate, sizeof(DOS.szDate) - 1);

}

//---------------------------------------------------------------------------------
// Set DOS information
//---------------------------------------------------------------------------------

DWORD CTD4::SetDOS(OSI_DOS& DOS)
{

    // Set DOS version
    ((TD4_GAT*)m_pDir)->nDosVersion = DOS.nVersion;

    // Set disk name
    memcpy(((TD4_GAT*)m_pDir)->cDiskName, DOS.szName, sizeof(DOS.szName) - 1);

    // Set disk date
    memcpy(((TD4_GAT*)m_pDir)->cDiskDate, &DOS.szDate, sizeof(DOS.szDate) - 1);

    return DirRW(TD4_DIR_WRITE);

}

//---------------------------------------------------------------------------------
// Get file information
//---------------------------------------------------------------------------------

void CTD4::GetFile(void* pFile, OSI_FILE& File)
{

    // Zero the structure
    memset(&File, 0, sizeof(OSI_FILE));

    // Copy file name and extention
    memcpy(File.szName, ((TD4_FPDE*)pFile)->cName, sizeof(File.szName) - 1);
    memcpy(File.szType, ((TD4_FPDE*)pFile)->cType, sizeof(File.szType) - 1);

    // Get file size
    File.dwSize = GetFileSize(pFile);

    // Get file date
    File.Date.nMonth    = (((TD4_FPDE*)pFile)->nAttributes[1] & TD4_ATTR1_MONTH);
    File.Date.nDay      = (((TD4_FPDE*)pFile)->nAttributes[2] & TD4_ATTR2_DAY) >> 3;
    File.Date.wYear     = (((TD4_FPDE*)pFile)->nAttributes[2] & TD4_ATTR2_YEAR) + (File.Date.nMonth > 0 ? 1980 : 0);

    // If not a valid date, make it all zeroes
    if (File.Date.nMonth < 1 || File.Date.nMonth > 12 || File.Date.nDay < 1 || File.Date.nDay > 31)
        memset(&File.Date, 0, sizeof(File.Date));

    // Get file attributes
    File.nAccess    = ((TD4_FPDE*)pFile)->nAttributes[0] & TD4_ATTR0_ACCESS;
    File.bSystem    = ((TD4_FPDE*)pFile)->nAttributes[0] & TD4_ATTR0_SYSTEM;
    File.bInvisible = ((TD4_FPDE*)pFile)->nAttributes[0] & TD4_ATTR0_INVISIBLE;
    File.bModified  = ((TD4_FPDE*)pFile)->nAttributes[1] & TD4_ATTR1_MODIFIED;

}

//---------------------------------------------------------------------------------
// Set file information (public)
//---------------------------------------------------------------------------------

DWORD CTD4::SetFile(void* pFile, OSI_FILE& File)
{
    return SetFile(pFile, File, true);
}

//---------------------------------------------------------------------------------
// Set file information (protected)
//---------------------------------------------------------------------------------

DWORD CTD4::SetFile(void* pFile, OSI_FILE& File, bool bCommit)
{

    // Copy file name and extention
    memcpy(((TD4_FPDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
    memcpy(((TD4_FPDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

    // Set corresponding HIT DEC to the newly calculated name hash
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash((const char *) ((TD4_FPDE*)pFile)->cName);

    // Set file size
    ((TD4_FPDE*)pFile)->nEOF = File.dwSize % m_DG.LT.wSectorSize;
    ((TD4_FPDE*)pFile)->wERN = File.dwSize / m_DG.LT.wSectorSize + (((TD4_FPDE*)pFile)->nEOF > 0 ? 1 : 0);

    // Set file date

    ((TD4_FPDE*)pFile)->nAttributes[1] &= ~TD4_ATTR1_MONTH;
    ((TD4_FPDE*)pFile)->nAttributes[1] |= File.Date.nMonth;

    ((TD4_FPDE*)pFile)->nAttributes[2] &= ~TD4_ATTR2_DAY;
    ((TD4_FPDE*)pFile)->nAttributes[2] |= (File.Date.nDay << 3);

    ((TD4_FPDE*)pFile)->nAttributes[2] &= ~TD4_ATTR2_YEAR;
    ((TD4_FPDE*)pFile)->nAttributes[2] |= (File.Date.wYear - (File.Date.nMonth > 0 ? 1980 : 0));

    // Set file attributes

    ((TD4_FPDE*)pFile)->nAttributes[0] &= ~TD4_ATTR0_ACCESS;
    ((TD4_FPDE*)pFile)->nAttributes[0] |= File.nAccess;

    ((TD4_FPDE*)pFile)->nAttributes[0] &= ~TD4_ATTR0_SYSTEM;
    ((TD4_FPDE*)pFile)->nAttributes[0] |= (TD4_ATTR0_SYSTEM * File.bSystem);

    ((TD4_FPDE*)pFile)->nAttributes[0] &= ~TD4_ATTR0_INVISIBLE;
    ((TD4_FPDE*)pFile)->nAttributes[0] |= (TD4_ATTR0_INVISIBLE * File.bInvisible);

    ((TD4_FPDE*)pFile)->nAttributes[1] &= ~TD4_ATTR1_MODIFIED;
    ((TD4_FPDE*)pFile)->nAttributes[1] |= (TD4_ATTR1_MODIFIED * File.bModified);

    return (bCommit ? DirRW(TD4_DIR_WRITE) : NO_ERROR);

}

//---------------------------------------------------------------------------------
// Get file size
//---------------------------------------------------------------------------------

DWORD CTD4::GetFileSize(void* pFile)
{

    WORD wERN = ((TD4_FPDE*)pFile)->wERN;
    BYTE nEOF = ((TD4_FPDE*)pFile)->nEOF;

    return wERN * m_DG.LT.wSectorSize - (nEOF > 0 ? m_DG.LT.wSectorSize - nEOF : 0);

}

//---------------------------------------------------------------------------------
// Read or Write the entire directory
//---------------------------------------------------------------------------------

DWORD CTD4::DirRW(TD4_DIR nMode)
{

    DWORD   dwOffset = 0;
    DWORD   dwError = NO_ERROR;

    // Go through every side
    for (BYTE nSide = 0; nSide < m_nSides; nSide++)
    {   // Go through every sector
        for (BYTE nSector = m_DG.LT.nFirstSector; nSector <= m_DG.LT.nLastSector; nSector++, dwOffset += m_DG.LT.wSectorSize)
        {   // Read or write the sector according to the requested mode
            if (nMode == TD4_DIR_WRITE)
            {
                if ((dwError = m_pVDI->Write(m_nDirTrack, m_DG.LT.nFirstSide + nSide, nSector, &m_pDir[dwOffset], m_DG.LT.wSectorSize)) != NO_ERROR)
                    goto Done;
            }
            else
            {
                if ((dwError = m_pVDI->Read(m_nDirTrack, m_DG.LT.nFirstSide + nSide, nSector, &m_pDir[dwOffset], m_DG.LT.wSectorSize)) != NO_ERROR)
                    goto Done;
            }
        }
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Check the directory structure
//---------------------------------------------------------------------------------

DWORD CTD4::CheckDir(void)
{

    int     x;
    void*   pFile = NULL;
    int     nFiles = 0;
    DWORD   dwError = NO_ERROR;

    // Validate disk name
    for (x = 0; x < 8; x++)
    {
        if (((TD4_GAT*)m_pDir)->cDiskName[x] < ' ' || ((TD4_GAT*)m_pDir)->cDiskName[x] > 'z')
            goto Error;
    }

    // Validate disk date
    for (x = 0; x < 8; x++)
    {
        if (((TD4_GAT*)m_pDir)->cDiskDate[x] < ' ' || ((TD4_GAT*)m_pDir)->cDiskDate[x] > 'z')
            goto Error;
    }

    // Validate each file name in the directory (while counting the number of files)
    for (nFiles = 0; (dwError = Dir(&pFile, (pFile == NULL ? OSI_DIR_FIND_FIRST : OSI_DIR_FIND_NEXT))) == NO_ERROR; nFiles++)
    {

        // First 8 characters can be non-blanks
        for (x = 0; x < 8 && ((TD4_FPDE*)pFile)->cName[x] >= '0' && ((TD4_FPDE*)pFile)->cName[x] <= 'z'; x++);

        // But first one must be non-blank to be valid
        if (x == 0)
            goto Error;

        // If a blank is found, then only blanks can be used up to the end of the name
        for ( ; x < 8 && ((TD4_FPDE*)pFile)->cName[x] == ' '; x++);

        // If not, then this name is invalid
        if (x < 8)
            goto Error;

        // The extension can have up to 3 non-blank characters
        for (x = 0; x < 3 && ((TD4_FPDE*)pFile)->cType[x] >= '0' && ((TD4_FPDE*)pFile)->cType[x] <= 'z'; x++);

        // If a blank is found, then only blanks can be used up to the end of the extension
        for ( ; x < 3 && ((TD4_FPDE*)pFile)->cType[x] == ' '; x++);

        // If not, then this extension is invalid
        if (x < 3)
            goto Error;

    }

    // No error if exiting on "no more files"   // but found files
    if (dwError == ERROR_NO_MORE_FILES)         // && nFiles > 0)   // The disk may be empty
    {
        dwError = NO_ERROR;
        goto Done;
    }

    Error:
    dwError = ERROR_FILE_CORRUPT;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Scan the Hash Index Table
//---------------------------------------------------------------------------------


DWORD CTD4::ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash)
{

    static int nLastRow = -1;
    static int nLastCol = 0;

    int nRows, nRow;
    int nCols, nCol;
    int nSlot;

    DWORD dwError = NO_ERROR;

    // If mode is any "Find First", reset static variables
    if (nMode != TD4_HIT_FIND_NEXT_USED)
    {
        nLastRow = -1;
        nLastCol = 0;
    }

    // Retrieve last values from static variables
    nRow = nLastRow;
    nCol = nLastCol;

    // Calculate max HIT rows and columns
    nRows = m_DG.LT.wSectorSize / sizeof(TD4_FPDE);
    nCols = m_nDirSectors - 2;

    while (true)
    {

        // Increment row
        nRow++;

        // If row reaches max, reset it and advance to the next column
        if (nRow >= nRows)
        {
            nRow = 0;
            nCol++;
        }

        // If column reaches max, then we have reached the end of the HIT
        if (nCol >= nCols)
        {
            if (nHash != 0)
                dwError = ERROR_FILE_NOT_FOUND;
            else if (nMode == TD4_HIT_FIND_FIRST_FREE)
                dwError = ERROR_DISK_FULL;
            else
                dwError = ERROR_NO_MORE_FILES;
            break;
        }


        // Get value in HIT[Row * MaxDirSectors + Col] (MaxDirSectors normally equals 32)
        nSlot = m_pDir[m_DG.LT.wSectorSize + (nRow * m_nMaxDirSectors) + nCol];


        // If this is not what we are looking for, skip to the next
        if ((nMode == TD4_HIT_FIND_FIRST_FREE && nSlot != 0) || (nMode != TD4_HIT_FIND_FIRST_FREE && nSlot == 0))
            continue;


        // If there is a hash to match and it doesn't match, skip to the next
        if (nHash != 0 && nHash != nSlot)
            continue;

        if ((*pFile = DEC2FDE((nRow << 5) + nCol)) == NULL)

        // Return pointer corresponding to the current Directory Entry Code (DEC)
        if ((*pFile = DEC2FDE((nRow << 5) + nCol)) == NULL)
            dwError = ERROR_INVALID_ADDRESS;

        break;

    }

    // Update static variables
    nLastRow = nRow;
    nLastCol = nCol;

    return dwError;

}

//---------------------------------------------------------------------------------
// Allocate disk space
//---------------------------------------------------------------------------------

DWORD CTD4::CreateExtent(TD4_EXTENT& Extent, BYTE nGranules)
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

    // Calculate the number of valid cylinders
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

                // Set granule as reserved
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
    Extent.nGranules = (nInitialGranule << 5) + (nAllocatedGranules - 1); // 3 MSB: Initial Granule, 5 LSB: Contiguous Granules minus 1

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Release disk space
//---------------------------------------------------------------------------------

DWORD CTD4::DeleteExtent(TD4_EXTENT& Extent)
{

    DWORD dwError = NO_ERROR;

    // Get initial granule and count of granules in the extent
    int nIndex = (Extent.nGranules & TD4_GRANULE_INITIAL) >> 5;
    int nCount = (Extent.nGranules & TD4_GRANULE_COUNT) + 1;

    // Calculate the number of valid cylinders
    int nCylinders = m_DG.LT.nTrack - m_DG.FT.nTrack + 1;

    while (true)
    {

        // Reset bit nIndex of GAT[nCylinder]
        m_pDir[Extent.nCylinder] &= ~(1 << nIndex);

        // Repeat until nCount equals zero
        if (--nCount == 0)
            break;

        // If nIndex reaches the maximum number of granules per cylinder
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

DWORD CTD4::CopyExtent(void* pFile, TD4_EXT nMode, BYTE nExtent, TD4_EXTENT& Extent)
{

    TD4_EXTENT* pExtent;
    DWORD       dwError = NO_ERROR;

    // Go through the extents table
    for (int x = 1, y = 1; x < 6; x++)
    {

        // Point to File.Extent[x-1]
        pExtent = &(((TD4_FPDE*)pFile)->Extent[x - 1]);

        // Check whether last extent links this entry to another one
        if (pExtent->nCylinder == 0xFE && x == 5)
        {
            // The other extent field contains the DEC to the extended directory entry (FXDE)
            if ((pFile = DEC2FDE(pExtent->nGranules)) == NULL)
            {
                dwError = ERROR_INVALID_ADDRESS;
                goto Done;
            }

            // Restart from the beginning of the extents list
            x = 0;
            continue;

        }

        // Check whether we've reached the end of the extents table while trying to GET an extent's data
        if (pExtent->nCylinder == 0xFF && nMode == TD4_EXTENT_GET)
            break;

        // Check whether we've reached the requested extent number (but not a corrupted link)
        if (y == nExtent && x != 5)
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

DWORD CTD4::GetFDE(void** pFile)
{

    DWORD dwError = NO_ERROR;

    if ((dwError = ScanHIT(pFile, TD4_HIT_FIND_FIRST_FREE)) != NO_ERROR)
        goto Done;

    memset(*pFile, 0, sizeof(TD4_FPDE));

    for (int x = 0; x < 5; x++)
    {
        ((TD4_FPDE*)(*pFile))->Extent[x].nCylinder = 0xFF;
        ((TD4_FPDE*)(*pFile))->Extent[x].nGranules = 0xFF;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
//---------------------------------------------------------------------------------

void* CTD4::DEC2FDE(BYTE nDEC)
{

    DWORD dwSectorOffset = ((nDEC & TD4_DEC_SECTOR) + 2) * m_DG.LT.wSectorSize;
    DWORD dwEntryOffset = ((nDEC & TD4_DEC_ENTRY) >> 5) * sizeof(TD4_FPDE);

    return ((nDEC & TD4_DEC_SECTOR) < m_nDirSectors ? &m_pDir[dwSectorOffset + dwEntryOffset] : NULL);

}

//---------------------------------------------------------------------------------
// Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
//---------------------------------------------------------------------------------

BYTE CTD4::FDE2DEC(void* pFile)
{

    // ((pFile - pDir) - ((GAT + HIT) * SectorSize)) / sizeof(FPDE)
    BYTE nDEC = (((BYTE*)pFile - m_pDir) - (2 * m_DG.LT.wSectorSize)) / sizeof(TD4_FPDE);

    // Reorganize bits: 11111000 -> 00011111
    return ((nDEC << 5) + (nDEC >> 3));

}

//---------------------------------------------------------------------------------
// Return the hash code of a given file name
//---------------------------------------------------------------------------------

BYTE CTD4::Hash(const char* pName)
{

    BYTE nHash = 0;

    for (int x = 0; x < 11; x++)
    {
        nHash ^= pName[x];
        nHash = (nHash << 1) + ((nHash & 0b10000000) >> 7); // rol nHash, 1
    }

    return (nHash != 0 ? nHash : 0x01);

}

//---------------------------------------------------------------------------------
// Return the Cylinder/Head/Sector (CHS) of a given relative sector
//---------------------------------------------------------------------------------

void CTD4::CHS(WORD wSector, BYTE& nTrack, BYTE& nSide, BYTE& nSector)
{

    // Calculate SectorsPerCylinder
    int nSectorsPerCylinder = m_nSectorsPerTrack * m_nSides;

    // Track = RelativeSector / SectorsPerCylinder
    nTrack = wSector / nSectorsPerCylinder;

    // Side = Remainder / SectorsPerTrack
    nSide = (wSector - (nTrack * nSectorsPerCylinder)) / m_nSectorsPerTrack;

    // Sector = Remainder
    nSector = (wSector - (nTrack * nSectorsPerCylinder + nSide * m_nSectorsPerTrack));

    // Adjust Track, Side, Sector
//    nTrack += m_DG.FT.nTrack;     // Track number in Extent.Cylinder is supposed to be correct (adjusted)
    nSide += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSide : m_DG.LT.nFirstSide);
    nSector += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSector : m_DG.LT.nFirstSector);

}
