/*****************************************************************************
 * rawdv.c : raw dv input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: rawdv.c,v 1.12 2003/11/24 19:19:02 fenrir Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("raw dv demuxer") );
    set_capability( "demux", 2 );
    set_callbacks( Open, Close );
    add_shortcut( "rawdv" );
vlc_module_end();


/*****************************************************************************
 A little bit of background information (copied over from libdv glossary).

 - DIF block: A block of 80 bytes. This is the basic data framing unit of the
       DVC tape format, analogous to sectors of hard disc.

 - Video Section: Each DIF sequence contains a video section, consisting of
       135 DIF blocks, which are further subdivided into Video Segments.

 - Video Segment: A video segment consists of 5 DIF blocks, each corresponding
       to a single compressed macroblock.

*****************************************************************************/


/*****************************************************************************
 * Constants
 *****************************************************************************/
#define DV_PAL_FRAME_SIZE  144000
#define DV_NTSC_FRAME_SIZE 122000

/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/
typedef struct {
    int8_t sct;      /* Section type (header,subcode,aux,audio,video) */
    int8_t dsn;      /* DIF sequence number (0-12) */
    int    fsc;      /* First (0)/Second channel (1) */
    int8_t dbn;      /* DIF block number (0-134) */
} dv_id_t;

typedef struct {
    int    dsf;      /* DIF sequence flag: 525/60 (0) or 625,50 (1) */
    int8_t apt;
    int    tf1;
    int8_t ap1;
    int    tf2;
    int8_t ap2;
    int    tf3;
    int8_t ap3;
} dv_header_t;

struct demux_sys_t
{
    int    frame_size;

    es_out_id_t *p_es_video;
    es_format_t  fmt_video;

    es_out_id_t *p_es_audio;
    es_format_t  fmt_audio;

    double f_rate;
    int    i_bitrate;

    /* program clock reference (in units of 90kHz) */
    mtime_t i_pcr;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux     ( input_thread_t * );

/*****************************************************************************
 * Open: initializes raw dv demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    byte_t         *p_peek, *p_peek_backup;

    uint32_t       i_dword;
    dv_header_t    dv_header;
    dv_id_t        dv_id;
    char           *psz_ext;

    /* It isn't easy to recognize a raw dv stream. The chances that we'll
     * mistake a stream from another type for a raw dv stream are too high, so
     * we'll rely on the file extension to trigger this demux. Alternatively,
     * it is possible to force this demux. */

    /* Check for dv file extension */
    psz_ext = strrchr ( p_input->psz_name, '.' );
    if( ( !psz_ext || strcasecmp( psz_ext, ".dv") )&&
        ( !p_input->psz_demux || strcmp(p_input->psz_demux, "rawdv") ) )
    {
        return VLC_EGENERIC;
    }

    if( stream_Peek( p_input->s, &p_peek, DV_PAL_FRAME_SIZE ) < DV_NTSC_FRAME_SIZE )
    {
        /* Stream too short ... */
        msg_Err( p_input, "cannot peek()" );
        return VLC_EGENERIC;
    }
    p_peek_backup = p_peek;

    /* fill in the dv_id_t structure */
    i_dword = GetDWBE( p_peek ); p_peek += 4;
    dv_id.sct = i_dword >> (32 - 3);
    i_dword <<= 8;
    dv_id.dsn = i_dword >> (32 - 4);
    i_dword <<= 4;
    dv_id.fsc = i_dword >> (32 - 1);
    i_dword <<= 4;
    dv_id.dbn = i_dword >> (32 - 8);
    i_dword <<= 8;

    if( dv_id.sct != 0 )
    {
        msg_Warn( p_input, "not a raw dv stream header" );
        return VLC_EGENERIC;
    }

    /* fill in the dv_header_t structure */
    dv_header.dsf = i_dword >> (32 - 1);
    i_dword <<= 1;
    if( i_dword >> (32 - 1) ) /* incorrect bit */
    {
        msg_Warn( p_input, "incorrect bit" );
        return VLC_EGENERIC;
    }

    i_dword = GetDWBE( p_peek ); p_peek += 4;
    i_dword <<= 5;
    dv_header.apt = i_dword >> (32 - 3);
    i_dword <<= 3;
    dv_header.tf1 = i_dword >> (32 - 1);
    i_dword <<= 5;
    dv_header.ap1 = i_dword >> (32 - 3);
    i_dword <<= 3;
    dv_header.tf2 = i_dword >> (32 - 1);
    i_dword <<= 5;
    dv_header.ap2 = i_dword >> (32 - 3);
    i_dword <<= 3;
    dv_header.tf3 = i_dword >> (32 - 1);
    i_dword <<= 5;
    dv_header.ap3 = i_dword >> (32 - 3);

