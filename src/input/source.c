/*****************************************************************************
 * source.c: input thread source handling
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
# include <config.h>
#endif

#include "./source.h"

input_source_t *input_source_Hold(input_source_t *in)
{
    vlc_atomic_rc_inc( &in->rc );
    return in;
}

void input_source_Release(input_source_t *in)
{
    if( vlc_atomic_rc_dec( &in->rc ) )
    {
        free( in->str_id );
        free( in );
    }
}

const char *input_source_GetStrId(input_source_t *in)
{
    return in->str_id;
}

int input_source_GetNewAutoId(input_source_t *in)
{
    return in->auto_id++;
}

bool input_source_IsAutoSelected(input_source_t *in)
{
    return in->autoselected;
}

