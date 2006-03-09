/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Remote server hook DLL

   Based on code copyright (C) 2004-2005 Martin Wickett

   Copyright (C) Peter Åstrand <astrand@cendio.se> 2005-2006
   Copyright (C) Pierre Ossman <ossman@cendio.se> 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdarg.h>

#include <windows.h>
#include <winuser.h>
#include <wtsapi32.h>
#include <cchannel.h>

#include "hookdll.h"

#define DLL_EXPORT __declspec(dllexport)

// Shared DATA
#pragma data_seg ( "SHAREDDATA" )

// this is the total number of processes this dll is currently attached to
int g_instance_count = 0;

#pragma data_seg ()

#pragma comment(linker, "/section:SHAREDDATA,rws")

static HHOOK g_cbt_hook = NULL;
static HHOOK g_wndproc_hook = NULL;

static HINSTANCE g_instance = NULL;

static HANDLE g_mutex = NULL;

static HANDLE g_vchannel = NULL;

static void
debug(char *format, ...)
{
	va_list argp;
	char buf[256];

	sprintf(buf, "DEBUG1,");

	va_start(argp, format);
	_vsnprintf(buf + sizeof("DEBUG1,") - 1, sizeof(buf) - sizeof("DEBUG1,") + 1, format, argp);
	va_end(argp);

	vchannel_write(buf);
}

static LRESULT CALLBACK
wndproc_hook_proc(int code, WPARAM cur_thread, LPARAM details)
{
	HWND hwnd = ((CWPSTRUCT *) details)->hwnd;
	UINT msg = ((CWPSTRUCT *) details)->message;
	WPARAM wparam = ((CWPSTRUCT *) details)->wParam;
	LPARAM lparam = ((CWPSTRUCT *) details)->lParam;

	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	WINDOWPOS *wp = (WINDOWPOS *) lparam;
	RECT rect;

	if (code < 0)
		goto end;

	switch (msg)
	{

		case WM_WINDOWPOSCHANGED:
			if (style & WS_CHILD)
				break;


			if (wp->flags & SWP_SHOWWINDOW)
			{
				// FIXME: Now, just like create!
				debug("SWP_SHOWWINDOW for %p!", hwnd);
				vchannel_write("CREATE1,0x%p,0x%x", hwnd, 0);

				// FIXME: SETSTATE

				if (!GetWindowRect(hwnd, &rect))
				{
					debug("GetWindowRect failed!\n");
					break;
				}
				vchannel_write("POSITION1,0x%p,%d,%d,%d,%d,0x%x",
					       hwnd,
					       rect.left, rect.top,
					       rect.right - rect.left, rect.bottom - rect.top, 0);
			}


			if (wp->flags & SWP_HIDEWINDOW)
				vchannel_write("DESTROY1,0x%p,0x%x", hwnd, 0);


			if (!(style & WS_VISIBLE))
				break;

			if (wp->flags & SWP_NOMOVE && wp->flags & SWP_NOSIZE)
				break;

			if (!GetWindowRect(hwnd, &rect))
			{
				debug("GetWindowRect failed!\n");
				break;
			}

			vchannel_write("POSITION1,0x%p,%d,%d,%d,%d,0x%x",
				       hwnd,
				       rect.left, rect.top,
				       rect.right - rect.left, rect.bottom - rect.top, 0);

			break;


			/* Note: WM_WINDOWPOSCHANGING/WM_WINDOWPOSCHANGED are
			   strange. Sometimes, for example when bringing up the
			   Notepad About dialog, only an WM_WINDOWPOSCHANGING is
			   sent. In some other cases, for exmaple when opening
			   Format->Text in Notepad, both events are sent. Also, for
			   some reason, when closing the Notepad About dialog, an
			   WM_WINDOWPOSCHANGING event is sent which looks just like
			   the event that was sent when the About dialog was opened...  */
		case WM_WINDOWPOSCHANGING:
			if (style & WS_CHILD)
				break;

			if (!(style & WS_VISIBLE))
				break;

			if (!(wp->flags & SWP_NOZORDER))
				vchannel_write("ZCHANGE1,0x%p,0x%p,0x%x",
					       hwnd,
					       wp->flags & SWP_NOACTIVATE ? wp->hwndInsertAfter : 0,
					       0);

			break;




		case WM_DESTROY:
			if (style & WS_CHILD)
				break;

			vchannel_write("DESTROY1,0x%p,0x%x", hwnd, 0);

			break;


		default:
			break;
	}

      end:
	return CallNextHookEx(g_wndproc_hook, code, cur_thread, details);
}

