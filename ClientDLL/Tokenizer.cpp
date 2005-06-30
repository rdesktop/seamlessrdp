/////////////////////////////////////////////////////////////////////////////
// Tokenizer.cpp
//
// Date:        Thursday, November 18, 1999
// Autor:       Eduardo Velasquez
// Description: Tokenizer class for CStrings. Works like strtok().
///////////////

//#include "atlstr.h"



#include "Tokenizer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTokenizer::CTokenizer(const CStdString& cs, const CStdString& csDelim):
	m_cs(cs),
	m_nCurPos(0)
{
	SetDelimiters(csDelim);
}

void CTokenizer::SetDelimiters(const CStdString& csDelim)
{
	for(int i = 0; i < csDelim.GetLength(); ++i)
		m_delim.set(static_cast<BYTE>(csDelim[i]));
}

bool CTokenizer::Next(CStdString& cs)
{
	cs.Empty();

	while(m_nCurPos < m_cs.GetLength() && m_delim[static_cast<BYTE>(m_cs[m_nCurPos])])
		++m_nCurPos;

	if(m_nCurPos >= m_cs.GetLength())
		return false;

	int nStartPos = m_nCurPos;
	while(m_nCurPos < m_cs.GetLength() && !m_delim[static_cast<BYTE>(m_cs[m_nCurPos])])
		++m_nCurPos;
	
	cs = m_cs.Mid(nStartPos, m_nCurPos - nStartPos);

	return true;
}

CStdString CTokenizer::Tail() const
{
	int nCurPos = m_nCurPos;

	while(nCurPos < m_cs.GetLength() && m_delim[static_cast<BYTE>(m_cs[nCurPos])])
		++nCurPos;

	CStdString csResult;

	if(nCurPos < m_cs.GetLength())
		csResult = m_cs.Mid(nCurPos);

	return csResult;
}
