/*****************************************************************************
 * demux.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id$
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "input_internal.h"

/*****************************************************************************
 * demux2_New:
 *  if s is NULL then load a access_demux
 *****************************************************************************/
demux_t *__demux2_New( vlc_object_t *p_obj,
                       char *psz_access, char *psz_demux, char *psz_path,
                       stream_t *s, es_out_t *out, vlc_bool_t b_quick )
{
    demux_t *p_demux = vlc_object_create( p_obj, VLC_OBJECT_DEMUX );
    char *psz_module;

    if( p_demux == NULL )
    {
        return NULL;
    }

    /* Parse URL */
    p_demux->psz_access = strdup( psz_access );
    p_demux->psz_demux  = strdup( psz_demux );
    p_demux->psz_path   = strdup( psz_path );

    /* Take into account "demux" to be able to do :demux=dump */
    if( *p_demux->psz_demux == '\0' )
    {
        free( p_demux->psz_demux );
        p_demux->psz_demux = var_GetString( p_obj, "demux" );
    }

    if( !b_quick )
    {
        msg_Dbg( p_obj, "creating demux: access='%s' demux='%s' path='%s'",
                 p_demux->psz_access, p_demux->psz_demux, p_demux->psz_path );
    }

    p_demux->s          = s;
    p_demux->out        = out;

    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;
    p_demux->info.i_update = 0;
    p_demux->info.i_title  = 0;
    p_demux->info.i_seekpoint = 0;

    if( s )
        psz_module = p_demux->psz_demux;
    else
        psz_module = p_demux->psz_access;

    if( s && *psz_module == '\0' && strrchr( p_demux->psz_path, '.' ) )
    {
        /* XXX: add only file without any problem here and with strong detection.
         *  - no .mp3, .a52, ... (aac is added as it works only by file ext anyway
         *  - wav can't be added 'cause of a52 and dts in them as raw audio
         */
         static struct { char *ext; char *demux; } exttodemux[] =
         {
            { "aac",  "aac" },
            { "aiff", "aiff" },
            { "asf",  "asf" }, { "wmv",  "asf" }, { "wma",  "asf" },
            { "avi",  "avi" },
            { "au",   "au" },
            { "flac", "flac" },
            { "dv",   "dv" },
            { "m3u",  "m3u" },
            { "mkv",  "mkv" }, { "mka",  "mkv" }, { "mks",  "mkv" },
            { "mp4",  "mp4" }, { "m4a",  "mp4" }, { "mov",  "mp4" }, { "moov", "mp4" },
            { "mod",  "mod" }, { "xm",   "mod" },
            { "nsv",  "nsv" },
            { "ogg",  "ogg" }, { "ogm",  "ogg" },
            { "pva",  "pva" },
            { "rm",   "rm" },
            { NULL,  NULL },
        };
        /* Here, we don't mind if it does not work, it must be quick */
        static struct { char *ext; char *demux; } exttodemux_quick[] = 
        {
            { "mp3", "mpga" },
            { "ogg", "ogg" },
            { "wma", "asf" },
            { NULL, NULL }
        };

        char *psz_ext = strrchr( p_demux->psz_path, '.' ) + 1;
        int  i;

        if( !b_quick )
        {
            for( i = 0; exttodemux[i].ext != NULL; i++ )
            {
                if( !strcasecmp( psz_ext, exttodemux[i].ext ) )
                {
                    psz_module = exttodemux[i].demux;
                    break;
                }
            }
        }
        else
        {
            for( i = 0; exttodemux_quick[i].ext != NULL; i++ )
            {
                if( !strcasecmp( psz_ext, exttodemux_quick[i].ext ) )
                {
                    psz_module = exttodemux_quick[i].demux;
                    break;
                }
            }

        }
    }

    /* Before module_Need (for var_Create...) */
    vlc_object_attach( p_demux, p_obj );

    if( s )
    {
        p_demux->p_module =
            module_Need( p_demux, "demux2", psz_module,
                         !strcmp( psz_module, p_demux->psz_demux ) ?
                         VLC_TRUE : VLC_FALSE );
    }
    else
    {
        p_demux->p_module =
            module_Need( p_demux, "access_demux", psz_module,
                         !strcmp( psz_module, p_demux->psz_access ) ?
                         VLC_TRUE : VLC_FALSE );
    }

    if( p_demux->p_module == NULL )
    {
        vlc_object_detach( p_demux );
        free( p_demux->psz_path );
        free( p_demux->psz_demux );
        free( p_demux->psz_access );
        vlc_object_destroy( p_demux );
        return NULL;
    }

    return p_demux;
}

/*****************************************************************************
 * demux2_Delete:
 *****************************************************************************/
