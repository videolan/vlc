/*****************************************************************************
 * drms.h : DRMS
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Author: Jon Lech Johansen <jon-vl@nanocrew.net>
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
 *****************************************************************************/

#ifndef _VLC_DRMS_H
#define _VLC_DRMS_H 1

extern void *drms_alloc( const char *psz_homedir );
extern void drms_free( void *p_drms );
extern int drms_init( void *p_drms, uint32_t i_type,
                      uint8_t *p_info, uint32_t i_len );
extern void drms_decrypt( void *p_drms, uint32_t *p_buffer,
                          uint32_t i_len );

#endif
