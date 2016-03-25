/**
 @file rd.cpp

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
// Operating System Interface for RapiDOS
//---------------------------------------------------------------------------------

#include "windows.h"
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "td4.h"
#include "rd.h"

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CRD::Load(CVDI* pVDI, DWORD dwFlags)
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
    if ((dwError = m_pVDI->Read(m_DG.FT.nTrack, m_DG.FT.nFirstSide, m_DG.FT.nFirstSector + 1, m_Buffer, m_DG.FT.wSectorSize)) != NO_ERROR)  // [PATCH]
        goto Done;

    // Get directory track and reset MSB (set on some DOS to indicate the disk density)
    m_nDirTrack = m_Buffer[2] & 0x7F;

    // Calculate number of directory sectors
    m_nDirSectors = m_nSectorsPerTrack * m_nSides;

    // Max Dir Sectors = HIT Size / Entries per Sector
    m_nMaxDirSectors = (m_DG.LT.wSectorSize / (BYTE)(m_DG.LT.wSectorSize / sizeof(TD4_FPDE)));

    // If dir sectors exceeds max sectors, limit to max
    if (m_nDirSectors > m_nMaxDirSectors)
        m_nDirSectors = m_nMaxDirSectors;

    // If not the first Load, release the previously allocated memory
    if (m_pDir != NULL)
        free(m_pDir);

    // Calculate the needed memory to hold the entire directory
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

    // Check the directory structure
    if (!(m_dwFlags & V80_FLAG_CHKDIR))
        if ((dwError = CheckDir()) != NO_ERROR)
            goto Done;

    Done:
    return dwError;

}
