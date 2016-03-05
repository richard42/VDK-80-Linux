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
