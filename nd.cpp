//---------------------------------------------------------------------------------
// Operating System Interface for NewDOS/80
//---------------------------------------------------------------------------------

#include "windows.h"
#include <stdio.h>
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "nd.h"

//---------------------------------------------------------------------------------
// Initialize member variables
//---------------------------------------------------------------------------------

CND::CND()
:   m_pDir(NULL), m_wDirSector(0), m_nDirSectors(0), m_nSides(0), m_nDensity(VDI_DENSITY_SINGLE),
    m_dwFilePos(0), m_wSector(0), m_Buffer(), m_nLumps(0), m_nFlags1(0), m_nFlags2(0), m_nTC(0),
    m_nSPC(0), m_nGPL(0), m_nDDSL(0), m_nDDGA(0), m_nSPG(0), m_wTI(0), m_nTD(0)
{
}

//---------------------------------------------------------------------------------
// Release allocated memory
//---------------------------------------------------------------------------------

CND::~CND()
{
    if (m_pDir != NULL)
        free(m_pDir);
}

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CND::Load(CVDI* pVDI, DWORD dwFlags)
{

    ND_PDRIVE*  pPDRIVE;
    BYTE        nFT;
    BYTE        nTC;
    BYTE        nSPT;
    DWORD       dwBytes;
    DWORD       dwError = NO_ERROR;

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

    // Determine the disk density
    m_nDensity = ((m_DG.FT.nLastSector - m_DG.FT.nFirstSector + 1) != (m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1) || (m_DG.FT.nDensity != m_DG.LT.nDensity) ? VDI_DENSITY_MIXED : m_DG.LT.nDensity);

    // Determine the first track depending on disk density
    nFT = m_DG.FT.nTrack + (m_nDensity == VDI_DENSITY_MIXED ? 1 : 0);

    // Calculate the number of tracks depending on disk density
    nTC = (m_DG.LT.nTrack - m_DG.FT.nTrack + 1) - (m_nDensity == VDI_DENSITY_MIXED ? 1 : 0);

    // Calculate the number of sectors per track
    nSPT = m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1;

    // Check internal buffer size (just in case)
    if (sizeof(m_Buffer) < m_DG.FT.wSectorSize)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Load the PDRIVE table from first track, third sector
    if ((dwError = m_pVDI->Read(nFT, m_DG.LT.nFirstSide, m_DG.LT.nFirstSector + 2, m_Buffer, m_DG.LT.wSectorSize)) != NO_ERROR)
        goto Done;

    // Find the proper table entry
    for (int i = 0; i < 16; i++)
    {

        // Point to entry[i] in the table
        pPDRIVE = (ND_PDRIVE*)&m_Buffer[i * 16];

        // Get parameters
        m_nLumps    = pPDRIVE->nLumps;
        m_nFlags1   = pPDRIVE->nFlags1;
        m_nFlags2   = pPDRIVE->nFlags2;
        m_nTC       = pPDRIVE->nTC;
        m_nSPC      = pPDRIVE->nSPC;
        m_nGPL      = pPDRIVE->nGPL;
        m_nDDSL     = pPDRIVE->nDDSL;
        m_nDDGA     = pPDRIVE->nDDGA;
        m_nSPG      = pPDRIVE->nSPG;
        m_nTSR      = pPDRIVE->nTSR;
        m_wTI       = pPDRIVE->wTI;
        m_nTD       = pPDRIVE->nTD;

        // Check match between disk geometry and PDRIVE parameters
        if (!(m_dwFlags & V80_FLAG_CHKDSK) && (nTC != m_nTC || nSPT != (m_nSPC / m_nSides)))
            continue;

        // Check some other entry parameters
        if ((m_nLumps == 0) || (m_nGPL < 2) || (m_nGPL > 8) || (m_nDDSL > m_nLumps) || (m_nDDGA < 2) || (m_nDDGA > 8) || (m_nTD > 7))
            continue;

        goto Success;

    }

    dwError = ERROR_NOT_DOS_DISK;

    goto Done;

    Success:

    // SPG = Total Sectors / Total Granules
    if (m_nSPG == 0)
        m_nSPG = (m_nTC * m_nSPC) / (m_nLumps * m_nGPL);

    // Convert DDSA into DDGA
    if (m_nDDGA % m_nSPG == 0)  // The division of DDGA by SPG must be an integer
        m_nDDGA /= m_nSPG;

    // Calculate directory relative sector
    m_wDirSector = m_nDDSL * m_nGPL * m_nSPG;

    // Calculate number of directory sectors
    m_nDirSectors = m_nDDGA * m_nSPG;

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
    if ((dwError = DirRW(ND_DIR_READ)) != NO_ERROR)
        goto Done;

    // Check directory structure
    if (!(m_dwFlags & V80_FLAG_CHKDIR))
        if ((dwError = CheckDir()) != NO_ERROR)
            goto Done;

    if (m_dwFlags & V80_FLAG_INFO)
        printf("DOS: TI=%s TD=%c TC=%d SPT=%d TSR=%d GPL=%d DDSL=%d DDGA=%d Lumps=%d\r\n", TI(m_wTI), 'A' + m_nTD, m_nTC, m_nSPC, m_nTSR, m_nGPL, m_nDDSL, m_nDDGA, m_nLumps);

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the first/next directory entry
//---------------------------------------------------------------------------------

DWORD CND::Dir(void** pFile, OSI_DIR nFlag)
{

    ND_HIT nMode = (nFlag == OSI_DIR_FIND_FIRST ? ND_HIT_FIND_FIRST_USED : ND_HIT_FIND_NEXT_USED);

    DWORD dwError = NO_ERROR;

    do
    {

        // Return a pointer to the first/next non-empty file entry
        if ((dwError = ScanHIT(pFile, nMode)) != NO_ERROR)
            break;

        // Change mode to "next" for the remaining searches
        nMode = ND_HIT_FIND_NEXT_USED;

    }   // Repeat until we get a pointer to a file entry which is Active and Primary
    while (!(((ND_FPDE*)(*pFile))->wAttributes & ND_ATTR_ACTIVE) || (((ND_FPDE*)(*pFile))->wAttributes & ND_ATTR_EXTENDED));

    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the directory entry matching the file name
//---------------------------------------------------------------------------------

DWORD CND::Open(void** pFile, const char cName[11])
{

    ND_HIT nMode = ND_HIT_FIND_FIRST_USED;

    BYTE nHash = Hash(cName);

    DWORD dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Repeat while there is no match
    while (true)
    {

        // Get first/next file whose hash matches the requested name
        if ((dwError = ScanHIT(pFile, nMode, nHash)) != NO_ERROR)
            break;

        // Change mode to "next" for the remaining searches
        nMode = ND_HIT_FIND_NEXT_USED;

        // If file entry is not Active or Primary, skip to the next
        if (!(((ND_FPDE*)(*pFile))->wAttributes & ND_ATTR_ACTIVE) || (((ND_FPDE*)(*pFile))->wAttributes & ND_ATTR_EXTENDED))
            continue;

        // If entry's filename matches the caller's filename, we are done
        if (memcmp(((ND_FPDE*)(*pFile))->cName, cName, sizeof(cName)) == 0)
            break;

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Create a new file with the indicated name and size
//---------------------------------------------------------------------------------

DWORD CND::Create(void** pFile, OSI_FILE& File)
{

    ND_EXTENT   Extent;
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
    ((ND_FPDE*)(*pFile))->wAttributes = ND_ATTR_ACTIVE;

    // Set file password
    ((ND_FPDE*)(*pFile))->wUpdatePassword = 0x4296;
    ((ND_FPDE*)(*pFile))->wAccessPassword = 0x4296;

    // Set file properties as indicated by the caller
    SetFile(*pFile, File);

    // Calculate number of granules needed

    nGranules = ((ND_FPDE*)(*pFile))->wNext / m_nSPG + (((ND_FPDE*)(*pFile))->wNext % m_nSPG > 0 ? 1 : 0);

    if (nGranules == 0 && ((ND_FPDE*)(*pFile))->nEOF > 0)
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
        if ((dwError = CopyExtent(*pFile, ND_EXTENT_SET, nExtent, Extent)) != NO_ERROR)
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
            ((ND_FPDE*)pPrevious)->Link.nLump = 0xFE;
            ((ND_FPDE*)pPrevious)->Link.nGranules = FDE2DEC(*pFile);

            // Set a backward link from the previously existing directory entry
            ((ND_FXDE*)(*pFile))->nAttributes = ND_ATTR_ACTIVE|ND_ATTR_EXTENDED;
            ((ND_FXDE*)(*pFile))->nDEC = FDE2DEC(pPrevious);

            // Copy file name and extention to the new directory entry
            memcpy(((ND_FXDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
            memcpy(((ND_FXDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

            // Set corresponding HIT DEC with the calculated name hash
            m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash((const char *) ((ND_FXDE*)pFile)->cName);

            continue;

        }

        // Subtract number of allocated granules in this extent from the total required
        nGranules -= (Extent.nGranules & ND_GRANULE_COUNT) + 1;

    }

    // Write updated directory data and exit
    dwError = DirRW(ND_DIR_WRITE);
    goto Done;

    // Restore previous directory state
    Abort:
    DirRW(ND_DIR_READ);

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Move the file pointer
//---------------------------------------------------------------------------------

DWORD CND::Seek(void* pFile, DWORD dwPos)
{

    ND_EXTENT  Extent;
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
        if ((dwError = CopyExtent(pFile, ND_EXTENT_GET, nExtent, Extent)) != NO_ERROR)
        {
            m_wSector = 0xFFFF;
            break;
        }

        // Calculate number of contiguous sectors contained in this extent
        nGranuleSectors = ((Extent.nGranules & ND_GRANULE_COUNT) + 1) * m_nSPG;

        // Check whether it exceeds our need
        if (nGranuleSectors > nRemainingSectors)
        {   // RelativeSector = (Lump * GranulesPerLump + InitialGranule) * SectorsPerGranule + RemainingSectors
            m_wSector = (((Extent.nLump - m_DG.FT.nTrack) * m_nGPL) + ((Extent.nGranules & ND_GRANULE_INITIAL) >> 5)) * m_nSPG + nRemainingSectors;
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

DWORD CND::Read(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
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

        // Check whether last Seek() was successful
        if (m_wSector == 0xFFFF)
        {
            dwError = ERROR_SEEK;
            break;
        }

        // Check whether requested position doesn't exceed the file size
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

}

//---------------------------------------------------------------------------------
// Save data to file
//---------------------------------------------------------------------------------

DWORD CND::Write(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
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

        // Check whether last Seek() was successful
        if (m_wSector == 0xFFFF)
        {
            dwError = ERROR_SEEK;
            break;
        }

        // Check whether requested position doesn't exceed the file size
        if (m_dwFilePos > dwFileSize)
        {
            dwError = ERROR_WRITE_FAULT;
            break;
        }

        // Convert relative sector into Track, Side, Sector
        CHS(m_wSector, nTrack, nSide, nSector);

        // Get a pointer to the correct track descriptor
        pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

        // Calculate starting transfer point
        wSectorBegin = m_dwFilePos % pTrack->wSectorSize;

        // Calculate ending transfer point
        wSectorEnd = (dwFileSize - m_dwFilePos >= pTrack->wSectorSize ? pTrack->wSectorSize : dwFileSize % pTrack->wSectorSize);

        // Calculate transfer length
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

DWORD CND::Delete(void* pFile)
{

    ND_EXTENT  Extent;
    DWORD       dwError = NO_ERROR;

    // Invalidate any previous Seek()
    m_wSector = 0xFFFF;

    // Inactivate directory entry
    ((ND_FPDE*)pFile)->wAttributes &= !ND_ATTR_ACTIVE;

    // Release corresponding HIT slot
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = 0;

    // Loop through all extents releasing every allocated granule
    for (int x = 1; (dwError = CopyExtent(pFile, ND_EXTENT_GET, x, Extent)) == NO_ERROR; x++)
    {
        if ((dwError = DeleteExtent(Extent)) != NO_ERROR)
            break;
    }

    // If exited loop because reached the end of the extents table
    if (dwError == ERROR_NO_MATCH)
        dwError = DirRW(ND_DIR_WRITE); // Save the directory
    else
        DirRW(ND_DIR_READ);            // Otherwise, restore its previous state

    return dwError;

}

//---------------------------------------------------------------------------------
// Get DOS information (NewDOS/80 doesn't support DOS Version)
//---------------------------------------------------------------------------------

void CND::GetDOS(OSI_DOS& DOS)
{

    // Zero the structure
    memset(&DOS, 0, sizeof(OSI_DOS));

    // Get disk name
    memcpy(DOS.szName, ((ND_GAT*)m_pDir)->cDiskName, sizeof(DOS.szName) - 1);

    // Get disk date
    memcpy(&DOS.szDate, ((ND_GAT*)m_pDir)->cDiskDate, sizeof(DOS.szDate) - 1);

}

//---------------------------------------------------------------------------------
// Set DOS information
//---------------------------------------------------------------------------------

DWORD CND::SetDOS(OSI_DOS& DOS)
{

    // Set disk name
    memcpy(((ND_GAT*)m_pDir)->cDiskName, DOS.szName, sizeof(DOS.szName) - 1);

    // Set disk date
    memcpy(((ND_GAT*)m_pDir)->cDiskDate, &DOS.szDate, sizeof(DOS.szDate) - 1);

    return DirRW(ND_DIR_WRITE);

}

//---------------------------------------------------------------------------------
// Get file information (NewDOS/80 doesn't support file dates)
//---------------------------------------------------------------------------------

void CND::GetFile(void* pFile, OSI_FILE& File)
{

    // Zero the structure
    memset(&File, 0, sizeof(OSI_FILE));

    // Copy file name and extention
    memcpy(File.szName, ((ND_FPDE*)pFile)->cName, sizeof(File.szName) - 1);
    memcpy(File.szType, ((ND_FPDE*)pFile)->cType, sizeof(File.szType) - 1);

    // Get file size
    File.dwSize = GetFileSize(pFile);

    // Get file attributes
    File.nAccess    = ((ND_FPDE*)pFile)->wAttributes & ND_ATTR_ACCESS;
    File.bSystem    = ((ND_FPDE*)pFile)->wAttributes & ND_ATTR_SYSTEM;
    File.bInvisible = ((ND_FPDE*)pFile)->wAttributes & ND_ATTR_INVISIBLE;
    File.bModified  = ((ND_FPDE*)pFile)->wAttributes & ND_ATTR_MODIFIED;

}

//---------------------------------------------------------------------------------
// Set file information (public)
//---------------------------------------------------------------------------------

DWORD CND::SetFile(void* pFile, OSI_FILE& File)
{
    return SetFile(pFile, File, true);
}

//---------------------------------------------------------------------------------
// Set file information (protected)
//---------------------------------------------------------------------------------

DWORD CND::SetFile(void* pFile, OSI_FILE& File, bool bCommit)
{

    // Copy file name and extention
    memcpy(((ND_FPDE*)pFile)->cName, File.szName, sizeof(File.szName) - 1);
    memcpy(((ND_FPDE*)pFile)->cType, File.szType, sizeof(File.szType) - 1);

    // Set corresponding HIT DEC to the newly calculated name hash
    m_pDir[m_DG.LT.wSectorSize + FDE2DEC(pFile)] = Hash((const char *) ((ND_FPDE*)pFile)->cName);

    // Set file size
    ((ND_FPDE*)pFile)->nEOF = File.dwSize % m_DG.LT.wSectorSize;
    ((ND_FPDE*)pFile)->wNext = File.dwSize / m_DG.LT.wSectorSize + (((ND_FPDE*)pFile)->nEOF > 0 ? 1 : 0);

    // Set file attributes

    ((ND_FPDE*)pFile)->wAttributes &= ~ND_ATTR_ACCESS;
    ((ND_FPDE*)pFile)->wAttributes |= File.nAccess;

    ((ND_FPDE*)pFile)->wAttributes &= ~ND_ATTR_SYSTEM;
    ((ND_FPDE*)pFile)->wAttributes |= (ND_ATTR_SYSTEM * File.bSystem);

    ((ND_FPDE*)pFile)->wAttributes &= ~ND_ATTR_INVISIBLE;
    ((ND_FPDE*)pFile)->wAttributes |= (ND_ATTR_INVISIBLE * File.bInvisible);

    ((ND_FPDE*)pFile)->wAttributes &= ~ND_ATTR_MODIFIED;
    ((ND_FPDE*)pFile)->wAttributes |= (ND_ATTR_MODIFIED * File.bModified);

    return (bCommit ? DirRW(ND_DIR_WRITE) : NO_ERROR);

}

//---------------------------------------------------------------------------------
// Get file size
//---------------------------------------------------------------------------------

DWORD CND::GetFileSize(void* pFile)
{

    WORD wNext = ((ND_FPDE*)pFile)->wNext;
    BYTE nEOF = ((ND_FPDE*)pFile)->nEOF;

    return (wNext * m_DG.LT.wSectorSize - (nEOF > 0 ? m_DG.LT.wSectorSize - nEOF : 0));

}

//---------------------------------------------------------------------------------
// Read or Write the entire directory
//---------------------------------------------------------------------------------

DWORD CND::DirRW(ND_DIR nMode)
{

    BYTE    nTrack;
    BYTE    nSide;
    BYTE    nSector;
    DWORD   dwOffset = 0;
    DWORD   dwError = NO_ERROR;

    // Go through every relative sector
    for (BYTE nIndex = 0; nIndex < m_nDirSectors; nIndex++)
    {

        // Convert relative sector into Track/Side/Sector
        CHS(m_wDirSector + nIndex, nTrack, nSide, nSector);

        // Read or write the sector according to the requested mode
        if (nMode == ND_DIR_WRITE)
        {
            if ((dwError = m_pVDI->Write(nTrack, nSide, nSector, &m_pDir[dwOffset], m_DG.LT.wSectorSize)) != NO_ERROR)
                break;
        }
        else
        {
            if ((dwError = m_pVDI->Read(nTrack, nSide, nSector, &m_pDir[dwOffset], m_DG.LT.wSectorSize)) != NO_ERROR)
                break;
        }

        // Advance buffer pointer
        dwOffset += m_DG.LT.wSectorSize;

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Check the directory structure
//---------------------------------------------------------------------------------

DWORD CND::CheckDir(void)
{

    int     x;
    void*   pFile = NULL;
    int     nFiles = 0;
    DWORD   dwError = NO_ERROR;

    // Validate disk name
    for (x = 0; x < 8; x++)
    {
        if (((ND_GAT*)m_pDir)->cDiskName[x] < ' ' || ((ND_GAT*)m_pDir)->cDiskName[x] > 'z')
            goto Error;
    }

    // Validate disk date
    for (x = 0; x < 8; x++)
    {
        if (((ND_GAT*)m_pDir)->cDiskDate[x] < ' ' || ((ND_GAT*)m_pDir)->cDiskDate[x] > 'z')
            goto Error;
    }

    // Validate each file name in the directory (while counting the number of files)
    for (nFiles = 0; (dwError = Dir(&pFile, (pFile == NULL ? OSI_DIR_FIND_FIRST : OSI_DIR_FIND_NEXT))) == NO_ERROR; nFiles++)
    {

        // First 8 characters can be non-blanks
        for (x = 0; x < 8 && ((ND_FPDE*)pFile)->cName[x] >= '0' && ((ND_FPDE*)pFile)->cName[x] <= 'z'; x++);

        // But first one must be non-blank to be valid
        if (x == 0)
            goto Error;

        // If a blank is found, then only blanks can be used up to the end of the name
        for ( ; x < 8 && ((ND_FPDE*)pFile)->cName[x] == ' '; x++);

        // If not, then this name is invalid
        if (x < 8)
            goto Error;

        // The extension can have up to 3 non-blank characters
        for (x = 0; x < 3 && ((ND_FPDE*)pFile)->cType[x] >= '0' && ((ND_FPDE*)pFile)->cType[x] <= 'z'; x++);

        // If a blank is found, then only blanks can be used up to the end of the extension
        for ( ; x < 3 && ((ND_FPDE*)pFile)->cType[x] == ' '; x++);

        // If not, then this extension is invalid
        if (x < 3)
            goto Error;

    }

    // No error if exited on "no more files"    // but found files
    if (dwError == ERROR_NO_MORE_FILES)         // && nFiles > 0)
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

DWORD CND::ScanHIT(void** pFile, ND_HIT nMode, BYTE nHash)
{

    static int nLastCol = -1;
    static int nLastRow = 0;

    int nCols, nCol;
    int nRows, nRow;
    int nSlot;

    DWORD dwError = NO_ERROR;

    // If mode is any "Find First", reset static variables
    if (nMode != ND_HIT_FIND_NEXT_USED)
    {
        nLastCol = -1;
        nLastRow = 0;
    }

    // Retrieve last values from static variables
    nCol = nLastCol;
    nRow = nLastRow;

    // Calculates max HIT columns and rows
    nCols = m_nDirSectors - 2;
    nRows = m_DG.LT.wSectorSize / sizeof(ND_FPDE);

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
            else if (nMode == ND_HIT_FIND_FIRST_FREE)
                dwError = ERROR_DISK_FULL;
            else
                dwError = ERROR_NO_MORE_FILES;
            break;
        }

        // The 32nd byte of a NewDOS/80 HIT contains the count of extra FDE sectors
        if (nRow * sizeof(ND_FPDE) + nCol == 31)
            continue;

        // Get value in HIT[Row * sizeof(FPDE) + Col]
        nSlot = m_pDir[m_DG.LT.wSectorSize + (nRow * nCols * sizeof(ND_FPDE)) + nCol];

        // If this is not what we are looking for, skip to the next
        if ((nMode == ND_HIT_FIND_FIRST_FREE && nSlot != 0) || (nMode != ND_HIT_FIND_FIRST_FREE && nSlot == 0))
            continue;

        // If there is a hash to match and it doesn't match, skip to the next
        if (nHash != 0 && nHash != nSlot)
            continue;

        // Return pointer corresponding to the current Directory Entry Code (DEC)
        if ((*pFile = DEC2FDE((nRow << 5) + nCol)) == NULL)
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

DWORD CND::CreateExtent(ND_EXTENT& Extent, BYTE nGranules)
{

    BYTE    nInitialLump = 0;
    BYTE    nInitialGranule = 0;
    BYTE    nCurrentLump = 0;
    BYTE    nCurrentGranule = 0;
    BYTE    nAllocatedGranules = 0;
    DWORD   dwError = NO_ERROR;

    // Requested number of granules must be greater than zero
    if (nGranules == 0)
    {
        dwError = ERROR_INVALID_PARAMETER;
        goto Done;
    }

    // Find first empty slot (0) then continue up to the first non-empty slot (1)
    for (int nExpectedBit = 0; nExpectedBit < 2; nExpectedBit++)
    {

        // Test state of 'CurrentGranule' at GAT[CurrentCylinder]
        while ((m_pDir[nCurrentLump] & (1 << nCurrentGranule)) != nExpectedBit)
        {

            // Check if we are in the "continue up to the first non-empty slot" phase
            if (nExpectedBit == 1)
            {

                // Set granule as reserved
                m_pDir[nCurrentLump] |= (1 << nCurrentGranule);

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
            if (nCurrentGranule == m_nGPL)
            {

                // Reset granule index and advance to the next cylinder
                nCurrentGranule = 0;
                nCurrentLump++;

                // Check whether we've reached the disk's limit
                if (nCurrentLump == m_nLumps)
                {
                    dwError = ERROR_DISK_FULL;
                    goto Done;
                }

            }

        }

        // Check whether we are just leaving the "find the first empty slot" phase
        if (nExpectedBit == 0)
        {
            nInitialLump = nCurrentLump;
            nInitialGranule = nCurrentGranule;
        }

    }

    // Assemble Extent
    Stop:
    Extent.nLump = nInitialLump;
    Extent.nGranules = (nInitialGranule << 5) + (nAllocatedGranules - 1); // 3 MSB: Initial Granule, 5 LSB: Contiguous Granules minus 1

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Release disk space
//---------------------------------------------------------------------------------

DWORD CND::DeleteExtent(ND_EXTENT& Extent)
{

    DWORD dwError = NO_ERROR;

    // Get initial granule and count of granules in the extent
    int nIndex = (Extent.nGranules & ND_GRANULE_INITIAL) >> 5;
    int nCount = (Extent.nGranules & ND_GRANULE_COUNT) + 1;

    while (true)
    {

        // Reset bit nIndex of GAT[nCylinder]
        m_pDir[Extent.nLump] &= ~(1 << nIndex);

        // Repeat until nCount equals zero
        if (--nCount == 0)
            break;

        // If nIndex reaches the maximum number of granules per cylinder
        if (++nIndex == m_nGPL)
        {

            // Reset nIndex and advance to the next cylinder
            nIndex = 0;

            // If have reached the end of the disk, then something is wrong
            if (++Extent.nLump == m_nLumps)
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

DWORD CND::CopyExtent(void* pFile, ND_EXT nMode, BYTE nExtent, ND_EXTENT& Extent)
{

    ND_EXTENT*  pExtent;
    DWORD       dwError = NO_ERROR;

    // Go through the extents table
    for (int x = 1, y = 1; x < 6; x++)
    {

        // Point to File.Extent[x-1]
        pExtent = &(((ND_FPDE*)pFile)->Extent[x - 1]);

        // Check whether the last extent links this directory entry to another one
        if (pExtent->nLump == 0xFE && x == 5)
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
        if (pExtent->nLump == 0xFF && nMode == ND_EXTENT_GET)
            break;

        // Check whether we've reached the requested extent number (but not a corrupted link)
        if (y == nExtent && x != 5)
        {
            if (nMode == ND_EXTENT_GET)
            {
                Extent.nLump = pExtent->nLump;
                Extent.nGranules = pExtent->nGranules;
            }
            else
            {
                pExtent->nLump = Extent.nLump;
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

DWORD CND::GetFDE(void** pFile)
{

    DWORD dwError = NO_ERROR;

    if ((dwError = ScanHIT(pFile, ND_HIT_FIND_FIRST_FREE)) != NO_ERROR)
        goto Done;

    memset(*pFile, 0, sizeof(ND_FPDE));

    for (int x = 0; x < 5; x++)
    {
        ((ND_FPDE*)(*pFile))->Extent[x].nLump = 0xFF;
        ((ND_FPDE*)(*pFile))->Extent[x].nGranules = 0xFF;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
//---------------------------------------------------------------------------------

void* CND::DEC2FDE(BYTE nDEC)
{

    DWORD dwSectorOffset = ((nDEC & ND_DEC_SECTOR) + 2) * m_DG.LT.wSectorSize;
    DWORD dwEntryOffset = ((nDEC & ND_DEC_ENTRY) >> 5) * sizeof(ND_FPDE);

    return ((nDEC & ND_DEC_SECTOR) < m_nDirSectors ? &m_pDir[dwSectorOffset + dwEntryOffset] : NULL);

}

//---------------------------------------------------------------------------------
// Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
//---------------------------------------------------------------------------------

BYTE CND::FDE2DEC(void* pFile)
{

    // ((pFile - pDir) - ((GAT + HIT) * SectorSize)) / sizeof(FPDE)
    BYTE nDEC = (((BYTE*)pFile - m_pDir) - (2 * m_DG.LT.wSectorSize)) / sizeof(ND_FPDE);

    // Reorganize bits: 11111000 -> 00011111
    return ((nDEC << 5) + (nDEC >> 3));

}

//---------------------------------------------------------------------------------
// Return the hash code of a given file name
//---------------------------------------------------------------------------------

BYTE CND::Hash(const char* pName)
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

void CND::CHS(WORD wSector, BYTE& nTrack, BYTE& nSide, BYTE& nSector)
{

    // Track = RelativeSector / SectorsPerCylinder
    nTrack = wSector / m_nSPC;

    // Side = Remainder / SectorsPerTrack
    nSide = (wSector - (nTrack * m_nSPC)) / (m_nSPC / m_nSides);

    // Sector = Remainder
    nSector = wSector - (nTrack * m_nSPC + nSide * (m_nSPC / m_nSides));

    // Adjust Track, Side, Sector
    nTrack += m_DG.FT.nTrack + (m_nDensity == VDI_DENSITY_MIXED ? 1 : 0);
    nSide += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSide : m_DG.LT.nFirstSide);
    nSector += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSector : m_DG.LT.nFirstSector);

}

//---------------------------------------------------------------------------------
// Convert the TI bitmap in a printable string
//---------------------------------------------------------------------------------

char* CND::TI(WORD wTI)
{

    static char szTI[23];

    szTI[0] = 0;

    if (wTI & ND_TI_A)
        strcat(szTI, (strlen(szTI) ? ",A" : "A"));

    if (wTI & ND_TI_B)
        strcat(szTI, (strlen(szTI) ? ",B" : "B"));

    if (wTI & ND_TI_C)
        strcat(szTI, (strlen(szTI) ? ",C" : "C"));

    if (wTI & ND_TI_D)
        strcat(szTI, (strlen(szTI) ? ",D" : "D"));

    if (wTI & ND_TI_E)
        strcat(szTI, (strlen(szTI) ? ",E" : "E"));

    if (wTI & ND_TI_H)
        strcat(szTI, (strlen(szTI) ? ",H" : "H"));

    if (wTI & ND_TI_I)
        strcat(szTI, (strlen(szTI) ? ",I" : "I"));

    if (wTI & ND_TI_J)
        strcat(szTI, (strlen(szTI) ? ",J" : "J"));

    if (wTI & ND_TI_K)
        strcat(szTI, (strlen(szTI) ? ",K" : "K"));

    if (wTI & ND_TI_L)
        strcat(szTI, (strlen(szTI) ? ",L" : "L"));

    if (wTI & ND_TI_M)
        strcat(szTI, (strlen(szTI) ? ",M" : "M"));

    return szTI;

}
