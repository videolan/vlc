/* es.c: functions to find and select ES
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: es.c,v 1.5 2003/05/05 22:23:32 gbazin Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#ifdef GOD_DAMN_DMCA
#   include "dvdcss.h"
#else
#   include <dvdcss/dvdcss.h>
#endif

#include "dvd.h"
#include "ifo.h"
#include "summary.h"
#include "iso_lang.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

void DVDLaunchDecoders( input_thread_t * p_input );

#define vmg p_dvd->p_ifo->vmg
#define vts p_dvd->p_ifo->vts

#define ADDES( stream_id, private_id, fourcc, cat, lang, descr, size )  \
    i_id = ( (private_id) << 8 ) | (stream_id);                         \
    {                                                                   \
        char *psz_descr;                                                \
        psz_descr = malloc( strlen(DecodeLanguage( lang )) +            \
                            strlen(descr) + 1 );                        \
        if( psz_descr ) {strcpy( psz_descr, DecodeLanguage( lang ) );   \
            strcat( psz_descr, descr );}                                \
        p_es = input_AddES( p_input, NULL, i_id, cat,                   \
                            psz_descr, size );                          \
        if( psz_descr ) free( psz_descr );                              \
    }                                                                   \
    p_es->i_stream_id = (stream_id);                                    \
    p_es->i_fourcc = (fourcc);


/*****************************************************************************
 * DVDReadVideo: read video ES
 *****************************************************************************/
void DVDReadVideo( input_thread_t * p_input )
{
    thread_dvd_data_t * p_dvd;
    es_descriptor_t *   p_es;
    int                 i_id;
    int                 i_ratio;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

    /* ES 0 -> video MPEG2 */
    IfoPrintVideo( p_dvd );
    i_ratio = vts.manager_inf.video_attr.i_ratio;

    if( i_ratio )
    {
        ADDES( 0xe0, 0, VLC_FOURCC('m','p','g','v'), VIDEO_ES, 0,
               "", sizeof(int) );
        *(int*)(p_es->p_demux_data) = i_ratio;
    }
    else
    {
        ADDES( 0xe0, 0, VLC_FOURCC('m','p','g','v'), VIDEO_ES, 0, "", 0 );
    }

}

/*****************************************************************************
 * DVDReadAudio: read audio ES
 *****************************************************************************/
#define audio_status \
    vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_audio_status[i-1]

void DVDReadAudio( input_thread_t * p_input )
{
    thread_dvd_data_t * p_dvd;
    es_descriptor_t *   p_es;
    int                 i_lang;
    int                 i_id;
    int                 i;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);
    p_dvd->i_audio_nb = 0;

    /* Audio ES, in the order they appear in .ifo */
    for( i = 1 ; i <= vts.manager_inf.i_audio_nb ; i++ )
    {
        IfoPrintAudio( p_dvd, i );

        /* audio channel is active if first byte is 0x80 */
        if( audio_status.i_available )
        {
            p_dvd->i_audio_nb++;
            i_lang = vts.manager_inf.p_audio_attr[i-1].i_lang_code;
            i_id = audio_status.i_position;

            switch( vts.manager_inf.p_audio_attr[i-1].i_coding_mode )
            {
            case 0x00:              /* A52 */
                ADDES( 0xbd, 0x80 + audio_status.i_position,
                       VLC_FOURCC('a','5','2','b'), AUDIO_ES, i_lang,
                       " (A52)", 0 );

                break;
            case 0x02:
            case 0x03:              /* MPEG audio */
                ADDES( 0xc0 + audio_status.i_position, 0,
                       VLC_FOURCC('m','p','g','a'), AUDIO_ES, i_lang,
                       " (mpeg)", 0 );

                break;
            case 0x04:              /* LPCM */
                ADDES( 0xbd, 0xa0 + audio_status.i_position,
                       VLC_FOURCC('l','p','c','b'), AUDIO_ES, i_lang,
                       " (lpcm)", 0 );

                break;
            case 0x06:              /* DTS */
                ADDES( 0xbd, 0x88 + audio_status.i_position,
                       VLC_FOURCC('d','t','s','b'), AUDIO_ES, i_lang,
                       " (dts)", 0 );

                break;
            default:
                i_id = 0;
                msg_Err( p_input, "unknown audio type %.2x",
                         vts.manager_inf.p_audio_attr[i-1].i_coding_mode );
            }
        }
    }
}
#undef audio_status

