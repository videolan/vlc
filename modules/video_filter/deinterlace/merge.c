/*****************************************************************************
 * merge.c : Merge (line blending) routines for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>                      (generic C routine)
 *         Sigmund Augdal Helberg <sigmunau@videolan.org> (MMXEXT, 3DNow, SSE2)
 *         Eric Petit <eric.petit@lapsus.org>             (Altivec)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdlib.h>
#include <stdint.h>

#include <vlc_common.h>
#include "merge.h"


/*****************************************************************************
 * Merge (line blending) routines
 *****************************************************************************/

void Merge8BitGeneric( void *_p_dest, const void *_p_s1,
                       const void *_p_s2, size_t i_bytes )
{
    uint8_t *p_dest = _p_dest;
    const uint8_t *p_s1 = _p_s1;
    const uint8_t *p_s2 = _p_s2;

    for( ; i_bytes > 0; i_bytes-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ + 1) >> 1;
}

void Merge16BitGeneric( void *_p_dest, const void *_p_s1,
                        const void *_p_s2, size_t i_bytes )
{
    uint16_t *p_dest = _p_dest;
    const uint16_t *p_s1 = _p_s1;
    const uint16_t *p_s2 = _p_s2;

    for( size_t i_words = i_bytes / 2; i_words > 0; i_words-- )
        *p_dest++ = ( *p_s1++ + *p_s2++ + 1) >> 1;
}
