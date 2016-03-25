/**
 @file md.cpp

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
// Operating System Interface for MicroDOS and OS/80 III
//---------------------------------------------------------------------------------

#include "windows.h"
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "md.h"

//---------------------------------------------------------------------------------
// Initialize member variables
//---------------------------------------------------------------------------------

CMD::CMD()
:   m_Flavor(MD_MICRODOS), m_Dir(), m_nSides(0), m_nSectorsPerTrack(0), m_nFile(0),
    m_wSectors(0), m_dwFilePos(0), m_wSector(0), m_Buffer()
{
}

//---------------------------------------------------------------------------------
// Release allocated memory
//---------------------------------------------------------------------------------

CMD::~CMD()
{
}

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CMD::Load(CVDI* pVDI, DWORD dwFlags)
{

    DWORD dwError = NO_ERROR;

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

    m_Flavor = MD_MICRODOS;

    // Read track 0, sector 0
    if ((dwError = m_pVDI->Read(m_DG.FT.nTrack, m_DG.FT.nFirstSide, m_DG.FT.nFirstSector, m_Buffer, m_DG.FT.wSectorSize)) != NO_ERROR)
        goto Done;

    // Test for MicroDOS
    if (strncmp((const char*)m_Buffer + 241, "MICRODOS", 8) == 0)
        goto FillDir;

    m_Flavor = MD_OS80;

    // Read track 1, sector 0
    if ((dwError = m_pVDI->Read(m_DG.FT.nTrack + 1, m_DG.FT.nFirstSide, m_DG.FT.nFirstSector, m_Buffer, m_DG.FT.wSectorSize)) != NO_ERROR)
        goto Done;

    // Test for OS/80 III
    if (strncmp((const char*)m_Buffer + 20, "OS/80 III", 9) != 0)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    FillDir:

    m_wSectors = (m_DG.LT.nTrack - m_DG.FT.nTrack + 1) * (m_DG.LT.nLastSide - m_DG.LT.nFirstSide + 1) * (m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1);

    strcpy(m_Dir[0].szName, "SYSTEM");
    strcpy(m_Dir[0].szType, "SYS");

    m_Dir[0].dwSize = 20 * m_DG.LT.wSectorSize;

    m_Dir[0].bSystem = true;
    m_Dir[0].bInvisible = true;

    strcpy(m_Dir[1].szName, "DATA");
    strcpy(m_Dir[1].szType, "TXT");

    m_Dir[1].dwSize = (m_wSectors - 20 - (m_Flavor == MD_OS80 ? m_DG.FT.nLastSector - m_DG.FT.nFirstSector + 1 : 0)) * m_DG.LT.wSectorSize;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the first/next directory entry
//---------------------------------------------------------------------------------

DWORD CMD::Dir(void** pFile, OSI_DIR nFlag)
{

    DWORD dwError = NO_ERROR;

    if (nFlag == OSI_DIR_FIND_FIRST)
        m_nFile = 0;

    if (m_nFile > 1)
    {
        dwError = ERROR_NO_MORE_FILES;
        goto Done;
    }

    *pFile = &m_Dir[m_nFile++];

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Return a pointer to the directory entry matching the file name
//---------------------------------------------------------------------------------

DWORD CMD::Open(void** pFile, const char cName[11])
{

    DWORD dwError = NO_ERROR;

    for (int i = 0; i < 2; i++)
    {
        if (memcmp(m_Dir[i].szName, cName, sizeof(cName)) == 0)
        {
            *pFile = &m_Dir[i];
            goto Done;
        }
    }

    dwError = ERROR_FILE_NOT_FOUND;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Create a new file with the indicated name and size
//---------------------------------------------------------------------------------

DWORD CMD::Create(void** pFile, OSI_FILE& File)
{
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Move the file pointer
//---------------------------------------------------------------------------------

DWORD CMD::Seek(void* pFile, DWORD dwPos)
{

    DWORD dwError = NO_ERROR;

    // Check whether dwPos exceeds the file size
    if (dwPos > ((OSI_FILE*)pFile)->dwSize)
    {
        dwError = ERROR_HANDLE_EOF;
        goto Done;
    }

    // Calculate absolute sector corresponding to the relative file position
    m_wSector = dwPos / m_DG.LT.wSectorSize + (pFile == &m_Dir[1] ? 20 : 0) + (m_Flavor == MD_OS80 ? m_DG.FT.nLastSector - m_DG.FT.nFirstSector + 1 : 0);
    m_dwFilePos = dwPos;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Read data from file
//---------------------------------------------------------------------------------

DWORD CMD::Read(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
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
    dwFileSize = ((OSI_FILE*)pFile)->dwSize;

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

        // Copy data from the internal buffer to the caller's buffer
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

DWORD CMD::Write(void* pFile, BYTE* pBuffer, DWORD& dwBytes)
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

    // Get file size
    dwFileSize = ((OSI_FILE*)pFile)->dwSize;

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

        // If needed, read the sector before writing to it
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

DWORD CMD::Delete(void* pFile)
{
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Get DOS information
//---------------------------------------------------------------------------------

void CMD::GetDOS(OSI_DOS& DOS)
{
    // Zero the structure
    memset(&DOS, 0, sizeof(OSI_DOS));
}

//---------------------------------------------------------------------------------
// Set DOS information
//---------------------------------------------------------------------------------

DWORD CMD::SetDOS(OSI_DOS& DOS)
{
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Get file information
//---------------------------------------------------------------------------------

void CMD::GetFile(void* pFile, OSI_FILE& File)
{
    // Just copy the information
    memcpy(&File, pFile, sizeof(File));
}

//---------------------------------------------------------------------------------
// Set file information (public)
//---------------------------------------------------------------------------------

DWORD CMD::SetFile(void* pFile, OSI_FILE& File)
{
    return ERROR_NOT_SUPPORTED;
}

//---------------------------------------------------------------------------------
// Return the Cylinder/Head/Sector (CHS) of a given relative sector
//---------------------------------------------------------------------------------

void CMD::CHS(WORD wSector, BYTE& nTrack, BYTE& nSide, BYTE& nSector)
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
    nTrack += m_DG.FT.nTrack;
    nSide += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSide : m_DG.LT.nFirstSide);
    nSector += (nTrack == m_DG.FT.nTrack ? m_DG.FT.nFirstSector : m_DG.LT.nFirstSector);

}
