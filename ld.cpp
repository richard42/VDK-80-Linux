//---------------------------------------------------------------------------------
// Operating System Interface for LDOS and LSDOS
//---------------------------------------------------------------------------------

#include "windows.h"
#include "v80.h"
#include "vdi.h"
#include "osi.h"
#include "td4.h"
#include "ld.h"

//---------------------------------------------------------------------------------
// Validate DOS version and define operating parameters
//---------------------------------------------------------------------------------

DWORD CLD::Load(CVDI* pVDI, DWORD dwFlags)
{

    BYTE    nMaxSectors;
    BYTE    nSides;
    DWORD   dwBytes;
    DWORD   dwError = NO_ERROR;

    // Copy VDI pointer and user flags to member variables
    m_pVDI = pVDI;
    m_dwFlags = dwFlags;

    // Get disk geometry
    m_pVDI->GetDG(m_DG);

    // Calculate the number of disk sides
    if (m_dwFlags & V80_FLAG_SS)
        m_nSides = 1;
    else if (m_dwFlags & V80_FLAG_DS)
        m_nSides = 2;
    else
        m_nSides = m_DG.LT.nLastSide - m_DG.LT.nFirstSide + 1;

    // Calculate the number of sectors per track
    m_nSectorsPerTrack = m_DG.LT.nLastSector - m_DG.LT.nFirstSector + 1;

    // Check internal buffer size (just in case)
    if (sizeof(m_Buffer) < m_DG.FT.wSectorSize)
    {
        dwError = ERROR_INVALID_USER_BUFFER;
        goto Done;
    }

    // Read boot sector
    if ((dwError = m_pVDI->Read(m_DG.FT.nTrack, m_DG.FT.nFirstSide, m_DG.FT.nFirstSector, m_Buffer, m_DG.FT.wSectorSize)) != NO_ERROR)
        goto Done;

    // Get directory track and reset MSB (set on some DOS to indicate the disk density)
    m_nDirTrack = m_Buffer[2] & 0x7F;

    // Calculate the number of directory sectors
    m_nDirSectors = m_nSectorsPerTrack * m_nSides;

    // Max Dir Sectors = HIT Size / Entries per Sector + 2 sectors (GAT/HIT)
    nMaxSectors = (m_DG.LT.wSectorSize / (BYTE)(m_DG.LT.wSectorSize / sizeof(TD4_FPDE))) + 2;

    // If dir sectors exceeds max sectors, limit to max
    if (m_nDirSectors > nMaxSectors)
        m_nDirSectors = nMaxSectors;

    // If not first Load, release the previously allocated memory
    if (m_pDir != NULL)
        HeapFree(GetProcessHeap(), 0, m_pDir);

    // Calculate needed memory to hold the entire directory
    dwBytes = m_nDirSectors * m_DG.LT.wSectorSize + (V80_MEM - (m_nDirSectors * m_DG.LT.wSectorSize) % V80_MEM);

    // Allocate memory for the entire directory
    if ((m_pDir = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytes)) == NULL)
    {
        dwError = ERROR_OUTOFMEMORY;
        goto Done;
    }

    // Load directory into memory
    if ((dwError = DirRW(TD4_DIR_READ)) != NO_ERROR)
        goto Done;

    // Calculate DOS disk parameters

    nSides = ((((TD4_GAT*)m_pDir)->nDiskConfig & TD4_GAT_SIDES) >> 5) + 1;

    if (!(m_dwFlags & (V80_FLAG_SS+V80_FLAG_DS)) && (nSides < m_nSides))
        m_nSides = nSides;

    m_nGranulesPerTrack = (((TD4_GAT*)m_pDir)->nDiskConfig & TD4_GAT_GRANULES) + 1;

    m_nGranulesPerCylinder = m_nGranulesPerTrack * m_nSides;

    m_nSectorsPerGranule = m_nSectorsPerTrack / m_nGranulesPerTrack;

    // GranulesPerCylinder must fit in a 8-bit GAT byte
    if (m_nGranulesPerCylinder < 2 || m_nGranulesPerCylinder > 8)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // This division must leave no remainder
    if (m_nSectorsPerTrack % m_nGranulesPerTrack != 0)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // SectorsPerGranule must be either 5 or 8 for SD disks
    if (m_DG.LT.nDensity == VDI_DENSITY_SINGLE && m_nSectorsPerGranule != 5 && m_nSectorsPerGranule != 8)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // SectorsPerGranule must be either 6 or 10 for DD disks
    if (m_DG.LT.nDensity == VDI_DENSITY_DOUBLE && m_nSectorsPerGranule != 6 && m_nSectorsPerGranule != 10)
    {
        dwError = ERROR_NOT_DOS_DISK;
        goto Done;
    }

    // Check directory structure
    if (!(m_dwFlags & V80_FLAG_CHKDIR))
        if ((dwError = CheckDir()) != NO_ERROR)
            goto Done;

    Done:
    return dwError;

}

//---------------------------------------------------------------------------------
// Scan the Hash Index Table
//---------------------------------------------------------------------------------

DWORD CLD::ScanHIT(void** pFile, TD4_HIT nMode, BYTE nHash)
{

    static int nLastCol = -1;
    static int nLastRow = 0;

    int nCols, nCol;
    int nRows, nRow;
    int nSlot;

    DWORD dwError = NO_ERROR;

    // If mode is any "Find First", reset static variables
    if (nMode != TD4_HIT_FIND_NEXT_USED)
    {
        nLastCol = -1;
        nLastRow = 0;
    }

    // Retrieve last values from static variables
    nCol = nLastCol;
    nRow = nLastRow;

    // Calculate max HIT columns and rows
    nCols = m_nDirSectors - 2;
    nRows = m_DG.LT.wSectorSize / sizeof(TD4_FPDE);

    while (true)
    {

        // Increment column
        nCol++;

        // If column reaches max, reset it and advance to the next row
        if (nCol >= nCols)
        {
            nCol = 0;
            nRow++;
        }

        // If row reaches max, then we have reached the end of the HIT
        if (nRow >= nRows)
        {
            if (nHash != 0)
                dwError = ERROR_FILE_NOT_FOUND;
            else if (nMode == TD4_HIT_FIND_FIRST_FREE)
                dwError = ERROR_DISK_FULL;
            else
                dwError = ERROR_NO_MORE_FILES;
            break;
        }

        // Get value in HIT[Row * Cols + Col]
        nSlot = m_pDir[m_DG.LT.wSectorSize + (nRow * nCols) + nCol];

        // If this is not what we are looking for, skip to the next
        if ((nMode == TD4_HIT_FIND_FIRST_FREE && nSlot != 0) || (nMode != TD4_HIT_FIND_FIRST_FREE && nSlot == 0))
            continue;

        // If there is a hash to match and it doesn't match, skip to the next
        if (nHash != 0 && nHash != nSlot)
            continue;

        // Return pointer corresponding to the current Directory Entry Code (DEC)
        if ((*pFile = DEC2FDE((nRow << 5) + nCol)) == NULL)
            dwError = ERROR_INVALID_ADDRESS;

        break;

    }

    // Update static variables
    nLastCol = nCol;
    nLastRow = nRow;

    return dwError;

}
