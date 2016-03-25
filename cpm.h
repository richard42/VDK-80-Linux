/**
 @file cpm.h

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
// Operating System Interface for CP/M
//---------------------------------------------------------------------------------

#define CPM_OPT_DD      0b10000000                                                  // Drive density (0:Single, 1:Double)
#define CPM_OPT_DS      0b01000000                                                  // Drive sides (0:Single Sided, 1:Double Sided)
#define CPM_OPT_STEP    0b00100000                                                  // Drive stepping (0:Normal, 1:Double step (on 80 track drive))
#define CPM_OPT_STATUS  0b00010000                                                  // Data status (0:Normal, 1:Inverted (Superbrain))
#define CPM_OPT_SN      0b00001000                                                  // Sector numbering (0:Same number on each side of disk, 1:Side 1 continues where side 0 left off)
#define CPM_OPT_TN      0b00000100                                                  // Track numbering (0:Track numbers same on each side, 1:Even tracks on side 0, odd tracks on side 1)
#define CPM_OPT_SSEL    0b00000010                                                  // Side selection on double-sided drives (0:Tracks map on alternating sides, 1:Tracks map first on side 0, then on side 1)
#define CPM_OPT_T1      0b00000001                                                  // Track usage on side 1 if bit 1 is set to 1 (0:Tracks run from track 0 to innermost track, 1:Tracks run from innermost track back to track 0)

enum    CPM_DIR                                                                     // Directory enumerator
{
    CPM_DIR_READ,                                                                   // Read Directory
    CPM_DIR_WRITE                                                                   // Write Directory
};

struct CPM_DPB                                                                      // Disk Parameter Block
{
    BYTE        nSPT;                                                               // Sectors Per Track
    BYTE        nBSH;                                                               // Block Shift Count
    BYTE        nBLM;                                                               // Block Mask
    BYTE        nEXM;                                                               // Extent Mask
    WORD        wDSM;                                                               // Disk Storage Maximum (max blocks - 1)
    WORD        wDRM;                                                               // Directory Maximum (max entries - 1)
    BYTE        nAL0;                                                               // Directory Allocation Bitmap, first byte
    BYTE        nAL1;                                                               // Directory Allocation Bitmap, second byte
    BYTE        nCKS;                                                               // Directory Check Size ((DRM + 1) / 4)
    BYTE        nOFF;                                                               // Track Offset (# of reserved tracks)
    BYTE        nRPT;                                                               // Records Per Track (# of 128-byte records per track)
    WORD        wBLS;                                                               // Block Size
    BYTE        nSSZ;                                                               // Sector Size (0:128, 1:256, 2:512, 3:1024)
    BYTE        nSKF;                                                               // Skew Factor (sector interleaving)
    BYTE        nOPT;                                                               // Option flags
    BOOL        b8Bit;                                                              // True=8-bit Allocation Bitmap, False=16-bit Allocation BitMap
    BYTE        nXLT[30];                                                           // Sector Interleaving Translation Table
};

struct  CPM_FCB                                                                     // File Control Block
{
    BYTE        nET;                                                                // Entry Type: 0-1F: User number, 20: Disc Label, 21: Time Stamp, E5: File Deleted
    CHAR        cFN[8];                                                             // File Name
    CHAR        cFT[3];                                                             // File Type
    BYTE        nEX;                                                                // Extent counter, low byte (0-31)(0=1)
    BYTE        nS1;                                                                // Last record byte count (0=128)
    BYTE        nS2;                                                                // Extent counter, high byte
    BYTE        nRC;                                                                // Record Counter (0-127)
    union                                                                           // Disk Map
    {
        BYTE        nDM[16];                                                        // 8-bit vector (x16)
        WORD        wDM[8];                                                         // 16-bit vector (x8)
    } DM;
};

class   CCPM: public COSI
{
protected:
    BYTE*           m_pDir;                                                         // Pointer to buffer containing the directory data
    CPM_DPB         m_DPB;                                                          // Disk Parameter Block
    BYTE            m_nSectorsPerBlock;                                             // Number of sectors per block
    BYTE            m_nReservedSectors;                                             // Number of sectors reserved for system usage
    DWORD           m_dwFilePos;                                                    // Current file position - Seek()
    WORD            m_wSector;                                                      // Current relative sector - Seek()
    BYTE            m_Buffer[1024];                                                 // Generic buffer for operations on sectors
public:
                    CCPM();                                                         // Initialize member variables
    virtual         ~CCPM();                                                        // Release allocated memory
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
    virtual DWORD   SetFile(void* pFile, OSI_FILE& File, bool bCommit);             // Set the file properties (protected)
    virtual DWORD   GetFileSize(void* pFile);                                       // Get file size
    virtual DWORD   GetDiskParams();                                                // Discover the disk parameters
    virtual DWORD   DirRW(CPM_DIR nMode);                                           // Read or Write the entire directory
    virtual DWORD   CheckDir(void);                                                 // Check the directory structure
    virtual DWORD   FindExtent(void** pFile, BYTE nEX);                             // Search the directory for a given file extent
//    virtual DWORD   CreateExtent(CPM_EXTENT& Extent, BYTE nGranules);               // Allocate disk space
//    virtual DWORD   DeleteExtent(CPM_EXTENT& Extent);                               // Release disk space
//    virtual DWORD   CopyExtent(void* pFile, CPM_EXT nMode, BYTE nExtent, CPM_EXTENT& Extent);   // Get or Set extent data
//    virtual DWORD   GetFDE(void** pFile);                                           // Return a pointer to an available File Directory Entry
//    virtual void*   DEC2FDE(BYTE nDEC);                                             // Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
//    virtual BYTE    FDE2DEC(void* pFile);                                           // Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
//    virtual BYTE    Hash(const char* pName);                                        // Return the hash code of a given file name
    virtual bool    Is8Bit();
    virtual WORD    GetBLS(bool b8Bit);
    virtual WORD    GetALM(WORD wDRM, WORD wBLS);
    virtual void    CHS(WORD wSector, BYTE& pTrack, BYTE& pSide, BYTE& pSector);    // Return the Cylinder/Head/Sector (CHS) of a given relative sector
    virtual void    InitXLT();                                                      // Initialize the translation table applying the skew factor
    virtual BYTE    XLT(BYTE nSector);                                              // Translate a sector address using the translation table
    virtual BYTE    Log2(WORD wNumber);                                             // Return the logarithm of a number in base 2
};

