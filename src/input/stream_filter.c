/*****************************************************************************
 * stream_filter.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>
#include <libvlc.h>

#include "stream.h"

static void StreamDelete( stream_t * );

stream_t *stream_FilterNew( stream_t *p_source,
                            const char *psz_stream_filter )
{
    stream_t *s;

    s = stream_CommonNew( VLC_OBJECT( p_source ) );
    if( s == NULL )
        return NULL;

    /* */
    s->p_source = p_source;

    /* */
    vlc_object_attach( s, p_source );

    s->p_module = module_need( s, "stream_filter", psz_stream_filter, true );

    if( !s->p_module )
    {
        stream_CommonDelete( s );
        return NULL;
    }

    s->pf_destroy = StreamDelete;

    return s;
}

static void StreamDelete( stream_t *s )
{
    module_unneed( s, s->p_module );

    if( s->p_source )
        stream_Delete( s->p_source );

    stream_CommonDelete( s );
}

