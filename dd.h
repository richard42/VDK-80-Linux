//---------------------------------------------------------------------------------
// Operating System Interface for DBLDOS
//---------------------------------------------------------------------------------

class   CDD: public CND
{
public:
    DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                        // Validate DOS version and define operating parameters
protected:
    DWORD   CheckDir(void);                                                         // Check the directory structure
};
