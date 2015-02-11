/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Remote server hook DLL

   Based on code copyright (C) 2004-2005 Martin Wickett

   Copyright 2005-2010 Peter Åstrand <astrand@cendio.se> for Cendio AB
   Copyright 2006-2008 Pierre Ossman <ossman@cendio.se> for Cendio AB
   Copyright 2013-2015 Henrik Andersson <hean01@cendio.se> for Cendio AB

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include <windows.h>
#include <winuser.h>

#include "shared.h"
#include "vchannel.h"

#define EXTERN __declspec(dllexport)

#define FOCUS_MSG_NAME "WM_SEAMLESS_FOCUS"
static UINT g_wm_seamless_focus = 0;	// Non-zero if DLL is initialized

static HHOOK g_cbt_hook = NULL;
static HHOOK g_wndproc_hook = NULL;
static HHOOK g_wndprocret_hook = NULL;

static HINSTANCE g_instance = NULL;

static int g_initialized = 0;

static unsigned int g_conn_serial;

/* 
   The data shared between 32 and 64 bit processes contains HWNDs. On
   win64, HWND is 64 bit but only 32 bits are used. Thus, our
   structure only contains 32 bit, using this data type. The structure
   alignment is the same on win32 and win64 (the default being 8 byte
   boundaries). 
 */
typedef ULONG32 HWND32;

typedef struct _shared_variables
{
	int instance_count;

	// blocks for locally generated events
	HWND32 block_move_hwnd;
	unsigned int block_move_serial;
	RECT block_move;

	unsigned int blocked_zchange_serial;
	HWND32 blocked_zchange[2];

	unsigned int blocked_focus_serial;
	HWND32 blocked_focus;

	unsigned int blocked_state_serial;
	HWND32 blocked_state_hwnd;
	int blocked_state;

	unsigned int conn_serial;

} shared_variables;

static HANDLE g_mutex = NULL;
static shared_variables *g_shdata = NULL;

static BOOL
is_toplevel(HWND hwnd)
{
	BOOL toplevel;
	HWND parent;
	parent = GetAncestor(hwnd, GA_PARENT);

	/* According to MS: "A window that has no parent, or whose
	   parent is the desktop window, is called a top-level
	   window." See http://msdn2.microsoft.com/en-us/library/ms632597(VS.85).aspx. */
	toplevel = (!parent || parent == GetDesktopWindow());
	return toplevel;
}

/* checks instance connection serial against shared. */
static void
check_conn_serial()
{
	WaitForSingleObject(g_mutex, INFINITE);
	if (g_conn_serial != g_shdata->conn_serial) {
		vchannel_reopen();
		g_conn_serial = g_shdata->conn_serial;
	}
	ReleaseMutex(g_mutex);
}

/* Returns true if a window is a menu window. */
static BOOL
is_menu(HWND hwnd)
{
	/* Notepad menus have an owner, but Seamonkey menus does not,
	   so we cannot use the owner in our check. This leaves us with 
	   checking WS_EX_TOOLWINDOW and WS_EX_TOPMOST. */

	LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	return (exstyle & (WS_EX_TOOLWINDOW | WS_EX_TOPMOST));
}

/* Determine the "parent" field for the CREATE response. */
static HWND
get_parent(HWND hwnd)
{
	HWND result;
	HWND owner;
	LONG exstyle;
	char name[512];

	/* Use the same logic to determine if the window should be
	   "transient" (ie have no task icon) as MS uses. This is documented at 
	   http://msdn2.microsoft.com/en-us/library/bb776822.aspx */
	owner = GetWindow(hwnd, GW_OWNER);
	exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (!owner && !(exstyle & WS_EX_TOOLWINDOW)) {
		/* display taskbar icon */
		result = NULL;
	} else {
		/* no taskbar icon */
		if (owner) {
			result = owner;

			/* MS Office assigns wrong owner for tooltips which will
			   create problems with focusing of correct window after
			   hide of tooltip. This is a workaround to always ignore
			   use of owner of Office Tooltips. */
			GetClassName(hwnd, name, sizeof(name));
			if (strcmp(name, "OfficeTooltip") == 0)
				result = (HWND) - 1;			
		} else {
			result = (HWND) - 1;
		}
	}

	return result;
}

