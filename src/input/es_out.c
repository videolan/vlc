/*****************************************************************************
 * es_out.c: Es Out handler for input.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: es_out.c,v 1.12 2004/01/04 15:32:13 fenrir Exp $
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include "codecs.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct es_out_id_t
{
    int             i_channel;
    es_descriptor_t *p_es;
};

struct es_out_sys_t
{
    input_thread_t *p_input;
    vlc_bool_t      b_pcr_set;

    /* all es */
    int         i_id;

    int         i_es;
    es_out_id_t **es;

    /* mode gestion */
    vlc_bool_t  b_active;
    int         i_mode;

    /* es count */
    int         i_audio;
    int         i_video;
    int         i_sub;

    /* es to select */
    int         i_audio_last;
    int         i_sub_last;

    /* current main es */
    es_out_id_t *p_es_audio;
    es_out_id_t *p_es_video;
    es_out_id_t *p_es_sub;
};

static es_out_id_t *EsOutAdd    ( es_out_t *, es_format_t * );
static int          EsOutSend   ( es_out_t *, es_out_id_t *, block_t * );
static void         EsOutDel    ( es_out_t *, es_out_id_t * );
static int          EsOutControl( es_out_t *, int i_query, va_list );


/*****************************************************************************
 * input_EsOutNew:
 *****************************************************************************/
es_out_t *input_EsOutNew( input_thread_t *p_input )
{
    es_out_t     *out = malloc( sizeof( es_out_t ) );
    es_out_sys_t *p_sys = malloc( sizeof( es_out_sys_t ) );
    vlc_value_t  val;

    out->pf_add     = EsOutAdd;
    out->pf_send    = EsOutSend;
    out->pf_del     = EsOutDel;
    out->pf_control = EsOutControl;
    out->p_sys      = p_sys;

    p_sys->p_input = p_input;
    p_sys->b_pcr_set = VLC_FALSE;

    p_sys->b_active = VLC_FALSE;
    p_sys->i_mode   = ES_OUT_MODE_AUTO;

    p_sys->i_id    = 1;

    p_sys->i_es    = 0;
    p_sys->es      = NULL;

    p_sys->i_audio = 0;
    p_sys->i_video = 0;
    p_sys->i_sub   = 0;

    var_Get( p_input, "audio-channel", &val );
    p_sys->i_audio_last = val.i_int;

    var_Get( p_input, "spu-channel", &val );
    p_sys->i_sub_last = val.i_int;

    p_sys->p_es_audio = NULL;
    p_sys->p_es_video = NULL;
    p_sys->p_es_sub   = NULL;

    return out;
}

/*****************************************************************************
 * input_EsOutDelete:
 *****************************************************************************/
void input_EsOutDelete( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;
    int i;

    for( i = 0; i < p_sys->i_es; i++ )
    {
        free( p_sys->es[i] );
    }
    if( p_sys->es )
    {
        free( p_sys->es );
    }
    free( p_sys );
    free( out );
}

/*****************************************************************************
 * EsOutSelect: Select an ES given the current mode
 * XXX: you need to take a the lock before (stream.stream_lock)
 *****************************************************************************/
static void EsOutSelect( es_out_t *out, es_out_id_t *es, vlc_bool_t b_force )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;

    int i_cat = es->p_es->i_cat;

    if( !p_sys->b_active || ( !b_force && es->p_es->fmt.i_priority < 0 ) )
    {
        return;
    }

    if( p_sys->i_mode == ES_OUT_MODE_ALL || b_force )
    {
        input_SelectES( p_input, es->p_es );
    }
    else if( p_sys->i_mode == ES_OUT_MODE_AUTO )
    {
        int i_wanted  = -1;

        if( i_cat == AUDIO_ES )
        {
            if( p_sys->p_es_audio &&
                p_sys->p_es_audio->p_es->fmt.i_priority >=
                    es->p_es->fmt.i_priority )
            {
                return;
            }
            i_wanted  = p_sys->i_audio_last >= 0 ?
                            p_sys->i_audio_last : es->i_channel;
        }
        else if( i_cat == SPU_ES )
        {
            if( p_sys->p_es_sub &&
                p_sys->p_es_sub->p_es->fmt.i_priority >=
                    es->p_es->fmt.i_priority )
            {
                return;
            }
            i_wanted  = p_sys->i_sub_last;
        }
        else if( i_cat == VIDEO_ES )
        {
            i_wanted  = es->i_channel;
        }

        if( i_wanted == es->i_channel && es->p_es->p_dec == NULL )
        {
            input_SelectES( p_input, es->p_es );
        }
    }

    /* FIXME TODO handle priority here */
    if( es->p_es->p_dec )
    {
        if( i_cat == AUDIO_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_audio && p_sys->p_es_audio->p_es->p_dec )
            {
                input_UnselectES( p_input, p_sys->p_es_audio->p_es );
            }
            p_sys->p_es_audio = es;
        }
        else if( i_cat == SPU_ES )
        {
            if( p_sys->i_mode == ES_OUT_MODE_AUTO &&
                p_sys->p_es_sub && p_sys->p_es_sub->p_es->p_dec )
            {
                input_UnselectES( p_input, p_sys->p_es_sub->p_es );
            }
            p_sys->p_es_sub = es;
        }
        else if( i_cat == VIDEO_ES )
        {
            p_sys->p_es_video = es;
        }
    }
}

