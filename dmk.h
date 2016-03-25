/**
 @file dmk.h

 @brief based on TRS-80 Virtual Disk Kit v1.7 for Windows by Miguel Dutra
 Linux port VDK-80-Linux done by Mike Gore, 2016

 @par Tools to Read and Write files inside common TRS-80 emulator images

 @par Copyright &copy; 2016 Miguel Dutra, GPL License
 @par You are free to use this code under the terms of GPL
   please retain a copy of this notice in any code you use it in.

 This is free software: you can redistribute it and/or modify it under the 
 terms of the GNU General Public License as published by the Free Software 
 Foundation, either version 3 of the License, or (at your option) any later version.

 The software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 @par Original Windows Code
  @see http://www.trs-80.com/wordpress/category/contributors/miguel-dutra/
  @see http://www.trs-80.com/wordpress/dsk-and-dmk-image-utilities/
  @see Miguel Dutra www.mdutra.com
*/
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
