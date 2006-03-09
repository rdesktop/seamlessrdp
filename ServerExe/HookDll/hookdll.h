//
// Copyright (C) 2004-2005 Martin Wickett
//

#include <windows.h>

#ifndef __HOOKDLL_H__
#define __HOOKDLL_H__

#define CHANNELNAME "seamrdp"

#pragma once

//Terminal Server
int OpenVirtualChannel();
int CloseVirtualChannel();
int ChannelIsOpen();
int WriteToChannel( char *format, ... );

#endif
