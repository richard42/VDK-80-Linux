/**
 @file jv1.cpp

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
// Virtual Disk Interface for JV1 images
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
#include "jv1.h"

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

DWORD CJV1::Load(HANDLE hFile, DWORD dwFlags)
{

    DWORD   dwFileSize;
    DWORD   dwError = NO_ERROR;

    // Zero the disk geometry structure
    memset(&m_DG, 0, sizeof(m_DG));

    // Set sector size fields to JV1 standard
    m_DG.FT.wSectorSize = JV1_SECTORSIZE;
    m_DG.LT.wSectorSize = JV1_SECTORSIZE;


    // Get file size
    dwFileSize = GetFileSize(hFile);


    // It must be a multiple of a sector size (there are no headers or anything)
    if (dwFileSize % JV1_SECTORSIZE != 0)
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // Calculate number of disk sectors
    m_wSectors = dwFileSize / JV1_SECTORSIZE;

    if (m_wSectors <= 40*10*1)              // 40 Tracks, 10 Sectors per Track, Single Sided
    {
        m_DG.LT.nTrack = m_wSectors / 10 - 1;
        m_DG.FT.nLastSector = 9;
        m_DG.LT.nLastSector = 9;
    }
    else if (m_wSectors <= 40*18*1)         // 40 Tracks, 18 Sectors per Track, Single Sided
    {
        m_DG.LT.nTrack = m_wSectors / 18 - 1;
        m_DG.FT.nLastSector = 17;
        m_DG.LT.nLastSector = 17;
        m_DG.FT.nDensity = VDI_DENSITY_DOUBLE;
        m_DG.LT.nDensity = VDI_DENSITY_DOUBLE;
    }
    else if (m_wSectors <= 40*10*2)         // 40 Tracks, 10 Sectors per Track, Double Sided
    {
        m_DG.LT.nTrack = m_wSectors / 20 - 1;
        m_DG.FT.nLastSector = 9;
        m_DG.LT.nLastSector = 9;
        m_DG.FT.nLastSide = 1;
        m_DG.LT.nLastSide = 1;
    }
    else                                    // +40 Tracks, 18 Sectors per Track, Double Sided
    {
        m_DG.LT.nTrack = m_wSectors / 36 - 1;
        m_DG.FT.nLastSector = 17;
        m_DG.LT.nLastSector = 17;
        m_DG.FT.nLastSide = 1;
        m_DG.LT.nLastSide = 1;
        m_DG.FT.nDensity = VDI_DENSITY_DOUBLE;
        m_DG.LT.nDensity = VDI_DENSITY_DOUBLE;
    }

    // Track count must be exact (no remainder) or this is not a JV1 image
    if ((m_wSectors % (m_DG.FT.nLastSector + 1)) != 0)
    {
        dwError = ERROR_UNRECOGNIZED_MEDIA;
        goto Done;
    }

    // Track count must be between 34 and 96 or this is probably not a JV1 image
    if (!(dwFlags & V80_FLAG_CHKDSK) && (m_DG.LT.nTrack < 33 || m_DG.LT.nTrack > 95))
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

DWORD CJV1::Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)
{

    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // Check caller's buffer size
    if (wSize < JV1_SECTORSIZE)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Set file pointer
    if ((dwError = Seek(nTrack, nSide, nSector)) != NO_ERROR)
        goto Done;

    // Read one sector directly to the caller's buffer
    if ((dwBytes = fread(pBuffer, 1, JV1_SECTORSIZE,m_hFile)) != JV1_SECTORSIZE)
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

DWORD CJV1::Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)
{

    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // Check caller's buffer size
    if (wSize > JV1_SECTORSIZE)
        wSize = JV1_SECTORSIZE;

    // Set file pointer
    if ((dwError = Seek(nTrack, nSide, nSector)) != NO_ERROR)
        goto Done;

    // Write one sector directly from the caller's buffer
    if ((dwBytes = fwrite(pBuffer, 1, wSize, m_hFile)) != wSize )
    {
        dwError = ERROR_WRITE_FAULT;
        goto Done;
    }

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Position the file pointer
//---------------------------------------------------------------------------------

DWORD CJV1::Seek(BYTE nTrack, BYTE nSide, BYTE nSector)
{

    DWORD   dwOffset;
    DWORD   dwError = NO_ERROR;

    // Validate Track, Side, Sector
    if (nTrack > m_DG.LT.nTrack || nSide > m_DG.LT.nLastSide || nSector > m_DG.LT.nLastSector)
    {
        dwError = ERROR_SECTOR_NOT_FOUND;
        goto Done;
    }

    // Compute sector offset
    dwOffset = ((nTrack * (m_DG.LT.nLastSide + 1) + nSide) * (m_DG.LT.nLastSector + 1) + nSector) * 256;

    // Set file pointer
    if (fseek(m_hFile, dwOffset, 0) == -1)
    {
        dwError = ERROR_SEEK;
        goto Done;
    }

    Done:
    return dwError;

}
