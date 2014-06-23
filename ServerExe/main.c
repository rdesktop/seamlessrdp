/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Remote server executable

   Based on code copyright (C) 2004-2005 Martin Wickett

   Copyright 2005-2010 Peter Åstrand <astrand@cendio.se> for Cendio AB
   Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB
   Copyright 2012-2014 Henrik Andersson <hean01@cendio.se> for Cendio AB

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
#include <sys/time.h>
#include <wtsapi32.h>
#include <cchannel.h>

#include "shared.h"
#include "vchannel.h"
#include "resource.h"

#define APP_NAME "SeamlessRDP Shell"
// TODO: the session timeout should match the liftime of the RDP reconnection blob
#define SESSION_TIMEOUT 60
#define HELPER_TIMEOUT 2000

/* Global data */
static DWORD g_session_id;
static DWORD *g_startup_procs;
static int g_startup_num_procs;

static char **g_system_procs;
static DWORD g_system_num_procs;

static BOOL g_connected;
static BOOL g_desktop_hidden;
static time_t g_session_disconnect_ts;

typedef void (*inc_conn_serial_t) ();
typedef void (*set_hooks_proc_t) ();
typedef void (*remove_hooks_proc_t) ();
typedef int (*get_instance_count_proc_t) ();

typedef void (*move_window_proc_t) (unsigned int serial, HWND hwnd, int x,
	int y, int width, int height);
typedef void (*zchange_proc_t) (unsigned int serial, HWND hwnd, HWND behind);
typedef void (*focus_proc_t) (unsigned int serial, HWND hwnd);
typedef void (*set_state_proc_t) (unsigned int serial, HWND hwnd, int state);
typedef int (*vchannel_reopen_t)();
typedef void (*vchannel_block_t)();
typedef void (*vchannel_unblock_t)();
typedef int (*vchannel_write_t)(const char *command, const char *format, ...);
typedef int (*vchannel_read_t)(char *line, size_t length);
typedef const char *(*vchannel_strfilter_unicode_t)(const unsigned short *string);
typedef void (*vchannel_debug_t)(char *format, ...);

static move_window_proc_t g_move_window_fn = NULL;
static zchange_proc_t g_zchange_fn = NULL;
static focus_proc_t g_focus_fn = NULL;
static set_state_proc_t g_set_state_fn = NULL;
static vchannel_reopen_t g_vchannel_reopen_fn = NULL;
static vchannel_block_t g_vchannel_block_fn = NULL;
static vchannel_unblock_t g_vchannel_unblock_fn = NULL;
static vchannel_write_t g_vchannel_write_fn = NULL;
static vchannel_read_t g_vchannel_read_fn = NULL;
static vchannel_strfilter_unicode_t g_vchannel_strfilter_unicode_fn = NULL;
static vchannel_debug_t g_vchannel_debug_fn = NULL;


static void
messageW(const wchar_t * wtext)
{
	MessageBoxW(GetDesktopWindow(), wtext, L"SeamlessRDP Shell", MB_OK);
}

static int
utf8_to_utf16(const char *input, wchar_t ** output)
{
	size_t size;

	*output = NULL;

	/* convert utf-8 to utf-16 */
	size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		input, strlen(input) + 1, NULL, 0) * 2;

	if (size == 0)
		return 1;

	if (size) {
		*output = malloc(size);
		memset(*output, 0, size);
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			input, strlen(input) + 1, *output, size);
	}

	return 0;
}

static char *
unescape(const char *str)
{
	char *ns, *ps, *pd;
	unsigned int c;

	ns = malloc(strlen(str) + 1);
	memcpy(ns, str, strlen(str) + 1);
	ps = pd = ns;

	while (*ps != '\0') {
		/* check if found escaped character */
		if (ps[0] == '%') {
			if (sscanf(ps, "%%%2X", &c) == 1) {
				pd[0] = c;
				ps += 3;
				pd++;
				continue;
			}
		}

		/* just copy over the char */
		*pd = *ps;
		ps++;
		pd++;
	}
	pd[0] = '\0';

	return ns;
}

