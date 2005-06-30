//*********************************************************************************
//
//Title: Terminal Services Window Clipper
//
//Author: Martin Wickett
//
//Date: 2004
//
//*********************************************************************************

#include "StdString.h"

#if !defined(__WINDOWDATA_H__)
#define __WINDOWDATA_H__

class CWindowData
{
  public:
    CWindowData(const CStdString & csId);

    void CWindowData::SetId(const CStdString & csId);
    void CWindowData::SetTitle(const CStdString & csTitle);
    void CWindowData::SetX1(int iX1);
    void CWindowData::SetY1(int iY1);
    void CWindowData::SetX2(int iX2);
    void CWindowData::SetY2(int iY2);
    HWND CWindowData::TaskbarWindowHandle;

    CStdString CWindowData::GetId();
    CStdString CWindowData::GetTitle();
    int CWindowData::GetX1();
    int CWindowData::GetY1();
    int CWindowData::GetX2();
    int CWindowData::GetY2();

  private:
      CStdString m_csTitle;
    CStdString m_csId;
    int m_iX1, m_iY1, m_iX2, m_iY2;
};

#endif // !defined(__WINDOWDATA_H__)