/*****************************************************************************
 * DVDReadSPU: read subpictures ES
 *****************************************************************************/
#define spu_status \
    vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_spu_status[i-1]
#define palette \
    vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_yuv_color

void DVDReadSPU( input_thread_t * p_input )
{
    thread_dvd_data_t * p_dvd;
    es_descriptor_t *   p_es;
    int                 i_id;
    int                 i;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);
    p_dvd->i_spu_nb = 0;

    for( i = 1 ; i <= vts.manager_inf.i_spu_nb; i++ )
    {
        IfoPrintSpu( p_dvd, i );

        if( spu_status.i_available )
        {
            p_dvd->i_spu_nb++;

            /*  there are several streams for one spu */
            if(  vts.manager_inf.video_attr.i_ratio )
            {
                /* 16:9 */
                switch( vts.manager_inf.video_attr.i_perm_displ )
                {
                case 1:
                    i_id = spu_status.i_position_pan;
                    break;
                case 2:
                    i_id = spu_status.i_position_letter;
                    break;
                default:
                    i_id = spu_status.i_position_wide;
                    break;
                }
            }
            else
            {
                /* 4:3 */
                i_id = spu_status.i_position_43;
            }

            if( vmg.title.pi_yuv_color )
            {
                ADDES( 0xbd, 0x20 + i_id, VLC_FOURCC('s','p','u','b'), SPU_ES,
                       vts.manager_inf.p_spu_attr[i-1].i_lang_code, "",
                       sizeof(int) + 16*sizeof(u32) );
                *(int*)p_es->p_demux_data = 0xBeeF;
                memcpy( (char*)p_es->p_demux_data + sizeof(int),
                        palette, 16*sizeof(u32) );
            }
            else
            {
                ADDES( 0xbd, 0x20 + i_id, VLC_FOURCC('s','p','u','b'), SPU_ES,
                   vts.manager_inf.p_spu_attr[i-1].i_lang_code, "", 0 );
            }
        }
    }
}
#undef palette
#undef spu_status

#undef vts
#undef vmg

/*****************************************************************************
 * DVDLaunchDecoders: select ES for video, audio and spu
 *****************************************************************************/
void DVDLaunchDecoders( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    unsigned int         i_audio;
    unsigned int         i_spu;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

    /* Select Video stream (always 0) */
    input_SelectES( p_input, p_input->stream.pp_es[0] );

    /* Select audio stream */
    if( p_dvd->i_audio_nb > 0 )
    {
        /* For audio: first one if none or a not existing one specified */
        i_audio = config_GetInt( p_input, "audio-channel" );
        if( i_audio <= 0 || i_audio > p_dvd->i_audio_nb )
        {
            config_PutInt( p_input, "audio-channel", 1 );
            i_audio = 1;
        }

        if( ( config_GetInt( p_input, "audio-type" )
               == REQUESTED_A52 ) )
        {
            int     i_a52 = i_audio;
            while( ( p_input->stream.pp_es[i_a52]->i_fourcc !=
                     VLC_FOURCC('a','5','2','b') ) && ( i_a52 <=
                     p_dvd->p_ifo->vts.manager_inf.i_audio_nb ) )
            {
                i_a52++;
            }
            if( p_input->stream.pp_es[i_a52]->i_fourcc
                 == VLC_FOURCC('a','5','2','b') )
            {
                input_SelectES( p_input,
                                p_input->stream.pp_es[i_a52] );
            }
        }
        else
        {
            input_SelectES( p_input,
                            p_input->stream.pp_es[i_audio] );
        }
    }

    /* Select subtitle */
    if( p_dvd->i_spu_nb )
    {
        /* for spu, default is none */
        i_spu = config_GetInt( p_input, "spu-channel" );
        if( i_spu < 0 || i_spu > p_dvd->i_spu_nb )
        {
            config_PutInt( p_input, "spu-channel", 0 );
            i_spu = 0;
        }
        if( i_spu > 0 )
        {
            unsigned int i = 0, j = 0;
            for( i = 0; i < p_input->stream.i_es_number; i++ )
            {
                if ( p_input->stream.pp_es[i]->i_fourcc
                      == VLC_FOURCC('s','p','u','b') )
                {
                    j++;
                    if ( i_spu == j ) break;
                }
            }
            if( i_spu == j )
            {
                input_SelectES( p_input, p_input->stream.pp_es[i] );
            }
        }
    }
}