static char *
get_token(char **s)
{
	char *comma, *head;
	head = *s;

	if (!head)
		return NULL;

	comma = strchr(head, ',');
	if (comma) {
		*comma = '\0';
		*s = comma + 1;
	} else {
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

	g_vchannel_write_fn("CREATE", "0x%08lx,0x%08lx,0x%08lx,0x%08x",
		hwnd_to_long(hwnd), (long) pid, hwnd_to_long(parent), flags);

	if (!GetWindowRect(hwnd, &rect)) {
		g_vchannel_debug_fn("GetWindowRect failed!");
		return TRUE;
	}

	g_vchannel_write_fn("POSITION", "0x%08lx,%d,%d,%d,%d,0x%08x",
		hwnd,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0);

	GetWindowTextW(hwnd, title, sizeof(title) / sizeof(*title));

	g_vchannel_write_fn("TITLE", "0x%x,%s,0x%x", hwnd,
		g_vchannel_strfilter_unicode_fn(title), 0);

	if (styles & WS_MAXIMIZE)
		state = 2;
	else if (styles & WS_MINIMIZE)
		state = 1;
	else
		state = 0;

	g_vchannel_write_fn("STATE", "0x%08lx,0x%08x,0x%08x", hwnd, state, 0);

	return TRUE;
}

// Returns process handle on success, or NULL on failure
static HANDLE
launch_app(LPSTR cmdline)
{
	BOOL result;
	PROCESS_INFORMATION proc_info;
	STARTUPINFO startup_info;

	memset(&startup_info, 0, sizeof(STARTUPINFO));
	startup_info.cb = sizeof(STARTUPINFO);

	result = CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0,
		NULL, NULL, &startup_info, &proc_info);
	// Release handles
	CloseHandle(proc_info.hThread);

	if (result) {
		return proc_info.hProcess;
	} else {
		return NULL;
	}
}

static HANDLE
launch_appW(LPWSTR cmdline)
{
	BOOL result;
	PROCESS_INFORMATION proc_info;
	STARTUPINFOW startup_info;

	memset(&startup_info, 0, sizeof(STARTUPINFO));
	startup_info.cb = sizeof(STARTUPINFO);

	result = CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0,
		NULL, NULL, &startup_info, &proc_info);

	// Release handles
	CloseHandle(proc_info.hThread);

	if (result) {
		return proc_info.hProcess;
	} else {
		return NULL;
	}
}

static void
do_sync(void)
{
	g_vchannel_block_fn();

	g_vchannel_write_fn("SYNCBEGIN", "0x0");

	EnumWindows(enum_cb, 0);

	g_vchannel_write_fn("SYNCEND", "0x0");

	g_vchannel_unblock_fn();
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
do_spawn(unsigned int serial, char *cmd)
{
	HANDLE app = NULL;
	wchar_t *wcmd, *wmsg;

	if (utf8_to_utf16(cmd, &wcmd) != 0) {
		messageW
			(L"Failed to launch application due to invalid unicode in command line.");
		return;
	}

	app = launch_appW(wcmd);
	if (!app) {
		char msg[256];
		_snprintf(msg, sizeof(msg),
			"Unable to launch the requested application:\n%s", cmd);
		utf8_to_utf16(msg, &wmsg);
		messageW(wmsg);
		free(wmsg);
	}

	free(wcmd);
}

static void
process_cmds(void)
{
	char line[VCHANNEL_MAX_LINE];
	int size;

	char *p, *tok1, *tok2, *tok3, *tok4, *tok5, *tok6, *tok7, *tok8;

	while ((size = g_vchannel_read_fn(line, sizeof(line))) >= 0) {

		p = unescape(line);

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
			do_state(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL,
						0)), strtol(tok4, NULL, 0));
		else if (strcmp(tok1, "POSITION") == 0)
			do_position(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL,
						0)), strtol(tok4, NULL, 0), strtol(tok5, NULL, 0),
				strtol(tok6, NULL, 0), strtol(tok7, NULL, 0));
		else if (strcmp(tok1, "ZCHANGE") == 0)
			do_zchange(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL,
						0)), long_to_hwnd(strtoul(tok4, NULL, 0)));
		else if (strcmp(tok1, "FOCUS") == 0)
			do_focus(strtoul(tok2, NULL, 0), long_to_hwnd(strtoul(tok3, NULL,
						0)));
		else if (strcmp(tok1, "DESTROY") == 0)
			do_destroy(long_to_hwnd(strtoul(tok3, NULL, 0)));
		else if (strcmp(tok1, "SPAWN") == 0)
			do_spawn(strtoul(tok2, NULL, 0), tok3);

		free(p);
	}
}

