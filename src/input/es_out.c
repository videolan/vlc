/*****************************************************************************
 * es_out.c: Es Out handler for input.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: es_out.c,v 1.1 2003/11/24 20:50:45 fenrir Exp $
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
struct es_out_sys_t
{
    input_thread_t *p_input;

    int         i_id;
    es_out_id_t **id;

    vlc_bool_t  i_audio;
    vlc_bool_t  i_video;
};

struct es_out_id_t
{
    es_descriptor_t *p_es;
};

static es_out_id_t *EsOutAdd    ( es_out_t *, es_format_t * );
static int          EsOutSend   ( es_out_t *, es_out_id_t *, block_t * );
static int          EsOutSendPES( es_out_t *, es_out_id_t *, pes_packet_t * );
static void         EsOutDel    ( es_out_t *, es_out_id_t * );
static int          EsOutControl( es_out_t *, int i_query, va_list );


/*****************************************************************************
 * input_EsOutNew:
 *****************************************************************************/
es_out_t *input_EsOutNew( input_thread_t *p_input )
{
    es_out_t *out = malloc( sizeof( es_out_t ) );

    out->pf_add     = EsOutAdd;
    out->pf_send    = EsOutSend;
    out->pf_send_pes= EsOutSendPES;
    out->pf_del     = EsOutDel;
    out->pf_control = EsOutControl;

    out->p_sys = malloc( sizeof( es_out_sys_t ) );
    out->p_sys->p_input = p_input;
    out->p_sys->i_id    = 0;
    out->p_sys->id      = NULL;
    out->p_sys->i_audio = -1;
    out->p_sys->i_video = -1;
    return out;
}

/*****************************************************************************
 * input_EsOutDelete:
 *****************************************************************************/
void input_EsOutDelete( es_out_t *out )
{
    es_out_sys_t *p_sys = out->p_sys;
    int i;

    for( i = 0; i < p_sys->i_id; i++ )
    {
        free( p_sys->id[i] );
    }
    if( p_sys->id )
    {
        free( p_sys->id );
    }
    free( p_sys );
    free( out );
}

/*****************************************************************************
 * EsOutAdd:
 *****************************************************************************/