    p_peek += 72;                                  /* skip rest of DIF block */


    /* Set p_input field */
    p_input->pf_demux = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );

    p_sys->frame_size = dv_header.dsf ? 12 * 150 * 80 : 10 * 150 * 80;
    p_sys->f_rate = dv_header.dsf ? 25 : 29.97;

    p_sys->i_pcr = 0;
    p_sys->p_es_video = NULL;
    p_sys->p_es_audio = NULL;

    p_sys->i_bitrate = 0;

    es_format_Init( &p_sys->fmt_video, VIDEO_ES, VLC_FOURCC( 'd','v','s','d' ) );
    p_sys->fmt_video.video.i_width = 720;
    p_sys->fmt_video.video.i_height= dv_header.dsf ? 576 : 480;;

    /* Audio stuff */
#if 0
    p_peek = p_peek_backup + 80*6+80*16*3 + 3; /* beginning of AAUX pack */

    if( *p_peek != 0x50 || *p_peek != 0x51 )
    {
        msg_Err( p_input, "AAUX should begin with 0x50" );
    }
#endif

    es_format_Init( &p_sys->fmt_audio, AUDIO_ES, VLC_FOURCC( 'd','v','a','u' ) );
    p_sys->fmt_audio.audio.i_channels = 2;
    p_sys->fmt_audio.audio.i_rate = 44100;  /* FIXME */
    p_sys->fmt_audio.audio.i_bitspersample = 16;
    p_sys->fmt_audio.audio.i_blockalign = p_sys->frame_size;    /* ??? */
    p_sys->fmt_audio.i_bitrate = p_sys->f_rate * p_sys->frame_size; /* ??? */

    /* necessary because input_SplitBuffer() will only get
     * INPUT_DEFAULT_BUFSIZE bytes at a time. */
    p_input->i_bufsize = p_sys->frame_size;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_input->stream.i_mux_rate = p_sys->frame_size * p_sys->f_rate;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_sys->p_es_video = es_out_Add( p_input->p_es_out, &p_sys->fmt_video );
    p_sys->p_es_audio = es_out_Add( p_input->p_es_out, &p_sys->fmt_audio );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t    *p_sys = p_input->p_demux_data;
    block_t        *p_block;
    vlc_bool_t     b_audio, b_video;


    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        off_t i_pos = stream_Tell( p_input->s );

        msg_Warn( p_input, "synchro reinit" );

        /* If the user tried to seek in the stream, we need to make sure
         * the new position is at a DIF block boundary. */
        if( i_pos % p_sys->frame_size > 0 )
        {
            i_pos += p_sys->frame_size - i_pos % p_sys->frame_size;

            if( stream_Seek( p_input->s, i_pos ) )
            {
                msg_Warn( p_input, "cannot resynch after seek (EOF?)" );
                return -1;
            }
        }
    }

    /* Call the pace control */
    input_ClockManageRef( p_input, p_input->stream.p_selected_program,
                          p_sys->i_pcr );

    if( ( p_block = stream_Block( p_input->s, p_sys->frame_size ) ) == NULL )
    {
        /* EOF */
        return 0;
    }

    es_out_Control( p_input->p_es_out, ES_OUT_GET_SELECT,
                    p_sys->p_es_audio, &b_audio );
    es_out_Control( p_input->p_es_out, ES_OUT_GET_SELECT,
                    p_sys->p_es_video, &b_video );

    p_block->i_dts =
    p_block->i_pts = input_ClockGetTS( p_input,
                                       p_input->stream.p_selected_program,
                                       p_sys->i_pcr );
    if( b_audio && b_video )
    {
        block_t *p_dup = block_Duplicate( p_block );

        es_out_Send( p_input->p_es_out, p_sys->p_es_video, p_block );
        if( p_dup )
        {
            es_out_Send( p_input->p_es_out, p_sys->p_es_video, p_dup );
        }
    }
    else if( b_audio )
    {
        es_out_Send( p_input->p_es_out, p_sys->p_es_audio, p_block );
    }
    else if( b_video )
    {
        es_out_Send( p_input->p_es_out, p_sys->p_es_video, p_block );
    }
    else
    {
        block_Release( p_block );
    }

    p_sys->i_pcr += ( 90000 / p_sys->f_rate );

    return 1;
}

