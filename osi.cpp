//---------------------------------------------------------------------------------
// Operating System Interface
//---------------------------------------------------------------------------------

#include "windows.h"
#include "v80.h"
#include "vdi.h"
#include "osi.h"

COSI::COSI()
: m_pVDI(NULL), m_dwFlags(0), m_DG()
{
}

COSI::~COSI()
{
}