/*****************************************************************************
 * EsOutAdd:
 *****************************************************************************/
static es_out_id_t *EsOutAdd( es_out_t *out, es_format_t *fmt )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_id_t       *es = malloc( sizeof( es_out_id_t ) );
    pgrm_descriptor_t *p_prgm = NULL;
    char              psz_cat[sizeof( "Stream " ) + 10];
    input_info_category_t *p_cat;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( fmt->i_group >= 0 )
    {
        /* search program */
        p_prgm = input_FindProgram( p_input, fmt->i_group );

        if( p_prgm == NULL )
        {
            /* create it */
            p_prgm = input_AddProgram( p_input, fmt->i_group, 0 );

            /* Select the first by default */
            if( p_input->stream.p_selected_program == NULL )
            {
                p_input->stream.p_selected_program = p_prgm;
            }
        }
    }

    es->p_es = input_AddES( p_input,
                            p_prgm,
                            out->p_sys->i_id,
                            fmt->i_cat,
                            fmt->psz_language, 0 );
    es->p_es->i_stream_id = out->p_sys->i_id;
    es->p_es->i_fourcc = fmt->i_codec;

    switch( fmt->i_cat )
    {
        case AUDIO_ES:
        {
            WAVEFORMATEX *p_wf =
                malloc( sizeof( WAVEFORMATEX ) + fmt->i_extra);

            p_wf->wFormatTag        = WAVE_FORMAT_UNKNOWN;
            p_wf->nChannels         = fmt->audio.i_channels;
            p_wf->nSamplesPerSec    = fmt->audio.i_rate;
            p_wf->nAvgBytesPerSec   = fmt->i_bitrate / 8;
            p_wf->nBlockAlign       = fmt->audio.i_blockalign;
            p_wf->wBitsPerSample    = fmt->audio.i_bitspersample;
            p_wf->cbSize            = fmt->i_extra;
            if( fmt->i_extra > 0 )
            {
                memcpy( &p_wf[1], fmt->p_extra, fmt->i_extra );
            }
            es->p_es->p_waveformatex = p_wf;

            es->i_channel = p_sys->i_audio;
            break;
        }
        case VIDEO_ES:
        {
            BITMAPINFOHEADER *p_bih = malloc( sizeof( BITMAPINFOHEADER ) +
                                              fmt->i_extra );
            p_bih->biSize           = sizeof(BITMAPINFOHEADER) + fmt->i_extra;
            p_bih->biWidth          = fmt->video.i_width;
            p_bih->biHeight         = fmt->video.i_height;
            p_bih->biPlanes         = 1;
            p_bih->biBitCount       = 24;
            p_bih->biCompression    = fmt->i_codec;
            p_bih->biSizeImage      = fmt->video.i_width *
                                          fmt->video.i_height;
            p_bih->biXPelsPerMeter  = 0;
            p_bih->biYPelsPerMeter  = 0;
            p_bih->biClrUsed        = 0;
            p_bih->biClrImportant   = 0;

            if( fmt->i_extra > 0 )
            {
                memcpy( &p_bih[1], fmt->p_extra, fmt->i_extra );
            }
            es->p_es->p_bitmapinfoheader = p_bih;

            es->i_channel = p_sys->i_video;
            break;
        }
        case SPU_ES:
        {
            subtitle_data_t *p_sub = malloc( sizeof( subtitle_data_t ) );
            memset( p_sub, 0, sizeof( subtitle_data_t ) );
            if( fmt->i_extra > 0 )
            {
                p_sub->psz_header = malloc( fmt->i_extra  + 1 );
                memcpy( p_sub->psz_header, fmt->p_extra , fmt->i_extra );
                /* just to be sure */
                ((uint8_t*)fmt->p_extra)[fmt->i_extra] = '\0';
            }
            /* FIXME beuuuuuurk */
            es->p_es->p_demux_data = p_sub;

            es->i_channel = p_sys->i_sub;
            break;
        }

        default:
            es->i_channel = 0;
            break;
    }

    sprintf( psz_cat, _("Stream %d"), out->p_sys->i_id - 1 );
    if( ( p_cat = input_InfoCategory( p_input, psz_cat ) ) )
    {
        /* Add information */
        switch( fmt->i_cat )
        {
            case AUDIO_ES:
                if( fmt->psz_description )
                {
                    input_AddInfo( p_cat, _("Description"), "%s",
                                   fmt->psz_description );
                }
                input_AddInfo( p_cat, _("Codec"), "%.4s",
                               (char*)&fmt->i_codec );
                input_AddInfo( p_cat, _("Type"), _("Audio") );
                if( fmt->audio.i_channels > 0 )
                {
                    input_AddInfo( p_cat, _("Channels"), "%d",
                                   fmt->audio.i_channels );
                }
                if( fmt->psz_language )
                {
                    input_AddInfo( p_cat, _("Language"), "%s",
                                   fmt->psz_language );
                }
                if( fmt->audio.i_rate > 0 )
                {
                  input_AddInfo( p_cat, _("Sample rate"), _("%d Hz"),
                                   fmt->audio.i_rate );
                }
                if( fmt->i_bitrate > 0 )
                {
                  input_AddInfo( p_cat, _("Bitrate"), _("%d bps"),
                                   fmt->i_bitrate );
                }
                if( fmt->audio.i_bitspersample )
                {
                    input_AddInfo( p_cat, _("Bits per sample"), "%d",
                                   fmt->audio.i_bitspersample );
                }
                break;
            case VIDEO_ES:
                if( fmt->psz_description )
                {
                    input_AddInfo( p_cat, _("Description"), "%s",
                                   fmt->psz_description );
                }
                input_AddInfo( p_cat, _("Type"), _("Video") );
                input_AddInfo( p_cat, _("Codec"), "%.4s",
                               (char*)&fmt->i_codec );
                if( fmt->video.i_width > 0 && fmt->video.i_height > 0 )
                {
                    input_AddInfo( p_cat, _("Resolution"), "%dx%d",
                                   fmt->video.i_width, fmt->video.i_height );
                }
                if( fmt->video.i_visible_width > 0 &&
                    fmt->video.i_visible_height > 0 )
                {
                    input_AddInfo( p_cat, _("Display resolution"), "%dx%d",
                                   fmt->video.i_visible_width,
                                   fmt->video.i_visible_height);
                }
                break;
            case SPU_ES:
                input_AddInfo( p_cat, _("Type"), _("Subtitle") );
                input_AddInfo( p_cat, _("Codec"), "%.4s",
                               (char*)&fmt->i_codec );
                break;
            default:

                break;
        }
    }


    /* Apply mode
     * XXX change that when we do group too */
    if( 1 )
    {
        EsOutSelect( out, es, VLC_FALSE );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    es->p_es->fmt = *fmt;

    TAB_APPEND( out->p_sys->i_es, out->p_sys->es, es );
    p_sys->i_id++;  /* always incremented */
    switch( fmt->i_cat )
    {
        case AUDIO_ES:
            p_sys->i_audio++;
            break;
        case SPU_ES:
            p_sys->i_sub++;
            break;
        case VIDEO_ES:
            p_sys->i_video++;
            break;
    }

    return es;
}