static BOOL
build_system_procs(void)
{
	HKEY hKey;
	DWORD j, res, spsize;

	res = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\SysProcs",
		0, KEY_READ, &hKey);

	if (res == ERROR_SUCCESS)
		RegQueryInfoKey(hKey, NULL, NULL, NULL,
			NULL, NULL, NULL, &g_system_num_procs, &spsize, NULL, NULL, NULL);

	if (!g_system_num_procs) {
		g_system_num_procs = 2;
		g_system_procs = malloc(sizeof(char *) * g_system_num_procs);
		g_system_procs[0] = strdup("ieuser.exe");
		g_system_procs[1] = strdup("ctfmon.exe");
	} else {
		spsize = spsize + 1;
		g_system_procs = malloc(sizeof(char *) * g_system_num_procs);
		for (j = 0; j < g_system_num_procs; j++) {
			DWORD s = spsize;
			g_system_procs[j] = malloc(s);
			memset(g_system_procs[j], 0, s);
			RegEnumValue(hKey, j, g_system_procs[j], &s,
				NULL, NULL, NULL, NULL);
		}
	}

	if (hKey)
		RegCloseKey(hKey);

	return TRUE;
}

static void
free_system_procs(void)
{
	DWORD j;

	for (j = 0; j < g_system_num_procs; j++)
		free(g_system_procs[j]);

	free(g_system_procs);
}

static BOOL
build_startup_procs(void)
{
	PWTS_PROCESS_INFO pinfo;
	DWORD i, j, count;

	if (!WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pinfo, &count))
		return FALSE;

	g_startup_num_procs = 0;

	for (i = 0; i < count; i++) {
		if (pinfo[i].SessionId != g_session_id)
			continue;

		g_startup_num_procs++;
	}

	g_startup_procs = malloc(sizeof(DWORD) * g_startup_num_procs);

	j = 0;
	for (i = 0; i < count; i++) {
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

	if (g_connected || g_session_disconnect_ts == 0)
		return FALSE;

	if ((time(NULL) - g_session_disconnect_ts) < SESSION_TIMEOUT)
		return FALSE;

	if (!WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pinfo, &count))
		return TRUE;

	for (i = 0; i < count; i++) {
		if (pinfo[i].SessionId != g_session_id)
			continue;

		for (j = 0; j < g_system_num_procs; j++) {
			if (0 == _stricmp(pinfo[i].pProcessName, g_system_procs[j]))
				goto skip_to_next_process;
		}

		for (j = 0; j < g_startup_num_procs; j++) {
			if (pinfo[i].ProcessId == g_startup_procs[j])
				break;
		}

		if (j == g_startup_num_procs) {
			WTSFreeMemory(pinfo);
			return FALSE;
		}

	  skip_to_next_process:
		continue;
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
		WTS_CURRENT_SESSION, WTSConnectState, (LPTSTR *) & state, &size);
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

