/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Virtual channel handling

   Copyright (C) Pierre Ossman <ossman@cendio.se> 2006
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

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#include <windows.h>
#include <wtsapi32.h>
#include <cchannel.h>

#include "vchannel.h"

#define CHANNELNAME "seamrdp"

#define INVALID_CHARS ","
#define REPLACEMENT_CHAR '_'

static HANDLE g_mutex = NULL;
static HANDLE g_vchannel = NULL;
static HANDLE g_vchannel_serial = NULL;
static unsigned int g_opencount = 0;

EXTERN void
debug(char *format, ...)
{
	va_list argp;
	char buf[256];

	va_start(argp, format);
	_vsnprintf(buf, sizeof(buf), format, argp);
	va_end(argp);

	vchannel_strfilter(buf);

	vchannel_write("DEBUG", buf);
}

/* 
   From http://msdn.microsoft.com/en-us/library/aa384203(VS.85).aspx:
   "64-bit versions of Windows use 32-bit handles for interoperability."
 */
long
hwnd_to_long(HWND hwnd)
{
	DWORD_PTR val;
	val = (DWORD_PTR) hwnd;
	return val;
}

HWND
long_to_hwnd(long l)
{
	DWORD_PTR val;
	val = l;
	return (HWND) val;
}


#define CONVERT_BUFFER_SIZE 1024
static char convert_buffer[CONVERT_BUFFER_SIZE];

EXTERN const char *
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
	g_opencount++;
	if (g_opencount > 1)
		return 0;

	g_vchannel = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE,
		WTS_CURRENT_SESSION, CHANNELNAME);

	if (g_vchannel == NULL)
		return -1;

	g_mutex = CreateMutex(NULL, FALSE, "Local\\SeamlessChannel");

	g_vchannel_serial =
		CreateSemaphore(NULL, 0, INT_MAX, "Local\\SeamlessRDPSerial");

	if (!g_mutex || !g_vchannel_serial) {
		WTSVirtualChannelClose(g_vchannel);
		g_vchannel = NULL;
		return -1;
	}

	return 0;
}

EXTERN void
vchannel_close()
{
	g_opencount--;
	if (g_opencount > 0)
		return;

	if (g_mutex)
		CloseHandle(g_mutex);

	if (g_vchannel)
		WTSVirtualChannelClose(g_vchannel);

	g_mutex = NULL;
	g_vchannel = NULL;
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
	int line_size;

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
	line_size = newline - buffer;

	size -= newline - buffer + 1;
	memmove(buffer, newline + 1, size);

	return 0;
}

EXTERN int
vchannel_write(const char *command, const char *format, ...)
{
	BOOL result;
	va_list argp;
	char buf[VCHANNEL_MAX_LINE];
	int size, ret;
	ULONG bytes_written;
	LONG prev_serial;

	assert(vchannel_is_open());

	WaitForSingleObject(g_mutex, INFINITE);

	/* Increase serial */
	if (!ReleaseSemaphore(g_vchannel_serial, 1, &prev_serial)) {
		if (GetLastError() == ERROR_TOO_MANY_POSTS) {
			/* Reset serial to zero */
			while (WaitForSingleObject(g_vchannel_serial, 0) == WAIT_OBJECT_0);
			if (!ReleaseSemaphore(g_vchannel_serial, 1, &prev_serial)) {
				return -1;
			}
		}
	}

	ret = _snprintf(buf, sizeof(buf), "%s,%u,", command, prev_serial);

	if (ret < 0) {
		// If the command and serial didn't fit, let's skip
		// this message
		return -1;
	} else {
		size = ret;
	}
	assert(size <= sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	va_start(argp, format);
	ret = _vsnprintf(buf + size, sizeof(buf) - size, format, argp);
	va_end(argp);

	if (ret < 0) {
		// Truncated, but let's write what we have              
		size = sizeof(buf) - 1;
	} else {
		size = ret;
	}
	buf[sizeof(buf) - 1] = '\0';
	assert(size < sizeof(buf));

	result =
		WTSVirtualChannelWrite(g_vchannel, buf, (ULONG) strlen(buf),
		&bytes_written);
	result =
		WTSVirtualChannelWrite(g_vchannel, "\n", (ULONG) 1, &bytes_written);

	ReleaseMutex(g_mutex);

	if (!result)
		return -1;

	return bytes_written;
}

EXTERN void
vchannel_block()
{
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
