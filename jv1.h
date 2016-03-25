//---------------------------------------------------------------------------------
// Virtual Disk Interface for JV1 images
//---------------------------------------------------------------------------------

#define JV1_SECTORSIZE  256                                                             // Standard sector size for a JV1 image

class CJV1: public CVDI
{
protected:
    WORD    m_wSectors;                                                                 // Total number of disk sectors in the disk
public:
    DWORD   Load(HANDLE hFile, DWORD dwFlags);                                          // Validate disk format and detect disk geometry
    DWORD   Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize);     // Read one sector from the disk
    DWORD   Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize);    // Write one sector to the disk
protected:
    virtual DWORD   Seek(BYTE nTrack, BYTE nSide, BYTE nSector);                        // Position the file pointer
};
