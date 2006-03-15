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

#include "../vchannel.h"

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

static void
update_position(HWND hwnd)
{
	RECT rect;

	if (!GetWindowRect(hwnd, &rect))
	{
		debug("GetWindowRect failed!\n");
		return;
	}

	vchannel_write("POSITION,0x%p,%d,%d,%d,%d,0x%x",
		       hwnd,
		       rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0);
}

static LRESULT CALLBACK
wndproc_hook_proc(int code, WPARAM cur_thread, LPARAM details)
{
	HWND hwnd, parent;
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;

	LONG style;

	if (code < 0)
		goto end;

	hwnd = ((CWPSTRUCT *) details)->hwnd;
	msg = ((CWPSTRUCT *) details)->message;
	wparam = ((CWPSTRUCT *) details)->wParam;
	lparam = ((CWPSTRUCT *) details)->lParam;

	style = GetWindowLong(hwnd, GWL_STYLE);

	/* Docs say that WS_CHILD and WS_POPUP is an illegal combination,
	   but they exist nonetheless. */
	if ((style & WS_CHILD) && !(style & WS_POPUP))
		goto end;

	if (style & WS_POPUP)
	{
		parent = (HWND) GetWindowLong(hwnd, GWL_HWNDPARENT);
		if (!parent)
			parent = (HWND) - 1;
	}
	else
		parent = NULL;

	switch (msg)
	{

		case WM_WINDOWPOSCHANGED:
			{
				WINDOWPOS *wp = (WINDOWPOS *) lparam;

				if (wp->flags & SWP_SHOWWINDOW)
				{
					char title[150];
					int state;

					vchannel_write("CREATE,0x%p,0x%p,0x%x", hwnd, parent, 0);

					GetWindowText(hwnd, title, sizeof(title));

					vchannel_write("TITLE,0x%x,%s,0x%x", hwnd,
						       vchannel_strfilter(title), 0);

					if (style & WS_MAXIMIZE)
						state = 2;
					else if (style & WS_MINIMIZE)
						state = 1;
					else
						state = 0;

					vchannel_write("STATE,0x%p,0x%x,0x%x", hwnd, state, 0);

					update_position(hwnd);

					/* FIXME: Figure out z order */
				}

				if (wp->flags & SWP_HIDEWINDOW)
					vchannel_write("DESTROY,0x%p,0x%x", hwnd, 0);

				if (!(style & WS_VISIBLE) || (style & WS_MINIMIZE))
					break;

				if (!(wp->flags & SWP_NOMOVE && wp->flags & SWP_NOSIZE))
					update_position(hwnd);

				if (!(wp->flags & SWP_NOZORDER))
				{
					vchannel_write("ZCHANGE,0x%p,0x%p,0x%x",
						       hwnd,
						       wp->flags & SWP_NOACTIVATE ? wp->
						       hwndInsertAfter : 0, 0);
				}

				break;
			}

		case WM_SIZE:
			if (!(style & WS_VISIBLE) || (style & WS_MINIMIZE))
				break;
			update_position(hwnd);
			break;

		case WM_MOVE:
			if (!(style & WS_VISIBLE) || (style & WS_MINIMIZE))
				break;
			update_position(hwnd);
			break;

		case WM_SETTEXT:
			{
				char *title;
				if (!(style & WS_VISIBLE))
					break;
				title = _strdup((char *) lparam);
				vchannel_write("TITLE,0x%p,%s,0x%x", hwnd,
					       vchannel_strfilter(title), 0);
				free(title);
				break;
			}

		case WM_DESTROY:
			if (!(style & WS_VISIBLE))
				break;
			vchannel_write("DESTROY,0x%p,0x%x", hwnd, 0);
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
	if (code < 0)
		goto end;

	switch (code)
	{
		case HCBT_MINMAX:
			{
				int show, state;

				show = LOWORD(lparam);

				if ((show == SW_NORMAL) || (show == SW_SHOWNORMAL)
				    || (show == SW_RESTORE))
					state = 0;
				else if ((show == SW_MINIMIZE) || (show == SW_SHOWMINIMIZED))
					state = 1;
				else if ((show == SW_MAXIMIZE) || (show == SW_SHOWMAXIMIZED))
					state = 2;
				else
				{
					debug("Unexpected show: %d", show);
					break;
				}
				vchannel_write("STATE,0x%p,0x%x,0x%x", (HWND) wparam, state, 0);
				break;
			}

		default:
			break;
	}

      end:
	return CallNextHookEx(g_cbt_hook, code, wparam, lparam);
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

			g_mutex = CreateMutex(NULL, FALSE, "Local\\SeamlessDLL");
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
