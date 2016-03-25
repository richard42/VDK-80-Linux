//---------------------------------------------------------------------------------
// Virtual Disk Interface for DMK images
//---------------------------------------------------------------------------------

#include "windows.h"
#include <math.h>
#include "v80.h"
#include "vdi.h"
#include "dmk.h"

//---------------------------------------------------------------------------------
// Initialize member variables
//---------------------------------------------------------------------------------

CDMK::CDMK()
: m_Header(), m_pTrack(NULL), m_nCacheTrack(0xFF), m_nCacheSide(0xFF), m_bCacheWrite(false), m_nSides(0)
{
}

//---------------------------------------------------------------------------------
// Flush the cache and release allocated memory
//---------------------------------------------------------------------------------

CDMK::~CDMK()
{

    if (m_bCacheWrite)
        SaveTrack();

    if (m_pTrack != NULL)
        free(m_pTrack);

}

//---------------------------------------------------------------------------------
// Validate disk format and detect disk geometry
//---------------------------------------------------------------------------------

DWORD CDMK::Load(HANDLE hFile, DWORD dwFlags)
{

    DWORD dwBytes;
    DWORD dwError = NO_ERROR;

    // If not first Load and cache write is pending, do it
    if (m_bCacheWrite)
        SaveTrack();

    // Position file pointer at the disk header
    if (fseek(hFile, 0, 0) == -1)
    {
        dwError = ERROR_SEEK;
        goto Done;
    }

    // Read disk header
    if ((dwBytes = fread(&m_Header, 1, sizeof(m_Header), hFile)) != sizeof(m_Header))
    {
        dwError = ERROR_READ_FAULT;
        goto Done;
    }

    // If header signature does not indicate a virtual disk, this is not a DMK image
    if (m_Header.dwSignature != DMK_DISK_VIRTUAL)
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // If Write-Protected flag contains a value other than NO or YES, this is not a DMK image
    if (m_Header.nWriteProtected != DMK_WP_NO && m_Header.nWriteProtected != DMK_WP_YES)
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // If track count is not between 35 and 96, this is probably not a DMK image
    if (!(dwFlags & V80_FLAG_CHKDSK) && (m_Header.nTracks < 35 || m_Header.nTracks > 96))
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // If not first Load, release the previously allocated memory
    if (m_pTrack != NULL)
        free(m_pTrack);

    // Calculate needed memory to hold an entire track
    dwBytes = m_Header.wTrackLength + (V80_MEM - m_Header.wTrackLength % V80_MEM);

    // Allocate memory for an entire track
    if ((m_pTrack = (BYTE*)calloc(dwBytes,1)) == NULL)
    {
        dwError = ERROR_OUTOFMEMORY;
        goto Done;
    }

    // Copy file handle and user flags to member variables
    m_hFile = hFile;
    m_dwFlags = dwFlags;

    // (Re)Set cache control
    m_nCacheTrack = 0xFF;
    m_nCacheSide = 0xFF;

    // Detect disk geometry
    if ((dwError = FindGeometry()) != NO_ERROR)
        goto Done;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Read one sector from the disk
//---------------------------------------------------------------------------------

DWORD CDMK::Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)
{

    VDI_TRACK*  pTrack;
    DMK_SECTOR  Sector;
    DWORD       dwError = NO_ERROR;

    // Get a pointer to the correct track descriptor
    pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

    // Validate requested track
    if (nTrack < m_DG.FT.nTrack || nTrack > m_DG.LT.nTrack)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Validate requested side
    if (nSide < pTrack->nFirstSide || nSide > pTrack->nLastSide)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Validate requested sector
    if (nSector < pTrack->nFirstSector || nSector > pTrack->nLastSector)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Check whether there is a need to load the track from the disk
    if ((dwError = LoadTrack(nTrack, nSide)) != NO_ERROR)
        goto Done;

    // Get sector header
    if ((dwError = GetSectorId(Sector, nTrack, nSide, nSector)) != NO_ERROR)
        goto Done;

    // Get sector data
    if ((dwError = GetSectorData(Sector, pBuffer, wSize)) != NO_ERROR)
        goto Done;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Write one sector to the disk
//---------------------------------------------------------------------------------

DWORD CDMK::Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)
{

    VDI_TRACK*  pTrack;
    DMK_SECTOR  Sector;
    DWORD       dwError = NO_ERROR;

    // Get a pointer to the correct track descriptor
    pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

    // Validate requested track
    if (nTrack < m_DG.FT.nTrack || nTrack > m_DG.LT.nTrack)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Validate requested side
    if (nSide < pTrack->nFirstSide || nSide > pTrack->nLastSide)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Validate requested sector
    if (nSector < pTrack->nFirstSector || nSector > pTrack->nLastSector)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Check whether there is a need to load the track from the disk
    if ((dwError = LoadTrack(nTrack, nSide)) != NO_ERROR)
        goto Done;

    // Get sector header
    if ((dwError = GetSectorId(Sector, nTrack, nSide, nSector)) != NO_ERROR)
        goto Done;

    // Update sector data
    if ((dwError = PutSectorData(Sector, pBuffer, wSize)) != NO_ERROR)
        goto Done;

    // Set the write-pending flag
    m_bCacheWrite = true;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Detect the disk geometry
//---------------------------------------------------------------------------------

DWORD CDMK::FindGeometry()
{

    DWORD       dwTrack[4];
    VDI_TRACK*  pTrack;
    BYTE*       pIDAM;
    WORD        wPTR;
    BYTE        nTrack;
    BYTE        nSide;
    BYTE        nSector;
    BYTE        nSize;
    VDI_DENSITY nDensity;
    BYTE        nDoubled;
    DWORD       dwBytes;
    DWORD       dwError = NO_ERROR;

    // Get number of disk sides from the disk header
    m_nSides = (m_Header.nFlags & DMK_FLAG_SINGLE_SIDED ? 1 : 2);

    // Calculate file offset for both sides of the first and last tracks in the disk
    dwTrack[0] = sizeof(DMK_HEADER);    // First track, first side
    dwTrack[1] = (m_nSides > 1 ? m_Header.wTrackLength + sizeof(DMK_HEADER) : 0);   // First track, second side (if exist)
    dwTrack[2] = (m_Header.nTracks - 1) * m_nSides * m_Header.wTrackLength + sizeof(DMK_HEADER); // Last track, first side
    dwTrack[3] = (m_nSides > 1 ? ((m_Header.nTracks - 1) * m_nSides + 1) * m_Header.wTrackLength + sizeof(DMK_HEADER) : 0); // Last track, second side (if exist)

    // Zero the disk geometry structure
    memset(&m_DG, 0, sizeof(m_DG));

    // Set some fields to high values, so we can look for the highest ones
    m_DG.FT.nTrack = 0xFF;
    m_DG.FT.nFirstSector = 0xFF;
    m_DG.FT.nFirstSide = 0xFF;
    m_DG.LT.nFirstSector = 0xFF;
    m_DG.LT.nFirstSide = 0xFF;

    // Go through the four tracks in the array
    for (int t = 0; t < 4; t++)
    {

        // If disk has only one side, skip to the next track
        if (dwTrack[t] == 0)
            continue;

        // Set file pointer to the calculated track offset
        if (fseek(m_hFile, dwTrack[t], 0) == -1)
        {
            dwError = ERROR_SEEK;
            goto Done;
        }

        // Read track header
        if ((dwBytes = fread(m_pTrack, 1, m_Header.wTrackLength, m_hFile)) != m_Header.wTrackLength )
        {
            dwError = ERROR_READ_FAULT;
            goto Done;
        }

        // Go through the IDAM pointers
        for (int x = 0; x < 64; x++)
        {

            // Get pointer at [x]
            wPTR = (m_pTrack[x * 2 + 1] << 8) + m_pTrack[x * 2];

            // If pointer equals zero then we have reached the end of the list
            if (wPTR == 0)
                break;

            // The pointer points to the sector ID Address Mark
            pIDAM = &m_pTrack[wPTR & DMK_IDAM_OFFSET];

            // The pointer also contains the sector density flag
            nDensity = (wPTR & DMK_IDAM_DENSITY ? VDI_DENSITY_DOUBLE : VDI_DENSITY_SINGLE);

            // Some sectors have each byte doubled for track consistency
            nDoubled = ((m_Header.nFlags & (DMK_FLAG_SINGLE_DENSITY+DMK_FLAG_IGNORE_DENSITY)) != 0 || nDensity != 0 ? 0 : 1);

            // Extract track number, side, sector and sector size from the sector header
            nTrack =    ((DMK_SECTOR*)(pIDAM + 1 * nDoubled))->SID.nTrack;
            nSide =     ((DMK_SECTOR*)(pIDAM + 2 * nDoubled))->SID.nSide;
            nSector =   ((DMK_SECTOR*)(pIDAM + 3 * nDoubled))->SID.nSector;
            nSize =     ((DMK_SECTOR*)(pIDAM + 4 * nDoubled))->SID.nSize;

            // Check whether sector number has the MSB set and clear it (TRSDOS protection scheme?)
            if (nSector >= 0x80)
                nSector &= 0x7F;

            // Initialize first track number, density and sector size
            if (t == 0 && x == 0)
            {
                m_DG.FT.nTrack = nTrack;
                m_DG.FT.nDensity = nDensity;
                m_DG.FT.wSectorSize = pow(2, nSize) * 128;
            }

            // Initialize last track number, density and sector size
            if (t == 2 && x == 0)
            {
//                m_DG.LT.nTrack = m_DG.FT.nTrack + (m_Header.nTracks - 1);
                m_DG.LT.nTrack = nTrack;
                m_DG.LT.nDensity = nDensity;
                m_DG.LT.wSectorSize = pow(2, nSize) * 128;
            }

            // Get a pointer to the correct track descriptor
            pTrack = (t < 2 ? &m_DG.FT : &m_DG.LT);

            // Check whether side number is lower than the lowest side number
            if (nSide < pTrack->nFirstSide)
                pTrack->nFirstSide = nSide;

            // Check whether side number is higher than the highest side number
            if (nSide > pTrack->nLastSide)
                pTrack->nLastSide = nSide;

            // Check whether sector number is lower than the lowest sector number
            if (nSector < pTrack->nFirstSector)
                pTrack->nFirstSector = nSector;

            // Check whether sector number is higher than the highest sector number
            if (nSector > pTrack->nLastSector)
                pTrack->nLastSector = nSector;

        }

    }

    // Check whether the disk structure is ok
    if (m_DG.FT.nTrack > m_DG.LT.nTrack || m_DG.FT.wSectorSize == 0 || m_DG.LT.wSectorSize == 0)
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Read one entire track from the disk
//---------------------------------------------------------------------------------

DWORD CDMK::LoadTrack(BYTE nTrack, BYTE nSide)
{

    VDI_TRACK*  pTrack;
    DWORD       dwBytes;
    DWORD       dwError = NO_ERROR;

    // Check whether the requested track is already loaded
    if (nTrack != m_nCacheTrack || nSide != m_nCacheSide)
    {

        // Flush cache before reloading it
        if ((dwError = SaveTrack()) != NO_ERROR)
            goto Done;

        // Get a pointer to the correct track descriptor
        pTrack = (nTrack == m_DG.FT.nTrack ? &m_DG.FT : &m_DG.LT);

        // Calculate track offset
        dwBytes = ((nTrack - m_DG.FT.nTrack) * m_nSides + (nSide - pTrack->nFirstSide)) * m_Header.wTrackLength + sizeof(DMK_HEADER);

        // Set file pointer
        if (fseek(m_hFile, dwBytes, 0) == -1)
        {
            dwError = ERROR_SEEK;
            goto Done;
        }

        // Invalidate current cache
        m_nCacheTrack = 0xFF;
        m_nCacheSide = 0xFF;

        // Read track
        if  ((dwBytes = fread(m_pTrack, 1, m_Header.wTrackLength, m_hFile)) != m_Header.wTrackLength)
        {
            dwError = ERROR_READ_FAULT;
            goto Done;
        }

        // Update cache control
        m_nCacheTrack = nTrack;
        m_nCacheSide = nSide;

    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Write one entire track to the disk
//---------------------------------------------------------------------------------

DWORD CDMK::SaveTrack()
{

    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // Check whether there is a pending cache write
    if (m_bCacheWrite)
    {

        // Calculate track offset
        dwBytes = (m_nCacheTrack * m_nSides + m_nCacheSide) * m_Header.wTrackLength + sizeof(DMK_HEADER);

        // Set file pointer
        if (fseek(m_hFile, dwBytes, 0) == -1)
        {
            dwError = ERROR_SEEK;
            goto Done;
        }

        // Write track
        if  ((dwBytes = fwrite(m_pTrack, 1, m_Header.wTrackLength, m_hFile)) != m_Header.wTrackLength)
        {
            dwError = ERROR_WRITE_FAULT;
            goto Done;
        }

        // Reset the write-pending flag
        m_bCacheWrite = false;

    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Retrieve sector header
//---------------------------------------------------------------------------------

DWORD CDMK::GetSectorId(DMK_SECTOR& Sector, BYTE nTrack, BYTE nSide, BYTE nSector)
{

    WORD    wPTR;
    BYTE*   pIDAM;
    BYTE*   pDAM;
    BYTE    nDensity;
    BYTE    nDoubled;
    BYTE    nBytes;
    DWORD   dwError = NO_ERROR;

    memset(&Sector, 0, sizeof(Sector));

    // Go through all IDAM pointers looking for a matching sector
    for (int x = 0; x < 64; x++)
    {

        // Get pointer at [x]
        wPTR = (m_pTrack[x * 2 + 1] << 8) + m_pTrack[x * 2];

        // If pointer equals zero then we have reached the end of the list
        if (wPTR == 0)
        {
            dwError = ERROR_SECTOR_NOT_FOUND;
            break;
        }

        // The pointer points to the sector ID Address Mark
        pIDAM = &m_pTrack[wPTR & DMK_IDAM_OFFSET];

        // The pointer also contains the sector density flag
        nDensity = (wPTR & DMK_IDAM_DENSITY ? 1 : 0);

        // Some sectors have each byte doubled for track consistency
        nDoubled = ((m_Header.nFlags & (DMK_FLAG_SINGLE_DENSITY+DMK_FLAG_IGNORE_DENSITY)) != 0 || nDensity != 0 ? 0 : 1);

        // If sector number does not match the requested sector, skip to the next
        if (((DMK_SID*)(pIDAM + 3 * nDoubled))->nSector != nSector)
            continue;

        // The Data Address Mark (DAM) should be looked right after the sector header
        pDAM = pIDAM + (sizeof(DMK_SID) << nDoubled);

        // The DAM must be found in a range no longer than 43 bytes
        for (int y = 0; (y < 43) && (*pDAM < 0xF8 || *pDAM > 0xFB); y++, pDAM++);

        // If DAM not found, quit
        if (*pDAM < 0xF8 || *pDAM > 0xFB)
        {
            dwError = ERROR_FLOPPY_ID_MARK_NOT_FOUND;
            break;
        }

        // Copy original sector header to the caller's structure

        nBytes = sizeof(DMK_SID);

        for (BYTE *pSource = pIDAM, *pTarget = (BYTE*)&Sector.SID; nBytes > 0; pSource += (1 + nDoubled), pTarget++, nBytes--)
            *pTarget = *pSource;

        // Fill the remaining structure fields
        Sector.wIDAM = wPTR;                                                // Original pointer (contains flags)
        Sector.pIDAM = pIDAM;                                               // Pointer to IDAM (header)
        Sector.pDAM = pDAM;                                                 // Pointer to DAM (data)
        Sector.wSize = pow(2, Sector.SID.nSize) * 128;                      // Sector size (the header contains a code)(if doubled, divide the size by 2)
        Sector.nDoubled = nDoubled;                                         // Flag indicating whether the sector content is doubled

        // Done
        break;

    }

    return dwError;

}

//---------------------------------------------------------------------------------
// Retrieve sector date
//---------------------------------------------------------------------------------

DWORD CDMK::GetSectorData(DMK_SECTOR& Sector, BYTE* pBuffer, WORD wSize)
{

    DWORD dwError = NO_ERROR;

    // Check whether the Sector structure is properly initialized
    if (Sector.pDAM == NULL)
    {
        dwError = ERROR_FLOPPY_ID_MARK_NOT_FOUND;
        goto Done;
    }

    // Check caller's buffer size
    if (wSize < (Sector.wSize >> Sector.nDoubled))
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Copy sector data to the caller's buffer
    for (int x = 0; x < Sector.wSize; x++)
        pBuffer[x] = Sector.pDAM[(x + 1) << Sector.nDoubled];

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Update sector data
//---------------------------------------------------------------------------------

DWORD CDMK::PutSectorData(DMK_SECTOR& Sector, BYTE* pBuffer, WORD wSize)
{

    DWORD dwError = NO_ERROR;

    // Check whether the Sector structure is properly initialized
    if (Sector.pDAM == NULL)
    {
        dwError = ERROR_FLOPPY_ID_MARK_NOT_FOUND;
        goto Done;
    }

    // Check caller's buffer size
    if (wSize > (Sector.wSize >> Sector.nDoubled))
        wSize = (Sector.wSize >> Sector.nDoubled);

    // Properly copy sector data from the caller's buffer (doubling it if required)
    for (int x = 0; x < wSize; x++)
    {

        Sector.pDAM[(x + 1) << Sector.nDoubled] = pBuffer[x];

        if (Sector.nDoubled != 0)
            Sector.pDAM[((x + 1) << Sector.nDoubled) + 1] = pBuffer[x];

    }

    // Update sector CRC
    UpdateCRC(Sector);

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Update sector CRC
//---------------------------------------------------------------------------------
// Algorithm (based on libdmk by Eric Smith):
//   CRC = 0xFFFF
//   For each Data byte:
//      Data = Data SHL 8
//      Repeat 8 times:
//          CRC = (CRC SHL 1) XOR (((CRC XOR Data) & 0x8000) ? 0x1021 : 0x0000)
//          Data = Data SHL 1
//---------------------------------------------------------------------------------

void CDMK::UpdateCRC(DMK_SECTOR& Sector)
{

    WORD    wCRC;
    WORD    wData;

    wCRC = (Sector.wIDAM & DMK_IDAM_DENSITY ? 0xCDB4 : 0xFFFF);

    for (int x = 0; x < (Sector.wSize + 1); x++)
    {

        wData = Sector.pDAM[x << Sector.nDoubled] << 8;

        for (int y = 0; y < 8; y++)
        {
            wCRC = (wCRC << 1) ^ ((wCRC ^ wData) & 0x8000 ? 0x1021 : 0x0000);
            wData <<= 1;
        }

    }

    if (Sector.nDoubled == 0)
    {
        Sector.pDAM[Sector.wSize + 1] = (wCRC & 0xFF00) >> 8;
        Sector.pDAM[Sector.wSize + 2] = (wCRC & 0x00FF);
    }
    else
    {
        Sector.pDAM[(Sector.wSize * 2) + 1] = (wCRC & 0xFF00) >> 8;
        Sector.pDAM[(Sector.wSize * 2) + 2] = (wCRC & 0xFF00) >> 8;
        Sector.pDAM[(Sector.wSize * 2) + 3] = (wCRC & 0x00FF);
        Sector.pDAM[(Sector.wSize * 2) + 4] = (wCRC & 0x00FF);
    }

}
