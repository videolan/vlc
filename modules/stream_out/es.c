/*****************************************************************************
 * es.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: es.c,v 1.3 2003/07/05 21:31:02 alexis Exp $
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
#include <vlc/input.h>
#include <vlc/sout.h>

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }
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
    set_description( _("ES stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "es" );
    add_shortcut( "es" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    int  i_count_audio;
    int  i_count_video;
    int  i_count;

    char *psz_mux;
    char *psz_mux_audio;
    char *psz_mux_video;

    char *psz_access;
    char *psz_access_audio;
    char *psz_access_video;

    char *psz_url;
    char *psz_url_audio;
    char *psz_url_video;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t   *p_sys;

    /* p_sout->i_preheader = __MAX( p_sout->i_preheader, p_mux->i_preheader ); */

    p_sys                   = malloc( sizeof( sout_stream_sys_t ) );

    p_sys->i_count          = 0;
    p_sys->i_count_audio    = 0;
    p_sys->i_count_video    = 0;

    p_sys->psz_access       = sout_cfg_find_value( p_stream->p_cfg, "access" );
    p_sys->psz_access_audio = sout_cfg_find_value( p_stream->p_cfg, "access_audio" );
    p_sys->psz_access_video = sout_cfg_find_value( p_stream->p_cfg, "access_video" );


    p_sys->psz_mux          = sout_cfg_find_value( p_stream->p_cfg, "mux" );
    p_sys->psz_mux_audio    = sout_cfg_find_value( p_stream->p_cfg, "mux_audio" );
    p_sys->psz_mux_video    = sout_cfg_find_value( p_stream->p_cfg, "mux_video" );

    p_sys->psz_url         = sout_cfg_find_value( p_stream->p_cfg, "url" );
    p_sys->psz_url_audio   = sout_cfg_find_value( p_stream->p_cfg, "url_audio" );
    p_sys->psz_url_video   = sout_cfg_find_value( p_stream->p_cfg, "url_video" );

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

    free( p_sys );
}

struct sout_stream_id_t
{
    sout_input_t *p_input;
    sout_mux_t   *p_mux;
};

static char * es_print_url( char *psz_fmt, vlc_fourcc_t i_fourcc, int i_count, char *psz_access, char *psz_mux )
{
    char *psz_url, *p;

    if( psz_fmt == NULL || !*psz_fmt )
    {
        psz_fmt = "stream-%n-%c.%m";
    }

    p = psz_url = malloc( 4096 );
    memset( p, 0, 4096 );
    for( ;; )
    {
        if( *psz_fmt == '\0' )
        {
            *p = '\0';
            break;
        }

        if( *psz_fmt != '%' )
        {
            *p++ = *psz_fmt++;
        }
        else
        {
            if( psz_fmt[1] == 'n' )
            {
                p += sprintf( p, "%d", i_count );
            }
            else if( psz_fmt[1] == 'c' )
            {
                p += sprintf( p, "%4.4s", (char*)&i_fourcc );
            }
            else if( psz_fmt[1] == 'm' )
            {
                p += sprintf( p, "%s", psz_mux );
            }
            else if( psz_fmt[1] == 'a' )
            {
                p += sprintf( p, "%s", psz_access );
            }
            else if( psz_fmt[1] != '\0' )
            {
                p += sprintf( p, "%c%c", psz_fmt[0], psz_fmt[1] );
            }
            else
            {
                p += sprintf( p, "%c", psz_fmt[0] );
                *p++ = '\0';
                break;
            }
            psz_fmt += 2;
        }
    }

    return( psz_url );
}

static sout_stream_id_t * Add      ( sout_stream_t *p_stream, sout_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_instance_t   *p_sout = p_stream->p_sout;
    sout_stream_id_t  *id;

    char              *psz_access;
    char              *psz_mux;
    char              *psz_url;

    sout_access_out_t *p_access;
    sout_mux_t        *p_mux;

    /* *** get access name *** */
    if( p_fmt->i_cat == AUDIO_ES && p_sys->psz_access_audio )
    {
        psz_access = p_sys->psz_access_audio;
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->psz_access_video )
    {
        psz_access = p_sys->psz_access_video;
    }
    else
    {
        psz_access = p_sys->psz_access;
    }

    /* *** get mux name *** */
    if( p_fmt->i_cat == AUDIO_ES && p_sys->psz_mux_audio )
    {
        psz_mux = p_sys->psz_mux_audio;
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->psz_mux_video )
    {
        psz_mux = p_sys->psz_mux_video;
    }
    else
    {
        psz_mux = p_sys->psz_mux;
    }

    /* *** get url (%d expanded as a codec count, %c expanded as codec fcc ) *** */
    if( p_fmt->i_cat == AUDIO_ES && p_sys->psz_url_audio )
    {
        psz_url = es_print_url( p_sys->psz_url_audio, p_fmt->i_fourcc, p_sys->i_count_audio, psz_access, psz_mux );
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->psz_url_video )
    {
        psz_url = es_print_url( p_sys->psz_url_video, p_fmt->i_fourcc, p_sys->i_count_video, psz_access, psz_mux );
    }
    else
    {
        int i_count;
        if( p_fmt->i_cat == VIDEO_ES )
        {
            i_count = p_sys->i_count_video;
        }
        else if( p_fmt->i_cat == AUDIO_ES )
        {
            i_count = p_sys->i_count_audio;
        }
        else
        {
            i_count = p_sys->i_count;
        }

        psz_url = es_print_url( p_sys->psz_url, p_fmt->i_fourcc, i_count, psz_access, psz_mux );
    }

    p_sys->i_count++;
    if( p_fmt->i_cat == VIDEO_ES )
    {
        p_sys->i_count_video++;
    }
    else if( p_fmt->i_cat == AUDIO_ES )
    {
        p_sys->i_count_audio++;
    }
    msg_Dbg( p_stream, "creating `%s/%s://%s'",
             psz_access, psz_mux, psz_url );

    /* *** find and open appropriate access module *** */
    p_access = sout_AccessOutNew( p_sout, psz_access, psz_url );
    if( p_access == NULL )
    {
        msg_Err( p_stream, "no suitable sout access module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );
        return( NULL );
    }

    /* *** find and open appropriate mux module *** */
    p_mux = sout_MuxNew( p_sout, psz_mux, p_access );
    if( p_mux == NULL )
    {
        msg_Err( p_stream, "no suitable sout mux module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );
        sout_AccessOutDelete( p_access );
        return( NULL );
    }

    /* XXX beurk */
    p_sout->i_preheader = __MAX( p_sout->i_preheader, p_mux->i_preheader );

    id = malloc( sizeof( sout_stream_id_t ) );
    id->p_mux = p_mux;
    id->p_input = sout_MuxAddStream( p_mux, p_fmt );

    if( id->p_input == NULL )
    {
        free( id );

        sout_MuxDelete( p_mux );
        sout_AccessOutDelete( p_access );
        free( id );
        return NULL;
    }

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_access_out_t *p_access = id->p_mux->p_access;

    sout_MuxDeleteStream( id->p_mux, id->p_input );
    sout_AccessOutDelete( p_access );

    free( id );
    return VLC_SUCCESS;
}

static int     Send     ( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *p_buffer )
{
    sout_MuxSendBuffer( id->p_mux, id->p_input, p_buffer );

    return VLC_SUCCESS;
}

