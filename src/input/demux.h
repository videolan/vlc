/*****************************************************************************
 * demux.h: Input demux functions
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_INPUT_DEMUX_H
#define LIBVLC_INPUT_DEMUX_H 1

#include <vlc_common.h>
#include <vlc_demux.h>

#include "stream.h"

/* stream_t *s could be null and then it mean a access+demux in one */
demux_t *demux_New( vlc_object_t *p_obj, input_thread_t *p_parent_input, const char *psz_access, const char *psz_demux, const char *psz_path, stream_t *s, es_out_t *out, bool );
#define demux_New( a, b, c, d, e, f, g, h ) demux_New(VLC_OBJECT(a),b,c,d,e,f,g,h)

void demux_Delete( demux_t * );

static inline int demux_Demux( demux_t *p_demux )
{
    if( !p_demux->pf_demux )
        return 1;

    return p_demux->pf_demux( p_demux );
}
static inline int demux_vaControl( demux_t *p_demux, int i_query, va_list args )
{
    return p_demux->pf_control( p_demux, i_query, args );
}
static inline int demux_Control( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_demux, i_query, args );
    va_end( args );
    return i_result;
}

#endif
