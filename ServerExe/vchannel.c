/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Virtual channel handling

   Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB
   Copyright 2010 Peter Åstrand <astrand@cendio.se> for Cendio AB
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
#include <errno.h>
#include <limits.h>

#include <windows.h>
#include <wtsapi32.h>
#include <cchannel.h>
#include <inttypes.h>

#include "vchannel.h"

#define CHANNELNAME "seamrdp"

#define INVALID_CHARS ","
#define REPLACEMENT_CHAR '_'

#define TIMEOUT 10*1000
#define BUFFER_SIZE 96*1024
typedef struct __attribute__ ((__packed__)) buffer_t {
	uint32_t pw;
	uint32_t pr;
	uint32_t size;
	char data[BUFFER_SIZE];
} buffer_t;

_Static_assert(sizeof(buffer_t) == (4 + 4 + 4 + BUFFER_SIZE),
	"Unexpected size of buffer_t");

static HANDLE g_mutex = NULL;
static HANDLE g_vchannel = NULL;
static HANDLE g_vchannel_serial = NULL;
static HANDLE g_evwr = NULL;
static HANDLE g_evrd = NULL;

static HANDLE g_map_file;
static buffer_t *g_buffer;

static BOOL g_seamless_shell = TRUE;

EXTERN void
vchannel_debug(char *format, ...)
{
	va_list argp;
	char buf[256];

	va_start(argp, format);
	_vsnprintf(buf, sizeof(buf), format, argp);
	va_end(argp);

	vchannel_strfilter(buf);

	vchannel_write("DEBUG", buf);
}

#define CONVERT_BUFFER_SIZE 1024
static char convert_buffer[CONVERT_BUFFER_SIZE];

static const char *
unicode_to_utf8(const unsigned short *string)
{
	unsigned char *buf;
	size_t size;

	buf = (unsigned char *) convert_buffer;
	size = sizeof(convert_buffer) - 1;

	/* We do not handle characters outside BMP (i.e. we can't do UTF-16) */
	while (*string != 0x0000) {
		if (*string < 0x80) {
			if (size < 1)
				break;
			*buf++ = (unsigned char) *string;
			size--;
		} else if (*string < 0x800) {
			if (size < 2)
				break;
			*buf++ = 0xC0 | (*string >> 6);
			*buf++ = 0x80 | (*string & 0x3F);
			size -= 2;
		} else if ((*string < 0xD800) || (*string > 0xDFFF)) {
			if (size < 3)
				break;
			*buf++ = 0xE0 | (*string >> 12);
			*buf++ = 0x80 | (*string >> 6 & 0x3F);
			*buf++ = 0x80 | (*string & 0x3F);
			size -= 2;
		}

		string++;
	}

	*buf = '\0';

	return convert_buffer;
}

EXTERN int
vchannel_open()
{
	if (g_vchannel != NULL)
		return 0;

	g_vchannel = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE,
		WTS_CURRENT_SESSION, CHANNELNAME);

	if (g_vchannel == NULL)
		goto bail_out;

	g_mutex = CreateMutex(NULL, FALSE, "Local\\SeamlessChannel");
	if (g_mutex == NULL)
		goto bail_out;

	g_vchannel_serial =
		CreateSemaphore(NULL, 0, INT_MAX, "Local\\SeamlessRDPSerial");
	if (g_vchannel_serial == NULL)
		goto bail_out;

	g_map_file = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
		PAGE_READWRITE, 0, sizeof(buffer_t), "Local\\SeamlessChannelBuffer");
	if (!g_map_file)
		goto bail_out;

	g_buffer =
		MapViewOfFile(g_map_file, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(buffer_t));
	if (!g_buffer)
		goto bail_out;

	g_evrd =
		CreateEvent(NULL, FALSE, FALSE, "Local\\SeamlessChannelBufferRead");
	g_evwr =
		CreateEvent(NULL, FALSE, FALSE, "Local\\SeamlessChannelBufferWrite");

	if (!g_evrd || !g_evwr)
		goto bail_out;

	g_buffer->size = BUFFER_SIZE;
	g_buffer->pw = g_buffer->pr = 0;

	return 0;

  bail_out:
	MessageBoxW(GetDesktopWindow(), L"Failed to open vchannel",
		L"SeamlessRDP Shell", MB_OK);
	vchannel_close();

	return -1;
}

EXTERN int
vchannel_reopen()
{
	vchannel_block();
	if (g_vchannel)
		WTSVirtualChannelClose(g_vchannel);

	g_vchannel = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE,
		WTS_CURRENT_SESSION, CHANNELNAME);

	vchannel_unblock();

	if (g_vchannel == NULL)
		return -1;

	return 0;
}