static void
update_position(HWND hwnd)
{
	RECT rect, blocked;
	HWND blocked_hwnd;
	unsigned int serial;

	WaitForSingleObject(g_mutex, INFINITE);
	blocked_hwnd = long_to_hwnd(g_shdata->block_move_hwnd);
	serial = g_shdata->block_move_serial;
	memcpy(&blocked, &g_shdata->block_move, sizeof(RECT));
	ReleaseMutex(g_mutex);

	check_conn_serial();

	vchannel_block();

	if (!GetWindowRect(hwnd, &rect)) {
		vchannel_debug("GetWindowRect failed!");
		goto end;
	}

	if ((hwnd == blocked_hwnd) && (rect.left == blocked.left)
		&& (rect.top == blocked.top)
		&& (rect.right == blocked.right) && (rect.bottom == blocked.bottom))
		goto end;

	vchannel_write("POSITION", "0x%08lx,%d,%d,%d,%d,0x%08x",
		hwnd,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0);

  end:
	vchannel_unblock();
}

static void
update_zorder(HWND hwnd)
{
	HWND behind;
	HWND block_hwnd, block_behind;
	unsigned int serial;

	WaitForSingleObject(g_mutex, INFINITE);
	serial = g_shdata->blocked_zchange_serial;
	block_hwnd = long_to_hwnd(g_shdata->blocked_zchange[0]);
	block_behind = long_to_hwnd(g_shdata->blocked_zchange[1]);
	ReleaseMutex(g_mutex);

	check_conn_serial();

	vchannel_block();

	behind = GetNextWindow(hwnd, GW_HWNDPREV);
	while (behind) {
		LONG style;

		style = GetWindowLong(behind, GWL_STYLE);

		if ((!(style & WS_CHILD) || (style & WS_POPUP)) && (style & WS_VISIBLE))
			break;

		behind = GetNextWindow(behind, GW_HWNDPREV);
	}

	if ((hwnd == block_hwnd) && (behind == block_behind))
		vchannel_write("ACK", "%u", serial);
	else {
		int flags = 0;
		LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
		// handle always on top
		if (exstyle & WS_EX_TOPMOST)
			flags |= SEAMLESS_CREATE_TOPMOST;
		vchannel_write("ZCHANGE", "0x%08lx,0x%08lx,0x%08x", hwnd, behind,
			flags);
	}

	vchannel_unblock();
}

#define ICON_CHUNK 400

static void
update_icon(HWND hwnd, HICON icon, int large)
{
	int i, j, size, chunks;
	char buf[32 * 32 * 4];
	char asciibuf[ICON_CHUNK * 2 + 1];

	size = extract_icon(icon, buf, sizeof(buf));
	if (size <= 0)
		return;

	if ((!large && size != 16 * 16 * 4) || (large && size != 32 * 32 * 4)) {
		vchannel_debug("Unexpected icon size.");
		return;
	}

	check_conn_serial();

	chunks = (size + ICON_CHUNK - 1) / ICON_CHUNK;
	for (i = 0; i < chunks; i++) {
		for (j = 0; j < ICON_CHUNK; j++) {
			if (i * ICON_CHUNK + j >= size)
				break;
			sprintf(asciibuf + j * 2, "%02x",
				(int) (unsigned char) buf[i * ICON_CHUNK + j]);
		}

		vchannel_write("SETICON", "0x%08lx,%d,RGBA,%d,%d,%s", hwnd, i,
			large ? 32 : 16, large ? 32 : 16, asciibuf);
	}
}

