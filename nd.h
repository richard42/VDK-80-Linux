/**
 @file nd.h

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
// Operating System Interface for NewDOS/80
//---------------------------------------------------------------------------------

#define ND_TI_A                 0b0000000000000001                                  // Standard disk interface
#define ND_TI_B                 0b0000000000000010                                  // Omnikron mapper type interface
#define ND_TI_C                 0b0000000000000100                                  // Percom doubler type interface
#define ND_TI_D                 0b0000000000001000                                  // Apparat disk controller type interface
#define ND_TI_E                 0b0000000000010000                                  // LNW type interface
#define ND_TI_I                 0b0000000100100000                                  // Lowest numbered sector on each track is sector 1
#define ND_TI_H                 0b0000000010000000                                  // Head settle delay required
#define ND_TI_J                 0b0000001000000000                                  // Track numbers start from 1
#define ND_TI_K                 0b0000010000000000                                  // Track 0 is in opposite density to that of the other tracks
#define ND_TI_L                 0b0000100000000000                                  // Two step pulses between tracks required
#define ND_TI_M                 0b0001000000000000                                  // Standard NewDOS/80 Model III diskette

#define ND_TD_A                 0                                                   // Single Density, Single Sided, 5" Disk
#define ND_TD_B                 1                                                   // Single Density, Single Sided, 8" Disk
#define ND_TD_C                 2                                                   // Single Density, Double Sided, 5" Disk
#define ND_TD_D                 3                                                   // Single Density, Double Sided, 8" Disk
#define ND_TD_E                 4                                                   // Double Density, Single Sided, 5" Disk
#define ND_TD_F                 5                                                   // Double Density, Single Sided, 8" Disk
#define ND_TD_G                 6                                                   // Double Density, Double Sided, 5" Disk
#define ND_TD_H                 7                                                   // Double Density, Double Sided, 8" Disk

#define ND_FLAGS1_TSR           0b00000011                                          // Track Stepping Rate (TSR)
#define ND_FLAGS1_TI            0b00011100                                          // Type of Interface (TI)
#define ND_FLAGS1_M             0b00100000                                          // Standard NewDOS/80 Model III diskette
#define ND_FLAGS1_K             0b01000000                                          // Track 0 is in opposite density to that of the other tracks
#define ND_FLAGS1_8I            0b10000000                                          // 8-inch drives only

#define ND_FLAGS2_H             0b00000001                                          // Head settle delay required
#define ND_FLAGS2_DS            0b00000010                                          // Double sided
#define ND_FLAGS2_I             0b00001000                                          // Lowest numbered sector on each track is sector 1
#define ND_FLAGS2_L             0b00100000                                          // Two step pulses between tracks required
#define ND_FLAGS2_J             0b01000000                                          // Track numbers start from 1
#define ND_FLAGS2_DD            0b10000000                                          // Double density

#define ND_ATTR_EXTENDED        0b0000000010000000                                  // 1:Extended Entry, 0:Primary Entry
#define ND_ATTR_SYSTEM          0b0000000001000000                                  // 1:System File, 0:Normal File
#define ND_ATTR_ACTIVE          0b0000000000010000                                  // 1:Active Entry, 0:Deleted Entry
#define ND_ATTR_INVISIBLE       0b0000000000001000                                  // 1:Invisible File, 0:Visible File
#define ND_ATTR_ACCESS          0b0000000000000111                                  // Access Level Code
#define ND_ATTR_ALLOCATION      0b1000000000000000                                  // 0:File may allocate more space as needed
#define ND_ATTR_DEALLOCATION    0b0100000000000000                                  // 0:DOS may deallocate any excess granules above the EOF
#define ND_ATTR_MODIFIED        0b0010000000000000                                  // 1:At least one sector of the file has been written to

#define ND_GRANULE_INITIAL      0b11100000                                          // Extent's initial granule
#define ND_GRANULE_COUNT        0b00011111                                          // Number of contiguous granules in the Extent

#define ND_DEC_ENTRY            0b11100000                                          // Directory Entry Code (DEC) Entry bits (Row)
#define ND_DEC_SECTOR           0b00011111                                          // Directory Entry Code (DEC) Sector bits (Col)

enum    ND_DIR                                                                      // Directory enumerator
{
    ND_DIR_READ,                                                                    // Read Directory
    ND_DIR_WRITE                                                                    // Write Directory
};

enum    ND_HIT                                                                      // Hash Index Table enumerator
{
    ND_HIT_FIND_FIRST_FREE,                                                         // Find First Free Slot
    ND_HIT_FIND_FIRST_USED,                                                         // Find First Non-Empty Slot
    ND_HIT_FIND_NEXT_USED                                                           // Find Next Non-Empty Slot
};

enum    ND_EXT                                                                      // Extent enumerator
{
    ND_EXTENT_GET,                                                                  // Get Extent
    ND_EXTENT_SET                                                                   // Set Extent
};

struct  __attribute__((packed)) ND_PDRIVE                                           // NewDOS/80 PDRIVE Table
{
    BYTE    nUnknown1;                                                              // DDSL?
    BYTE    nLumps;                                                                 // Lumps count
    BYTE    nFlags1;                                                                // Flags (1st byte)
    BYTE    nTC;                                                                    // Tracks Count (TC)
    BYTE    nSPC;                                                                   // Sectors Per Cylinder (SPC)
    BYTE    nGPL;                                                                   // Granules Per Lump (GPL)
    BYTE    nUnknown2;                                                              // ?
    BYTE    nFlags2;                                                                // Flags (2nd byte)
    BYTE    nDDSL;                                                                  // Disk Directory Starting Lump (DDSL)
    BYTE    nDDGA;                                                                  // Disk Directory Granule Allocation (DDGA)
    BYTE    nSPG;                                                                   // Sectors Per Granule (SPG)
    BYTE    nUnknown3;                                                              // ?
    BYTE    nTSR;                                                                   // Track Stepping Rate (TSR)
    WORD    wTI;                                                                    // Type of Interface (TI)
    BYTE    nTD;                                                                    // Type of Drive (TD)
};

struct  ND_GAT                                                                      // Granule Allocation Table (GAT)
{
    BYTE        nAllocTbl[192];                                                     // Free/Assigned table
    BYTE        nUndefined[14];                                                     // Undefined
    WORD        wPassword;                                                          // Disk encoded password
    char        cDiskName[8];                                                       // Disk name
    char        cDiskDate[8];                                                       // Disk date
    char        cCommand[32];                                                       // AUTO command or 0x0D in the first byte if no command at all
};

struct  ND_EXTENT                                                                   // File Extent Element
{
    BYTE        nLump;                                                              // 0xFF:End of list, 0xFE:Link to FXDE, 0x00-0xFD:Granule's lump
    BYTE        nGranules;                                                          // Bits 7-5:Initial granule, Bits 4-0: # of contiguous granules minus 1
};

struct  ND_FPDE                                                                     // File Primary Directory Entry (FPDE)
{
    WORD        wAttributes;                                                        // File attributes
    BYTE        nUndefined;                                                         // Undefined
    BYTE        nEOF;                                                               // The lower order byte of the file size
    BYTE        nLRL;                                                               // Logical Record Lenght (0 = 256 bytes)
    CHAR        cName[8];                                                           // File name padded on right with blanks
    CHAR        cType[3];                                                           // File extension padded on right with blanks
    WORD        wUpdatePassword;                                                    // Encoded update password
    WORD        wAccessPassword;                                                    // Encoded access password
    WORD        wNext;                                                              // The middle and high order bytes of the file size
    ND_EXTENT   Extent[4];                                                          // Four Extent elements
    ND_EXTENT   Link;                                                               // Link to a File Extended Directory Entry (FXDE)
};

struct  ND_FXDE                                                                     // File Extended Directory Entry (FXDE)
{
    BYTE        nAttributes;                                                        // File attributes (bits 7 and 4 both set indicates active FXDE)
    BYTE        nDEC;                                                               // DEC of the previous Directory Entry pointing to this one
    BYTE        nUnused[3];                                                         // Unused bytes
    CHAR        cName[8];                                                           // File name padded on right with blanks
    CHAR        cType[3];                                                           // File extension padded on right with blanks
    WORD        wUnused[3];                                                         // Encoded words
    ND_EXTENT   Extent[4];                                                          // Four Extent elements
    ND_EXTENT   Link;                                                               // Link to another File Extended Directory Entry (FXDE)
};

class   CND: public COSI
{
protected:
    BYTE*           m_pDir;                                                         // Pointer to buffer containing disk directory
    WORD            m_wDirSector;                                                   // Directory relative sector
    BYTE            m_nDirSectors;                                                  // Number of directory sectors
    BYTE            m_nSides;                                                       // Number of disk sides
    VDI_DENSITY     m_nDensity;                                                     // Disk density
    DWORD           m_dwFilePos;                                                    // Current file position - Seek()
    WORD            m_wSector;                                                      // Current relative sector - Seek()
    BYTE            m_Buffer[1024];                                                 // Generic buffer for operations on sectors
    BYTE            m_nLumps;                                                       // Lumps count
    BYTE            m_nFlags1;                                                      // Flags (1st byte)
    BYTE            m_nFlags2;                                                      // Flags (2nd byte)
    BYTE            m_nTC;                                                          // Tracks Count (TC)
    BYTE            m_nSPC;                                                         // Sectors Per Cylinder (SPC)
    BYTE            m_nGPL;                                                         // Granules Per Lump (GPL)
    BYTE            m_nDDSL;                                                        // Disk Directory Starting Lump (DDSL)
    BYTE            m_nDDGA;                                                        // Disk Directory Granule Allocation (DDGA)
    BYTE            m_nSPG;                                                         // Sectors Per Granule (SPG)
    BYTE            m_nTSR;                                                         // Track Stepping Rate (TSR)
    WORD            m_wTI;                                                          // Type of Interface (TI)
    BYTE            m_nTD;                                                          // Type of Drive (TD)
public:
                    CND();                                                          // Initialize member variables
    virtual         ~CND();                                                         // Release allocated memory
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
    virtual DWORD   DirRW(ND_DIR nMode);                                            // Read or Write the entire directory
    virtual DWORD   CheckDir(void);                                                 // Check the directory structure
    virtual DWORD   ScanHIT(void** pFile, ND_HIT nMode, BYTE nHash = 0);            // Scan the Hash Index Table
    virtual DWORD   CreateExtent(ND_EXTENT& Extent, BYTE nGranules);                // Allocate disk space
    virtual DWORD   DeleteExtent(ND_EXTENT& Extent);                                // Release disk space
    virtual DWORD   CopyExtent(void* pFile, ND_EXT nMode, BYTE nExtent, ND_EXTENT& Extent); // Get or Set extent data
    virtual DWORD   GetFDE(void** pFile);                                           // Return a pointer to an available File Directory Entry
    virtual void*   DEC2FDE(BYTE nDEC);                                             // Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
    virtual BYTE    FDE2DEC(void* pFile);                                           // Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
    virtual BYTE    Hash(const char* pName);                                        // Return the hash code of a given file name
    virtual void    CHS(WORD wSector, BYTE& pTrack, BYTE& pSide, BYTE& pSector);    // Return the Cylinder/Head/Sector (CHS) of a given relative sector
    virtual char*   TI(WORD wTI);                                                   // Convert the TI bitmap in a printable string
};
