/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Remote server executable

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

#include <windows.h>
#include <stdio.h>

#include "vchannel.h"

#include "resource.h"

#define APP_NAME "SeamlessRDP Shell"

/* Global data */
static HINSTANCE g_instance;

typedef void (*set_hooks_proc_t) ();
typedef void (*remove_hooks_proc_t) ();
typedef int (*get_instance_count_proc_t) ();

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
	char title[150];
	LONG styles;
	int state;
	HWND parent;

	styles = GetWindowLong(hwnd, GWL_STYLE);

	if (!(styles & WS_VISIBLE))
		return TRUE;

	if (styles & WS_POPUP)
		parent = (HWND) GetWindowLong(hwnd, GWL_HWNDPARENT);
	else
		parent = NULL;

	vchannel_write("CREATE,0x%p,0x%p,0x%x", hwnd, parent, 0);

	if (!GetWindowRect(hwnd, &rect))
	{
		debug("GetWindowRect failed!");
		return TRUE;
	}

	vchannel_write("POSITION,0x%p,%d,%d,%d,%d,0x%x",
		       hwnd,
		       rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0);

	GetWindowText(hwnd, title, sizeof(title));

	/* FIXME: Strip title of dangerous characters */

	vchannel_write("TITLE,0x%x,%s,0x%x", hwnd, title, 0);

	if (styles & WS_MAXIMIZE)
		state = 2;
	else if (styles & WS_MINIMIZE)
		state = 1;
	else
		state = 0;

	vchannel_write("STATE,0x%p,0x%x,0x%x", hwnd, state, 0);

	return TRUE;
}

static void
do_sync(void)
{
	vchannel_block();

	vchannel_write("SYNCBEGIN,0x0");

	EnumWindows(enum_cb, 0);

	vchannel_write("SYNCEND,0x0");

	vchannel_unblock();
}

static void
do_state(HWND hwnd, int state)
{
	if (state == 0)
		ShowWindow(hwnd, SW_RESTORE);
	else if (state == 1)
		ShowWindow(hwnd, SW_MINIMIZE);
	else if (state == 2)
		ShowWindow(hwnd, SW_MAXIMIZE);
	else
		debug("Invalid state %d sent.", state);
}

static void
process_cmds(void)
{
	char line[VCHANNEL_MAX_LINE];
	int size;

	char *p, *tok1, *tok2, *tok3, *tok4, *tok5, *tok6, *tok7, *tok8;

	while ((size = vchannel_read(line, sizeof(line))) >= 0)
	{
		debug("Got: %s", line);

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
			do_state((HWND) strtol(tok2, NULL, 0), strtol(tok3, NULL, 0));
	}
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmdline, int cmdshow)
{
	HMODULE hookdll;

	set_hooks_proc_t set_hooks_fn;
	remove_hooks_proc_t remove_hooks_fn;
	get_instance_count_proc_t instance_count_fn;

	g_instance = instance;

	hookdll = LoadLibrary("seamlessrdp.dll");
	if (!hookdll)
	{
		message("Could not load hook DLL. Unable to continue.");
		return -1;
	}

	set_hooks_fn = (set_hooks_proc_t) GetProcAddress(hookdll, "SetHooks");
	remove_hooks_fn = (remove_hooks_proc_t) GetProcAddress(hookdll, "RemoveHooks");
	instance_count_fn = (get_instance_count_proc_t) GetProcAddress(hookdll, "GetInstanceCount");

	if (!set_hooks_fn || !remove_hooks_fn || !instance_count_fn)
	{
		FreeLibrary(hookdll);
		message("Hook DLL doesn't contain the correct functions. Unable to continue.");
		return -1;
	}

	/* Check if the DLL is already loaded */
	if (instance_count_fn() != 1)
	{
		FreeLibrary(hookdll);
		message("Another running instance of Seamless RDP detected.");
		return -1;
	}

	vchannel_open();

	set_hooks_fn();

	/* Since we don't see the entire desktop we must resize windows
	   immediatly. */
	SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, TRUE, NULL, 0);

	/* Disable screen saver since we cannot catch its windows. */
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

	if (strlen(cmdline) == 0)
	{
		message("No command line specified.");
		return -1;
	}
	else
	{
		BOOL result;
		DWORD exitcode;
		PROCESS_INFORMATION proc_info;
		STARTUPINFO startup_info;

		memset(&startup_info, 0, sizeof(STARTUPINFO));
		startup_info.cb = sizeof(STARTUPINFO);

		result = CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0,
				       NULL, NULL, &startup_info, &proc_info);

		if (result)
		{
			do
			{
				process_cmds();
				Sleep(100);
				GetExitCodeProcess(proc_info.hProcess, &exitcode);
			}
			while (exitcode == STILL_ACTIVE);

			// Release handles
			CloseHandle(proc_info.hProcess);
			CloseHandle(proc_info.hThread);
		}
		else
		{
			// CreateProcess failed.
			char msg[256];
			_snprintf(msg, sizeof(msg),
				  "Unable to launch the requested application:\n%s", cmdline);
			message(msg);
		}
	}

	remove_hooks_fn();

	FreeLibrary(hookdll);

	vchannel_close();

	return 1;
}
