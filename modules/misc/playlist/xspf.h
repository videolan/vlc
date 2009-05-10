/*****************************************************************************
 * Copyright (C) 2006 Daniel Stränger <vlc at schmaller dot de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *******************************************************************************/
/**
 * \file modules/misc/playlist/xspf.h
 * \brief XSPF playlist export module header file
 */

/* defs */
#define B10000000 0x80
#define B01000000 0x40
#define B11000000 0xc0
#define B00001111 0x0f

#define XSPF_MAX_CONTENT 2000

/* constants */
const char hexchars[16] = "0123456789ABCDEF";

/* prototypes */
int xspf_export_playlist( vlc_object_t * );
