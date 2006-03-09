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

#define VCHANNEL_MAX_LINE 1024

void debug(char *format, ...);

int vchannel_open();
void vchannel_close();

int vchannel_is_open();

/* read may only be used by a single process. write is completely safe */
int vchannel_read(char *line, size_t length);
int vchannel_write(char *format, ...);

#endif
