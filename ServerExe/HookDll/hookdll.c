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

#ifdef __GNUC__
#define SHARED __attribute__((section ("SHAREDDATA"), shared))
#else
#define SHARED
#endif

// Shared DATA
#pragma data_seg ( "SHAREDDATA" )

// this is the total number of processes this dll is currently attached to
int g_instance_count SHARED = 0;

// blocks for locally generated events
HWND g_block_move_hwnd SHARED = NULL;
RECT g_block_move SHARED = { 0, 0, 0, 0 };
HWND g_blocked_zchange[2] SHARED = { NULL, NULL };
HWND g_blocked_focus SHARED = NULL;
HWND g_blocked_state_hwnd SHARED = NULL;
int g_blocked_state SHARED = -1;

#pragma data_seg ()

#pragma comment(linker, "/section:SHAREDDATA,rws")

#define FOCUS_MSG_NAME "WM_SEAMLESS_FOCUS"
static UINT g_wm_seamless_focus;

static HHOOK g_cbt_hook = NULL;
static HHOOK g_wndproc_hook = NULL;
static HHOOK g_wndprocret_hook = NULL;

static HINSTANCE g_instance = NULL;

static HANDLE g_mutex = NULL;

static void
update_position(HWND hwnd)
{
	RECT rect, blocked;
	HWND blocked_hwnd;

	if (!GetWindowRect(hwnd, &rect))
	{
		debug("GetWindowRect failed!\n");
		return;
	}

	WaitForSingleObject(g_mutex, INFINITE);
	blocked_hwnd = hwnd;
	memcpy(&blocked, &g_block_move, sizeof(RECT));
	ReleaseMutex(g_mutex);

	if ((hwnd == blocked_hwnd) && (rect.left == blocked.left) && (rect.top == blocked.top)
	    && (rect.right == blocked.right) && (rect.bottom == blocked.bottom))
		return;

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
					unsigned short title[150];
					int state;

					vchannel_write("CREATE,0x%p,0x%p,0x%x", hwnd, parent, 0);

					GetWindowTextW(hwnd, title, sizeof(title) / sizeof(*title));

					vchannel_write("TITLE,0x%x,%s,0x%x", hwnd,
						       vchannel_strfilter_unicode(title), 0);

					if (style & WS_MAXIMIZE)
						state = 2;
					else if (style & WS_MINIMIZE)
						state = 1;
					else
						state = 0;

					update_position(hwnd);

					vchannel_write("STATE,0x%p,0x%x,0x%x", hwnd, state, 0);
				}

				if (wp->flags & SWP_HIDEWINDOW)
					vchannel_write("DESTROY,0x%p,0x%x", hwnd, 0);

				if (!(style & WS_VISIBLE) || (style & WS_MINIMIZE))
					break;

				if (!(wp->flags & SWP_NOMOVE && wp->flags & SWP_NOSIZE))
					update_position(hwnd);

				if (!(wp->flags & SWP_NOZORDER))
				{
					HWND block_hwnd, block_behind;
					WaitForSingleObject(g_mutex, INFINITE);
					block_hwnd = g_blocked_zchange[0];
					block_behind = g_blocked_zchange[1];
					ReleaseMutex(g_mutex);

					if ((hwnd != block_hwnd)
					    || (wp->hwndInsertAfter != block_behind))
					{
						vchannel_write("ZCHANGE,0x%p,0x%p,0x%x",
							       hwnd,
							       wp->flags & SWP_NOACTIVATE ? wp->
							       hwndInsertAfter : 0, 0);
					}
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
wndprocret_hook_proc(int code, WPARAM cur_thread, LPARAM details)
{
	HWND hwnd, parent;
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;

	LONG style;

	if (code < 0)
		goto end;

	hwnd = ((CWPRETSTRUCT *) details)->hwnd;
	msg = ((CWPRETSTRUCT *) details)->message;
	wparam = ((CWPRETSTRUCT *) details)->wParam;
	lparam = ((CWPRETSTRUCT *) details)->lParam;

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
		case WM_SETTEXT:
			{
				unsigned short title[150];
				if (!(style & WS_VISIBLE))
					break;
				/* We cannot use the string in lparam because
				   we need unicode. */
				GetWindowTextW(hwnd, title, sizeof(title) / sizeof(*title));
				vchannel_write("TITLE,0x%p,%s,0x%x", hwnd,
					       vchannel_strfilter_unicode(title), 0);
				break;
			}

		default:
			break;
	}

	if (msg == g_wm_seamless_focus)
	{
		/* FIXME: SetActiveWindow() kills menus. Need to find a clean
		   way to solve this. */
		if ((GetActiveWindow() != hwnd) && !parent)
			SetActiveWindow(hwnd);
	}

      end:
	return CallNextHookEx(g_wndprocret_hook, code, cur_thread, details);
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
				int show, state, blocked;
				HWND blocked_hwnd;

				WaitForSingleObject(g_mutex, INFINITE);
				blocked_hwnd = g_blocked_state_hwnd;
				blocked = g_blocked_state;
				ReleaseMutex(g_mutex);

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

				if ((blocked_hwnd != (HWND) wparam) || (blocked != state))
					vchannel_write("STATE,0x%p,0x%x,0x%x", (HWND) wparam, state,
						       0);

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

	if (!g_wndprocret_hook)
		g_wndprocret_hook =
			SetWindowsHookEx(WH_CALLWNDPROCRET, wndprocret_hook_proc, g_instance, 0);
}

