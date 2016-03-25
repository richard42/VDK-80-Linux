/**
 @file v80.h

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
// TRS-80 Virtual Disk Kit                                  Written by Miguel Dutra
//---------------------------------------------------------------------------------

#define V80_MEM             4096                                                    // Heap memory page

#define V80_FLAG_SYSTEM     0b00000000000000000000000000000001                      // 1: Include System files
#define V80_FLAG_INVISIBLE  0b00000000000000000000000000000010                      // 1: Include Invisible files
#define V80_FLAG_INFO       0b00000000000000000000000000000100                      // 1: Show extra information
#define V80_FLAG_CHKDIR     0b00000000000000000000000000001000                      // 1: Skip the directory structure check
#define V80_FLAG_CHKDSK     0b00000000000000000000000000010000                      // 1: Skip the disk parameters check
#define V80_FLAG_READBAD    0b00000000000000000000000000100000                      // 1: Read as much as possible from bad files
#define V80_FLAG_GATFIX     0b00000000000000000000000001000000                      // 1: Skip GAT auto-fix in TRSDOS Model III system disks
#define V80_FLAG_SS         0b00000000000000000000000010000000                      // 1: Force disk geometry to single-sided
#define V80_FLAG_DS         0b00000000000000000000000100000000                      // 1: Force disk geometry to double-sided
