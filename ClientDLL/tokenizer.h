/////////////////////////////////////////////////////////////////////////////
// Tokenizer.h
//
// Date:        Monday, October 22, 2001
// Autor:       Eduardo Velasquez
// Description: Tokenizer class for CStrings. Works like strtok.
///////////////


#include "StdString.h"

#if !defined(__TOKENIZER_H__)
#define __TOKENIZER_H__

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#if !defined(_BITSET_)
# include <bitset>
#endif // !defined(_BITSET_)

class CTokenizer
{
    public:
        CTokenizer( const CStdString & cs, const CStdString & csDelim );
        void SetDelimiters( const CStdString & csDelim );
        
        bool Next( CStdString & cs );
        CStdString Tail() const;
        
    private:
        CStdString m_cs;
        std::bitset < 256 > m_delim;
        int m_nCurPos;
};

#endif // !defined(__TOKENIZER_H__)