static HANDLE
launch_helper()
{
	wchar_t *wmsg;
	HANDLE app = NULL;
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	/* If we are running on a x64 system, hook 32 bit apps as well
	   by launching a 32 bit helper process. */
	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
		char cmd[] = "seamlessrdphook32.exe";
		app = launch_app(cmd);
		if (!app) {
			char msg[256];
			_snprintf(msg, sizeof(msg),
				"Unable to launch the requested application:\n%s", cmd);
			utf8_to_utf16(msg, &wmsg);
			messageW(wmsg);
			free(wmsg);
		}

		/* Wait until helper is started, so that it gets included in
		   the process enum */
		DWORD ret;
		ret = WaitForInputIdle(app, HELPER_TIMEOUT);
		switch (ret) {
		case 0:
			break;
		case WAIT_TIMEOUT:
		case WAIT_FAILED:
			messageW(L"Hooking helper failed to start within time limit");
			break;
		}
	}
	return app;
}


// Ask process to quit, otherwise kill it
static void
kill_15_9(HANDLE proc, const char *wndname, DWORD timeout)
{
	HWND procwnd;
	DWORD ret;
	procwnd = FindWindowEx(HWND_MESSAGE, NULL, "Message", wndname);
	if (procwnd) {
		PostMessage(procwnd, WM_CLOSE, 0, 0);
	}
	ret = WaitForSingleObject(proc, timeout);
	switch (ret) {
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		break;
	case WAIT_TIMEOUT:
		// Still running, kill hard
		if (!TerminateProcess(proc, 1)) {
			messageW(L"Unable to terminate process");
		}
		break;
	case WAIT_FAILED:
		messageW(L"Unable to wait for process");
		break;
	}
}


