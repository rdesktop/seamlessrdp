//*********************************************************************************
//
//Title: Terminal Services Window Clipper
//
//Author: Martin Wickett
//
//Date: 2004
//
//*********************************************************************************

#include "WindowData.h"

CWindowData::CWindowData(const CStdString & csId):m_csTitle(""), m_csId(""), m_iX1(0), m_iY1(0), m_iX2(0),
        m_iY2(0)
{}

void CWindowData::SetId(const CStdString & csId)
{
    m_csId = csId;
}

void CWindowData::SetTitle(const CStdString & csTitle)
{
    m_csTitle = csTitle;
}

void CWindowData::SetX1(int iX1)
{
    m_iX1 = iX1;
}

void CWindowData::SetY1(int iY1)
{
    m_iY1 = iY1;
}

void CWindowData::SetX2(int iX2)
{
    m_iX2 = iX2;
}

void CWindowData::SetY2(int iY2)
{
    m_iY2 = iY2;
}

CStdString CWindowData::GetId()
{
    return this->m_csId;
}

CStdString CWindowData::GetTitle()
{
    return this->m_csTitle;
}

int CWindowData::GetX1()
{
    return this->m_iX1;
}

int CWindowData::GetY1()
{
    return this->m_iY1;
}

int CWindowData::GetX2()
{
    return this->m_iX2;
}

int CWindowData::GetY2()
{
    return this->m_iY2;
}