/*****************************************************************************
 * EsOutSend:
 *****************************************************************************/
static int EsOutSend( es_out_t *out, es_out_id_t *es, block_t *p_block )
{
    es_out_sys_t *p_sys = out->p_sys;

    if( p_sys->b_pcr_set && p_sys->p_input->stream.p_selected_program )
    {
        input_thread_t *p_input = p_sys->p_input;

        if( p_block->i_dts > 0 )
        {
            p_block->i_dts =
                input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                                  p_block->i_dts * 9 / 100 );
        }
        if( p_block->i_pts > 0 )
        {
            p_block->i_pts =
                input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                                  p_block->i_pts * 9 / 100 );
        }
    }

    vlc_mutex_lock( &out->p_sys->p_input->stream.stream_lock );
    p_block->i_rate = out->p_sys->p_input->stream.control.i_rate;
    if( es->p_es->p_dec &&
        (es->p_es->i_cat!=AUDIO_ES || !p_sys->p_input->stream.control.b_mute) )
    {
        input_DecodeBlock( es->p_es->p_dec, p_block );
    }
    else
    {
        block_Release( p_block );
    }
    vlc_mutex_unlock( &out->p_sys->p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EsOutDel:
 *****************************************************************************/
static void EsOutDel( es_out_t *out, es_out_id_t *es )
{
    es_out_sys_t *p_sys = out->p_sys;

    TAB_REMOVE( p_sys->i_es, p_sys->es, es );

    switch( es->p_es->i_cat )
    {
        case AUDIO_ES:
            p_sys->i_audio--;
            break;
        case SPU_ES:
            p_sys->i_sub--;
            break;
        case VIDEO_ES:
            p_sys->i_video--;
            break;
    }

    /* We don't try to reselect */
    vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
    if( es->p_es->p_dec )
    {
        input_UnselectES( p_sys->p_input, es->p_es );
    }

    if( es->p_es->p_waveformatex )
    {
        free( es->p_es->p_waveformatex );
        es->p_es->p_waveformatex = NULL;
    }
    if( es->p_es->p_bitmapinfoheader )
    {
        free( es->p_es->p_bitmapinfoheader );
        es->p_es->p_bitmapinfoheader = NULL;
    }
    input_DelES( p_sys->p_input, es->p_es );

    if( p_sys->p_es_audio == es ) p_sys->p_es_audio = NULL;
    if( p_sys->p_es_video == es ) p_sys->p_es_video = NULL;
    if( p_sys->p_es_sub   == es ) p_sys->p_es_sub   = NULL;

    vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

    free( es );
}

/*****************************************************************************
 * EsOutControl:
 *****************************************************************************/
static int EsOutControl( es_out_t *out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = out->p_sys;
    vlc_bool_t  b, *pb;
    int         i, *pi;
    int         i_group;
    int64_t     i_pcr;

    es_out_id_t *es;

    switch( i_query )
    {
        case ES_OUT_SET_ES_STATE:
            vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            b = (vlc_bool_t) va_arg( args, vlc_bool_t );
            if( b && es->p_es->p_dec == NULL )
            {
                input_SelectES( p_sys->p_input, es->p_es );
                vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
                return es->p_es->p_dec ? VLC_SUCCESS : VLC_EGENERIC;
            }
            else if( !b && es->p_es->p_dec )
            {
                input_UnselectES( p_sys->p_input, es->p_es );
                vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
                return VLC_SUCCESS;
            }
            vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
            return VLC_SUCCESS;

        case ES_OUT_GET_ES_STATE:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            pb = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            *pb = es->p_es->p_dec ? VLC_TRUE : VLC_FALSE;
            return VLC_SUCCESS;

        case ES_OUT_SET_ACTIVE:
        {
            b = (vlc_bool_t) va_arg( args, vlc_bool_t );
            p_sys->b_active = b;

            if( b )
            {
                vlc_value_t val;
                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "intf-change", val );
            }
            return VLC_SUCCESS;
        }

        case ES_OUT_GET_ACTIVE:
            pb = (vlc_bool_t*) va_arg( args, vlc_bool_t * );
            *pb = p_sys->b_active;
            return VLC_SUCCESS;

        case ES_OUT_SET_MODE:
            i = (int) va_arg( args, int );
            if( i == ES_OUT_MODE_NONE || i == ES_OUT_MODE_ALL ||
                i == ES_OUT_MODE_AUTO )
            {
                vlc_value_t val;

                p_sys->i_mode = i;

                /* Reapply policy mode */
                vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    if( p_sys->es[i]->p_es->p_dec )
                    {
                        input_UnselectES( p_sys->p_input, p_sys->es[i]->p_es );
                    }
                }
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    EsOutSelect( out, p_sys->es[i], VLC_FALSE );
                }
                vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

                val.b_bool = VLC_TRUE;
                var_Set( p_sys->p_input, "intf-change", val );

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case ES_OUT_GET_MODE:
            pi = (int*) va_arg( args, int* );
            *pi = p_sys->i_mode;
            return VLC_SUCCESS;

        case ES_OUT_SET_ES:
            es = (es_out_id_t*) va_arg( args, es_out_id_t * );
            if( es == NULL )
            {
                for( i = 0; i < p_sys->i_es; i++ )
                {
                    vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
                    if( p_sys->es[i]->p_es->p_dec )
                    {
                        input_UnselectES( p_sys->p_input, p_sys->es[i]->p_es );
                    }
                    vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
                }
            }
            else
            {
                vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
                EsOutSelect( out, es, VLC_TRUE );
                vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
            }
            return VLC_SUCCESS;

        case ES_OUT_SET_PCR:
        {
            pgrm_descriptor_t *p_prgm = NULL;

            i_group = (int)va_arg( args, int );
            i_pcr   = (int64_t)va_arg( args, int64_t );

            /* search program */
            if( ( p_prgm = input_FindProgram( p_sys->p_input, i_group ) ) )
            {
                input_ClockManageRef( p_sys->p_input, p_prgm, i_pcr * 9 / 100);
            }
            p_sys->b_pcr_set = VLC_TRUE;
            return VLC_SUCCESS;
        }

        case ES_OUT_RESET_PCR:
            /* FIXME do it for all program */
            if( p_sys->p_input->stream.p_selected_program )
            {
                p_sys->p_input->stream.p_selected_program->i_synchro_state =
                    SYNCHRO_REINIT;
            }
            p_sys->b_pcr_set = VLC_TRUE;
            return VLC_SUCCESS;

        default:
            msg_Err( p_sys->p_input, "unknown query in es_out_Control" );
            return VLC_EGENERIC;
    }
}
