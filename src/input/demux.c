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

#include "ninput.h"

int demux_vaControl( input_thread_t *p_input, int i_query, va_list args )
{
    if( p_input->pf_demux_control )
    {
        return p_input->pf_demux_control( p_input, i_query, args );
    }
    return VLC_EGENERIC;
}

int demux_Control( input_thread_t *p_input, int i_query, ...  )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_input, i_query, args );
    va_end( args );

    return i_result;
}

static void SeekOffset( input_thread_t *p_input, int64_t i_pos );

int demux_vaControlDefault( input_thread_t *p_input, int i_query,
                            va_list args )
{
    int     i_ret;
    double  f, *pf;
    int64_t i64, *pi64;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_input->stream.p_selected_area->i_size <= 0 )
            {
                *pf = 0.0;
            }
            else
            {
                *pf = (double)p_input->stream.p_selected_area->i_tell /
                      (double)p_input->stream.p_selected_area->i_size;
            }
            i_ret = VLC_SUCCESS;
            break;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( p_input->stream.b_seekable && p_input->pf_seek != NULL &&
                f >= 0.0 && f <= 1.0 )
            {
                SeekOffset( p_input, (int64_t)(f *
                            (double)p_input->stream.p_selected_area->i_size) );
                i_ret = VLC_SUCCESS;
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_input->stream.i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 *
                        ( p_input->stream.p_selected_area->i_tell / 50 ) /
                        p_input->stream.i_mux_rate;
                i_ret = VLC_SUCCESS;
            }
            else
            {
                *pi64 = 0;
                i_ret = VLC_EGENERIC;
            }
            break;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( p_input->stream.i_mux_rate > 0 &&
                p_input->stream.b_seekable &&
                p_input->pf_seek != NULL && i64 >= 0 )
            {
                SeekOffset( p_input, i64 * 50 *
                                     (int64_t)p_input->stream.i_mux_rate /
                                     (int64_t)1000000 );
                i_ret = VLC_SUCCESS;
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_input->stream.i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 *
                        ( p_input->stream.p_selected_area->i_size / 50 ) /
                        p_input->stream.i_mux_rate;
                i_ret = VLC_SUCCESS;
            }
            else
            {
                *pi64 = 0;
                i_ret = VLC_EGENERIC;
            }
            break;
        case DEMUX_GET_FPS:
            i_ret = VLC_EGENERIC;
            break;
        case DEMUX_GET_META:
            i_ret = VLC_EGENERIC;
            break;

        default:
            msg_Err( p_input, "unknown query in demux_vaControlDefault" );
            i_ret = VLC_EGENERIC;
            break;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_ret;
}

static void SeekOffset( input_thread_t *p_input, int64_t i_pos )
{
    /* Reinitialize buffer manager. */
    input_AccessReinit( p_input );

    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_input->pf_seek( p_input, i_pos );
    vlc_mutex_lock( &p_input->stream.stream_lock );
}


/*****************************************************************************
 * demux2_New:
 *****************************************************************************/
demux_t *__demux2_New( vlc_object_t *p_obj,
                       char *psz_mrl, stream_t *s, es_out_t *out )
{
    demux_t *p_demux = vlc_object_create( p_obj, VLC_OBJECT_DEMUX );

    char    *psz_dup = strdup( psz_mrl ? psz_mrl : "" );
    char    *psz = strchr( psz_dup, ':' );
    char    *psz_module;

    if( p_demux == NULL )
    {
        free( psz_dup );
        return NULL;
    }

    /* Parse URL */
    p_demux->psz_access = NULL;
    p_demux->psz_demux  = NULL;
    p_demux->psz_path   = NULL;

    if( psz )
    {
        *psz++ = '\0';

        if( psz[0] == '/' && psz[1] == '/' )
        {
            psz += 2;
        }
        p_demux->psz_path = strdup( psz );

        psz = strchr( psz_dup, '/' );
        if( psz )
        {
            *psz++ = '\0';
            p_demux->psz_demux  = strdup( psz );
        }
        p_demux->psz_access = strdup( psz_dup );
    }
    else
    {
        p_demux->psz_path = strdup( psz_mrl );
    }
    free( psz_dup );


    if( p_demux->psz_access == NULL )
    {
        p_demux->psz_access = strdup( "" );
    }
    if( p_demux->psz_demux == NULL )
    {
        p_demux->psz_demux = strdup( "" );
    }
    if( p_demux->psz_path == NULL )
    {
        p_demux->psz_path = strdup( "" );
    }
    msg_Dbg( p_obj, "demux2_New: '%s' -> access='%s' demux='%s' path='%s'",
             psz_mrl,
             p_demux->psz_access, p_demux->psz_demux, p_demux->psz_path );

    p_demux->s          = s;
    p_demux->out        = out;

    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;
    p_demux->info.i_update = 0;
    p_demux->info.i_title  = 0;
    p_demux->info.i_seekpoint = 0;

    psz_module = p_demux->psz_demux;
    if( *psz_module == '\0' && strrchr( p_demux->psz_path, '.' ) )
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
            { "",  "" },
        };

        char *psz_ext = strrchr( p_demux->psz_path, '.' ) + 1;
        int  i;

        for( i = 0; exttodemux[i].ext != NULL; i++ )
        {
            if( !strcasecmp( psz_ext, exttodemux[i].ext ) )
            {
                psz_module = exttodemux[i].demux;
                break;
            }
        }
    }

    /* Before module_Need (for var_Create...) */
    vlc_object_attach( p_demux, p_obj );

    p_demux->p_module =
        module_Need( p_demux, "demux2", psz_module,
                     !strcmp( psz_module, p_demux->psz_demux ) ? VLC_TRUE : VLC_FALSE );

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
            return VLC_EGENERIC;

        default:
            msg_Err( s, "unknown query in demux_vaControlDefault" );
            return VLC_EGENERIC;
    }
}

