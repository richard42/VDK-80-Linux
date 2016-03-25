//---------------------------------------------------------------------------------
// Operating System Interface for TRSDOS (Model 4)
//---------------------------------------------------------------------------------

#define TD4_GAT_TYPE            0b10000000                                          // 0:System Disk, 1:Data Disk (affects # of reserved HIT slots)
#define TD4_GAT_DENSITY         0b01000000                                          // 0:Single Density, 1:Double Density
#define TD4_GAT_SIDES           0b00100000                                          // 0:Single Sided, 1:Double Sided
#define TD4_GAT_GRANULES        0b00000111                                          // Number of Granules per Track minus 1

#define TD4_ATTR0_EXTENDED      0b10000000                                          // 0:FPDE, 1:FXDE
#define TD4_ATTR0_SYSTEM        0b01000000                                          // 0:Normal File, 1:System File
#define TD4_ATTR0_PDS           0b00100000                                          // 1:PDS File (Partition Data Set)
#define TD4_ATTR0_ACTIVE        0b00010000                                          // 0:Dir Record Inactive, 1:Dir Record In Use
#define TD4_ATTR0_INVISIBLE     0b00001000                                          // 0:Visible File, 1:Invisible File
#define TD4_ATTR0_ACCESS        0b00000111                                          // User Protection Level (0:Full, 1:Remove, 2:Rename, 3:Write, 4:Update, 5:Read, 6:Execute, 7:No Access)

#define TD4_ATTR1_CREATED       0b10000000                                          // 1:File was CREATEd
#define TD4_ATTR1_MODIFIED      0b01000000                                          // 1:Backup Pending
#define TD4_ATTR1_OPEN          0b00100000                                          // 1:File in Open condition with Update access or greater
#define TD4_ATTR1_BADDATE       0b00010000                                          // 1:The date of the next bits is inaccurate
#define TD4_ATTR1_MONTH         0b00001111                                          // Binary Month of the Last Modification Date

#define TD4_ATTR2_DAY           0b11111000                                          // Binary Day of the Last Modification Date
#define TD4_ATTR2_YEAR          0b00000111                                          // Binary Year minus 80 (1980=0, 1981=1 etc.)

#define TD4_GRANULE_INITIAL     0b11100000                                          // Extent's initial granule
#define TD4_GRANULE_COUNT       0b00011111                                          // Number of contiguous granules in the Extent

#define TD4_DEC_ENTRY           0b11100000                                          // Directory Entry Code (DEC) Entry bits (Row)
#define TD4_DEC_SECTOR          0b00011111                                          // Directory Entry Code (DEC) Sector bits (Col)

enum    TD4_DIR                                                                     // Directory enumerator
{
    TD4_DIR_READ,                                                                   // Read Directory
    TD4_DIR_WRITE                                                                   // Write Directory
};

enum    TD4_HIT                                                                     // Hash Index Table enumerator
{
    TD4_HIT_FIND_FIRST_FREE,                                                        // Find First Free Slot
    TD4_HIT_FIND_FIRST_USED,                                                        // Find First Non-Empty Slot
    TD4_HIT_FIND_NEXT_USED                                                          // Find Next Non-Empty Slot
};

enum    TD4_EXT                                                                     // Extent enumerator
{
    TD4_EXTENT_GET,                                                                 // Get Extent
    TD4_EXTENT_SET                                                                  // Set Extent
};

struct  TD4_GAT                                                                     // Granule Allocation Table (GAT)
{
    BYTE        nAllocTbl[96];                                                      // Free/Assigned table
    BYTE        nLockoutTbl[96];                                                    // Used in mirror-image backups to check target disk capacity
    BYTE        nExtendedTbl[11];                                                   // Used in hard-drive configs to extend table from 0x00 to 0xCA
    BYTE        nDosVersion;                                                        // Operating System version which formatted the disk
    BYTE        nCylinderExcess;                                                    // Number of cylinders in excess of 35
    BYTE        nDiskConfig;                                                        // Data specific to the formatting of the diskette
    WORD        wPasswordHash;                                                      // Master password hash
    char        cDiskName[8];                                                       // Disk name
    char        cDiskDate[8];                                                       // Disk date
    BYTE        nReserved[21];                                                      // Reserved for system use
    BYTE        cMDB[4];                                                            // Media Data Block (0x03, 'LSI')
    BYTE        nDCT[7];                                                            // Last 7 bytes of the DCT when the media was formatted
};

