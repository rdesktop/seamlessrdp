//
// Copyright (C) 2004-2005 Martin Wickett
//

#include <windows.h>

#ifndef __HOOKDLL_H__
#define __HOOKDLL_H__

#define CHANNELNAME "CLIPPER"

#pragma once

//Hooking
typedef void ( *SETHOOKS ) ();
typedef void ( *REMOVEHOOKS ) ();
typedef int ( *GETINSTANCECOUNT ) ();

//Terminal Server
int OpenVirtualChannel();
int CloseVirtualChannel();
int ChannelIsOpen();
int WriteToChannel( PCHAR buffer );

#endif
