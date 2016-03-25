//---------------------------------------------------------------------------------
// Virtual Disk Interface for JV3 images
//---------------------------------------------------------------------------------

#include "windows.h"

#include <typeinfo>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "v80.h"
#include "vdi.h"
#include "jv3.h"

//---------------------------------------------------------------------------------
// Initialize member variables
//---------------------------------------------------------------------------------

CJV3::CJV3()
: m_pHeader(NULL), m_bExtended(false), m_wSectors(0)
{
}

//---------------------------------------------------------------------------------
// Release allocated memory
//---------------------------------------------------------------------------------

CJV3::~CJV3()
{
    if (m_pHeader != NULL)
        delete[] m_pHeader;
}

static DWORD GetFileSize(HANDLE hFile)
{
    struct stat buf;
    int no = fileno(hFile);
    fstat(no, (struct stat *) &buf);
    return(buf.st_size);
}

//---------------------------------------------------------------------------------
// Validate disk format and detect disk geometry
//---------------------------------------------------------------------------------

DWORD CJV3::Load(HANDLE hFile, DWORD dwFlags)
{

    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // If not first Load, release the previously allocated memory
    if (m_pHeader != NULL)
        delete[] m_pHeader;

    // Allocate memory for two JV3 headers
    m_pHeader = new JV3_HEADER[2];

    // Position file pointer at the first header
    if (fseek(hFile, 0, 0) == -1)
    {
        dwError = ERROR_SEEK;
        goto Done;
    }

    // Read first header to m_pHeader[0]
    if ((dwBytes = fread(&m_pHeader[0], 1, sizeof(JV3_HEADER), hFile)) != sizeof(JV3_HEADER) )
    {
        dwError = ERROR_READ_FAULT;
        goto Done;
    }

    // Calculate position of the second header as the sum of sector sizes plus the first header size

    dwBytes = sizeof(JV3_HEADER);

    for (int x = 0; x < 2901 && m_pHeader->Sector[x].nTrack != 0xFF; x++)
        dwBytes += GetSectorSize(m_pHeader->Sector[x]);

    // Check if file size indicates the existance of a second header

    m_bExtended = false;

    if (GetFileSize(hFile) > (dwBytes + sizeof(JV3_HEADER)))
    {

        // Position file pointer at the second header
        if (fseek(hFile, dwBytes, 0) == -1)
            throw ERROR_SEEK;

        // Read second header to m_pHeader[1]
        if ((dwBytes = fread(&m_pHeader[1], 1, sizeof(JV3_HEADER), hFile)) != sizeof(JV3_HEADER))
            throw ERROR_READ_FAULT;

        // Set flag indicating that this is an extended disk
        m_bExtended = true;

    }

    // Detect disk geometry
    FindGeometry();

    // Track count must be between 35 and 96 or this is probably not a JV3 image
    if (!(dwFlags & V80_FLAG_CHKDSK) && (m_DG.LT.nTrack < 34 || m_DG.LT.nTrack > 95))
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // Sector count must be between 10 and 30 or this is probably not a JV3 image
    if (m_DG.LT.nLastSector < 9 || m_DG.LT.nLastSector > 29)
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // Copy file handle and user flags to member variables
    m_hFile = hFile;
    m_dwFlags = dwFlags;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Read one sector from the disk
//---------------------------------------------------------------------------------

DWORD CJV3::Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)
{

    VDI_TRACK*  pTrack;
    DWORD       dwBytes;
    DWORD       dwError = NO_ERROR;

    // Get a pointer to the correct track descriptor
    pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

    // Check caller's buffer size
    if (wSize < pTrack->wSectorSize)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Set file pointer
    if ((dwError = Seek(nTrack, nSide, nSector)) != NO_ERROR)
        goto Done;

    // Read one sector directly to the caller's buffer
    if ((dwBytes = fread(pBuffer, 1, pTrack->wSectorSize, m_hFile)) != pTrack->wSectorSize )
    {
        dwError = ERROR_READ_FAULT;
        goto Done;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Write one sector to the disk
//---------------------------------------------------------------------------------

DWORD CJV3::Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)
{

    VDI_TRACK*  pTrack;
    DWORD       dwBytes;
    DWORD       dwError = NO_ERROR;

    // Get a pointer to the correct track descriptor
    pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

    // Check caller's buffer size
    if (wSize > pTrack->wSectorSize)
        wSize = pTrack->wSectorSize;

    // Set file pointer
    if ((dwError = Seek(nTrack, nSide, nSector)) != NO_ERROR)
        goto Done;

    // Write one sector directly from the caller's buffer
    if ((dwBytes = fwrite(pBuffer, 1, wSize, m_hFile)) != wSize  )
    {
        dwError = ERROR_WRITE_FAULT;
        goto Done;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Detect the disk geometry
//---------------------------------------------------------------------------------

void CJV3::FindGeometry()
{

    VDI_TRACK*  pTrack;
    JV3_SECTOR  Sector;

    // Zero the disk geometry structure
    memset(&m_DG, 0, sizeof(m_DG));

    // Set last sectors to high values, so we can look for the highest ones
    m_DG.FT.nFirstSector = 0xFF;
    m_DG.LT.nFirstSector = 0xFF;

    // Zero total disk sectors
    m_wSectors = 0;

    // Go through the entire disk header
    for (int x = 0, y = 2901 * (m_bExtended ? 2 : 1); x < y; x++)
    {

        // Get a sector header
        GetSectorHeader(Sector, x);

        // If has reached the area of free sectors, stop
        if (Sector.nTrack == JV3_SECTOR_FREE || Sector.nSector == JV3_SECTOR_FREE || Sector.nFlags >= JV3_SECTOR_FREEF)
            break;

        // Check whether the track number is higher than the highest track number
        if (Sector.nTrack > m_DG.LT.nTrack)
            m_DG.LT.nTrack = Sector.nTrack;

        // Get a pointer to the correct track descriptor
        pTrack = (Sector.nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

        // If this descriptor is found to be "uninitialized", set its sector size and density
        if (pTrack->wSectorSize == 0)
        {
            pTrack->wSectorSize = GetSectorSize(Sector);
            pTrack->nDensity = (((Sector.nFlags & JV3_FLAG_DENSITY) >> 7) ? VDI_DENSITY_DOUBLE : VDI_DENSITY_SINGLE);
        }

        // Check whether sector number has the MSB set and clear it (TRSDOS protection scheme?)
        if (Sector.nSector >= 0x80)
            Sector.nSector &= 0x7F;

        // Check whether sector number is lower than the lowest sector number
        if (Sector.nSector < pTrack->nFirstSector)
            pTrack->nFirstSector = Sector.nSector;

        // Check whether sector number is higher than the highest sector number
        if (Sector.nSector > pTrack->nLastSector)
            pTrack->nLastSector = Sector.nSector;

        // Check whether side number is other than zero
        if ((Sector.nFlags & JV3_FLAG_SIDE) != 0)
            pTrack->nLastSide = 1;

        // Increment sector count
        m_wSectors++;

    }

}

//---------------------------------------------------------------------------------
// Position the file pointer
//---------------------------------------------------------------------------------

DWORD CJV3::Seek(BYTE nTrack, BYTE nSide, BYTE nSector)
{

    VDI_TRACK*  pTrack;
    JV3_SECTOR  Sector;
    WORD        wCurrent;
    WORD        wTotal;
    DWORD       dwOffset = sizeof(JV3_HEADER);
    DWORD       dwError = NO_ERROR;

    // Get a pointer to the correct track descriptor
    pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

    // Validate Track, Side, Sector

    if (nTrack < m_DG.FT.nTrack || nTrack > m_DG.LT.nTrack)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    if (nSide < pTrack->nFirstSide || nSide > pTrack->nLastSide)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    if (nSector < pTrack->nFirstSector || nSector > pTrack->nLastSector)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Scan entire disk header for the requested sector
    for (wCurrent = 0, wTotal = 2901 * (m_bExtended ? 2 : 1); wCurrent < wTotal; wCurrent++)
    {

        // Get a sector header
        GetSectorHeader(Sector, wCurrent);

        // If has reached the area of free sectors without any matches, return sector not found
        if (Sector.nTrack == JV3_SECTOR_FREE || Sector.nSector == JV3_SECTOR_FREE || Sector.nFlags >= JV3_SECTOR_FREEF)
        {
            dwError = ERROR_SECTOR_NOT_FOUND;
            goto Done;
        }

        // If current sector matches the requested sector, end the search
        if (Sector.nTrack == nTrack && ((Sector.nFlags & JV3_FLAG_SIDE) >> 4) == nSide && Sector.nSector == nSector)
            break;

        // Sum current sector size to the offset
        dwOffset += GetSectorSize(Sector);

    }

    // If sector was found in the second header, add the header size to the offset
    if (wCurrent > 2900)
        dwOffset += sizeof(JV3_HEADER);

    // Set file pointer to the calculated sector offset
    if (fseek(m_hFile, dwOffset, 0) == -1)
    {
        dwError = ERROR_SEEK;
        goto Done;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a sector size
//---------------------------------------------------------------------------------

WORD CJV3::GetSectorSize(const JV3_SECTOR& Sector)
{

    // Assemble an array of sector sizes as they are found in the sector header
    WORD wSize[4] = {JV3_SIZE0, JV3_SIZE1, JV3_SIZE2, JV3_SIZE3};

    // Isolate sector size bits from the sector flags
    BYTE nSize = Sector.nFlags & JV3_FLAG_SIZE;

    // If sector is free the bits must be inverted for the array to work as expected
    return wSize[(Sector.nFlags >= JV3_SECTOR_FREEF ? ~nSize : nSize)];

}

//---------------------------------------------------------------------------------
// Copy sector data from 1st or 2nd header
//---------------------------------------------------------------------------------

void CJV3::GetSectorHeader(JV3_SECTOR& Sector, WORD wSector)
{
    Sector = (wSector < 2901 ? m_pHeader->Sector[wSector] : (m_pHeader + sizeof(JV3_HEADER))->Sector[wSector]);
}
