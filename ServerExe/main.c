/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Remote server executable

   Based on code copyright (C) 2004-2005 Martin Wickett

   Copyright (C) Peter Åstrand <astrand@cendio.se> 2005-2006
   Copyright (C) Pierre Ossman <ossman@cendio.se> 2006

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

#define WINVER 0x0501

#include <windows.h>
#include <stdio.h>
#include <wtsapi32.h>
#include <cchannel.h>

#include "vchannel.h"

#include "resource.h"

#define APP_NAME "SeamlessRDP Shell"

/* Global data */
static HINSTANCE g_instance;

static DWORD g_session_id;
static DWORD *g_startup_procs;
static int g_startup_num_procs;

static BOOL g_connected;
static BOOL g_desktop_hidden;

typedef void (*set_hooks_proc_t) ();
typedef void (*remove_hooks_proc_t) ();
typedef int (*get_instance_count_proc_t) ();

typedef void (*move_window_proc_t) (unsigned int serial, HWND hwnd, int x, int y, int width,
				    int height);
typedef void (*zchange_proc_t) (unsigned int serial, HWND hwnd, HWND behind);
typedef void (*focus_proc_t) (unsigned int serial, HWND hwnd);
typedef void (*set_state_proc_t) (unsigned int serial, HWND hwnd, int state);

static move_window_proc_t g_move_window_fn = NULL;
static zchange_proc_t g_zchange_fn = NULL;
static focus_proc_t g_focus_fn = NULL;
static set_state_proc_t g_set_state_fn = NULL;

static void
message(const char *text)
{
	MessageBox(GetDesktopWindow(), text, "SeamlessRDP Shell", MB_OK);
}

static char *
get_token(char **s)
{
	char *comma, *head;
	head = *s;

	if (!head)
		return NULL;

	comma = strchr(head, ',');
	if (comma)
	{
		*comma = '\0';
		*s = comma + 1;
	}
	else
	{
		*s = NULL;
	}

	return head;
}

static BOOL CALLBACK
enum_cb(HWND hwnd, LPARAM lparam)
{
	RECT rect;
	unsigned short title[150];
	LONG styles;
	int state;
	HWND parent;
	DWORD pid;
	int flags;

	styles = GetWindowLong(hwnd, GWL_STYLE);

	if (!(styles & WS_VISIBLE))
		return TRUE;

	/* Since WH_CALLWNDPROC is not effective on cmd.exe, make sure
	   we ignore it during enumeration as well. Make sure to
	   remove this when cmd.exe support has been added, though. */
	char classname[32];
	if (GetClassName(hwnd, classname, sizeof(classname))
	    && !strcmp(classname, "ConsoleWindowClass"))
		return TRUE;

	if (styles & WS_POPUP)
		parent = (HWND) GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
	else
		parent = NULL;

	GetWindowThreadProcessId(hwnd, &pid);

	flags = 0;
	if (styles & DS_MODALFRAME)
		flags |= SEAMLESS_CREATE_MODAL;

	vchannel_write("CREATE", "0x%08lx,0x%08lx,0x%08lx,0x%08x",
		       hwnd_to_long(hwnd), (long) pid, hwnd_to_long(parent), flags);

	if (!GetWindowRect(hwnd, &rect))
	{
		debug("GetWindowRect failed!");
		return TRUE;
	}

	vchannel_write("POSITION", "0x%08lx,%d,%d,%d,%d,0x%08x",
		       hwnd,
		       rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0);

	GetWindowTextW(hwnd, title, sizeof(title) / sizeof(*title));

	vchannel_write("TITLE", "0x%x,%s,0x%x", hwnd, vchannel_strfilter_unicode(title), 0);

	if (styles & WS_MAXIMIZE)
		state = 2;
	else if (styles & WS_MINIMIZE)
		state = 1;
	else
		state = 0;

	vchannel_write("STATE", "0x%08lx,0x%08x,0x%08x", hwnd, state, 0);

	return TRUE;
}

static void
do_sync(void)
{
	vchannel_block();

	vchannel_write("SYNCBEGIN", "0x0");

	EnumWindows(enum_cb, 0);

	vchannel_write("SYNCEND", "0x0");

	vchannel_unblock();
}

static void
do_state(unsigned int serial, HWND hwnd, int state)
{
	g_set_state_fn(serial, hwnd, state);
}

