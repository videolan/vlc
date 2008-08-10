/*****************************************************************************
 * subtitle_asa.c: Demux for subtitle text files using the asa engine.
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Authors: David Lamparter <equinox at videolan dot org>
 *
 * Originated from subtitle.c
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>


#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#include <ctype.h>

#include <vlc_demux.h>
#include <vlc_charset.h>

#include "asademux.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

#define SUB_DELAY_LONGTEXT \
    N_("Apply a delay to all subtitles (in 1/10s, eg 100 means 10s).")
#define SUB_FPS_LONGTEXT \
    N_("Override the normal frames per second settings. " \
    "This will only affect frame-based subtitle formats without a fixed value.")
#define SUB_TYPE_LONGTEXT \
    N_("Force the subtiles format. Use \"auto\", the set of supported values varies.")

vlc_module_begin();
    set_shortname( N_("Subtitles (asa demuxer)"));
    set_description( N_("Text subtitles parser") );
    set_capability( "demux", 50 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    add_float( "sub-fps", 0.0, NULL,
               N_("Frames per second"),
               SUB_FPS_LONGTEXT, true );
    add_integer( "sub-delay", 0, NULL,
               N_("Subtitles delay"),
               SUB_DELAY_LONGTEXT, true );
    add_string( "sub-type", "auto", NULL, N_("Subtitles format"),
                SUB_TYPE_LONGTEXT, true );
    set_callbacks( Open, Close );

    add_shortcut( "asademux" );
vlc_module_end();

/*****************************************************************************
 * Prototypes:
 *****************************************************************************/
typedef struct
{
    int64_t i_start;
    int64_t i_stop;

    char    *psz_text;
} subtitle_t;


struct demux_sys_t
{
    int         i_type;
    es_out_id_t *es;

    int64_t     i_next_demux_date;
    int64_t     i_microsecperframe;

    char        *psz_header;
    int         i_subtitle;
    int         i_subtitles;
    int         i_subs_alloc;
    subtitle_t  *subtitle;

    int64_t     i_length;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

static void Fix( demux_t * );

static int ProcessLine( demux_t *, void *, int64_t, int64_t,
                         const char *, size_t );

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys;
    es_format_t    fmt;
    input_thread_t *p_input;
    float          f_fps;
    char           *psz_type;
    int64_t        i_ssize;
    void           *p_data;
    struct asa_import_detect *p_detect = NULL;

