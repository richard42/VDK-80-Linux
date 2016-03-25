//---------------------------------------------------------------------------------
// Operating System Interface for RapiDOS
//---------------------------------------------------------------------------------

class   CRD: public CTD4
{
public:
    DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                        // Validate DOS version and define operating parameters
};
