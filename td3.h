//---------------------------------------------------------------------------------
// Operating System Interface for TRSDOS (Model III)
//---------------------------------------------------------------------------------

struct  TD3_FPDE                                                                    // File Primary Directory Entry (FPDE)
{
    BYTE        nAttributes;                                                        // File attributes  [PATCH]
    BYTE        nMonth;                                                             // Last Modification Date (Month)   [PATCH]
    BYTE        nYear;                                                              // Last Modification Date (Year)    [PATCH]
    BYTE        nEOF;                                                               // End Of File (EOF)
    BYTE        nLRL;                                                               // Logical Record Length (LRL)
    CHAR        cName[8];                                                           // File name padded on right with blanks
    CHAR        cType[3];                                                           // File extension padded on right with blanks
    WORD        wOwnerHash;                                                         // OWNER password hash
    WORD        wUserHash;                                                          // USER password hash
    WORD        wERN;                                                               // Ending Record Number (ERN)
    TD4_EXTENT  Extent[13];                                                         // Extent elements  [PATCH]
};

struct  TD3_SYS                                                                     // System File Vector in the HIT
{
    BYTE        nGranules:5;                                                        // Initial granule in the referred track
    BYTE        nGranule:3;                                                         // Number of contiguous granules used by the file
    BYTE        nCylinder;                                                          // Track number
};

class   CTD3: public CTD4
{
public:
    DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                        // Validate DOS version and define operating parameters
    DWORD   Create(void** pFile, OSI_FILE& File);                                   // Create a new file with the indicated properties
    DWORD   Seek(void* pFile, DWORD dwPos);                                         // Move the file pointer
    void    GetFile(void* pFile, OSI_FILE& File);                                   // Get the file properties
protected:
    DWORD   SetFile(void* pFile, OSI_FILE& File, bool bCommit);                     // Set the file properties (protected)
    DWORD   GetFileSize(void* pFile);                                               // Get file size
    DWORD   FixGAT();                                                               // Fix the GAT according to HIT System Files
    DWORD   ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash = 0);                   // Scan the Hash Index Table
    DWORD   CreateExtent(TD4_EXTENT& Extent, BYTE nGranules);                       // Allocate disk space
    DWORD   DeleteExtent(TD4_EXTENT& Extent);                                       // Release disk space
    DWORD   CopyExtent(void* pFile, TD4_EXT nMode, BYTE nExtent, TD4_EXTENT& Extent);   // Get or Set extent data
    DWORD   GetFDE(void** pFile);                                                   // Return a pointer to an available File Directory Entry
    void*   DEC2FDE(BYTE nDEC);                                                     // Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
    BYTE    FDE2DEC(void* pFile);                                                   // Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
};
