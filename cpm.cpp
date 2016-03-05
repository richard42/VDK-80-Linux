//---------------------------------------------------------------------------------
// Operating System Interface for CP/M
//---------------------------------------------------------------------------------

#include "windows.h"
#include <math.h>
#include <stdio.h>
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "cpm.h"

void    Dump(unsigned char* pBuffer, int nSize);
void    PrintError(DWORD dwError);

//---------------------------------------------------------------------------------
// Initialize member variables
//---------------------------------------------------------------------------------

CCPM::CCPM()
: m_pDir(NULL), m_DPB(), m_nSectorsPerBlock(0), m_nReservedSectors(0), m_dwFilePos(0), m_wSector(0), m_Buffer()
{
}

//---------------------------------------------------------------------------------
// Release allocated memory
//---------------------------------------------------------------------------------

CCPM::~CCPM()
{
    if (m_pDir != NULL)
        free(m_pDir);
}

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CCPM::Load(CVDI* pVDI, DWORD dwFlags)
{

    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // Copy VDI pointer and user flags to member variables
    m_pVDI = pVDI;
    m_dwFlags = dwFlags;

    // Get disk geometry
    m_pVDI->GetDG(m_DG);

    // Check internal buffer size (just in case)
    if (sizeof(m_Buffer) < m_DG.LT.wSectorSize)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // If not first Load, release the previously allocated memory
    if (m_pDir != NULL)
        free(m_pDir);

    // Calculate needed memory to hold the largest CP/M directory
    dwBytes = sizeof(CPM_FCB) * 512 + (V80_MEM - ((sizeof(CPM_FCB) * 512) % V80_MEM));

    // Allocate memory for the entire directory
    if ((m_pDir = (BYTE*)calloc(dwBytes,1)) == NULL)
    {
        dwError = ERROR_OUTOFMEMORY;
        goto Done;
    }

    if ((dwError = GetDiskParams()) != NO_ERROR)
        goto Done;

    if (m_dwFlags & V80_FLAG_INFO)
    {
        printf("DOS:   BLS RPT BSH BLM EXM DSM DRM AL0 AL1 CKS OFF SSZ SKF OPT\r\n");
        printf("DOS: %5d %3d %3d %3d %3d %3d %3d  %02X  %02X %3d %3d %3d %3d  %02X\r\n",
               m_DPB.wBLS, m_DPB.nRPT, m_DPB.nBSH, m_DPB.nBLM, m_DPB.nEXM, m_DPB.wDSM, m_DPB.wDRM, m_DPB.nAL0, m_DPB.nAL1, m_DPB.nCKS, m_DPB.nOFF, m_DPB.nSSZ, m_DPB.nSKF, m_DPB.nOPT);
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the first/next directory entry
//---------------------------------------------------------------------------------

DWORD CCPM::Dir(void** pFile, OSI_DIR nFlag)
{

    static WORD wFCB = -1;

    DWORD dwError = NO_ERROR;

    // Reset index if FindFirst has been requested
    if (nFlag == OSI_DIR_FIND_FIRST)
        wFCB = -1;

    while (++wFCB <= m_DPB.wDRM)
    {

        // Get a pointer to the next entry
        *pFile = &((CPM_FCB*)m_pDir)[wFCB];

        // Check whether entry is deleted
        if (((CPM_FCB*)(*pFile))->nET == 0xE5)
            continue;

        // Check whether entry is primary
        if (((CPM_FCB*)(*pFile))->nRC != 0x80)
            goto Done;

    }

    dwError = ERROR_NO_MORE_FILES;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the directory entry matching the file name
//---------------------------------------------------------------------------------

DWORD CCPM::Open(void** pFile, const char cName[11])
{
/*
    DWORD   dwError = NO_ERROR;
    WORD    x, y;

    for (x = 0; x <= m_DPB.wDRM; x++)
    {

        // Get a pointer to entry[x]
        *pFile = &((CPM_FCB*)m_pDir)[x];

        // Compare File Name and Type
        for (y = 0; y < sizeof(cName) && ((((CPM_FCB*)(*pFile))->cFN[y] & 0x7F) == cName[y]); y++);

        // If not equal, skip to the next
        if (y < sizeof(cName))
            continue;

        // Check whether entry is primary
        if (((CPM_FCB*)(*pFile))->nRC != 0x80)
            goto Done;

    }

    dwError = ERROR_FILE_NOT_FOUND;

    Done:
    return dwError;
*/
    return ERROR_NOT_SUPPORTED;

}

//---------------------------------------------------------------------------------
// Create a new file with the indicated name and size
//---------------------------------------------------------------------------------

DWORD CCPM::Create(void** pFile, OSI_FILE& File)
{
/*
    CPM_EXTENT  Extent;
    void*       pPrevious;
    BYTE        nGranules;
    BYTE        nExtent = 0;
    DWORD       dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Get a new directory entry
    if ((dwError = GetFDE(pFile)) != NO_ERROR)
        goto Abort;

    // Set the directory entry as Active
    ((CPM_FPDE*)(*pFile))->nAttributes[0] = CPM_ATTR0_ACTIVE;

    // Set the file password
    ((CPM_FPDE*)(*pFile))->wOwnerHash = 0x4296;
    ((CPM_FPDE*)(*pFile))->wUserHash = 0x4296;

    // Set the file properties as indicated by the caller
    SetFile(*pFile, File, false);

    // Calculate the number of granules needed

    nGranules = ((CPM_FPDE*)(*pFile))->wERN / m_nSectorsPerGranule + (((CPM_FPDE*)(*pFile))->wERN % m_nSectorsPerGranule > 0 ? 1 : 0);

    if (nGranules == 0 && ((CPM_FPDE*)(*pFile))->nEOF > 0)
        nGranules++;

    // Repeat while the number of needed granules is greater than zero
    while (nGranules > 0)
    {

        // Create one extent with as many granules as possible, based on the calculated quantity
        if ((dwError = CreateExtent(Extent, nGranules)) != NO_ERROR)
            goto Abort;

        // Increment extent counter
        nExtent++;

        // Copy the extent data to the directory entry
        if ((dwError = CopyExtent(*pFile, CPM_EXTENT_SET, nExtent, Extent)) != NO_ERROR)
        {

            // Abort if error is other than extent not found
            if (dwError != ERROR_NOT_FOUND)
                goto Abort;

            // Preserve current file handle
            pPrevious = *pFile;

            // Get an additional directory entry
            if ((dwError = GetFDE(pFile)) != NO_ERROR)
                goto Abort;

            // Set a forward link to the newly created directory entry
            ((CPM_FPDE*)pPrevious)->Link.nCylinder = 0xFE;
            ((CPM_FPDE*)pPrevious)->Link.nGranules = FDE2DEC(*pFile);

            // Set a backward link from the previously existing directory entry
            ((CPM_FPDE*)(*pFile))->nAttributes[0] = CPM_ATTR0_ACTIVE|CPM_ATTR0_EXTENDED;
            ((CPM_FPDE*)(*pFile))->nAttributes[1] = FDE2DEC(pPrevious);

            // Copy the file name and extention to the new directory entry
            memcpy(((CPM_FPDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
            memcpy(((CPM_FPDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

            // Set the corresponding HIT DEC with the calculated name hash
            m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash(((CPM_FPDE*)pFile)->cName);

            continue;

        }

        // Subtract the number of granules allocated in this extent from the total required
        nGranules -= (Extent.nGranules & CPM_GRANULE_COUNT) + 1;

    }

    // Write the updated directory data and exit
    dwError = DirRW(CPM_DIR_WRITE);
    goto Done;

    // Restore the previous directory state
    Abort:
    DirRW(CPM_DIR_READ);

    Done:
    return dwError;
*/
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Move the file pointer
//---------------------------------------------------------------------------------

DWORD CCPM::Seek(void* pFile, DWORD dwPos)
{
/*
    WORD    wBlock;
    DWORD   dwError = NO_ERROR;

    // Calculate how many bytes each extent can hold
    DWORD dwExCap = m_DPB.wBLS * (m_DPB.b8Bit ? 16 : 8);

    // Calculate in which extent the required position is
    BYTE nExtent = dwPos / dwExCap;

    // Calculate from which extent's vector to get the block number
    BYTE nVector = (dwPos % dwExCap) / m_DPB.wBLS;

    // Calculate in which sector, inside the block, the position is
    BYTE nSector = (dwPos - nExtent * dwExCap - nVector * m_DPB.wBLS) / m_DG.LT.wSectorSize;

    // Locate file extent
    if ((dwError = FindExtent(&pFile, nExtent)) != NO_ERROR)
        goto Done;

    // Get block number from the disk allocation map
    wBlock = (m_DPB.b8Bit ? ((CPM_FCB*)pFile)->DM.nDM[nVector] : ((CPM_FCB*)pFile)->DM.wDM[nVector]);

    // If there is no block number then position can't be determined
    if (wBlock == 0)
    {
        m_wSector = 0xFFFF;
        goto Done;
    }

    // Convert block into relative sector
    m_wSector = (((m_DPB.wDRM + 1) * sizeof(CPM_FCB) + (wBlock - 2) * m_DPB.wBLS) / m_DG.LT.wSectorSize) + nSector;
    m_dwFilePos = dwPos;

    Done:
    return dwError;
*/
    return ERROR_NOT_SUPPORTED;

}

//---------------------------------------------------------------------------------
// Read data from file
//---------------------------------------------------------------------------------

DWORD CCPM::Read(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
{
/*
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

        // Calculate starting transfer point
        wSectorBegin = m_dwFilePos % pTrack->wSectorSize;

        // Calculate ending transfer point
        wSectorEnd = (dwFileSize - m_dwFilePos >= pTrack->wSectorSize ? pTrack->wSectorSize : dwFileSize % pTrack->wSectorSize);

        // Calculate transfer length
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
*/
    return ERROR_NOT_SUPPORTED;

}

//---------------------------------------------------------------------------------
// Save data to file
//---------------------------------------------------------------------------------

DWORD CCPM::Write(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
{
/*
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

        // Calculate ending transfer point
        wSectorEnd = (dwFileSize - m_dwFilePos >= pTrack->wSectorSize ? pTrack->wSectorSize : dwFileSize % pTrack->wSectorSize);

        // Calculate transfer length
        wLength = ((wSectorEnd - wSectorBegin) <= (dwBytes - dwWritten) ? wSectorEnd - wSectorBegin : dwBytes - dwWritten);

        // If needed, read the sector before writing to it
        if (wLength < pTrack->wSectorSize)
        {
            if ((dwError = m_pVDI->Read(nTrack, nSide, nSector, m_Buffer, pTrack->wSectorSize)) != NO_ERROR)
                break;
        }

        // Copy data from the caller's buffer to the internal buffer
        memcpy(&m_Buffer[wSectorBegin], pBuffer, wLength);

        // Write sector from internal buffer
        if ((dwError = m_pVDI->Write(nTrack, nSide, nSector, m_Buffer, pTrack->wSectorSize)) != NO_ERROR)
            break;

        // Update bytes count and caller's buffer pointer
        dwWritten += wLength;
        pBuffer += wLength;

        // Advance the file pointer
        Seek(pFile, m_dwFilePos + wLength);

    }

    dwBytes = dwWritten;

    return dwError;
*/
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Delete file
//---------------------------------------------------------------------------------

DWORD CCPM::Delete(void* pFile)
{
/*
    CPM_EXTENT  Extent;
    DWORD       dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Inactivate the directory entry
    ((CPM_FPDE*)pFile)->nAttributes[0] &= !CPM_ATTR0_ACTIVE;

    // Release the corresponding HIT slot
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = 0;

    // Loop through all extents releasing every allocated granule
    for (int x = 1; (dwError = CopyExtent(pFile, CPM_EXTENT_GET, x, Extent)) == NO_ERROR; x++)
    {
        if ((dwError = DeleteExtent(Extent)) != NO_ERROR)
            break;
    }

    // If exited the loop because reached the end of the extents table
    if (dwError == ERROR_NO_MATCH)
        dwError = DirRW(CPM_DIR_WRITE); // Save the directory
    else
        DirRW(CPM_DIR_READ);            // Otherwise, restore its previous state

    return dwError;
*/
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Get DOS information
//---------------------------------------------------------------------------------

void CCPM::GetDOS(OSI_DOS& DOS)
{

    CPM_FCB* pEntry;

    // Zero the DOS structure
    memset(&DOS, 0, sizeof(OSI_DOS));

    // Search through all directory entries
    for (int x = 0; x <= m_DPB.wDRM; x++)
    {

        pEntry = &((CPM_FCB*)m_pDir)[x];

        // Look for a Disk Label entry type
        if (pEntry->nET == 0x20)
        {

            // Get disk name
            memcpy(DOS.szName, pEntry->cFN, sizeof(DOS.szName) - 1);

            // Strip the 8th bit from each byte
            for (BYTE y = 0; y < sizeof(DOS.szName); y++)
                DOS.szName[y] &= 0x7F;

            break;

        }

    }

}

//---------------------------------------------------------------------------------
// Set DOS information
//---------------------------------------------------------------------------------

DWORD CCPM::SetDOS(OSI_DOS& DOS)
{
/*
    // Set DOS version
    ((CPM_GAT*)m_pDir)->nDosVersion = DOS.nVersion;

    // Set the disk name
    memcpy(((CPM_GAT*)m_pDir)->cDiskName, DOS.szName, sizeof(DOS.szName) - 1);

    // Set the disk date
    memcpy(((CPM_GAT*)m_pDir)->cDiskDate, &DOS.szDate, sizeof(DOS.szDate) - 1);

    return DirRW(CPM_DIR_WRITE);
*/
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Get file information
//---------------------------------------------------------------------------------

void CCPM::GetFile(void* pFile, OSI_FILE& File)
{

    // Zero the structure
    memset(&File, 0, sizeof(OSI_FILE));

    // Copy file name and extention
    memcpy(File.szName, ((CPM_FCB*)pFile)->cFN, sizeof(File.szName) - 1);
    memcpy(File.szType, ((CPM_FCB*)pFile)->cFT, sizeof(File.szType) - 1);

    // Remove attribute information from extension
    File.szType[0] &= 0x7F;
    File.szType[1] &= 0x7F;
    File.szType[2] &= 0x7F;

    // Get file size
    File.dwSize = GetFileSize(pFile);

    // Get file date
//    File.Date.nMonth    = (((CPM_FPDE*)pFile)->nAttributes[1] & CPM_ATTR1_MONTH);
//    File.Date.nDay      = (((CPM_FPDE*)pFile)->nAttributes[2] & CPM_ATTR2_DAY) >> 3;
//    File.Date.wYear     = (((CPM_FPDE*)pFile)->nAttributes[2] & CPM_ATTR2_YEAR) + (File.Date.nMonth > 0 ? 1980 : 0);

    // If not a valid date, make it all zeroes
    if (File.Date.nMonth < 1 || File.Date.nMonth > 12 || File.Date.nDay < 1 || File.Date.nDay > 31)
        memset(&File.Date, 0, sizeof(File.Date));

    // Get file attributes
    File.nAccess    = (((CPM_FCB*)pFile)->cFT[0] & 0x80 ? 5 : 0);
    File.bSystem    = (((CPM_FCB*)pFile)->cFT[1] & 0x80);
    File.bInvisible = (((CPM_FCB*)pFile)->cFT[1] & 0x80) || (((CPM_FCB*)pFile)->nET == 0x80);
    File.bModified  = ((CPM_FCB*)pFile)->cFT[2] & 0x80;;

}

//---------------------------------------------------------------------------------
// Set file information (public function)
//---------------------------------------------------------------------------------

DWORD CCPM::SetFile(void* pFile, OSI_FILE& File)
{
    return SetFile(pFile, File, true);
}

//---------------------------------------------------------------------------------
// Set file information (protected)
//---------------------------------------------------------------------------------

DWORD CCPM::SetFile(void* pFile, OSI_FILE& File, bool bCommit)
{
/*
    // Copy the file name and extention
    memcpy(((CPM_FPDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
    memcpy(((CPM_FPDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

    // Set the corresponding HIT DEC to the newly calculated name hash
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash(((CPM_FPDE*)pFile)->cName);

    // Set the file size
    ((CPM_FPDE*)pFile)->nEOF = File.dwSize % m_DG.LT.wSectorSize;
    ((CPM_FPDE*)pFile)->wERN = File.dwSize / m_DG.LT.wSectorSize + (((CPM_FPDE*)pFile)->nEOF > 0 ? 1 : 0);

    // Set the file date

    ((CPM_FPDE*)pFile)->nAttributes[1] &= ~CPM_ATTR1_MONTH;
    ((CPM_FPDE*)pFile)->nAttributes[1] |= File.Date.nMonth;

    ((CPM_FPDE*)pFile)->nAttributes[2] &= ~CPM_ATTR2_DAY;
    ((CPM_FPDE*)pFile)->nAttributes[2] |= (File.Date.nDay << 3);

    ((CPM_FPDE*)pFile)->nAttributes[2] &= ~CPM_ATTR2_YEAR;
    ((CPM_FPDE*)pFile)->nAttributes[2] |= (File.Date.wYear - (File.Date.nMonth > 0 ? 1980 : 0));

    // Set the file attributes

    ((CPM_FPDE*)pFile)->nAttributes[0] &= ~CPM_ATTR0_ACCESS;
    ((CPM_FPDE*)pFile)->nAttributes[0] |= File.nAccess;

    ((CPM_FPDE*)pFile)->nAttributes[0] &= ~CPM_ATTR0_SYSTEM;
    ((CPM_FPDE*)pFile)->nAttributes[0] |= (CPM_ATTR0_SYSTEM * File.bSystem);

    ((CPM_FPDE*)pFile)->nAttributes[0] &= ~CPM_ATTR0_INVISIBLE;
    ((CPM_FPDE*)pFile)->nAttributes[0] |= (CPM_ATTR0_INVISIBLE * File.bInvisible);

    ((CPM_FPDE*)pFile)->nAttributes[1] &= ~CPM_ATTR1_MODIFIED;
    ((CPM_FPDE*)pFile)->nAttributes[1] |= (CPM_ATTR1_MODIFIED * File.bModified);

    return (bCommit ? DirRW(CPM_DIR_WRITE) : NO_ERROR);
*/
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Get file size
//---------------------------------------------------------------------------------

DWORD CCPM::GetFileSize(void* pFile)
{

    DWORD dwSize = 0;                                                           // Zero the file size accumulator

    BYTE nEX = ((CPM_FCB*)pFile)->nEX;                                          // Extent counter

    while (true)
    {

        BYTE nRC = ((CPM_FCB*)pFile)->nRC;                                      // Record counter
        BYTE nS1 = ((CPM_FCB*)pFile)->nS1;                                      // Last record byte count

        // *** A formula varia conforme o sistema ***
        // *** Alguns consideram o S1 como bytes a mais, outros a menos ***
        // *** Verificar também diferentes interpretações do RC ***

        dwSize += (nRC - (nRC == 0x80 ? 1 : 0)) * 128 - nS1;                    // Add this entry's size to the accumulator

        if (nRC < 0x80)                                                         // Check whether this is an extended entry
            break;

        if (FindExtent(&pFile, ++nEX) != NO_ERROR)                              // Locate the next entry and continue
            break;

    }

    return dwSize;                                                              // Return the accumulated size for all entries belonging to the file

}

//---------------------------------------------------------------------------------
// Discover the disk parameters
//---------------------------------------------------------------------------------

DWORD CCPM::GetDiskParams()
{

    int     iTracks, iSides, iSize;
    DWORD   dwError = NO_ERROR;

    // Set DPB option flags

    if (m_DG.LT.nDensity == VDI_DENSITY_DOUBLE)                                 // Set flag for Double Density disks
        m_DPB.nOPT |= CPM_OPT_DD;

    if (m_dwFlags & V80_FLAG_SS)                                                // Set flag for Single or Double-Sided disks
        m_DPB.nOPT &= ~CPM_OPT_DS;
    else if (m_dwFlags & V80_FLAG_DS)
        m_DPB.nOPT |= CPM_OPT_DS;
    else
        m_DPB.nOPT |= (m_DG.LT.nFirstSide != m_DG.LT.nLastSide ? CPM_OPT_DS : 0);

//    if (m_DG.LT.nFirstSide != m_DG.LT.nLastSide)                                // Set flag for Double-Sided disks
//        m_DPB.nOPT |= CPM_OPT_DS;

    // Try several values until the directory table is found

    m_DPB.nSPT = m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1;                // SPT = Sectors Per Track

    for (m_DPB.nOFF = 0; m_DPB.nOFF < 4; m_DPB.nOFF++)                          // OFF = System reserved cylinders
    {
        for (int nDRM = m_DPB.nSPT; nDRM > 0; nDRM--)                           // Start with a full track directory and decrease to up one sector
        {
            for (m_DPB.nSKF = 0; m_DPB.nSKF < (m_DPB.nSPT >> 1); m_DPB.nSKF++)  // SKF = Skew Factor (sector interleaving)
            {

                InitXLT();

                m_DPB.wDRM = nDRM * (m_DG.LT.wSectorSize / sizeof(CPM_FCB)) - 1;// DRM = Sectors * Entries Per Sector - 1

                if (DirRW(CPM_DIR_READ) == NO_ERROR)                            // Load directory into memory and check its structure
                    if (CheckDir() == NO_ERROR)
                        goto Found;

            }
        }
    }

    dwError = ERROR_NOT_DOS_DISK;
    goto Done;

    Found:

    // Calculate disk size in bytes (excluding system reserved cylinders)

    iTracks = m_DG.LT.nTrack - m_DG.FT.nTrack + 1;
    iSides = m_DG.LT.nLastSide - m_DG.LT.nFirstSide + 1;
    iSize = (iTracks * iSides - m_DPB.nOFF) * m_DPB.nSPT * m_DG.LT.wSectorSize;

    // Set DPB parameters

    m_DPB.b8Bit = Is8Bit();
    m_DPB.wBLS = GetBLS(m_DPB.b8Bit);
    m_DPB.nRPT = m_DPB.nSPT * (m_DG.LT.wSectorSize / 128);                      // RPT = 128-byte records per track
    m_DPB.nSSZ = Log2(m_DG.LT.wSectorSize) - 6;                                 // SSZ = Sector Size (0=128, 1=256, 2=512, 3=1024)
    m_DPB.wDSM = (iSize / m_DPB.wBLS) - 1;                                      // DSM = Disk Storage Max (max blocks minus 1)
    m_DPB.nBSH = Log2(m_DPB.wBLS) - 6;                                          // BSH = Block Shift Factor
    m_DPB.nBLM = pow(2, m_DPB.nBSH) - 1;                                        // BLM = Block Mask
    m_DPB.nEXM = pow(2, Log2(m_DPB.wBLS) - (m_DPB.wDSM < 256 ? 9 : 10)) - 1;    // EXM = Extent Mask
    m_DPB.nCKS = (m_DPB.wDRM + 1) / 4;                                          // CKS = Directory Check Size
    m_DPB.nAL0 = GetALM(m_DPB.wDRM, m_DPB.wBLS) >> 8;                           // AL0 = Directory Allocation Bitmap #0
    m_DPB.nAL1 = GetALM(m_DPB.wDRM, m_DPB.wBLS) & 0xFF;                         // AL1 = Directory Allocation Bitmap #1

//    printf("  BLS RPT BSH BLM EXM DSM DRM AL0 AL1 CKS OFF SSZ SKF OPT\r\n");
//    printf("%5d %3d %3d %3d %3d %3d %3d  %02X  %02X %3d %3d %3d %3d  %02X\r\n",
//        m_DPB.wBLS, m_DPB.nRPT, m_DPB.nBSH, m_DPB.nBLM, m_DPB.nEXM, m_DPB.wDSM, m_DPB.wDRM, m_DPB.nAL0, m_DPB.nAL1, m_DPB.nCKS, m_DPB.nOFF, m_DPB.nSSZ, m_DPB.nSKF, m_DPB.nOPT);

//    Dump(m_pDir, m_DG.LT.wSectorSize);

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Read or Write the entire directory
//---------------------------------------------------------------------------------

DWORD CCPM::DirRW(CPM_DIR nMode)
{

    BYTE    nSectors = ((m_DPB.wDRM + 1) * sizeof(CPM_FCB)) / m_DG.LT.wSectorSize;
    DWORD   dwOffset = 0;
    DWORD   dwError = NO_ERROR;

    for (BYTE nExSide = m_DG.LT.nFirstSide; nExSide <= (m_DPB.nOPT & CPM_OPT_SSEL ? m_DG.LT.nLastSide : m_DG.LT.nFirstSide) && nSectors > 0; nExSide++)
    {
        for (BYTE nTrack = m_DG.FT.nTrack + m_DPB.nOFF; nTrack <= m_DG.LT.nTrack && nSectors > 0; nTrack++)
        {
            for (BYTE nInSide = m_DG.LT.nFirstSide; nInSide <= (m_DPB.nOPT & CPM_OPT_SSEL ? m_DG.LT.nFirstSide : m_DG.LT.nLastSide) && nSectors > 0; nInSide++)
            {
                for (BYTE nSector = m_DG.LT.nFirstSector; nSector <= m_DG.LT.nLastSector && nSectors > 0; nSector++, nSectors--, dwOffset += m_DG.LT.wSectorSize)
                {
                    if (nMode == CPM_DIR_WRITE)
                    {
                        if ((dwError = m_pVDI->Write(nTrack, (m_DPB.nOPT & CPM_OPT_SSEL ? nExSide : nInSide), XLT(nSector), &m_pDir[dwOffset], m_DG.LT.wSectorSize)) != NO_ERROR)
                            goto Done;
                    }
                    else
                    {
                        if ((dwError = m_pVDI->Read(nTrack, (m_DPB.nOPT & CPM_OPT_SSEL ? nExSide : nInSide), XLT(nSector), &m_pDir[dwOffset], m_DG.LT.wSectorSize)) != NO_ERROR)
                            goto Done;
//                        printf("[%02d:%d:%02d]\r\n", nTrack, (m_DPB.nOPT & CPM_OPT_SSEL ? nExSide : nInSide), L2P(nSector));
//                        Dump(&m_pDir[dwOffset], m_DG.LT.wSectorSize);
                    }
                }
            }
        }
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Check the directory structure
//---------------------------------------------------------------------------------

DWORD CCPM::CheckDir(void)
{

    BYTE        nSector;
    BYTE        nLastSector = 0xFF;
    bool        bIsEmpty;
    bool        bAnyEmpty = false;
    WORD        wFiles = 0;
    CPM_FCB*    pFCB;
    char        szFileNameChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$&+-@_~";
    DWORD       dwError = NO_ERROR;
    WORD        x;

    // For each entry in the directory...
    for (WORD wFCB = 0; wFCB <= m_DPB.wDRM; wFCB++)
    {

//printf("1");
        // Check sector interleaving (there may be no empty sectors between used ones)

        // Calculate current FCB's sector
        nSector = (wFCB * sizeof(CPM_FCB)) / m_DG.LT.wSectorSize;

        // Check each sector only once
        if (nSector != nLastSector)
        {

            nLastSector = nSector;

            // Check whether sector is filled with E5 (except for the last 16 byte that may contain the Tandy Copyright)
            for (x = 0; x < (m_DG.LT.wSectorSize - 16); x++)
                if (m_pDir[wFCB * sizeof(CPM_FCB) + x] != 0xE5) // Need test for 1A (complement)?
                    break;

            bIsEmpty = (x < (m_DG.LT.wSectorSize - 16) ? false : true);

            // If current sector is used but some empty sector has been detected before, then we have an error
            if (!bIsEmpty && bAnyEmpty)
            {
                dwError = ERROR_DISK_TOO_FRAGMENTED;
                goto Done;
            }

            // If sector is empty, there should be only empty sectors from now on
            if (bIsEmpty)
                bAnyEmpty = true;

        }

//printf(", 2");
        // Get a pointer to the directory entry
        pFCB = &((CPM_FCB*)m_pDir)[wFCB];

        // If entry is inactive, skip to the next
        if (pFCB->nET == 0xE5)
            continue;

//printf(", 3");
        // First 8 characters of file name can be non-blanks
        for (x = 0; x < sizeof(pFCB->cFN) && memchr(szFileNameChars, (pFCB->cFN[x] & 0x7F), strlen(szFileNameChars)) != NULL; x++);

        // But at least the first one must be non-blank for the name to be valid
        if (x == 0 && !(m_dwFlags & V80_FLAG_CHKDIR))
        {
            dwError = ERROR_INVALID_NAME;
            goto Done;
        }

//printf(", 4");
        // If a blank is found, then only blanks can exist to the end of the name
        for ( ; x < sizeof(pFCB->cFN) && (pFCB->cFN[x] & 0x7F) == ' '; x++);

        // If not, then this name is invalid
        if (x < sizeof(pFCB->cFN))
        {
            dwError = ERROR_INVALID_NAME;
            goto Done;
        }

//printf(", 5");
        // The file extension can have up to 3x 7-bit non-blank characters
        for (x = 0; x < sizeof(pFCB->cFT) && memchr(szFileNameChars, (pFCB->cFT[x] & 0x7F), strlen(szFileNameChars)) != NULL; x++);

        // If a blank is found, then only blanks can exist to the end of the extension
        for ( ; x < sizeof(pFCB->cFT) && (pFCB->cFT[x] & 0x7F) == ' '; x++);

        // If not, then this extension is invalid
        if (x < sizeof(pFCB->cFT))
        {
            dwError = ERROR_INVALID_NAME;
            goto Done;
        }

//printf(", 6\r\n");
        // Increment file count
        wFiles++;

    }

    if (wFiles == 0)
        dwError = ERROR_EMPTY;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Scan the Directory
//---------------------------------------------------------------------------------

DWORD CCPM::FindExtent(void** pFile, BYTE nEX)
{

    DWORD dwError = NO_ERROR;

    // Cast a pointer to the target FCB
    CPM_FCB* pTarget = (CPM_FCB*)*pFile;

    for (int x = 0; x <= m_DPB.wDRM; x++)
    {

        // Point to the directory entry[x]
        CPM_FCB* pSource = &((CPM_FCB*)m_pDir)[x];

        // Check whether Entry Type, File Name and File Type match the entry we're looking for
        if (memcmp(pSource, pTarget, 12) != 0)
            continue;

        // Check whether Extent Count matches the one we're looking for
        if (pSource->nEX == nEX)
        {
            *pFile = pSource;
            goto Done;
        }

    }

    dwError = ERROR_NO_MORE_FILES;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Allocate disk space
//---------------------------------------------------------------------------------
/*
DWORD CCPM::CreateExtent(CPM_EXTENT& Extent, BYTE nGranules)
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

    // Find the first empty slot (0) then continue up to the first non-empty slot (1)
    for (int nExpectedBit = 0; nExpectedBit < 2; nExpectedBit++)
    {

        // Test the state of 'CurrentGranule' at GAT[CurrentCylinder]
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
                // Also exit when the number of requested granules have been reached
                if (nAllocatedGranules == 32 || nGranules == 0)
                    goto Stop;

            }

            // Advance to the next granule
            nCurrentGranule++;

            // Check whether we've reached the cylinder's limit
            if (nCurrentGranule == m_nGranulesPerCylinder)
            {

                // Reset the granule index and advance to the next cylinder
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

    // Assemble the Extent
    Stop:
    Extent.nCylinder = nInitialCylinder;
    Extent.nGranules = (nInitialGranule << 5) + (nAllocatedGranules - 1); // 3 MSB: Initial Granule, 5 LSB: Contiguous Granules minus 1

    Done:
    return dwError;

}
*/
//---------------------------------------------------------------------------------
// Release disk space
//---------------------------------------------------------------------------------
/*
DWORD CCPM::DeleteExtent(CPM_EXTENT& Extent)
{

    DWORD dwError = NO_ERROR;

    // Get the initial granule and count of granules in the extent
    int nIndex = (Extent.nGranules & CPM_GRANULE_INITIAL) >> 5;
    int nCount = (Extent.nGranules & CPM_GRANULE_COUNT) + 1;

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
*/
//---------------------------------------------------------------------------------
// Get or Set extent data
//---------------------------------------------------------------------------------
/*
DWORD CCPM::CopyExtent(void* pFile, CPM_EXT nMode, BYTE nExtent, CPM_EXTENT& Extent)
{

    CPM_EXTENT* pExtent;
    DWORD       dwError = NO_ERROR;

    // Go through the extents table
    for (int x = 1, y = 1; x < 6; x++)
    {

        // Point to File.Extent[x-1]
        pExtent = &(((CPM_FPDE*)pFile)->Extent[x - 1]);

        // Check whether the last extent links this directory entry to another one
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
        if (pExtent->nCylinder == 0xFF && nMode == CPM_EXTENT_GET)
            break;

        // Check whether we've reached the requested extent number (but not a corrupted link)
        if (y == nExtent && x != 5)
        {
            if (nMode == CPM_EXTENT_GET)
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

        // Advance to the next extent
        y++;

    }

    dwError = ERROR_NO_MATCH;

    Done:
    return dwError;

}
*/
//---------------------------------------------------------------------------------
// Return a pointer to an available File Directory Entry
//---------------------------------------------------------------------------------
/*
DWORD CCPM::GetFDE(void** pFile)
{

    DWORD dwError = NO_ERROR;

    if ((dwError = ScanHIT(pFile, CPM_HIT_FIND_FIRST_FREE)) != NO_ERROR)
        goto Done;

    memset(*pFile, 0, sizeof(CPM_FPDE));

    for (int x = 0; x < 5; x++)
    {
        ((CPM_FPDE*)(*pFile))->Extent[x].nCylinder = 0xFF;
        ((CPM_FPDE*)(*pFile))->Extent[x].nGranules = 0xFF;
    }

    Done:
    return dwError;

}
*/
//---------------------------------------------------------------------------------
// Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
//---------------------------------------------------------------------------------
/*
void* CCPM::DEC2FDE(BYTE nDEC)
{

    DWORD dwSectorOffset = ((nDEC & CPM_DEC_SECTOR) + 2) * m_DG.LT.wSectorSize;
    DWORD dwEntryOffset = ((nDEC & CPM_DEC_ENTRY) >> 5) * sizeof(CPM_FPDE);

    return ((nDEC & CPM_DEC_SECTOR) < m_nDirSectors ? &m_pDir[dwSectorOffset + dwEntryOffset] : NULL);

}
*/
//---------------------------------------------------------------------------------
// Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
//---------------------------------------------------------------------------------
/*
BYTE CCPM::FDE2DEC(void* pFile)
{

    // ((pFile - pDir) - ((GAT + HIT) * SectorSize)) / sizeof(FPDE)
    BYTE nDEC = (((BYTE*)pFile - m_pDir) - (2 * m_DG.LT.wSectorSize)) / sizeof(CPM_FPDE);

    // Reorganize bits: 11111000 -> 00011111
    return ((nDEC << 5) + (nDEC >> 3));

}
*/
//---------------------------------------------------------------------------------
// Return the hash code of a given file name
//---------------------------------------------------------------------------------
/*
BYTE CCPM::Hash(const char* pName)
{

    BYTE nHash = 0;

    for (int x = 0; x < 11; x++)
    {
        nHash ^= pName[x];
        nHash = (nHash << 1) + ((nHash & 0b10000000) >> 7); // rol nHash, 1
    }

    return (nHash != 0 ? nHash : 0x01);

}
*/
//---------------------------------------------------------------------------------
// Analyse directory and determine whether allocation vectors are 8-bit or 16-bit
//---------------------------------------------------------------------------------

bool CCPM::Is8Bit()
{

    int i8 = 0;
    int i16 = 0;
    int iEntries = m_DG.LT.wSectorSize / sizeof(CPM_FCB);

    for (int x = 0; x < iEntries; x++)
    {

        CPM_FCB* pFile = (CPM_FCB*)&m_pDir[x * sizeof(CPM_FCB)];

        if (pFile->nET == 0xE5)
            continue;

        if (pFile->DM.wDM[1] != 0 && pFile->DM.wDM[1] < 256 && pFile->DM.wDM[1] > pFile->DM.wDM[0])
            i16++;

        if (pFile->DM.nDM[1] != 0 && pFile->DM.nDM[1] > pFile->DM.nDM[0])
            i8++;

    }

    return (i8 > i16 ? true : false);

}

//---------------------------------------------------------------------------------
// Analyse directory and determine the block size
//---------------------------------------------------------------------------------

WORD CCPM::GetBLS(bool b8Bit)
{

    int iSize = 0;
    int iSlots = 0;
    int iEntries = m_DG.LT.wSectorSize / sizeof(CPM_FCB);

    for (int x = 0; x < iEntries; x++)
    {

        CPM_FCB* pFile = (CPM_FCB*)&m_pDir[x * sizeof(CPM_FCB)];

        if (pFile->nET == 0xE5)
            continue;

        iSize += GetFileSize(pFile);

        for (int y = 0; y < (b8Bit ? 16 : 8); y++)
            if ((b8Bit ? pFile->DM.nDM[y] : pFile->DM.wDM[y]) != 0)
                iSlots++;

    }

    iSize /= iSlots;

    if (iSize <= 1024)
        iSize = 1024;
    else if (iSize <= 2048)
        iSize = 2048;
    else if (iSize <= 4096)
        iSize = 4096;
    else if (iSize <= 8192)
        iSize = 8192;
    else
        iSize = 16384;

    return (WORD)iSize;

}

//---------------------------------------------------------------------------------
// Create a directory allocation bitmap
//---------------------------------------------------------------------------------

WORD CCPM::GetALM(WORD wDRM, WORD wBLS)
{

    WORD wBits = ((wDRM + 1) * sizeof(CPM_FCB)) / wBLS;                         // Number of blocks required to hold the directory

    if (wBits == 0)                                                             // If less than 1, round up to 1
        wBits = 1;

    return (WORD)(pow(2, wBits) - 1) << (16 - wBits);                           // Create a 16-bit mask based on the number of bits/blocks calculated

}

//---------------------------------------------------------------------------------
// Return the Cylinder/Head/Sector (CHS) of a given relative sector
//---------------------------------------------------------------------------------

void CCPM::CHS(WORD wSector, BYTE& nTrack, BYTE& nSide, BYTE& nSector)
{

// *** Adaptar esta rotina para levar em consideração os flags SN e TN ***

    // Calculate SectorsPerCylinder
    BYTE nSPC = m_DPB.nSPT * (m_DPB.nOPT & CPM_OPT_DS ? 2 : 1);

    // Track = RelativeSector / SectorsPerCylinder
    nTrack = wSector / nSPC;

    // Side = Remainder / SectorsPerTrack
    nSide = (wSector - (nTrack * nSPC)) / m_DPB.nSPT;

    // Sector = Remainder
    nSector = wSector - (nTrack * nSPC) - (nSide * m_DPB.nSPT);

    // Adjust Track and Side
    nTrack += m_DPB.nOFF;
    nSide += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSide : m_DG.LT.nFirstSide);
    nSector += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSector : m_DG.LT.nFirstSector);
    nSector = XLT(nSector);

    if (nSector > m_DG.LT.nLastSector)
    {
        nTrack++;
        nSector = m_DG.LT.nFirstSector;
    }

}

//---------------------------------------------------------------------------------
// Convert sector numbers from Logical to Physical based on the skew factor
//---------------------------------------------------------------------------------

void CCPM::InitXLT()
{

    memset(m_DPB.nXLT, 0, sizeof(m_DPB.nXLT));

    for (BYTE nIndex = 1, nSlot = 0; nIndex <= m_DPB.nSPT; nIndex++, nSlot += (m_DPB.nSKF + 1))
    {

        if (nSlot >= m_DPB.nSPT)
        {

            nSlot %= m_DPB.nSPT;

            while (m_DPB.nXLT[nSlot] != 0 && nSlot < m_DPB.nSPT)
                nSlot++;

        }

        if (nSlot < m_DPB.nSPT)
            m_DPB.nXLT[nSlot] = nIndex;

    }

}

//---------------------------------------------------------------------------------
// Convert sector numbers from Logical to Physical based on the skew factor
//---------------------------------------------------------------------------------

BYTE CCPM::XLT(BYTE nSector)
{

    BYTE nSlot;

    for (nSlot = 0; nSlot < m_DPB.nSPT; nSlot++)
        if (m_DPB.nXLT[nSlot] == (nSector - m_DG.LT.nFirstSector + 1))
            break;


    return nSlot + m_DG.LT.nFirstSector;

}

//---------------------------------------------------------------------------------
// Base-2 Logarithm: Calculate the exponent of a base-2 value (e.g. 1024 = 2^10)
//---------------------------------------------------------------------------------

BYTE CCPM::Log2(WORD wNumber)
{
    return ((1 / log(2)) * log(wNumber));
}
