/**
 @file osi.h

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
// Operating System Interface
//---------------------------------------------------------------------------------

#define OSI_PROT_FULL           0x00                                                // User Protection Level: Full
#define OSI_PROT_REMOVE         0x01                                                // User Protection Level: Remove
#define OSI_PROT_RENAME         0x02                                                // User Protection Level: Rename
#define OSI_PROT_WRITE          0x03                                                // User Protection Level: Write
#define OSI_PROT_UPDATE         0x04                                                // User Protection Level: Update
#define OSI_PROT_READ           0x05                                                // User Protection Level: Read
#define OSI_PROT_EXECUTE        0x06                                                // User Protection Level: Execute
#define OSI_PROT_NOACCESS       0x07                                                // User Protection Level: No Access

enum    OSI_DIR                                                                     // Dir function enumerator
{
    OSI_DIR_FIND_FIRST,                                                             // Find first file in directory
    OSI_DIR_FIND_NEXT                                                               // Find next file in directory
};

struct  OSI_DOS                                                                     // DOS Disk Descriptor
{
    BYTE        nVersion;                                                           // DOS version
    char        szName[8+1];                                                        // Disk name
    char        szDate[8+1];                                                        // Disk date
};

struct OSI_DATE                                                                     // File Date Descriptor
{
    WORD    wYear;                                                                  // Year
    BYTE    nMonth;                                                                 // Month
    BYTE    nDay;                                                                   // Day
};

struct  OSI_FILE                                                                    // File Descriptor
{
    char        szName[8+1];                                                        // Name
    char        szType[3+1];                                                        // Extension
    DWORD       dwSize;                                                             // Size
    OSI_DATE    Date;                                                               // Date
    BYTE        nAccess;                                                            // User Protection Level (0:Full, 7:No Access)
    bool        bSystem;                                                            // System attribute (true:System, false:Normal)
    bool        bInvisible;                                                         // Invisible attribute (true:Invisible, false:Visible)
    bool        bModified;                                                          // Backup Pending attribute (true:Pending, false:Not pending)
};

class   COSI
{
protected:
    CVDI*           m_pVDI;                                                         // Pointer to a init'd Virtual Disk Interface
    DWORD           m_dwFlags;                                                      // User flags (future usage)
    VDI_GEOMETRY    m_DG;                                                           // Disk Geometry
public:
                    COSI();                                                         // Initialize member variables
    virtual         ~COSI();                                                        // Release allocated memory
    virtual DWORD   Load(CVDI* pVDI, DWORD dwFlags)=0;                              // Validate DOS version and define operating parameters
    virtual DWORD   Dir(void** pFile, OSI_DIR nFlag = OSI_DIR_FIND_NEXT)=0;         // Return a pointer to the first/next directory entry
    virtual DWORD   Open(void** pFile, const char cName[11])=0;                     // Return a pointer to the directory entry matching the file name
    virtual DWORD   Create(void** pFile, OSI_FILE& File)=0;                         // Create a new file with the indicated properties
    virtual DWORD   Seek(void* pFile, DWORD dwPos)=0;                               // Move the file pointer
    virtual DWORD   Read(void* pFile, BYTE* pBuffer, DWORD& dwBytes)=0;             // Read data from file
    virtual DWORD   Write(void* pFile, BYTE* pBuffer, DWORD& dwBytes)=0;            // Save data to file
    virtual DWORD   Delete(void* pFile)=0;                                          // Delete the file
    virtual void    GetDOS(OSI_DOS& DOS)=0;                                         // Get DOS information
    virtual DWORD   SetDOS(OSI_DOS& DOS)=0;                                         // Set DOS information
    virtual void    GetFile(void* pFile, OSI_FILE& File)=0;                         // Get the file properties
    virtual DWORD   SetFile(void* pFile, OSI_FILE& File)=0;                         // Set the file properties
};
