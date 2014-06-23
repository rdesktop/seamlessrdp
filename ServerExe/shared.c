/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless windows - Shared helper functions

   Copyright 2014 Henrik Andersson <hean01@cendio.se> for Cendio AB

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

#include "shared.h"

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


HICON
get_icon(HWND hwnd, int large)
{
	HICON icon;

	if (!SendMessageTimeout(hwnd, WM_GETICON, large ? ICON_BIG : ICON_SMALL,
			0, SMTO_ABORTIFHUNG, 1000, (PDWORD_PTR) & icon))
		return NULL;

	if (icon)
		return icon;

	/*
	 * Modern versions of Windows uses the voodoo value of 2 instead of 0
	 * for the small icons.
	 */
	if (!large) {
		if (!SendMessageTimeout(hwnd, WM_GETICON, 2,
				0, SMTO_ABORTIFHUNG, 1000, (PDWORD_PTR) & icon))
			return NULL;
	}

	if (icon)
		return icon;

	icon = (HICON) GetClassLongPtr(hwnd, large ? GCLP_HICON : GCLP_HICONSM);

	if (icon)
		return icon;

	return NULL;
}


int
extract_icon(HICON icon, char *buffer, int maxlen)
{
	ICONINFO info;
	HDC hdc;
	BITMAP mask_bmp, color_bmp;
	BITMAPINFO bmi;
	int size, i;
	char *mask_buf, *color_buf;
	char *o, *m, *c;
	int ret = -1;

	assert(buffer);
	assert(maxlen > 0);

	if (!GetIconInfo(icon, &info))
		goto fail;

	if (!GetObject(info.hbmMask, sizeof(BITMAP), &mask_bmp))
		goto free_bmps;
	if (!GetObject(info.hbmColor, sizeof(BITMAP), &color_bmp))
		goto free_bmps;

	if (mask_bmp.bmWidth != color_bmp.bmWidth)
		goto free_bmps;
	if (mask_bmp.bmHeight != color_bmp.bmHeight)
		goto free_bmps;

	if ((mask_bmp.bmWidth * mask_bmp.bmHeight * 4) > maxlen)
		goto free_bmps;

	size = (mask_bmp.bmWidth + 3) / 4 * 4;
	size *= mask_bmp.bmHeight;
	size *= 4;

	mask_buf = malloc(size);
	if (!mask_buf)
		goto free_bmps;
	color_buf = malloc(size);
	if (!color_buf)
		goto free_mbuf;

	memset(&bmi, 0, sizeof(BITMAPINFO));

	bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmi.bmiHeader.biWidth = mask_bmp.bmWidth;
	bmi.bmiHeader.biHeight = -mask_bmp.bmHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = size;

	hdc = CreateCompatibleDC(NULL);
	if (!hdc)
		goto free_cbuf;

	if (!GetDIBits(hdc, info.hbmMask, 0, mask_bmp.bmHeight, mask_buf, &bmi,
			DIB_RGB_COLORS))
		goto del_dc;
	if (!GetDIBits(hdc, info.hbmColor, 0, color_bmp.bmHeight, color_buf, &bmi,
			DIB_RGB_COLORS))
		goto del_dc;

	o = buffer;
	m = mask_buf;
	c = color_buf;
	for (i = 0; i < size / 4; i++) {
		o[0] = c[2];
		o[1] = c[1];
		o[2] = c[0];

		o[3] = ((int) (unsigned char) m[0] + (unsigned char) m[1] +
			(unsigned char) m[2]) / 3;
		o[3] = 0xff - o[3];

		o += 4;
		m += 4;
		c += 4;
	}

	ret = size;

  del_dc:
	DeleteDC(hdc);

  free_cbuf:
	free(color_buf);
  free_mbuf:
	free(mask_buf);

  free_bmps:
	DeleteObject(info.hbmMask);
	DeleteObject(info.hbmColor);

  fail:
	return ret;
}
