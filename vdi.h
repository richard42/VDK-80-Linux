//---------------------------------------------------------------------------------
// Virtual Disk Interface
//---------------------------------------------------------------------------------

enum    VDI_DENSITY
{
    VDI_DENSITY_SINGLE = 0,                                                         // Single density disk
    VDI_DENSITY_DOUBLE = 1,                                                         // Double density disk
    VDI_DENSITY_MIXED  = 2                                                          // First track in opposity density to that of the other tracks
};

struct  VDI_TRACK                                                                   // Track Descriptor
{
    BYTE        nTrack;                                                             // Track number
    BYTE        nFirstSide;                                                         // First side number (0 or 1)
    BYTE        nLastSide;                                                          // Last side number (1 or 2)
    BYTE        nFirstSector;                                                       // First sector number (0 or 1)
    BYTE        nLastSector;                                                        // Last sector number
    WORD        wSectorSize;                                                        // Sector size (128, 256, 512, 1024)
    VDI_DENSITY nDensity;                                                           // Sector density
};

struct  VDI_GEOMETRY                                                                // Disk Descriptor
{
    VDI_TRACK   FT;                                                                 // First track parameters
    VDI_TRACK   LT;                                                                 // Last track (rest of the disk) parameters
};

class   CVDI
{
protected:
    FILE			*m_hFile;                                                        // Handle for operations on the associated disk file
    DWORD           m_dwFlags;                                                      // User flags (future usage)
    VDI_GEOMETRY    m_DG;                                                           // Disk descriptor
public:
                    CVDI();                                                                     // Initialize member variables
    virtual         ~CVDI();                                                                    // Release allocated memory
    virtual DWORD   Load(HANDLE hFile, DWORD dwFlags = 0)=0;                                    // Validate disk format and detect disk geometry
    virtual DWORD   Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)=0;   // Read one sector from the disk
    virtual DWORD   Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize)=0;  // Write one sector to the disk
    virtual void    GetDG(VDI_GEOMETRY& DG);                                                    // Copy the disk geometry to the caller's struct
};