static void
do_position(unsigned int serial, HWND hwnd, int x, int y, int width, int height)
{
	g_move_window_fn(serial, hwnd, x, y, width, height);
}

static void
do_zchange(unsigned int serial, HWND hwnd, HWND behind)
{
	g_zchange_fn(serial, hwnd, behind);
}

static void
do_focus(unsigned int serial, HWND hwnd)
{
	g_focus_fn(serial, hwnd);
}

/* No need for locking, since this is a request rather than a message
   that needs to indicate what has already happened. */
static void
do_destroy(HWND hwnd)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0);
}

static void
process_cmds(void)
{
	char line[VCHANNEL_MAX_LINE];
	int size;

	char *p, *tok1, *tok2, *tok3, *tok4, *tok5, *tok6, *tok7, *tok8;

	while ((size = vchannel_read(line, sizeof(line))) >= 0)
	{
		p = line;

		tok1 = get_token(&p);
		tok2 = get_token(&p);
		tok3 = get_token(&p);
		tok4 = get_token(&p);
		tok5 = get_token(&p);
		tok6 = get_token(&p);
		tok7 = get_token(&p);
		tok8 = get_token(&p);

		if (strcmp(tok1, "SYNC") == 0)
			do_sync();
		else if (strcmp(tok1, "STATE") == 0)
			do_state(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL, 0)),
				 strtol(tok4, NULL, 0));
		else if (strcmp(tok1, "POSITION") == 0)
			do_position(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL, 0)),
				    strtol(tok4, NULL, 0), strtol(tok5, NULL, 0), strtol(tok6, NULL,
											 0),
				    strtol(tok7, NULL, 0));
		else if (strcmp(tok1, "ZCHANGE") == 0)
			do_zchange(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL, 0)),
				   long_to_hwnd(strtoul(tok4, NULL, 0)));
		else if (strcmp(tok1, "FOCUS") == 0)
			do_focus(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL, 0)));
		else if (strcmp(tok1, "DESTROY") == 0)
			do_destroy(long_to_hwnd(strtoul(tok3, NULL, 0)));
	}
}

static BOOL
build_startup_procs(void)
{
	PWTS_PROCESS_INFO pinfo;
	DWORD i, j, count;

	if (!WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pinfo, &count))
		return FALSE;

	g_startup_num_procs = 0;

	for (i = 0; i < count; i++)
	{
		if (pinfo[i].SessionId != g_session_id)
			continue;

		g_startup_num_procs++;
	}

	g_startup_procs = malloc(sizeof(DWORD) * g_startup_num_procs);

	j = 0;
	for (i = 0; i < count; i++)
	{
		if (pinfo[i].SessionId != g_session_id)
			continue;

		g_startup_procs[j] = pinfo[i].ProcessId;
		j++;
	}

	WTSFreeMemory(pinfo);

	return TRUE;
}

static void
free_startup_procs(void)
{
	free(g_startup_procs);

	g_startup_procs = NULL;
	g_startup_num_procs = 0;
}

static BOOL
should_terminate(void)
{
	PWTS_PROCESS_INFO pinfo;
	DWORD i, j, count;

	if (!WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pinfo, &count))
		return TRUE;

	for (i = 0; i < count; i++)
	{
		if (pinfo[i].SessionId != g_session_id)
			continue;

		for (j = 0; j < g_startup_num_procs; j++)
		{
			if (pinfo[i].ProcessId == g_startup_procs[j])
				break;
		}

		if (j == g_startup_num_procs)
		{
			WTSFreeMemory(pinfo);
			return FALSE;
		}
	}

	WTSFreeMemory(pinfo);

	return TRUE;
}

static BOOL
is_connected(void)
{
	BOOL res;
	INT *state;
	DWORD size;

	res = WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
					 WTS_CURRENT_SESSION, WTSConnectState, (LPTSTR *) & state,
					 &size);
	if (!res)
		return TRUE;

	res = *state == WTSActive;

	WTSFreeMemory(state);

	return res;
}

