/*****************************************************************************
 * duplicate.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: duplicate.c,v 1.1 2003/04/13 20:00:21 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, sout_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Duplicate stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "duplicate" );
    add_shortcut( "dup" );
    set_callbacks( Open, Close );
vlc_module_end();


struct sout_stream_sys_t
{
    int             i_nb_streams;
    sout_stream_t   **pp_streams;
};

struct sout_stream_id_t
{
    int                 i_nb_ids;
    void                **pp_ids;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    sout_cfg_t        *p_cfg;

    msg_Dbg( p_stream, "creating a duplication" );

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->i_nb_streams = 0;
    p_sys->pp_streams   = NULL;

    for( p_cfg = p_stream->p_cfg; p_cfg != NULL; p_cfg = p_cfg->p_next )
    {
        if( !strncmp( p_cfg->psz_name, "dst", strlen( "dst" ) ) )
        {
            sout_stream_t *s;

            msg_Dbg( p_stream, " * adding `%s'", p_cfg->psz_value );
            s = sout_stream_new( p_stream->p_sout, p_cfg->psz_value );

            TAB_APPEND( p_sys->i_nb_streams, p_sys->pp_streams, s );
        }
    }

    if( p_sys->i_nb_streams == 0 )
    {
        msg_Err( p_stream, "no destination given" );
        free( p_sys );

        return VLC_EGENERIC;
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    int i;

    msg_Dbg( p_stream, "closing a duplication" );
    for( i = 0; i < p_sys->i_nb_streams; i++ )
    {
        sout_stream_delete( p_sys->pp_streams[i] );
    }
    free( p_sys->pp_streams );

    free( p_sys );
}



static sout_stream_id_t * Add      ( sout_stream_t *p_stream, sout_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;
    int               i_stream;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->i_nb_ids = 0;
    id->pp_ids   = NULL;

    for( i_stream = 0; i_stream < p_sys->i_nb_streams; i_stream++ )
    {
        void *id_new;

        /* XXX not the same sout_stream_id_t definition ... */
        id_new = (void*)p_sys->pp_streams[i_stream]->pf_add( p_sys->pp_streams[i_stream], p_fmt );
        TAB_APPEND( id->i_nb_ids, id->pp_ids, id_new );
    }

    if( id->i_nb_ids <= 0 )
    {
        free( id );
        return NULL;
    }

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int               i_stream;

    for( i_stream = 0; i_stream < p_sys->i_nb_streams; i_stream++ )
    {
        if( id->pp_ids[i_stream] )
        {
            p_sys->pp_streams[i_stream]->pf_del( p_sys->pp_streams[i_stream],
                                                 id->pp_ids[i_stream] );
        }
    }

    free( id->pp_ids );
    free( id );
    return VLC_SUCCESS;
}

static int     Send     ( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int               i_stream;

    for( i_stream = 0; i_stream < p_sys->i_nb_streams - 1; i_stream++ )
    {
        sout_buffer_t *p_dup;

        if( id->pp_ids[i_stream] )
        {
            p_dup = sout_BufferDuplicate( p_stream->p_sout, p_buffer );

            p_sys->pp_streams[i_stream]->pf_send( p_sys->pp_streams[i_stream],
                                                  id->pp_ids[i_stream],
                                                  p_dup );
        }
    }

    i_stream = p_sys->i_nb_streams - 1;
    if( id->pp_ids[i_stream] )
    {
        p_sys->pp_streams[i_stream]->pf_send( p_sys->pp_streams[i_stream],
                                              id->pp_ids[i_stream],
                                              p_buffer);
    }

    return VLC_SUCCESS;
}