DLL_EXPORT void
RemoveHooks(void)
{
	if (g_cbt_hook)
		UnhookWindowsHookEx(g_cbt_hook);

	if (g_wndproc_hook)
		UnhookWindowsHookEx(g_wndproc_hook);

	if (g_wndprocret_hook)
		UnhookWindowsHookEx(g_wndprocret_hook);
}

DLL_EXPORT void
SafeMoveWindow(HWND hwnd, int x, int y, int width, int height)
{
	WaitForSingleObject(g_mutex, INFINITE);
	g_block_move_hwnd = hwnd;
	g_block_move.left = x;
	g_block_move.top = y;
	g_block_move.right = x + width;
	g_block_move.bottom = y + height;
	ReleaseMutex(g_mutex);

	SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER);

	WaitForSingleObject(g_mutex, INFINITE);
	g_block_move_hwnd = NULL;
	memset(&g_block_move, 0, sizeof(RECT));
	ReleaseMutex(g_mutex);
}

DLL_EXPORT void
SafeZChange(HWND hwnd, HWND behind)
{
	if (behind == NULL)
		behind = HWND_TOP;

	WaitForSingleObject(g_mutex, INFINITE);
	g_blocked_zchange[0] = hwnd;
	g_blocked_zchange[1] = behind;
	ReleaseMutex(g_mutex);

	SetWindowPos(hwnd, behind, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

	WaitForSingleObject(g_mutex, INFINITE);
	g_blocked_zchange[0] = NULL;
	g_blocked_zchange[1] = NULL;
	ReleaseMutex(g_mutex);
}

DLL_EXPORT void
SafeFocus(HWND hwnd)
{
	WaitForSingleObject(g_mutex, INFINITE);
	g_blocked_focus = hwnd;
	ReleaseMutex(g_mutex);

	SendMessage(hwnd, g_wm_seamless_focus, 0, 0);

	WaitForSingleObject(g_mutex, INFINITE);
	g_blocked_focus = NULL;
	ReleaseMutex(g_mutex);
}

DLL_EXPORT void
SafeSetState(HWND hwnd, int state)
{
	WaitForSingleObject(g_mutex, INFINITE);
	g_blocked_state_hwnd = hwnd;
	g_blocked_state = state;
	ReleaseMutex(g_mutex);

	if (state == 0)
		ShowWindow(hwnd, SW_RESTORE);
	else if (state == 1)
		ShowWindow(hwnd, SW_MINIMIZE);
	else if (state == 2)
		ShowWindow(hwnd, SW_MAXIMIZE);
	else
		debug("Invalid state %d sent.", state);

	WaitForSingleObject(g_mutex, INFINITE);
	g_blocked_state_hwnd = NULL;
	g_blocked_state = -1;
	ReleaseMutex(g_mutex);
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

			g_wm_seamless_focus = RegisterWindowMessage(FOCUS_MSG_NAME);

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
