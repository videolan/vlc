/*****************************************************************************
 * libdvdcss.h: DVD reading library, exported functions.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvdcss.h,v 1.1 2001/11/25 05:04:38 stef Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * The libdvdcss structure
 *****************************************************************************/
typedef struct dvdcss_s* dvdcss_handle;

/*****************************************************************************
 * Flags
 *****************************************************************************/
#define DVDCSS_NOFLAGS         0

#define DVDCSS_INIT_QUIET      (1 << 0)
#define DVDCSS_INIT_DEBUG      (1 << 1)

#define DVDCSS_READ_DECRYPT    (1 << 0)

#define DVDCSS_BLOCK_SIZE      2048