EXTERN void
vchannel_close()
{
	if (g_mutex)
		CloseHandle(g_mutex);

	if (g_vchannel)
		WTSVirtualChannelClose(g_vchannel);

	if (g_buffer)
		UnmapViewOfFile(g_buffer);

	if (g_map_file)
		CloseHandle(g_map_file);

	g_mutex = NULL;
	g_vchannel = NULL;
	g_map_file = NULL;
	g_buffer = NULL;
}

EXTERN int
vchannel_is_open()
{
	if (g_vchannel == NULL)
		return 0;
	else
		return 1;
}

EXTERN int
vchannel_read(char *line, size_t length)
{
	static BOOL overflow_mode = FALSE;
	static char buffer[VCHANNEL_MAX_LINE];
	static size_t size = 0;

	char *newline;

	BOOL result;
	ULONG bytes_read;

	result = WTSVirtualChannelRead(g_vchannel, 0, buffer + size,
		sizeof(buffer) - size, &bytes_read);

	if (!result) {
		errno = EIO;
		return -1;
	}

	if (overflow_mode) {
		newline = strchr(buffer, '\n');
		if (newline && (newline - buffer) < bytes_read) {
			size = bytes_read - (newline - buffer) - 1;
			memmove(buffer, newline + 1, size);
			overflow_mode = FALSE;
		}
	} else
		size += bytes_read;

	if (overflow_mode) {
		errno = -EAGAIN;
		return -1;
	}

	newline = strchr(buffer, '\n');
	if (!newline || (newline - buffer) >= size) {
		if (size == sizeof(buffer)) {
			overflow_mode = TRUE;
			size = 0;
		}
		errno = -EAGAIN;
		return -1;
	}

	if ((newline - buffer) >= length) {
		errno = ENOMEM;
		return -1;
	}

	*newline = '\0';

	strcpy(line, buffer);

	size -= newline - buffer + 1;
	memmove(buffer, newline + 1, size);

	return 0;
}

EXTERN int
vchannel_write(const char *command, const char *format, ...)
{
	DWORD res;
	va_list argp;
	char args[VCHANNEL_MAX_LINE];
	int i, size, ret;
	ULONG bytes_written;

	if (g_seamless_shell == FALSE)
		return -1;

	if (!g_buffer || !g_mutex || !g_evwr || !g_evrd) {
		/* Setup the client side of buffer writing if not yet
		   initialized */
		g_mutex = OpenMutex(SYNCHRONIZE, FALSE, "Local\\SeamlessChannel");
		if (!g_mutex) {
			/* fatal: failed to open required mutex */
			g_seamless_shell = FALSE;
			return -1;
		}

		g_map_file =
			OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE,
			"Local\\SeamlessChannelBuffer");
		if (!g_map_file) {
			/* fatal: failed to open require file mapping object */
			g_seamless_shell = FALSE;
			return -1;
		}

		g_buffer =
			MapViewOfFile(g_map_file, FILE_MAP_ALL_ACCESS, 0, 0,
			sizeof(buffer_t));
		if (!g_buffer) {
			/* fatal: failed to map view of file mapping object */
			g_seamless_shell = FALSE;
			return -1;
		}

		g_evrd =
			OpenEvent(EVENT_ALL_ACCESS, FALSE,
			"Local\\SeamlessChannelBufferRead");
		if (!g_evrd) {
			/* fatal: failed to open required buffer read event */
			g_seamless_shell = FALSE;
			return -1;
		}

		g_evwr =
			OpenEvent(EVENT_MODIFY_STATE, FALSE,
			"Local\\SeamlessChannelBufferWrite");
		if (!g_evwr) {
			/* fatal: failed to open required buffer write event */
			g_seamless_shell = FALSE;
			return -1;
		}
	}

	va_start(argp, format);
	ret = _vsnprintf(args, sizeof(args), format, argp);
	va_end(argp);
	assert(ret > 0);

	/* verify that command + args fits in buffer, if not wait for
	   read event which indicates that free space in buffer has
	   changed */
	size = strlen(command) + 1 + strlen(args) + 1;
	while (1) {
		res = WaitForSingleObject(g_mutex, TIMEOUT);
		if (res != WAIT_OBJECT_0) {
			/* failed to aquire lock, assume that seamlessrdpshell is killed */
			g_seamless_shell = FALSE;
			return -1;
		}

		/* check if there is free space to write size data into buffer */
		if (g_buffer->pw < g_buffer->pr && g_buffer->pw + size >= g_buffer->pr) {
			/* Write of size would override read pointer */
		} else if (g_buffer->pw + size >= g_buffer->size
			&& (g_buffer->pw + size) % g_buffer->size >= g_buffer->pr) {
			/* Write of size would wrap and override read pointer */
		} else {
			/* message fits in buffer, break out of retry loop */
			break;
		}

		/* lets wait for read event and recheck available
		   space in buffer for a complete write operation of
		   full message size */
		ReleaseMutex(g_mutex);
		res = WaitForSingleObject(g_evrd, TIMEOUT);
		if (res != WAIT_OBJECT_0) {
			/* Error or Timeout while waiting for event,
			   assume seamlessrdpshell is killed */
			WaitForSingleObject(g_mutex, TIMEOUT);
			g_seamless_shell = FALSE;
			ReleaseMutex(g_mutex);
			return -1;
		}
	}

	/* write NULL terminated strings command and args to buffer
	   write buffer position */
	for (i = 0; i < strlen(command) + 1; i++) {
		g_buffer->data[g_buffer->pw] = command[i];
		g_buffer->pw = (g_buffer->pw + 1) % g_buffer->size;
	}

	for (i = 0; i < strlen(args) + 1; i++) {
		g_buffer->data[g_buffer->pw] = args[i];
		g_buffer->pw = (g_buffer->pw + 1) % g_buffer->size;
	}

	/* signal that data is available on buffer */
	SetEvent(g_evwr);
	ReleaseMutex(g_mutex);
	return 0;
}