static LRESULT CALLBACK
cbt_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
	char title[150];

	if (code < 0)
		goto end;

	switch (code)
	{
		case HCBT_MINMAX:
			{
				int show;

				show = LOWORD(lparam);

				if ((show == SW_SHOWMINIMIZED) || (show == SW_MINIMIZE))
				{
					MessageBox(0,
						   "Minimizing windows is not allowed in this version. Sorry!",
						   "SeamlessRDP", MB_OK);
					return 1;
				}

				GetWindowText((HWND) wparam, title, sizeof(title));

				/* FIXME: Strip title of dangerous characters */

				vchannel_write("SETSTATE1,0x%p,%s,0x%x,0x%x",
					       (HWND) wparam, title, show, 0);
				break;
			}

		default:
			break;
	}

      end:
	return CallNextHookEx(g_cbt_hook, code, wparam, lparam);
}

int
vchannel_open()
{
	g_vchannel = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE,
					   WTS_CURRENT_SESSION, CHANNELNAME);

	if (g_vchannel == NULL)
		return 0;
	else
		return 1;
}

int
vchannel_close()
{
	BOOL result;

	result = WTSVirtualChannelClose(g_vchannel);

	g_vchannel = NULL;

	if (result)
		return 1;
	else
		return 0;
}

int
vchannel_is_open()
{
	if (g_vchannel == NULL)
		return 0;
	else
		return 1;
}

int
vchannel_write(char *format, ...)
{
	BOOL result;
	va_list argp;
	char buf[1024];
	int size;
	ULONG bytes_written;

	if (!vchannel_is_open())
		return 1;

	va_start(argp, format);
	size = _vsnprintf(buf, sizeof(buf), format, argp);
	va_end(argp);

	if (size >= sizeof(buf))
		return 0;

	WaitForSingleObject(g_mutex, INFINITE);
	result = WTSVirtualChannelWrite(g_vchannel, buf, (ULONG) strlen(buf), &bytes_written);
	result = WTSVirtualChannelWrite(g_vchannel, "\n", (ULONG) 1, &bytes_written);
	ReleaseMutex(g_mutex);

	if (result)
		return 1;
	else
		return 0;
}

DLL_EXPORT void
SetHooks(void)
{
	if (!g_cbt_hook)
		g_cbt_hook = SetWindowsHookEx(WH_CBT, cbt_hook_proc, g_instance, 0);

	if (!g_wndproc_hook)
		g_wndproc_hook = SetWindowsHookEx(WH_CALLWNDPROC, wndproc_hook_proc, g_instance, 0);
}

DLL_EXPORT void
RemoveHooks(void)
{
	if (g_cbt_hook)
		UnhookWindowsHookEx(g_cbt_hook);

	if (g_wndproc_hook)
		UnhookWindowsHookEx(g_wndproc_hook);
}

DLL_EXPORT int
GetInstanceCount()
{
	return g_instance_count;
}

BOOL APIENTRY
DllMain(HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			// remember our instance handle
			g_instance = hinstDLL;

			g_mutex = CreateMutex(NULL, FALSE, "Local\\Seamless");
			if (!g_mutex)
				return FALSE;

			WaitForSingleObject(g_mutex, INFINITE);
			++g_instance_count;
			ReleaseMutex(g_mutex);

			vchannel_open();

			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_THREAD_DETACH:
			break;

		case DLL_PROCESS_DETACH:
			WaitForSingleObject(g_mutex, INFINITE);
			--g_instance_count;
			ReleaseMutex(g_mutex);

			vchannel_close();

			CloseHandle(g_mutex);

			break;
	}

	return TRUE;
}
