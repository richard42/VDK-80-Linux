//---------------------------------------------------------------------------------
// Virtual Disk Interface for DMK images
//---------------------------------------------------------------------------------

#define DMK_FLAG_SINGLE_SIDED   0b00010000                                          // 1:Single Sided
#define DMK_FLAG_SINGLE_DENSITY 0b01000000                                          // 1:Single Density, 0:Double Density
#define DMK_FLAG_IGNORE_DENSITY 0b10000000                                          // 1:Ignore Density (disk is DD but should always read 1 byte)

#define DMK_IDAM_DENSITY        0b1000000000000000                                  // 0:Single, 1:Double
#define DMK_IDAM_UNDEFINED      0b0100000000000000                                  // Undefined flag
#define DMK_IDAM_OFFSET         0b0011111111111111                                  // Pointer offset to the 0xFE byte of the IDAM

#define DMK_WP_NO               0x00                                                // Flag value for disks write-unprotected
#define DMK_WP_YES              0xFF                                                // Flag value for disks write-protected

#define DMK_DISK_REAL           0x12345678                                          // Header signature for real disks
#define DMK_DISK_VIRTUAL        0x00000000                                          // Header signature for virtual disks

struct  DMK_HEADER                                                                  // Disk Header
{
    BYTE        nWriteProtected;                                                    // 0x00:No, 0xFF:Yes
    BYTE        nTracks;                                                            // Number of tracks
    WORD        wTrackLength;                                                       // Track length
    BYTE        nFlags;                                                             // DMK flags
    BYTE        nReserved[7];                                                       // Reserved bytes
    DWORD       dwSignature;                                                        // 0x12345678:Real Disk, 0x00000000:Virtual Disk
};

struct  DMK_TRACK                                                                   // Track Header
{
    WORD        wIDAM_PTR[64];                                                      // Each track contains 64 pointers to the sector IDAMs
};

struct  DMK_SID                                                                     // Sector Header
{
    BYTE        nIDAM;                                                              // ID Address Mark (0xFE)
    BYTE        nTrack;                                                             // Track number
    BYTE        nSide;                                                              // Side number
    BYTE        nSector;                                                            // Sector number
    BYTE        nSize;                                                              // 0:128, 1:256, 2:512, 3:1024
    WORD        wCRC;                                                               // CRC
};

struct  DMK_SECTOR                                                                  // Sector Data
{
    DMK_SID     SID;                                                                // Sector ID structure
    WORD        wIDAM;                                                              // Original IDAM pointer/offset (with flags)
    BYTE*       pIDAM;                                                              // Pointer to sector header
    BYTE*       pDAM;                                                               // Pointer to sector data
    WORD        wSize;                                                              // Sector size
    BYTE        nDoubled;                                                           // Doubled bytes flag
};

class   CDMK: public CVDI
{
protected:
    DMK_HEADER  m_Header;                                                           // 16-byte DMK header
    BYTE*       m_pTrack;                                                           // Pointer to in-memory disk track
    BYTE        m_nCacheTrack;                                                      // In-memory track number
    BYTE        m_nCacheSide;                                                       // In-memory track side
    bool        m_bCacheWrite;                                                      // Write-pending flag
    BYTE        m_nSides;                                                           // Internal disk sides
public:
                CDMK();                                                                     // Initialize member variables
                ~CDMK();                                                                    // Flush the cache and release allocated memory
    DWORD       Load(HANDLE hFile, DWORD dwFlags);                                          // Validate disk format and detect disk geometry
    DWORD       Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize);     // Read one sector from the disk
    DWORD       Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize);    // Write one sector to the disk
protected:
    DWORD       FindGeometry();                                                             // Detect the disk geometry
    DWORD       LoadTrack(BYTE nTrack, BYTE nSide);                                         // Read one entire track from the disk
    DWORD       SaveTrack();                                                                // Write one entire track to the disk
    DWORD       GetSectorId(DMK_SECTOR& Sector, BYTE nTrack, BYTE nSide, BYTE nSector);     // Retrieve sector header
    DWORD       GetSectorData(DMK_SECTOR& Sector, BYTE* pBuffer, WORD wSize);               // Retrieve sector date
    DWORD       PutSectorData(DMK_SECTOR& Sector, BYTE* pBuffer, WORD wSize);               // Update sector data
    void        UpdateCRC(DMK_SECTOR& Sector);                                              // Update sector CRC
};
