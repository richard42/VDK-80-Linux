//---------------------------------------------------------------------------------
// Operating System Interface for TRSDOS (Model I)
//---------------------------------------------------------------------------------

class   CTD1: public CTD4
{
public:
    DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                        // Validate DOS version and define operating parameters
};