    if( strcmp( p_demux->psz_demux, "asademux" ) )
    {
        return VLC_EGENERIC;
    }

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys  )
        return VLC_ENOMEM;
    p_sys->psz_header         = NULL;
    p_sys->i_subtitle         = 0;
    p_sys->i_subtitles        = 0;
    p_sys->i_subs_alloc       = 0;
    p_sys->subtitle           = NULL;
    p_sys->i_microsecperframe = 40000;

    /* Get the FPS */
    p_input = (input_thread_t *)vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT );
    if( p_input )
    {
        f_fps = var_GetFloat( p_input, "sub-original-fps" );
        if( f_fps >= 1.0 )
            p_sys->i_microsecperframe = (int64_t)( (float)1000000 / f_fps );

        msg_Dbg( p_demux, "Movie fps: %f", f_fps );
        vlc_object_release( p_input );
    }

    /* Check for override of the fps */
    f_fps = var_CreateGetFloat( p_demux, "sub-fps" );
    if( f_fps >= 1.0 )
    {
        p_sys->i_microsecperframe = (int64_t)( (float)1000000 / f_fps );
        msg_Dbg( p_demux, "Override subtitle fps %f", f_fps );
    }

    /* Get or probe the type */
    psz_type = var_CreateGetString( p_demux, "sub-type" );
    if( *psz_type )
    {
        for( p_detect = asa_det_first; p_detect; p_detect = p_detect->next )
        {
            if( !strcmp( p_detect->name, psz_type ) )
            {
                break;
            }
        }
        if( !p_detect )
        {
            msg_Warn( p_demux, "unknown sub-type \"%s\"", psz_type );
        }
    }
    free( psz_type );

    /* Probe if unknown type */
    if( !p_detect )
    {
        int i_size;
        const uint8_t *p;

        msg_Dbg( p_demux, "autodetecting subtitle format" );
        i_size = stream_Peek( p_demux->s, &p, 4096 );

        if( i_size <= 0)
        {
            msg_Warn( p_demux, "cannot process subtitles (no data?)" );
            return VLC_EGENERIC;
        }
        p_detect = asa_imports_detect( p, i_size );

    }
    if( !p_detect )
    {
        msg_Err( p_demux, "failed to recognize subtitle type" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if( !p_detect->fmt )
    {
        msg_Err( p_demux, "detected %s subtitle format, no asa support", p_detect->name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "detected %s subtitle format", p_detect->name );

    stream_Control( p_demux->s, STREAM_GET_SIZE, &i_ssize );
    p_data = malloc( i_ssize );
    if( !p_data )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    if( stream_Read( p_demux->s, &p_data, i_ssize ) != i_ssize )
    {
        msg_Err( p_demux, "subtitle stream read error" );
        free( p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }
    asa_import( p_demux, p_data, i_ssize, p_sys->i_microsecperframe, p_detect,
                ProcessLine, NULL );
    free( p_data );

    msg_Dbg(p_demux, "loaded %d subtitles", p_sys->i_subtitles );

    /* Fix subtitle (order and time) *** */
    Fix( p_demux );
    p_sys->i_subtitle = 0;
    p_sys->i_length = 0;
    if( p_sys->i_subtitles > 0 )
    {
        p_sys->i_length = p_sys->subtitle[p_sys->i_subtitles-1].i_stop;
        /* +1 to avoid 0 */
        if( p_sys->i_length <= 0 )
            p_sys->i_length = p_sys->subtitle[p_sys->i_subtitles-1].i_start+1;
    }

    /* *** add subtitle ES *** */
    if( p_detect->fmt->target == ASAI_TARGET_SSA )
    {
        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','s','a',' ' ) );
    }
    else
    {
        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','u','b','t' ) );
    }
    p_sys->es = es_out_Add( p_demux->out, &fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ProcessLine: Callback for asa_import, fed one line
 * (note: return values are not kept. nonzero signals abort to asa_import)
 *****************************************************************************/
static int ProcessLine( demux_t *p_demux, void *p_arg,
                         int64_t i_start, int64_t i_stop,
                         const char *p_buffer, size_t i_buffer_length )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    subtitle_t *p_subtitle;
    char *psz_text;

    VLC_UNUSED(p_arg);

    if( p_sys->i_subtitles >= p_sys->i_subs_alloc )
    {
        p_sys->i_subs_alloc += 500;
        if( !( p_sys->subtitle = realloc( p_sys->subtitle, sizeof(subtitle_t)
                                          * p_sys->i_subs_alloc ) ) )
        {
            return VLC_ENOMEM;
        }
    }
    p_subtitle = &p_sys->subtitle[p_sys->i_subtitles];

    psz_text = malloc( i_buffer_length + 1 );
    if( !psz_text )
        return VLC_ENOMEM;
    memcpy( psz_text, p_buffer, i_buffer_length );
    psz_text[i_buffer_length] = '\0';

    p_subtitle->i_start = i_start;
    p_subtitle->i_stop  = i_stop;
    p_subtitle->psz_text = psz_text;

    p_sys->i_subtitles++;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: Close subtitle demux
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    for( i = 0; i < p_sys->i_subtitles; i++ )
        free( p_sys->subtitle[i].psz_text );
    free( p_sys->subtitle );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64, i64;
    double *pf, f;

    switch( i_query )
    {
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_subtitle < p_sys->i_subtitles )
            {
                *pi64 = p_sys->subtitle[p_sys->i_subtitle].i_start;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            p_sys->i_subtitle = 0;
            while( p_sys->i_subtitle < p_sys->i_subtitles &&
                   p_sys->subtitle[p_sys->i_subtitle].i_start < i64 )
            {
                p_sys->i_subtitle++;
            }

            if( p_sys->i_subtitle >= p_sys->i_subtitles )
                return VLC_EGENERIC;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->i_subtitle >= p_sys->i_subtitles )
            {
                *pf = 1.0;
            }
            else if( p_sys->i_subtitles > 0 )
            {
                *pf = (double)p_sys->subtitle[p_sys->i_subtitle].i_start /
                      (double)p_sys->i_length;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            i64 = f * p_sys->i_length;

            p_sys->i_subtitle = 0;
            while( p_sys->i_subtitle < p_sys->i_subtitles &&
                   p_sys->subtitle[p_sys->i_subtitle].i_start < i64 )
            {
                p_sys->i_subtitle++;
            }
            if( p_sys->i_subtitle >= p_sys->i_subtitles )
                return VLC_EGENERIC;
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->i_next_demux_date = (int64_t)va_arg( args, int64_t );
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_GET_TITLE_INFO:
            return VLC_EGENERIC;

        default:
            msg_Err( p_demux, "unknown query in subtitle control" );
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux: Send subtitle to decoder
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_maxdate;

    if( p_sys->i_subtitle >= p_sys->i_subtitles )
        return 0;

    i_maxdate = p_sys->i_next_demux_date - var_GetTime( p_demux->p_parent, "spu-delay" );;
    if( i_maxdate <= 0 && p_sys->i_subtitle < p_sys->i_subtitles )
    {
        /* Should not happen */
        i_maxdate = p_sys->subtitle[p_sys->i_subtitle].i_start + 1;
    }

    while( p_sys->i_subtitle < p_sys->i_subtitles &&
           p_sys->subtitle[p_sys->i_subtitle].i_start < i_maxdate )
    {
        block_t *p_block;
        int i_len = strlen( p_sys->subtitle[p_sys->i_subtitle].psz_text ) + 1;

        if( i_len <= 1 )
        {
            /* empty subtitle */
            p_sys->i_subtitle++;
            continue;
        }

        if( ( p_block = block_New( p_demux, i_len ) ) == NULL )
        {
            p_sys->i_subtitle++;
            continue;
        }

        if( p_sys->subtitle[p_sys->i_subtitle].i_start < 0 )
        {
            p_sys->i_subtitle++;
            continue;
        }

        p_block->i_pts = p_sys->subtitle[p_sys->i_subtitle].i_start;
        p_block->i_dts = p_block->i_pts;
        if( p_sys->subtitle[p_sys->i_subtitle].i_stop > 0 )
        {
            p_block->i_length =
                p_sys->subtitle[p_sys->i_subtitle].i_stop - p_block->i_pts;
        }

        memcpy( p_block->p_buffer,
                p_sys->subtitle[p_sys->i_subtitle].psz_text, i_len );
        if( p_block->i_pts > 0 )
        {
            es_out_Send( p_demux->out, p_sys->es, p_block );
        }
        else
        {
            block_Release( p_block );
        }
        p_sys->i_subtitle++;
    }

    /* */
    p_sys->i_next_demux_date = 0;

    return 1;
}

/*****************************************************************************
 * Fix: fix time stamp and order of subtitle
 *****************************************************************************/
static void Fix( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool b_done;
    int     i_index;

    /* *** fix order (to be sure...) *** */
    /* We suppose that there are near in order and this durty bubble sort
     * wont take too much time
     */
    do
    {
        b_done = true;
        for( i_index = 1; i_index < p_sys->i_subtitles; i_index++ )
        {
            if( p_sys->subtitle[i_index].i_start <
                    p_sys->subtitle[i_index - 1].i_start )
            {
                subtitle_t sub_xch;
                memcpy( &sub_xch,
                        p_sys->subtitle + i_index - 1,
                        sizeof( subtitle_t ) );
                memcpy( p_sys->subtitle + i_index - 1,
                        p_sys->subtitle + i_index,
                        sizeof( subtitle_t ) );
                memcpy( p_sys->subtitle + i_index,
                        &sub_xch,
                        sizeof( subtitle_t ) );
                b_done = false;
            }
        }
    } while( !b_done );
}

