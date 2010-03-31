/* -*- c-basic-offset: 8 -*-

   Copyright 2010 Peter Åstrand <astrand@cendio.se> for Cendio AB

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

#include "resource.h"

/* Global data */
typedef void (*set_hooks_proc_t) ();
typedef int (*get_instance_count_proc_t) ();

static void
message(const char *text)
{
	MessageBox(GetDesktopWindow(), text, "SeamlessRDP hooking", MB_OK);
}


int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmdline, int cmdshow)
{
	HMODULE hookdll = NULL;
	set_hooks_proc_t set_hooks_fn;
	get_instance_count_proc_t instance_count_fn;

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	switch (si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		hookdll = LoadLibrary("seamlessrdp32.dll");
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		hookdll = LoadLibrary("seamlessrdp64.dll");
		break;
	default:
		message("Unsupported processor architecture.");
		break;

	}

	if (!hookdll) {
		message("Could not load hook DLL. Unable to continue.");
		return 1;
	}

	set_hooks_fn = (set_hooks_proc_t) GetProcAddress(hookdll, "SetHooks");
	instance_count_fn =
		(get_instance_count_proc_t) GetProcAddress(hookdll, "GetInstanceCount");

	if (!set_hooks_fn || !instance_count_fn) {
		message
			("Hook DLL doesn't contain the correct functions. Unable to continue.");
		goto close_hookdll;
	}

	set_hooks_fn();


	/* Wait until seamlessrdpshell wants us to terminate. It
	   cannot use PostThreadMessage, because if we are showing a
	   dialog, such messages are lost. Instead, we need to create
	   a "Message-Only Window". */
	CreateWindow("Message", "SeamlessRDPHook", 0, 0, 0, 0, 0,
		HWND_MESSAGE, NULL, instance, NULL);

	MSG msg;
	while (1) {
		BOOL ret;
		ret = GetMessage(&msg, NULL, 0, 0);
		if (ret == -1) {
			message("GetMessage failed");
			break;
		} else if (ret == 0) {
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

  close_hookdll:
	FreeLibrary(hookdll);

	return 0;
}