struct  TD4_EXTENT                                                                  // File Extent Element
{
    BYTE        nCylinder;                                                          // 0xFF:End of list, 0xFE:Link to FXDE, 0x00-0xFD:Granule's cylinder
    BYTE        nGranules;                                                          // Bits 7-5:Initial granule, Bits 4-0: # of contiguous granules minus 1
};

struct  TD4_FPDE                                                                    // File Primary Directory Entry (FPDE)
{
    BYTE        nAttributes[3];                                                     // File attributes
    BYTE        nEOF;                                                               // End Of File (EOF)
    BYTE        nLRL;                                                               // Logical Record Length (LRL) (0=256 bytes)
    CHAR        cName[8];                                                           // File name padded on right with blanks
    CHAR        cType[3];                                                           // File extension padded on right with blanks
    WORD        wOwnerHash;                                                         // OWNER password hash
    WORD        wUserHash;                                                          // USER password hash
    WORD        wERN;                                                               // Ending Record Number (ERN)
    TD4_EXTENT  Extent[4];                                                          // Four Extent elements
    TD4_EXTENT  Link;                                                               // Link to a File Extended Directory Entry (FXDE)
};

class   CTD4: public COSI
{
protected:
    BYTE*           m_pDir;                                                         // Pointer to buffer containing disk directory
    BYTE            m_nDirTrack;                                                    // Directory track
    BYTE            m_nDirSectors;                                                  // Number of directory sectors
    BYTE            m_nMaxDirSectors;                                               // Number maximum of directory sectors
    BYTE            m_nSides;                                                       // Number of disk sides
    BYTE            m_nSectorsPerTrack;                                             // Sectors per track
    BYTE            m_nGranulesPerTrack;                                            // Granules per track
    BYTE            m_nGranulesPerCylinder;                                         // Granules per cylinder (two tracks in double sided disks)
    BYTE            m_nSectorsPerGranule;                                           // Sectors per granule
    DWORD           m_dwFilePos;                                                    // Current file position - Seek()
    WORD            m_wSector;                                                      // Current relative sector - Seek()
    BYTE            m_Buffer[1024];                                                 // Generic buffer for operations on sectors
public:
                    CTD4();                                                         // Initialize member variables
    virtual         ~CTD4();                                                        // Release allocated memory
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
    virtual DWORD   DirRW(TD4_DIR nMode);                                           // Read or Write the entire directory
    virtual DWORD   CheckDir(void);                                                 // Check the directory structure
    virtual DWORD   ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash = 0);           // Scan the Hash Index Table
    virtual DWORD   CreateExtent(TD4_EXTENT& Extent, BYTE nGranules);               // Allocate disk space
    virtual DWORD   DeleteExtent(TD4_EXTENT& Extent);                               // Release disk space
    virtual DWORD   CopyExtent(void* pFile, TD4_EXT nMode, BYTE nExtent, TD4_EXTENT& Extent);   // Get or Set extent data
    virtual DWORD   GetFDE(void** pFile);                                           // Return a pointer to an available File Directory Entry
    virtual void*   DEC2FDE(BYTE nDEC);                                             // Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
    virtual BYTE    FDE2DEC(void* pFile);                                           // Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
    virtual BYTE    Hash(const char* pName);                                        // Return the hash code of a given file name
    virtual void    CHS(WORD wSector, BYTE& pTrack, BYTE& pSide, BYTE& pSector);    // Return the Cylinder/Head/Sector (CHS) of a given relative sector
};
