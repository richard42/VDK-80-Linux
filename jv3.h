/**
 @file jv3.h

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
// Virtual Disk Interface for JV3 images
//---------------------------------------------------------------------------------

#define JV3_FLAG_DENSITY    0b10000000                                              // Sector Density (0:Single, 1:Double)
#define JV3_FLAG_DAM        0b01100000                                              // Data Address Mark
#define JV3_FLAG_SIDE       0b00010000                                              // Sector Side (0:Side 0, 1:Side 1)
#define JV3_FLAG_CRC        0b00001000                                              // CRC Status (0:OK, 1:Error)
#define JV3_FLAG_NONIBM     0b00000100                                              // 0:Normal, 1:Short
#define JV3_FLAG_SIZE       0b00000011                                              // Sector Size (00:256, 11:512, 01:128, 10:1024)

#define JV3_DAM_SD0         0xFB                                                    // (JV3_FLAG_DAM=00) Data Address Mark (SDEN) - Normal
#define JV3_DAM_SD1         0xFA                                                    // (JV3_FLAG_DAM=01) Data Address Mark (SDEN) - User-Defined
#define JV3_DAM_SD2         0xF9                                                    // (JV3_FLAG_DAM=10) Data Address Mark (SDEN) - User-Defined
#define JV3_DAM_SD3         0xF8                                                    // (JV3_FLAG_DAM=11) Data Address Mark (SDEN) - Deleted

#define JV3_DAM_DD0         0xFB                                                    // (JV3_FLAG_DAM=00) Data Address Mark (DDEN) - Normal
#define JV3_DAM_DD1         0xF8                                                    // (JV3_FLAG_DAM=01) Data Address Mark (DDEN) - Deleted
#define JV3_DAM_DD2         0x00                                                    // (JV3_FLAG_DAM=10) Data Address Mark (DDEN) - Invalid
#define JV3_DAM_DD3         0x00                                                    // (JV3_FLAG_DAM=11) Data Address Mark (DDEN) - Invalid

#define JV3_SIZE0           256                                                     // (JV3_FLAG_SIZE=00) Sector Size (in used sectors -- inverted in free ones)
#define JV3_SIZE1           128                                                     // (JV3_FLAG_SIZE=01) Sector Size (in used sectors -- inverted in free ones)
#define JV3_SIZE2           1024                                                    // (JV3_FLAG_SIZE=10) Sector Size (in used sectors -- inverted in free ones)
#define JV3_SIZE3           512                                                     // (JV3_FLAG_SIZE=11) Sector Size (in used sectors -- inverted in free ones)

#define JV3_SECTOR_FREE     0xFF                                                    // Value of Track and Sector fields of free sectors
#define JV3_SECTOR_FREEF    0xFC                                                    // Value of Flags field of free sectors

#define JV3_WP_NO           0x00                                                    // Flag value for disks write-unprotected
#define JV3_WP_YES          0xFF                                                    // Flag value for disks write-protected

struct  JV3_SECTOR                                                                  // JV3 Sector Header
{
    BYTE        nTrack;                                                             // Track number
    BYTE        nSector;                                                            // Sector number
    BYTE        nFlags;                                                             // Sector flags (JV3_FLAG_...)
};

struct  JV3_HEADER                                                                  // JV3 Disk Header
{
    JV3_SECTOR  Sector[2901];                                                       // Header contains up to 2901 sectors
    BYTE        nWriteProtected;                                                    // Write-Protected flag (0x00:No, 0xFF:Yes)
};

class   CJV3: public CVDI
{
protected:
    JV3_HEADER* m_pHeader;                                                          // Pointer to JV3 disk header
    bool        m_bExtended;                                                        // Flag indicating an extended disk (2nd header exists)
    WORD        m_wSectors;                                                         // Total count of disk sectors
public:
                CJV3();                                                                     // Initialize member variables
                ~CJV3();                                                                    // Release allocated memory
    DWORD       Load(HANDLE hFile, DWORD dwFlags);                                          // Validate disk format and detect disk geometry
    DWORD       Read(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize);     // Read one sector from the disk
    DWORD       Write(BYTE nTrack, BYTE nSide, BYTE nSector, BYTE* pBuffer, WORD wSize);    // Write one sector to the disk
protected:
    void        FindGeometry();                                                             // Detect the disk geometry
    DWORD       Seek(BYTE nTrack, BYTE nSide, BYTE nSector);                                // Position the file pointer
    WORD        GetSectorSize(const JV3_SECTOR& Sector);                                    // Return a sector size
    void        GetSectorHeader(JV3_SECTOR& Sector, WORD wSector);                          // Copy sector data from 1st or 2nd header
};