void demux2_Delete( demux_t *p_demux )
{
    module_Unneed( p_demux, p_demux->p_module );
    vlc_object_detach( p_demux );

    free( p_demux->psz_path );
    free( p_demux->psz_demux );
    free( p_demux->psz_access );

    vlc_object_destroy( p_demux );
}

/*****************************************************************************
 * demux2_vaControlHelper:
 *****************************************************************************/
int demux2_vaControlHelper( stream_t *s,
                            int64_t i_start, int64_t i_end,
                            int i_bitrate, int i_align,
                            int i_query, va_list args )
{
    int64_t i_tell;
    double  f, *pf;
    int64_t i64, *pi64;

    if( i_end < 0 )    i_end   = stream_Size( s );
    if( i_start < 0 )  i_start = 0;
    if( i_align <= 0 ) i_align = 1;
    i_tell = stream_Tell( s );

    switch( i_query )
    {
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( i_bitrate > 0 && i_end > i_start )
            {
                *pi64 = I64C(8000000) * (i_end - i_start) / i_bitrate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( i_bitrate > 0 && i_end > i_start )
            {
                *pi64 = I64C(8000000) * (i_tell - i_start) / i_bitrate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( i_start < i_end )
            {
                *pf = (double)( i_tell - i_start ) /
                      (double)( i_end  - i_start );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;


        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( i_start < i_end && f >= 0.0 && f <= 1.0 )
            {
                int64_t i_block = (f * ( i_end - i_start )) / i_align;

                if( stream_Seek( s, i_start + i_block * i_align ) )
                {
                    return VLC_EGENERIC;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( i_bitrate > 0 && i64 >= 0 )
            {
                int64_t i_block = i64 * i_bitrate / I64C(8000000) / i_align;
                if( stream_Seek( s, i_start + i_block * i_align ) )
                {
                    return VLC_EGENERIC;
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_GROUP:
            return VLC_EGENERIC;

        default:
            msg_Err( s, "unknown query in demux_vaControlDefault" );
            return VLC_EGENERIC;
    }
}

/****************************************************************************
 * stream_Demux*: create a demuxer for an outpout stream (allow demuxer chain)
 ****************************************************************************/
typedef struct
{
    /* Data buffer */
    block_fifo_t *p_fifo;
    block_t      *p_block;

    int64_t     i_pos;

    /* Demuxer */
    char        *psz_name;
    es_out_t    *out;
    demux_t     *p_demux;

} d_stream_sys_t;

static int DStreamRead   ( stream_t *, void *p_read, int i_read );
static int DStreamPeek   ( stream_t *, uint8_t **pp_peek, int i_peek );
static int DStreamControl( stream_t *, int i_query, va_list );
static int DStreamThread ( stream_t * );


stream_t *__stream_DemuxNew( vlc_object_t *p_obj, char *psz_demux,
                             es_out_t *out )
{
    /* We create a stream reader, and launch a thread */
    stream_t       *s;
    d_stream_sys_t *p_sys;

    if( psz_demux == NULL || *psz_demux == '\0' ) return NULL;

    s = vlc_object_create( p_obj, VLC_OBJECT_STREAM );
    s->pf_block  = NULL;
    s->pf_read   = DStreamRead;
    s->pf_peek   = DStreamPeek;
    s->pf_control= DStreamControl;

    s->p_sys = malloc( sizeof( d_stream_sys_t) );
    p_sys = (d_stream_sys_t*)s->p_sys;

    p_sys->i_pos = 0;
    p_sys->out = out;
    p_sys->p_demux = NULL;
    p_sys->p_block = NULL;
    p_sys->psz_name = strdup( psz_demux );

    /* decoder fifo */
    if( ( p_sys->p_fifo = block_FifoNew( s ) ) == NULL )
    {
        msg_Err( s, "out of memory" );
        vlc_object_destroy( s );
        free( p_sys );
        return NULL;
    }

    if( vlc_thread_create( s, "stream out", DStreamThread,
                           VLC_THREAD_PRIORITY_INPUT, VLC_FALSE ) )
    {
        vlc_object_destroy( s );
        free( p_sys );
        return NULL;
    }

    return s;
}

void stream_DemuxSend( stream_t *s, block_t *p_block )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    if( p_block ) block_FifoPut( p_sys->p_fifo, p_block );
}

void stream_DemuxDelete( stream_t *s )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    block_t *p_empty;

    s->b_die = VLC_TRUE;
    if( p_sys->p_demux ) p_sys->p_demux->b_die = VLC_TRUE;
    p_empty = block_New( s, 1 ); p_empty->i_buffer = 0;
    block_FifoPut( p_sys->p_fifo, p_empty );
    vlc_thread_join( s );

    if( p_sys->p_demux ) demux2_Delete( p_sys->p_demux );
    if( p_sys->p_block ) block_Release( p_sys->p_block );

    block_FifoRelease( p_sys->p_fifo );
    free( p_sys->psz_name );
    free( p_sys );

    vlc_object_destroy( s );
}


static int DStreamRead( stream_t *s, void *p_read, int i_read )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    uint8_t *p_out = p_read;
    int i_out = 0;

    //msg_Dbg( s, "DStreamRead: wanted %d bytes", i_read );

    while( !s->b_die && !s->b_error && i_read )
    {
        block_t *p_block = p_sys->p_block;
        int i_copy;

        if( !p_block )
        {
            p_block = block_FifoGet( p_sys->p_fifo );
            if( !p_block ) s->b_error = 1;
            p_sys->p_block = p_block;
        }

        if( p_block && i_read )
        {
            i_copy = __MIN( i_read, p_block->i_buffer );
            if( p_out && i_copy ) memcpy( p_out, p_block->p_buffer, i_copy );
            i_read -= i_copy;
            i_out += i_copy;
            p_block->i_buffer -= i_copy;
            p_block->p_buffer += i_copy;

            if( !p_block->i_buffer )
            {
                block_Release( p_block );
                p_sys->p_block = NULL;
            }
        }
    }

    p_sys->i_pos += i_out;
    return i_out;
}

static int DStreamPeek( stream_t *s, uint8_t **pp_peek, int i_peek )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    block_t **pp_block = &p_sys->p_block;
    int i_out = 0;
    *pp_peek = 0;

    //msg_Dbg( s, "DStreamPeek: wanted %d bytes", i_peek );

    while( !s->b_die && !s->b_error && i_peek )
    {
        int i_copy;

        if( !*pp_block )
        {
            *pp_block = block_FifoGet( p_sys->p_fifo );
            if( !*pp_block ) s->b_error = 1;
        }

        if( *pp_block && i_peek )
        {
            i_copy = __MIN( i_peek, (*pp_block)->i_buffer );
            i_peek -= i_copy;
            i_out += i_copy;

            if( i_peek ) pp_block = &(*pp_block)->p_next;
        }
    }

    if( p_sys->p_block )
    {
        p_sys->p_block = block_ChainGather( p_sys->p_block );
        *pp_peek = p_sys->p_block->p_buffer;
    }

    return i_out;
}

static int DStreamControl( stream_t *s, int i_query, va_list args )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    int64_t    *p_i64;
    vlc_bool_t *p_b;
    int        *p_int;

    switch( i_query )
    {
        case STREAM_GET_SIZE:
            p_i64 = (int64_t*) va_arg( args, int64_t * );
            *p_i64 = 0;
            return VLC_SUCCESS;

        case STREAM_CAN_SEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );
            *p_b = VLC_FALSE;
            return VLC_SUCCESS;

        case STREAM_CAN_FASTSEEK:
            p_b = (vlc_bool_t*) va_arg( args, vlc_bool_t * );
            *p_b = VLC_FALSE;
            return VLC_SUCCESS;

        case STREAM_GET_POSITION:
            p_i64 = (int64_t*) va_arg( args, int64_t * );
            *p_i64 = p_sys->i_pos;
            return VLC_SUCCESS;

        case STREAM_SET_POSITION:
        {
            int64_t i64 = (int64_t)va_arg( args, int64_t );
            int i_skip;
            if( i64 < p_sys->i_pos ) return VLC_EGENERIC;
            i_skip = i64 - p_sys->i_pos;

            while( i_skip > 0 )
            {
                int i_read = DStreamRead( s, NULL, i_skip );
                if( i_read <= 0 ) return VLC_EGENERIC;
                i_skip -= i_read;
            }
            return VLC_SUCCESS;
        }

        case STREAM_GET_MTU:
            p_int = (int*) va_arg( args, int * );
            *p_int = 0;
            return VLC_SUCCESS;

        case STREAM_CONTROL_ACCESS:
            return VLC_EGENERIC;

        default:
            msg_Err( s, "invalid DStreamControl query=0x%x", i_query );
            return VLC_EGENERIC;
    }
}

static int DStreamThread( stream_t *s )
{
    d_stream_sys_t *p_sys = (d_stream_sys_t*)s->p_sys;
    demux_t *p_demux;

    /* Create the demuxer */
    if( !(p_demux = demux2_New( s, "", p_sys->psz_name, "", s, p_sys->out,
                               VLC_FALSE )) )
    {
        return VLC_EGENERIC;
    }

    p_sys->p_demux = p_demux;

    /* Main loop */
    while( !s->b_die && !p_demux->b_die )
    {
        if( p_demux->pf_demux( p_demux ) <= 0 ) break;
    }

    p_demux->b_die = VLC_TRUE;
    return VLC_SUCCESS;
}