int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmdline, int cmdshow)
{
	int success = 0;
	HANDLE helper = NULL;
	HMODULE hookdll = NULL;

	set_hooks_proc_t set_hooks_fn;
	remove_hooks_proc_t remove_hooks_fn;
	get_instance_count_proc_t instance_count_fn;
	inc_conn_serial_t inc_conn_serial_fn;

	int check_counter;

	if (strlen(cmdline) != 0) {
		messageW
			(L"Seamless RDP Shell should be started without any arguments.");
		return -1;
	}

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
		messageW(L"Unsupported processor architecture.");
		break;

	}

	if (!hookdll) {
		messageW(L"Could not load hook DLL. Unable to continue.");
		goto bail_out;
	}

	inc_conn_serial_fn =
		(inc_conn_serial_t) GetProcAddress(hookdll, "IncConnectionSerial");
	set_hooks_fn = (set_hooks_proc_t) GetProcAddress(hookdll, "SetHooks");
	remove_hooks_fn =
		(remove_hooks_proc_t) GetProcAddress(hookdll, "RemoveHooks");
	instance_count_fn =
		(get_instance_count_proc_t) GetProcAddress(hookdll, "GetInstanceCount");
	g_move_window_fn =
		(move_window_proc_t) GetProcAddress(hookdll, "SafeMoveWindow");
	g_zchange_fn = (zchange_proc_t) GetProcAddress(hookdll, "SafeZChange");
	g_focus_fn = (focus_proc_t) GetProcAddress(hookdll, "SafeFocus");
	g_set_state_fn = (set_state_proc_t) GetProcAddress(hookdll, "SafeSetState");

	g_vchannel_reopen_fn = (vchannel_reopen_t) GetProcAddress(hookdll, "vchannel_reopen");
	g_vchannel_block_fn = (vchannel_block_t) GetProcAddress(hookdll, "vchannel_block");
	g_vchannel_unblock_fn = (vchannel_unblock_t) GetProcAddress(hookdll, "vchannel_unblock");
	g_vchannel_write_fn = (vchannel_write_t) GetProcAddress(hookdll, "vchannel_write");     
	g_vchannel_read_fn = (vchannel_read_t) GetProcAddress(hookdll, "vchannel_read");
	g_vchannel_strfilter_unicode_fn = (vchannel_strfilter_unicode_t) GetProcAddress(hookdll, "vchannel_strfilter_unicode");
	g_vchannel_debug_fn = (vchannel_debug_t) GetProcAddress(hookdll, "vchannel_debug");

	if (!set_hooks_fn || !remove_hooks_fn || !instance_count_fn
	    || !g_move_window_fn || !g_zchange_fn || !g_focus_fn || !g_set_state_fn
	    || !g_vchannel_reopen_fn || !g_vchannel_block_fn || !g_vchannel_unblock_fn
	    || !g_vchannel_write_fn || !g_vchannel_read_fn || !g_vchannel_strfilter_unicode_fn
	    || !g_vchannel_debug_fn)
        {
		messageW
			(L"Hook DLL doesn't contain the correct functions. Unable to continue.");
		goto close_hookdll;
	}

	/* Check if the DLL is already loaded */
	switch (instance_count_fn()) {
	case 0:
		messageW(L"Hook DLL failed to initialize.");
		goto close_hookdll;
		break;
	case 1:
		break;
	default:
		messageW(L"Another running instance of Seamless RDP detected.");
		goto close_hookdll;
	}

	helper = launch_helper();

	ProcessIdToSessionId(GetCurrentProcessId(), &g_session_id);

	build_startup_procs();

	build_system_procs();

	g_connected = is_connected();
	g_desktop_hidden = is_desktop_hidden();

	g_vchannel_write_fn("HELLO", "0x%08x",
		g_desktop_hidden ? SEAMLESS_HELLO_HIDDEN : 0);

	set_hooks_fn();

	/* Since we don't see the entire desktop we must resize windows
	   immediatly. */
	SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, TRUE, NULL, 0);

	/* Disable screen saver since we cannot catch its windows. */
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

	/* We don't want windows denying requests to activate windows. */
	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, 0, 0);

	g_session_disconnect_ts = 0;
	check_counter = 5;
	while (check_counter-- || !should_terminate()) {
		BOOL connected;
		MSG msg;

		connected = is_connected();

		if (!connected && !g_session_disconnect_ts)
			g_session_disconnect_ts = time(NULL);

		if (connected && g_session_disconnect_ts)
			g_session_disconnect_ts = 0;

		if (connected && !g_connected) {
			int flags;
			/* These get reset on each reconnect */
			SystemParametersInfo(SPI_SETDRAGFULLWINDOWS, TRUE, NULL, 0);
			SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
			SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, 0, 0);

			inc_conn_serial_fn();
			g_vchannel_reopen_fn();

			flags = SEAMLESS_HELLO_RECONNECT;
			if (g_desktop_hidden)
				flags |= SEAMLESS_HELLO_HIDDEN;
			g_vchannel_write_fn("HELLO", "0x%08x", flags);
		}

		g_connected = connected;

		if (check_counter < 0) {
			BOOL hidden;

			hidden = is_desktop_hidden();
			if (hidden && !g_desktop_hidden)
				g_vchannel_write_fn("HIDE", "0x%08x", 0);
			else if (!hidden && g_desktop_hidden)
				g_vchannel_write_fn("UNHIDE", "0x%08x", 0);

			g_desktop_hidden = hidden;

			check_counter = 5;
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		process_cmds();
		Sleep(100);
	}

	success = 1;

	remove_hooks_fn();

	free_system_procs();

	free_startup_procs();
	if (helper) {
		// Terminate seamlessrdphook32.exe
		kill_15_9(helper, "SeamlessRDPHook", HELPER_TIMEOUT);
	}

  close_hookdll:
	FreeLibrary(hookdll);

  bail_out:
	// Logoff the user. This is necessary because the session may
	// have started processes that are not included in Microsofts
	// list of processes to ignore. Typically ieuser.exe.
	// FIXME: Only do this if WTSQuerySessionInformation indicates
	// that we are the initial program. 
	ExitWindows(0, 0);

	if (success)
		return 1;
	else
		return -1;
}
