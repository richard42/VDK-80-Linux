//---------------------------------------------------------------------------------
// Virtual Disk Interface
//---------------------------------------------------------------------------------

#include "windows.h"
#include "v80.h"
#include "vdi.h"

CVDI::CVDI()
: m_hFile(NULL), m_dwFlags(0), m_DG()
{
}

CVDI::~CVDI()
{
}

void CVDI::GetDG(VDI_GEOMETRY& DG)
{
    DG = m_DG;
}
