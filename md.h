/**
 @file md.h

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

enum MD_FLAVOR
{
    MD_MICRODOS,
    MD_OS80
};

class   CMD: public COSI
{
protected:
    MD_FLAVOR       m_Flavor;                                                       // MicroDOS flavor
    OSI_FILE        m_Dir[2];                                                       // MicroDOS directory-like structure
    BYTE            m_nSides;                                                       // Number of disk sides
    BYTE            m_nSectorsPerTrack;                                             // Sectors per track
    BYTE            m_nFile;                                                        // Currently selected file
    WORD            m_wSectors;                                                     // Total number of disk sectors
    DWORD           m_dwFilePos;                                                    // Current file position - Seek()
    WORD            m_wSector;                                                      // Current relative sector - Seek()
    BYTE            m_Buffer[1024];                                                 // Generic buffer for operations on sectors
public:
                    CMD();                                                          // Initialize member variables
    virtual         ~CMD();                                                         // Release allocated memory
    virtual DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                // Validate DOS version and define operating parameters
    virtual DWORD   Dir(void** pFile, OSI_DIR nFlag = OSI_DIR_FIND_NEXT);           // Return a pointer to the first/next directory entry
    virtual DWORD   Open(void** pFile, const char cName[11]);                       // Return a pointer to the directory entry matching the file name
    virtual DWORD   Create(void** pFile, OSI_FILE& File);                           // Create a new file with the indicated properties
    virtual DWORD   Seek(void* pFile, DWORD dwPos);                                 // Move the file pointer
    virtual DWORD   Read(void* pFile, BYTE* pBuffer, DWORD& dwBytes);               // Read data from file
    virtual DWORD   Write(void* pFile, BYTE* pBuffer, DWORD& dwBytes);              // Save data to file
    virtual DWORD   Delete(void* pFile);                                            // Delete the file
    virtual void    GetDOS(OSI_DOS& DOS);                                           // Get DOS information
    virtual DWORD   SetDOS(OSI_DOS& DOS);                                           // Set DOS information
    virtual void    GetFile(void* pFile, OSI_FILE& File);                           // Get the file properties
    virtual DWORD   SetFile(void* pFile, OSI_FILE& File);                           // Set the file properties (public)
protected:
    virtual void    CHS(WORD wSector, BYTE& pTrack, BYTE& pSide, BYTE& pSector);    // Return the Cylinder/Head/Sector (CHS) of a given relative sector
};
