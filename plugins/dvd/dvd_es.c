/* dvd_es.c: functions to find and select ES
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_es.c,v 1.12 2002/06/01 12:31:58 sam Exp $
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
#   include "dummy_dvdcss.h"
#else
#   include <dvdcss/dvdcss.h>
#endif

#include "dvd.h"
#include "dvd_ifo.h"
#include "dvd_summary.h"
#include "iso_lang.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

void DVDLaunchDecoders( input_thread_t * p_input );

#define vmg p_dvd->p_ifo->vmg
#define vts p_dvd->p_ifo->vts

#define ADDES( stream_id, private_id, type, cat, lang, size )           \
    i_id = ( (private_id) << 8 ) | (stream_id);                         \
    p_es = input_AddES( p_input, NULL, i_id, size );                    \
    p_es->i_stream_id = (stream_id);                                    \
    p_es->i_type = (type);                                              \
    p_es->i_cat = (cat);                                                \
    if( lang )                                                          \
    {                                                                   \
        strcpy( p_es->psz_desc, DecodeLanguage( lang ) );               \
    }


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
        ADDES( 0xe0, 0, MPEG2_VIDEO_ES, VIDEO_ES, 0, sizeof(int) );
        *(int*)(p_es->p_demux_data) = i_ratio;
    }
    else
    {
        ADDES( 0xe0, 0, MPEG2_VIDEO_ES, VIDEO_ES, 0, 0 );
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
            case 0x00:              /* AC3 */
                ADDES( 0xbd, 0x80 + audio_status.i_position,
                       AC3_AUDIO_ES, AUDIO_ES, i_lang, 0 );
                p_es->b_audio = 1;
                strcat( p_es->psz_desc, " (ac3)" );

                break;
            case 0x02:
            case 0x03:              /* MPEG audio */
                ADDES( 0xc0 + audio_status.i_position, 0,
                       MPEG2_AUDIO_ES, AUDIO_ES, i_lang, 0 );
                p_es->b_audio = 1;
                strcat( p_es->psz_desc, " (mpeg)" );

                break;
            case 0x04:              /* LPCM */
                ADDES( 0xbd, 0xa0 + audio_status.i_position,
                       LPCM_AUDIO_ES, AUDIO_ES, i_lang, 0 );
                p_es->b_audio = 1;
                strcat( p_es->psz_desc, " (lpcm)" );

                break;
            case 0x06:              /* DTS */
                i_id = ( ( 0x88 + audio_status.i_position ) << 8 ) | 0xbd;
                msg_Err( p_input, "DTS audio not handled yet (0x%x)", i_id );
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
                ADDES( 0xbd, 0x20 + i_id, DVD_SPU_ES, SPU_ES,
                       vts.manager_inf.p_spu_attr[i-1].i_lang_code,
                       sizeof(int) + 16*sizeof(u32) );
                *(int*)p_es->p_demux_data = 0xBeeF;
                memcpy( (char*)p_es->p_demux_data + sizeof(int),
                        palette, 16*sizeof(u32) ); 
            }
            else
            {
                ADDES( 0xbd, 0x20 + i_id, DVD_SPU_ES, SPU_ES,
                   vts.manager_inf.p_spu_attr[i-1].i_lang_code, 0 );
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
    int                  i_audio;
    int                  i_spu;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

    /* Select Video stream (always 0) */
    if( !config_GetInt( p_input, "novideo" ) )
    {
        input_SelectES( p_input, p_input->stream.pp_es[0] );
    }

    /* Select audio stream */
    if( p_dvd->i_audio_nb > 0 && !config_GetInt( p_input, "noaudio" ) )
    {
        /* For audio: first one if none or a not existing one specified */
        i_audio = config_GetInt( p_input, "audio-channel" );
        if( i_audio <= 0 || i_audio > p_dvd->i_audio_nb )
        {
            config_PutInt( p_input, "audio-channel", 1 );
            i_audio = 1;
        }
        
        if( ( config_GetInt( p_input, "audio-type" )
               == REQUESTED_AC3 ) )
        {
            int     i_ac3 = i_audio;
            while( ( p_input->stream.pp_es[i_ac3]->i_type !=
                     AC3_AUDIO_ES ) && ( i_ac3 <=
                     p_dvd->p_ifo->vts.manager_inf.i_audio_nb ) )
            {
                i_ac3++;
            }
            if( p_input->stream.pp_es[i_ac3]->i_type == AC3_AUDIO_ES )
            {
                input_SelectES( p_input,
                                p_input->stream.pp_es[i_ac3] );
            }
        }
        else
        {
            input_SelectES( p_input,
                            p_input->stream.pp_es[i_audio] );
        }
    }

    /* Select subtitle */
    if( p_dvd->i_spu_nb > 0 && !config_GetInt( p_input, "novideo" ) )
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
            i_spu += p_dvd->p_ifo->vts.manager_inf.i_audio_nb;
            input_SelectES( p_input, p_input->stream.pp_es[i_spu] );
        }
    }
}