static LRESULT CALLBACK
wndproc_hook_proc(int code, WPARAM cur_thread, LPARAM details)
{
	HWND hwnd;
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;

	LONG style;

	if (!g_initialized)
		goto end;

	if (code < 0)
		goto end;

	check_conn_serial();

	hwnd = ((CWPSTRUCT *) details)->hwnd;
	msg = ((CWPSTRUCT *) details)->message;
	wparam = ((CWPSTRUCT *) details)->wParam;
	lparam = ((CWPSTRUCT *) details)->lParam;

	if (!is_toplevel(hwnd)) {
		goto end;
	}

	style = GetWindowLong(hwnd, GWL_STYLE);

	/* cmd.exe console window belongs to CSRSS.exe process which is not
	   owned by user and therefor hooking into it will fail in several ways,
	   we do ignore them upon vchannel SYNC command when enumerating windows
	   but we should also ignore them here if they exists at this point.
	 */
	char classname[32];
	if (GetClassName(hwnd, classname, sizeof(classname))
		&& !strcmp(classname, "ConsoleWindowClass"))
		goto end;

	switch (msg) {
	case WM_WINDOWPOSCHANGED:
		{
			WINDOWPOS *wp = (WINDOWPOS *) lparam;

			if (wp->flags & SWP_SHOWWINDOW) {
				unsigned short title[150];
				int state;
				DWORD pid;
				int flags;
				HICON icon;
				LONG exstyle;

				exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
				GetWindowThreadProcessId(hwnd, &pid);

				flags = 0;
				if (style & DS_MODALFRAME)
					flags |= SEAMLESS_CREATE_MODAL;
				// handle always on top
				if (exstyle & WS_EX_TOPMOST)
					flags |= SEAMLESS_CREATE_TOPMOST;

				vchannel_write("CREATE", "0x%08lx,0x%08lx,0x%08lx,0x%08x",
					hwnd_to_long(hwnd), (long) pid,
					hwnd_to_long(get_parent(hwnd)), flags);

				GetWindowTextW(hwnd, title, sizeof(title) / sizeof(*title));

				vchannel_write("TITLE", "0x%08lx,%s,0x%08x", hwnd,
					vchannel_strfilter_unicode(title), 0);

				icon = get_icon(hwnd, 1);
				if (icon) {
					update_icon(hwnd, icon, 1);
					DeleteObject(icon);
				}

				icon = get_icon(hwnd, 0);
				if (icon) {
					update_icon(hwnd, icon, 0);
					DeleteObject(icon);
				}

				if (style & WS_MAXIMIZE)
					state = 2;
				else if (style & WS_MINIMIZE)
					state = 1;
				else
					state = 0;

				update_position(hwnd);

				vchannel_write("STATE", "0x%08lx,0x%08x,0x%08x", hwnd,
					state, 0);
			}

			if (wp->flags & SWP_HIDEWINDOW)
				vchannel_write("DESTROY", "0x%08lx,0x%08x", hwnd, 0);

			if (!(style & WS_VISIBLE) || (style & WS_MINIMIZE))
				break;

			if (!(wp->flags & SWP_NOMOVE && wp->flags & SWP_NOSIZE))
				update_position(hwnd);

			break;
		}

	case WM_SETICON:
		if (!(style & WS_VISIBLE))
			break;

		switch (wparam) {
		case ICON_BIG:
			if (lparam)
				update_icon(hwnd, (HICON) lparam, 1);
			else
				vchannel_write("DELICON", "0x%08lx,RGBA,32,32", hwnd);
			break;
		case ICON_SMALL:
		case 2:
			if (lparam)
				update_icon(hwnd, (HICON) lparam, 0);
			else
				vchannel_write("DELICON", "0x%08lx,RGBA,16,16", hwnd);
			break;
		default:
			vchannel_debug("Weird icon size %d", (int) wparam);
		}

		break;

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
		vchannel_write("DESTROY", "0x%08lx,0x%08x", hwnd, 0);
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
	HWND hwnd;
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;

	LONG style;

	if (!g_initialized)
		goto end;

	if (code < 0)
		goto end;

	check_conn_serial();

	hwnd = ((CWPRETSTRUCT *) details)->hwnd;
	msg = ((CWPRETSTRUCT *) details)->message;
	wparam = ((CWPRETSTRUCT *) details)->wParam;
	lparam = ((CWPRETSTRUCT *) details)->lParam;

	if (!is_toplevel(hwnd)) {
		goto end;
	}

	style = GetWindowLong(hwnd, GWL_STYLE);

	switch (msg) {
	case WM_WINDOWPOSCHANGED:
		{
			WINDOWPOS *wp = (WINDOWPOS *) lparam;

			if (!(style & WS_VISIBLE) || (style & WS_MINIMIZE))
				break;

			if (!(wp->flags & SWP_NOZORDER))
				update_zorder(hwnd);

			break;
		}


	case WM_SETTEXT:
		{
			unsigned short title[150];
			if (!(style & WS_VISIBLE))
				break;
			/* We cannot use the string in lparam because
			   we need unicode. */
			GetWindowTextW(hwnd, title, sizeof(title) / sizeof(*title));
			vchannel_write("TITLE", "0x%08lx,%s,0x%08x", hwnd,
				vchannel_strfilter_unicode(title), 0);
			break;
		}

	case WM_SETICON:
		{
			HICON icon;
			if (!(style & WS_VISIBLE))
				break;

			/*
			 * Somehow, we never get WM_SETICON for the small icon.
			 * So trigger a read of it every time the large one is
			 * changed.
			 */
			icon = get_icon(hwnd, 0);
			if (icon) {
				update_icon(hwnd, icon, 0);
				DeleteObject(icon);
			}
		}

	default:
		break;
	}

	if (msg == g_wm_seamless_focus) {
		/* For some reason, SetForegroundWindow() on menus
		   closes them. Ignore focus requests for menu windows. */
		if ((GetForegroundWindow() != hwnd) && !is_menu(hwnd))
			SetForegroundWindow(hwnd);

		vchannel_write("ACK", "%u", g_shdata->blocked_focus_serial);
	}

  end:
	return CallNextHookEx(g_wndprocret_hook, code, cur_thread, details);
}

static LRESULT CALLBACK
cbt_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
	if (!g_initialized)
		goto end;

	if (code < 0)
		goto end;

	check_conn_serial();

	switch (code) {
	case HCBT_MINMAX:
		{
			int show, state, blocked;
			HWND hwnd, blocked_hwnd;
			unsigned int serial;
			LONG style;

			WaitForSingleObject(g_mutex, INFINITE);
			blocked_hwnd = long_to_hwnd(g_shdata->blocked_state_hwnd);
			serial = g_shdata->blocked_state_serial;
			blocked = g_shdata->blocked_state;
			ReleaseMutex(g_mutex);

			hwnd = (HWND) wparam;

			style = GetWindowLong(hwnd, GWL_STYLE);

			if (!(style & WS_VISIBLE))
				break;

			show = LOWORD(lparam);

			if ((show == SW_NORMAL) || (show == SW_SHOWNORMAL)
				|| (show == SW_RESTORE))
				state = 0;
			else if ((show == SW_MINIMIZE) || (show == SW_SHOWMINIMIZED))
				state = 1;
			else if ((show == SW_MAXIMIZE) || (show == SW_SHOWMAXIMIZED))
				state = 2;
			else {
				vchannel_debug("Unexpected show: %d", show);
				break;
			}

			if ((blocked_hwnd == hwnd) && (blocked == state))
				vchannel_write("ACK", "%u", serial);
			else
				vchannel_write("STATE", "0x%08lx,0x%08x,0x%08x",
					hwnd, state, 0);

			break;
		}

	default:
		break;
	}

  end:
	return CallNextHookEx(g_cbt_hook, code, wparam, lparam);
}

