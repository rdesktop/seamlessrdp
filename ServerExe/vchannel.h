/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Virtual channel handling

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

#ifndef __VCHANNEL_H__
#define __VCHANNEL_H__

#define DLL_EXPORT __declspec(dllexport)

#define VCHANNEL_MAX_LINE 1024

#define SEAMLESS_CREATE_MODAL	0x0001
#define SEAMLESS_CREATE_TOPMOST	0x0002

#define SEAMLESS_HELLO_RECONNECT	0x0001
#define SEAMLESS_HELLO_HIDDEN		0x0002

DLL_EXPORT void debug(char *format, ...);

DLL_EXPORT const char *unicode_to_utf8(const unsigned short *string);

DLL_EXPORT int vchannel_open();
DLL_EXPORT void vchannel_close();

DLL_EXPORT int vchannel_is_open();

/* read may only be used by a single process. write is completely safe */
DLL_EXPORT int vchannel_read(char *line, size_t length);
DLL_EXPORT int vchannel_write(const char *command, const char *format, ...);

DLL_EXPORT void vchannel_block();
DLL_EXPORT void vchannel_unblock();

DLL_EXPORT const char *vchannel_strfilter(char *string);
DLL_EXPORT const char *vchannel_strfilter_unicode(const unsigned short *string);

#endif
