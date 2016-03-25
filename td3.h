/**
 @file td3.h

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
// Operating System Interface for TRSDOS (Model III)
//---------------------------------------------------------------------------------

struct  TD3_FPDE                                                                    // File Primary Directory Entry (FPDE)
{
    BYTE        nAttributes;                                                        // File attributes  [PATCH]
    BYTE        nMonth;                                                             // Last Modification Date (Month)   [PATCH]
    BYTE        nYear;                                                              // Last Modification Date (Year)    [PATCH]
    BYTE        nEOF;                                                               // End Of File (EOF)
    BYTE        nLRL;                                                               // Logical Record Length (LRL)
    CHAR        cName[8];                                                           // File name padded on right with blanks
    CHAR        cType[3];                                                           // File extension padded on right with blanks
    WORD        wOwnerHash;                                                         // OWNER password hash
    WORD        wUserHash;                                                          // USER password hash
    WORD        wERN;                                                               // Ending Record Number (ERN)
    TD4_EXTENT  Extent[13];                                                         // Extent elements  [PATCH]
};

struct  TD3_SYS                                                                     // System File Vector in the HIT
{
    BYTE        nGranules:5;                                                        // Initial granule in the referred track
    BYTE        nGranule:3;                                                         // Number of contiguous granules used by the file
    BYTE        nCylinder;                                                          // Track number
};

class   CTD3: public CTD4
{
public:
    DWORD   Load(CVDI* pVDI, DWORD dwFlags);                                        // Validate DOS version and define operating parameters
    DWORD   Create(void** pFile, OSI_FILE& File);                                   // Create a new file with the indicated properties
    DWORD   Seek(void* pFile, DWORD dwPos);                                         // Move the file pointer
    void    GetFile(void* pFile, OSI_FILE& File);                                   // Get the file properties
protected:
    DWORD   SetFile(void* pFile, OSI_FILE& File, bool bCommit);                     // Set the file properties (protected)
    DWORD   GetFileSize(void* pFile);                                               // Get file size
    DWORD   FixGAT();                                                               // Fix the GAT according to HIT System Files
    DWORD   ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash = 0);                   // Scan the Hash Index Table
    DWORD   CreateExtent(TD4_EXTENT& Extent, BYTE nGranules);                       // Allocate disk space
    DWORD   DeleteExtent(TD4_EXTENT& Extent);                                       // Release disk space
    DWORD   CopyExtent(void* pFile, TD4_EXT nMode, BYTE nExtent, TD4_EXTENT& Extent);   // Get or Set extent data
    DWORD   GetFDE(void** pFile);                                                   // Return a pointer to an available File Directory Entry
    void*   DEC2FDE(BYTE nDEC);                                                     // Convert Directory Entry Code (DEC) into a pointer to File Directory Entry (FDE)
    BYTE    FDE2DEC(void* pFile);                                                   // Convert a pointer to a Directory Entry (FDE) into a Directory Entry Code (DEC)
};
