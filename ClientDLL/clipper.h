//*********************************************************************************
//
//Title: Terminal Services Window Clipper
//
//Author: Martin Wickett
//
//Date: 2004
//
//*********************************************************************************

#define VER_FILETYPE                VFT_DLL
#define VER_FILESUBTYPE             VFT2_UNKNOWN
#define VER_FILEDESCRIPTION_STR     "Virtual Channel sample DLL"
#define VER_INTERNALNAME_STR        "sysinf_c.dll"
#define VER_ORIGINALFILENAME_STR    "sysinf_c.dll"

#define STRICT
#define _

#include <windows.h>
#include <ntverp.h>
#include "common.ver"

#include <windows.h>
#include <stdio.h>
#include <wtsapi32.h>
#include <tchar.h>
#include <lmcons.h>

#ifdef TSDLL

#include <pchannel.h>
#include <cchannel.h>

#include <windows.h>
#include <winuser.h>

#endif

#include "hash.h"
#include "tokenizer.h"
#include "WindowData.h"

//
// definitions
//
#define CHANNELNAME "CLIPPER"

//
// GLOBAL variables
//

LPHANDLE gphChannel;
DWORD gdwOpenChannel;
PCHANNEL_ENTRY_POINTS gpEntryPoints;

hash_table m_ht;
HRGN m_regionResult;
HWND m_mainWindowHandle = NULL;
int classAlreadyRegistered = 0;

int const ALWAYS__CLIP = 0;     //set this to 0 to turn off clipping when there are no windows
int const HIDE_TSAC_WINDOW = 1;
int const OUTPUT_DEBUG_INFO = 0;
int const OUTPUT_WINDOW_TABLE_DEBUG_INFO = 0;

//
// declarations
//
void WINAPI VirtualChannelOpenEvent(DWORD openHandle, UINT event,
                                    LPVOID pdata, UINT32 dataLength,
                                    UINT32 totalLength, UINT32 dataFlags);
VOID VCAPITYPE VirtualChannelInitEventProc(LPVOID pInitHandle, UINT event,
                                           LPVOID pData, UINT dataLength);
BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints);

void DoClipping(int forceRedraw);
void CreateRegionFromWindowData(char *, void *);

void CreateAndShowWindow(CWindowData * wd);
void DestroyTaskbarWindow(CWindowData * wd);
