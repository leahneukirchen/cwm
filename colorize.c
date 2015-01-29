/* $Id$ */
/*
 * Copyright (c) 2015 Dimitri Sokolyuk <demon@dim13.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "calmwm.h"

#define CRC24_INIT	0x0B704CEL
#define CRC24_POLY	0x1864CFBL

long
crc24(char *s)
{
	long crc;
	int i;

	for (crc = CRC24_INIT; *s; s++) {
		crc ^= *s << 0x10;
		for (i = 0; i < 8; i++) {
			crc <<= 1;
			if (crc & 0x1000000)
				crc ^= CRC24_POLY;
		}
	}

	return crc;
}

long
shade(long c)
{
	unsigned char r = c >> 0x10;
	unsigned char g = c >> 0x08;
	unsigned char b = c;

	r >>= 2;
	g >>= 2;
	b >>= 2;

	return (r << 0x10) | (g << 0x8) | b;
}

long
tint(long c)
{
	unsigned char r = c >> 0x10;
	unsigned char g = c >> 0x08;
	unsigned char b = c;

	r += (UCHAR_MAX - r) >> 1;
	g += (UCHAR_MAX - g) >> 1;
	b += (UCHAR_MAX - b) >> 1;

	return (r << 0x10) | (g << 0x8) | b;
}