EXTERN void
IncConnectionSerial(void)
{
	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->conn_serial++;
	ReleaseMutex(g_mutex);

}

EXTERN void
SetHooks(void)
{
	if (!g_cbt_hook)
		g_cbt_hook = SetWindowsHookEx(WH_CBT, cbt_hook_proc, g_instance, 0);

	if (!g_wndproc_hook)
		g_wndproc_hook =
			SetWindowsHookEx(WH_CALLWNDPROC, wndproc_hook_proc, g_instance, 0);

	if (!g_wndprocret_hook)
		g_wndprocret_hook =
			SetWindowsHookEx(WH_CALLWNDPROCRET, wndprocret_hook_proc,
			g_instance, 0);
}

EXTERN void
RemoveHooks(void)
{
	if (g_cbt_hook)
		UnhookWindowsHookEx(g_cbt_hook);

	if (g_wndproc_hook)
		UnhookWindowsHookEx(g_wndproc_hook);

	if (g_wndprocret_hook)
		UnhookWindowsHookEx(g_wndprocret_hook);
}

EXTERN void
SafeMoveWindow(unsigned int serial, HWND hwnd, int x, int y, int width,
	int height)
{
	RECT rect;

	if (!g_initialized)
		return;

	check_conn_serial();

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->block_move_hwnd = hwnd_to_long(hwnd);
	g_shdata->block_move_serial = serial;
	g_shdata->block_move.left = x;
	g_shdata->block_move.top = y;
	g_shdata->block_move.right = x + width;
	g_shdata->block_move.bottom = y + height;
	ReleaseMutex(g_mutex);

	SetWindowPos(hwnd, NULL, x, y, width, height,
		SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

	vchannel_write("ACK", "%u", serial);

	if (!GetWindowRect(hwnd, &rect))
		vchannel_debug("GetWindowRect failed!");
	else if ((rect.left != x) || (rect.top != y) || (rect.right != x + width)
		|| (rect.bottom != y + height))
		update_position(hwnd);

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->block_move_hwnd = 0;
	memset(&g_shdata->block_move, 0, sizeof(RECT));
	ReleaseMutex(g_mutex);
}

EXTERN void
SafeZChange(unsigned int serial, HWND hwnd, HWND behind)
{
	if (!g_initialized)
		return;

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->blocked_zchange_serial = serial;
	g_shdata->blocked_zchange[0] = hwnd_to_long(hwnd);
	g_shdata->blocked_zchange[1] = hwnd_to_long(behind);
	ReleaseMutex(g_mutex);

	LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (behind) {
		LONG bhstyle = GetWindowLong(behind, GWL_EXSTYLE);
		/* Avoid that topmost windows references non-topmost
		   windows, and vice versa. */
		if (exstyle & WS_EX_TOPMOST) {
			if (!(bhstyle & WS_EX_TOPMOST)) {
				/* Disallow, move to bottom of the topmost
				   stack. */
				/* FIXME: Remains to be implemented */
				behind = HWND_TOP;
			}
		} else {
			if (bhstyle & WS_EX_TOPMOST) {
				/* Move to top of non-topmost
				   stack. */
				behind = HWND_TOP;
			}
		}
	} else {
		behind = HWND_TOP;
	}

	SetWindowPos(hwnd, behind, 0, 0, 0, 0,
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->blocked_zchange[0] = 0;
	g_shdata->blocked_zchange[1] = 0;
	ReleaseMutex(g_mutex);
}

EXTERN void
SafeFocus(unsigned int serial, HWND hwnd)
{
	if (!g_initialized)
		return;

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->blocked_focus_serial = serial;
	g_shdata->blocked_focus = hwnd_to_long(hwnd);
	ReleaseMutex(g_mutex);

	SendMessage(hwnd, g_wm_seamless_focus, 0, 0);

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->blocked_focus = 0;
	ReleaseMutex(g_mutex);
}

EXTERN void
SafeSetState(unsigned int serial, HWND hwnd, int state)
{
	LONG style;
	int curstate;

	if (!g_initialized)
		return;

	check_conn_serial();

	vchannel_block();

	style = GetWindowLong(hwnd, GWL_STYLE);

	if (style & WS_MAXIMIZE)
		curstate = 2;
	else if (style & WS_MINIMIZE)
		curstate = 1;
	else
		curstate = 0;

	if (state == curstate) {
		vchannel_write("ACK", "%u", serial);
		vchannel_unblock();
		return;
	}

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->blocked_state_hwnd = hwnd_to_long(hwnd);
	g_shdata->blocked_state_serial = serial;
	g_shdata->blocked_state = state;
	ReleaseMutex(g_mutex);

	vchannel_unblock();

	if (state == 0)
		ShowWindow(hwnd, SW_RESTORE);
	else if (state == 1)
		ShowWindow(hwnd, SW_MINIMIZE);
	else if (state == 2)
		ShowWindow(hwnd, SW_MAXIMIZE);
	else
		vchannel_debug("Invalid state %d sent.", state);

	WaitForSingleObject(g_mutex, INFINITE);
	g_shdata->blocked_state_hwnd = 0;
	g_shdata->blocked_state = -1;
	ReleaseMutex(g_mutex);
}

EXTERN int
GetInstanceCount()
{
	if (!g_initialized)
		return 0;

	return g_shdata->instance_count;
}

BOOL APIENTRY
DllMain(HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	HANDLE filemapping = NULL;
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		// remember our instance handle
		g_instance = hinstDLL;

		g_mutex = CreateMutex(NULL, FALSE, "Local\\SeamlessDLL");

		filemapping = CreateFileMapping(INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0, sizeof(shared_variables), "Local\\SeamlessRDPData");

		if (filemapping) {
			/* From MSDN: The initial contents of
			   the pages in a file mapping object
			   backed by the paging file are 0
			   (zero)." */
			g_shdata = MapViewOfFile(filemapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		}

		if (g_mutex && filemapping && g_shdata && vchannel_open() == 0) {
			WaitForSingleObject(g_mutex, INFINITE);
			++g_shdata->instance_count;
			ReleaseMutex(g_mutex);
			g_wm_seamless_focus = RegisterWindowMessage(FOCUS_MSG_NAME);
			g_initialized = 1;
		}
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		if (vchannel_is_open()) {
			vchannel_write("DESTROYGRP", "0x%08lx, 0x%08lx",
				GetCurrentProcessId(), 0);
			vchannel_close();
		}

		if (g_mutex) {
			WaitForSingleObject(g_mutex, INFINITE);
			if (g_shdata) {
				--g_shdata->instance_count;
				UnmapViewOfFile(g_shdata);
			}
			ReleaseMutex(g_mutex);
			CloseHandle(g_mutex);
		}

		if (filemapping) {
			CloseHandle(filemapping);
		}
		break;
	}

	return TRUE;
}
