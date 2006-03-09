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

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmdline, int cmdshow)
{
	HMODULE hookdll;

	set_hooks_proc_t set_hooks_fn;
	remove_hooks_proc_t remove_hooks_fn;
	get_instance_count_proc_t instance_count_fn;

	g_instance = instance;

	hookdll = LoadLibrary("hookdll.dll");
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

	set_hooks_fn();

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
				Sleep(1000);
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

	return 1;
}