static BOOL
is_desktop_hidden(void)
{
	HDESK desk;

	/* We cannot get current desktop. But we can try to open the current
	   desktop, which will most likely be a secure desktop (if it isn't
	   ours), and will thus fail. */
	desk = OpenInputDesktop(0, FALSE, GENERIC_READ);
	if (desk)
		CloseDesktop(desk);

	return desk == NULL;
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmdline, int cmdshow)
{
	int success = 0;

	HMODULE hookdll;

	set_hooks_proc_t set_hooks_fn;
	remove_hooks_proc_t remove_hooks_fn;
	get_instance_count_proc_t instance_count_fn;

	int check_counter;

	g_instance = instance;

	if (strlen(cmdline) == 0)
	{
		message("No command line specified.");
		return -1;
	}

	if (vchannel_open())
	{
		message("Unable to set up the virtual channel.");
		return -1;
	}

	hookdll = LoadLibrary("seamlessrdp.dll");
	if (!hookdll)
	{
		message("Could not load hook DLL. Unable to continue.");
		goto close_vchannel;
	}

	set_hooks_fn = (set_hooks_proc_t) GetProcAddress(hookdll, "SetHooks");
	remove_hooks_fn = (remove_hooks_proc_t) GetProcAddress(hookdll, "RemoveHooks");
	instance_count_fn = (get_instance_count_proc_t) GetProcAddress(hookdll, "GetInstanceCount");
	g_move_window_fn = (move_window_proc_t) GetProcAddress(hookdll, "SafeMoveWindow");
	g_zchange_fn = (zchange_proc_t) GetProcAddress(hookdll, "SafeZChange");
	g_focus_fn = (focus_proc_t) GetProcAddress(hookdll, "SafeFocus");
	g_set_state_fn = (set_state_proc_t) GetProcAddress(hookdll, "SafeSetState");

	if (!set_hooks_fn || !remove_hooks_fn || !instance_count_fn || !g_move_window_fn
	    || !g_zchange_fn || !g_focus_fn || !g_set_state_fn)
	{
		message("Hook DLL doesn't contain the correct functions. Unable to continue.");
		goto close_hookdll;
	}

	/* Check if the DLL is already loaded */
	if (instance_count_fn() != 1)
	{
		message("Another running instance of Seamless RDP detected.");
		goto close_hookdll;
	}

	ProcessIdToSessionId(GetCurrentProcessId(), &g_session_id);

	build_startup_procs();

	g_connected = is_connected();
	g_desktop_hidden = is_desktop_hidden();

	vchannel_write("HELLO", "0x%08x", g_desktop_hidden ? SEAMLESS_HELLO_HIDDEN : 0);

	set_hooks_fn();

	/* Since we don't see the entire desktop we must resize windows
	   immediatly. */
	SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, TRUE, NULL, 0);

	/* Disable screen saver since we cannot catch its windows. */
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

	/* We don't want windows denying requests to activate windows. */
	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, 0, 0);

	BOOL result;
	PROCESS_INFORMATION proc_info;
	STARTUPINFO startup_info;
	MSG msg;

	memset(&startup_info, 0, sizeof(STARTUPINFO));
	startup_info.cb = sizeof(STARTUPINFO);

	result = CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0,
			       NULL, NULL, &startup_info, &proc_info);
	// Release handles
	CloseHandle(proc_info.hProcess);
	CloseHandle(proc_info.hThread);

	if (!result)
	{
		// CreateProcess failed.
		char msg[256];
		_snprintf(msg, sizeof(msg),
			  "Unable to launch the requested application:\n%s", cmdline);
		message(msg);
		goto unhook;
	}

	check_counter = 5;
	while (check_counter-- || !should_terminate())
	{
		BOOL connected;

		connected = is_connected();
		if (connected && !g_connected)
		{
			int flags;
			/* These get reset on each reconnect */
			SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, TRUE, NULL, 0);
			SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
			SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, 0, 0);

			flags = SEAMLESS_HELLO_RECONNECT;
			if (g_desktop_hidden)
				flags |= SEAMLESS_HELLO_HIDDEN;
			vchannel_write("HELLO", "0x%08x", flags);
		}

		g_connected = connected;

		if (check_counter < 0)
		{
			BOOL hidden;

			hidden = is_desktop_hidden();
			if (hidden && !g_desktop_hidden)
				vchannel_write("HIDE", "0x%08x", 0);
			else if (!hidden && g_desktop_hidden)
				vchannel_write("UNHIDE", "0x%08x", 0);

			g_desktop_hidden = hidden;

			check_counter = 5;
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		process_cmds();
		Sleep(100);
	}

	success = 1;

      unhook:
	remove_hooks_fn();

	free_startup_procs();

      close_hookdll:
	FreeLibrary(hookdll);

      close_vchannel:
	vchannel_close();

	if (success)
		return 1;
	else
		return -1;
}