/* reads all available command in buffer and write them to the virtual
   channel. return number of messages, on error < 0 is return */
EXTERN int
vchannel_process()
{
	int messages;
	DWORD res;
	BOOL result;
	char *pd;
	char args[VCHANNEL_MAX_LINE];
	char command[VCHANNEL_MAX_LINE];
	char buf[VCHANNEL_MAX_LINE];
	int ret;
	ULONG bytes_written;
	LONG prev_serial;

	assert(vchannel_is_open());
	if (g_buffer == NULL)
		return -1;

	/* wait for data available event */
	res = WaitForSingleObject(g_evwr, 100);
	if (res == WAIT_TIMEOUT)
		return 0;

	/* aquire lock */
	WaitForSingleObject(g_mutex, INFINITE);

	/* check if there is data available for processing */
	if (g_buffer->pr == g_buffer->pw) {
		ReleaseMutex(g_mutex);
		return 0;
	}

	/* process all available comands in buffer */
	messages = 0;
	while (g_buffer->pr != g_buffer->pw) {
		/* Increase serial */
		if (!ReleaseSemaphore(g_vchannel_serial, 1, &prev_serial)) {
			if (GetLastError() == ERROR_TOO_MANY_POSTS) {
				/* Reset serial to zero */
				while (WaitForSingleObject(g_vchannel_serial,
						0) == WAIT_OBJECT_0);
				if (!ReleaseSemaphore(g_vchannel_serial, 1, &prev_serial)) {
					return -1;
				}
			}
		}

		/* read command string from buffer */
		pd = command;
		while (1) {
			*pd = g_buffer->data[g_buffer->pr];
			if (*pd == '\0')
				break;
			pd++;
			g_buffer->pr = (g_buffer->pr + 1) % g_buffer->size;
		}
		g_buffer->pr = (g_buffer->pr + 1) % g_buffer->size;

		/* read args string from buffer */
		pd = args;
		while (1) {
			*pd = g_buffer->data[g_buffer->pr];
			if (*pd == '\0')
				break;
			pd++;
			g_buffer->pr = (g_buffer->pr + 1) % g_buffer->size;
		}
		g_buffer->pr = (g_buffer->pr + 1) % g_buffer->size;

		SetEvent(g_evrd);

		/* build seamless rdp message be sent over channel */
		ret =
			_snprintf(buf, sizeof(buf), "%s,%u,%s", command, prev_serial, args);
		if (ret < 0 || ret >= sizeof(buf)) {
			/* Skip writing command if failed to create
			   message, either failure or truncation */
			continue;
		}

		/* write the message over the virtual channel */
		result =
			WTSVirtualChannelWrite(g_vchannel, buf, (ULONG) strlen(buf),
			&bytes_written);
		result =
			WTSVirtualChannelWrite(g_vchannel, "\n", (ULONG) 1, &bytes_written);

		if (!result) {
			/* TODO: handle write failed on virtual channel */
			break;
		}

		messages++;
	}
	ReleaseMutex(g_mutex);
	return messages;
}


EXTERN void
vchannel_block()
{
	/* FIXME: This should use TIMEOUT instead of INFINITE. However
	   that would require to implement error handling for all use
	   of vchannel_block() */
	WaitForSingleObject(g_mutex, INFINITE);
}

EXTERN void
vchannel_unblock()
{
	ReleaseMutex(g_mutex);
}

EXTERN const char *
vchannel_strfilter(char *string)
{
	char *c;

	for (c = string; *c != '\0'; c++) {
		if (((unsigned char) *c < 0x20) || (strchr(INVALID_CHARS, *c) != NULL))
			*c = REPLACEMENT_CHAR;
	}

	return string;
}

EXTERN const char *
vchannel_strfilter_unicode(const unsigned short *string)
{
	return vchannel_strfilter((char *) unicode_to_utf8(string));
}
