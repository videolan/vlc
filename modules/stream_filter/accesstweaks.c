/*****************************************************************************
 * accesstweaks.c Access controls tweaking stream filter
 *****************************************************************************
 * Copyright (C) 2015 VideoLAN Authors
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <assert.h>

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_shortname("accesstweaks")
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_STREAM_FILTER)
    set_capability ("stream_filter", 0)
    /* Developers only module, no translation please */
    set_description ("Access controls tweaking")
    set_callbacks (Open, Close)

    add_bool ("seek", true, "forces result of the CAN_SEEK control", NULL, false)
        change_volatile ()
    add_bool ("fastseek", true, "forces result of the CAN_FASTSEEK control", NULL, false)
        change_volatile ()
    add_shortcut("tweaks")
vlc_module_end ()

struct stream_sys_t
{
    bool b_seek;
    bool b_fastseek;
};

/**
 *
 */
static int Control( stream_t *p_stream, int i_query, va_list args )
{
    stream_sys_t *p_sys = p_stream->p_sys;

    switch( i_query )
    {
    case STREAM_CAN_FASTSEEK:
        if( !p_sys->b_fastseek || !p_sys->b_seek )
        {
            *((bool*)va_arg( args, bool* )) = false;
            return VLC_SUCCESS;
        }
        break;
    case STREAM_CAN_SEEK:
        if( !p_sys->b_seek )
        {
            *((bool*)va_arg( args, bool* )) = false;
            return VLC_SUCCESS;
        }
        break;

    default:
        break;
    }

    return vlc_stream_vaControl( p_stream->p_source, i_query, args );
}

static ssize_t Read( stream_t *s, void *buffer, size_t i_read )
{
    return vlc_stream_Read( s->p_source, buffer, i_read );
}

static int Seek( stream_t *s, uint64_t offset )
{
    stream_sys_t *p_sys = s->p_sys;

    assert( p_sys->b_seek );
    return vlc_stream_Seek( s->p_source, offset );
}

static int Open( vlc_object_t *p_object )
{
    stream_t *p_stream = (stream_t *) p_object;

    stream_sys_t *p_sys = p_stream->p_sys = malloc( sizeof(*p_sys) );
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->b_seek = var_InheritBool( p_stream, "seek" );
    p_sys->b_fastseek = var_InheritBool( p_stream, "fastseek" );

    p_stream->pf_read = Read;
    p_stream->pf_seek = p_sys->b_seek ? Seek : NULL;
    p_stream->pf_control = Control;

    return VLC_SUCCESS;
}

static void Close ( vlc_object_t *p_object )
{
    stream_t *p_stream = (stream_t *)p_object;
    free( p_stream->p_sys );
}
