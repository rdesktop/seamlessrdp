//*********************************************************************************
//
//Title: Terminal Services Window Clipper
//
//Author: Martin Wickett
//
//Date: 2004
//
//*********************************************************************************

#include <windows.h>

#ifndef __CBTDLL_H__
#define __CBTDLL_H__

#define CHANNELNAME "CLIPPER"

#pragma once

//Hooking
typedef void (*SETHOOKS) ();
typedef void (*REMOVEHOOKS) ();
typedef int  (*GETINSTANCECOUNT) ();

//Terminal Server
int OpenVirtualChannel();
int CloseVirtualChannel();
int ChannelIsOpen();
int WriteToChannel(PCHAR buffer);

#endif

