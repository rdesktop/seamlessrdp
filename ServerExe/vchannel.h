/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Virtual channel handling

   Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB
   Copyright 2010 Peter Åstrand <astrand@cendio.se> for Cendio AB
   Copyright 2013-2014 Henrik Andersson <hean01@cendio.se> for Cendio AB

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

#ifndef __VCHANNEL_H__
#define __VCHANNEL_H__

#define EXTERN __declspec(dllexport)

#define VCHANNEL_MAX_LINE 1024

#define SEAMLESS_CREATE_MODAL	0x0001
#define SEAMLESS_CREATE_TOPMOST	0x0002

#define SEAMLESS_HELLO_RECONNECT	0x0001
#define SEAMLESS_HELLO_HIDDEN		0x0002

EXTERN void vchannel_debug(char *format, ...);

EXTERN int vchannel_open();
EXTERN void vchannel_close();
EXTERN int vchannel_reopen();

EXTERN int vchannel_is_open();

/* read may only be used by a single process. write is completely safe */
EXTERN int vchannel_read(char *line, size_t length);
EXTERN int vchannel_write(const char *command, const char *format, ...);

EXTERN void vchannel_block();
EXTERN void vchannel_unblock();

EXTERN const char *vchannel_strfilter(char *string);
EXTERN const char *vchannel_strfilter_unicode(const unsigned short *string);

#endif
