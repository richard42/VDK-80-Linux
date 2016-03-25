/**
 @file dd.cpp

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
// Operating System Interface for DBLDOS
//---------------------------------------------------------------------------------

#include "windows.h"
#include <stdio.h>
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "nd.h"
#include "dd.h"

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CDD::Load(CVDI* pVDI, DWORD dwFlags)
{

    BYTE    nFT;
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

    // Check whether number of sector per tracks or disk density is different of the other tracks
    m_nDensity = ((m_DG.FT.nLastSector - m_DG.FT.nFirstSector + 1) != (m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1) || m_DG.FT.nDensity != m_DG.LT.nDensity ? VDI_DENSITY_MIXED : m_DG.LT.nDensity);

    // Calculate de first track number depending on disk density
    nFT = m_DG.FT.nTrack + (m_nDensity == VDI_DENSITY_MIXED ? 1 : 0);

    // Check internal buffer size (just in case)
    if (sizeof(m_Buffer) < m_DG.FT.wSectorSize)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Load PDRIVE table from first track, third sector
    if ((dwError = m_pVDI->Read(nFT, m_DG.LT.nFirstSide, m_DG.LT.nFirstSector + 2, m_Buffer, m_DG.LT.wSectorSize)) != NO_ERROR)
        goto Done;

    // Assume initial disk parameters

    m_nDDSL     = 17;
    m_nDDGA     = 2;
    m_nSPG      = 5;
    m_nGPL      = 2;
    m_nSPC      = (m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1) * m_nSides;
    m_nLumps    = ((m_DG.FT.nLastSector - m_DG.FT.nFirstSector + 1) + (m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1) * (m_DG.LT.nTrack - m_DG.FT.nTrack)) / m_nSPG / m_nGPL;

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
        if ((dwError = CheckDir()) == NO_ERROR)
            goto Done;

    dwError = ERROR_NOT_DOS_DISK;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Check the directory structure
//---------------------------------------------------------------------------------

DWORD CDD::CheckDir(void)
{

    int     x;
    void*   pFile = NULL;
    int     nFiles = 0;
    DWORD   dwError = NO_ERROR;

    // If dir check has not been disabled
    if  (!(m_dwFlags & V80_FLAG_CHKDIR))
    {

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

    // Ok if Dir() exited on this error     // but found files
    if (dwError == ERROR_NO_MORE_FILES)     // && nFiles > 0)
    {
        dwError = NO_ERROR;
        goto Done;
    }

    Error:
    dwError = ERROR_FILE_CORRUPT;

    Done:
    return dwError;

}
