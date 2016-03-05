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