static es_out_id_t *EsOutAdd( es_out_t *out, es_format_t *fmt )
{
    es_out_sys_t      *p_sys = out->p_sys;
    input_thread_t    *p_input = p_sys->p_input;
    es_out_id_t       *id = malloc( sizeof( es_out_id_t ) );
    pgrm_descriptor_t *p_prgm = NULL;
    char              psz_cat[strlen( "Stream " ) + 10];
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

    id->p_es = input_AddES( p_input,
                            p_prgm,
                            1 + out->p_sys->i_id,
                            fmt->i_cat,
                            fmt->psz_description, 0 );
    id->p_es->i_stream_id = 1 + out->p_sys->i_id;
    id->p_es->i_fourcc = fmt->i_codec;

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
            id->p_es->p_waveformatex = p_wf;
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
            id->p_es->p_bitmapinfoheader = p_bih;
            break;
        }
        case SPU_ES:
        {
            subtitle_data_t *p_sub = malloc( sizeof( subtitle_data_t ) );
            memset( p_sub, 0, sizeof( subtitle_data_t ) );
            if( fmt->i_extra > 0 )
            {
                p_sub->psz_header = malloc( fmt->i_extra  );
                memcpy( p_sub->psz_header, fmt->p_extra , fmt->i_extra );
            }
            /* FIXME beuuuuuurk */
            id->p_es->p_demux_data = p_sub;
            break;
        }
        default:
            break;
    }

    if( fmt->i_cat == AUDIO_ES && fmt->i_priority > out->p_sys->i_audio )
    {
        if( out->p_sys->i_audio >= 0 )
        {
            msg_Err( p_input, "FIXME unselect es in es_out_Add" );
        }
        input_SelectES( p_input, id->p_es );
        if( id->p_es->p_dec )
        {
            out->p_sys->i_audio = fmt->i_priority;
        }
    }
    else if( fmt->i_cat == VIDEO_ES && fmt->i_priority > out->p_sys->i_video )
    {
        if( out->p_sys->i_video >= 0 )
        {
            msg_Err( p_input, "FIXME unselect es in es_out_Add" );
        }
        input_SelectES( p_input, id->p_es );
        if( id->p_es->p_dec )
        {
            out->p_sys->i_video = fmt->i_priority;
        }
    }

    sprintf( psz_cat, _("Stream %d"), out->p_sys->i_id );
    if( ( p_cat = input_InfoCategory( p_input, psz_cat ) ) )
    {
        /* Add information */
        switch( fmt->i_cat )
        {
            case AUDIO_ES:
                input_AddInfo( p_cat, _("Type"), _("Audio") );
                input_AddInfo( p_cat, _("Codec"), "%.4s",
                               (char*)&fmt->i_codec );
                if( fmt->audio.i_channels > 0 )
                {
                    input_AddInfo( p_cat, _("Channels"), "%d",
                                   fmt->audio.i_channels );
                }
                if( fmt->audio.i_rate > 0 )
                {
                    input_AddInfo( p_cat, _("Sample Rate"), "%d",
                                   fmt->audio.i_rate );
                }
                if( fmt->i_bitrate > 0 )
                {
                    input_AddInfo( p_cat, _("Bitrate"), "%d",
                                   fmt->i_bitrate );
                }
                if( fmt->audio.i_bitspersample )
                {
                    input_AddInfo( p_cat, _("Bits Per Sample"), "%d",
                                   fmt->audio.i_bitspersample );
                }
                break;
            case VIDEO_ES:
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
                    input_AddInfo( p_cat, _("Display Resolution"), "%dx%d",
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
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    id->p_es->fmt = *fmt;

    TAB_APPEND( out->p_sys->i_id, out->p_sys->id, id );
    return id;
}

/*****************************************************************************
 * EsOutSend:
 *****************************************************************************/
static int EsOutSend( es_out_t *out, es_out_id_t *id, block_t *p_block )
{
    if( id->p_es->p_dec )
    {
        input_DecodeBlock( id->p_es->p_dec, p_block );
    }
    else
    {
        block_Release( p_block );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * EsOutSendPES:
 *****************************************************************************/
static int EsOutSendPES( es_out_t *out, es_out_id_t *id, pes_packet_t *p_pes )
{
    if( id->p_es->p_dec )
    {
        input_DecodePES( id->p_es->p_dec, p_pes );
    }
    else
    {
        input_DeletePES( out->p_sys->p_input->p_method_data, p_pes );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * EsOutDel:
 *****************************************************************************/
static void EsOutDel( es_out_t *out, es_out_id_t *id )
{
    es_out_sys_t *p_sys = out->p_sys;

    TAB_REMOVE( p_sys->i_id, p_sys->id, id );

    vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
    if( id->p_es->p_dec )
    {
        input_UnselectES( p_sys->p_input, id->p_es );
    }
    if( id->p_es->p_waveformatex )
    {
        free( id->p_es->p_waveformatex );
        id->p_es->p_waveformatex = NULL;
    }
    if( id->p_es->p_bitmapinfoheader )
    {
        free( id->p_es->p_bitmapinfoheader );
        id->p_es->p_bitmapinfoheader = NULL;
    }
    input_DelES( p_sys->p_input, id->p_es );
    vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

    free( id );
}

/*****************************************************************************
 * EsOutControl:
 *****************************************************************************/
static int EsOutControl( es_out_t *out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = out->p_sys;
    vlc_bool_t  b, *pb;
    es_out_id_t *id;
    switch( i_query )
    {
        case ES_OUT_SET_SELECT:
            vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
            id = (es_out_id_t*) va_arg( args, es_out_id_t * );
            b = (vlc_bool_t) va_arg( args, vlc_bool_t );
            if( b && id->p_es->p_dec == NULL )
            {
                input_SelectES( p_sys->p_input, id->p_es );
                vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
                return id->p_es->p_dec ? VLC_SUCCESS : VLC_EGENERIC;
            }
            else if( !b && id->p_es->p_dec )
            {
                input_UnselectES( p_sys->p_input, id->p_es );
                vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
                return VLC_SUCCESS;
            }
        case ES_OUT_GET_SELECT:
            id = (es_out_id_t*) va_arg( args, es_out_id_t * );
            pb = (vlc_bool_t*) va_arg( args, vlc_bool_t * );

            *pb = id->p_es->p_dec ? VLC_TRUE : VLC_FALSE;
            return VLC_SUCCESS;

        default:
            msg_Err( p_sys->p_input, "unknown query in es_out_Control" );
            return VLC_EGENERIC;
    }
}

