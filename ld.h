//---------------------------------------------------------------------------------
// Operating System Interface for LDOS and LSDOS
//---------------------------------------------------------------------------------

class   CLD: public CTD4
{
public:
    DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                        // Validate DOS version and define operating parameters
protected:
    DWORD   ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash = 0);                   // Scan the Hash Index Table
};
