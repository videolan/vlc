/*****************************************************************************
 * rawdv.c : raw dv input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: rawdv.c,v 1.5 2003/03/30 18:14:37 gbazin Exp $
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
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/types.h>

#include <codecs.h>                        /* BITMAPINFOHEADER, WAVEFORMATEX */

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

    es_descriptor_t  *p_video_es;
    es_descriptor_t  *p_audio_es;

    /* codec specific stuff */
    BITMAPINFOHEADER *p_bih;
    WAVEFORMATEX *p_wf;

    double f_rate;
    int    i_bitrate;

    /* program clock reference (in units of 90kHz) */
    mtime_t i_pcr;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux     ( input_thread_t * );

static uint32_t GetDWBE( uint8_t *p_buff )
{
    return (uint32_t)p_buff[3] | ( ((uint32_t)p_buff[2]) << 8 ) |
            ( ((uint32_t)p_buff[1]) << 16 ) | ( ((uint32_t)p_buff[0]) << 24 );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("raw dv demuxer") );
    set_capability( "demux", 2 );
    set_callbacks( Activate, NULL );
    add_shortcut( "rawdv" );
vlc_module_end();

/*****************************************************************************
 * Activate: initializes raw dv demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    byte_t         *p_peek, *p_peek_backup;
    uint32_t       i_dword;
    demux_sys_t    *p_rawdv;
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
        return -1;
    }

    p_input->pf_demux = Demux;

    /* Have a peep at the show. */
    if( input_Peek(p_input, &p_peek, DV_PAL_FRAME_SIZE) < DV_NTSC_FRAME_SIZE )
    {
        /* Stream too short ... */
        msg_Err( p_input, "cannot peek()" );
        return -1;
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
        return -1;
    }

    /* fill in the dv_header_t structure */
    dv_header.dsf = i_dword >> (32 - 1);
    i_dword <<= 1;
    if( i_dword >> (32 - 1) ) /* incorrect bit */
    {
        msg_Warn( p_input, "incorrect bit" );
        return -1;
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


    /* Setup the structures for our demuxer */
    if( !( p_rawdv = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }
    memset( p_rawdv, 0, sizeof( demux_sys_t ) );
    p_rawdv->p_bih = NULL;
    p_rawdv->p_wf = NULL;
    p_input->p_demux_data = p_rawdv;

    p_rawdv->p_bih = (BITMAPINFOHEADER *) malloc( sizeof(BITMAPINFOHEADER) );
    if( !p_rawdv->p_bih )
    {
        msg_Err( p_input, "out of memory" );
        goto error;
    }
    p_rawdv->p_bih->biSize = sizeof(BITMAPINFOHEADER);
    p_rawdv->p_bih->biCompression = VLC_FOURCC( 'd','v','s','d' );
    p_rawdv->p_bih->biSize = 40;
    p_rawdv->p_bih->biWidth = 720;
    p_rawdv->p_bih->biHeight = dv_header.dsf ? 576 : 480;
    p_rawdv->p_bih->biPlanes = 1;
    p_rawdv->p_bih->biBitCount = 24;
    p_rawdv->p_bih->biSizeImage =
        p_rawdv->p_bih->biWidth * p_rawdv->p_bih->biHeight
          * (p_rawdv->p_bih->biBitCount >> 3);

    /* Properties of our video */
    if( dv_header.dsf )
    {
        p_rawdv->frame_size = 12 * 150 * 80;
        p_rawdv->f_rate = 25;
    }
    else
    {
        p_rawdv->frame_size = 10 * 150 * 80;
        p_rawdv->f_rate = 29.97;
    }

    /* Audio stuff */
#if 0
    p_peek = p_peek_backup + 80*6+80*16*3 + 3; /* beginning of AAUX pack */

    if( *p_peek != 0x50 || *p_peek != 0x51 )
    {
        msg_Err( p_input, "AAUX should begin with 0x50" );
    }
#endif

    p_rawdv->p_wf = (WAVEFORMATEX *)malloc( sizeof(WAVEFORMATEX) );
    if( !p_rawdv->p_wf )
    {
        msg_Err( p_input, "out of memory" );
        goto error;
    }

    p_rawdv->p_wf->wFormatTag = 0;
    p_rawdv->p_wf->nChannels = 2;
    p_rawdv->p_wf->nSamplesPerSec = 44100; /* FIXME */
    p_rawdv->p_wf->nAvgBytesPerSec = p_rawdv->f_rate * p_rawdv->frame_size;
    p_rawdv->p_wf->nBlockAlign = p_rawdv->frame_size;
    p_rawdv->p_wf->wBitsPerSample = 16;
    p_rawdv->p_wf->cbSize = 0;


    /* necessary because input_SplitBuffer() will only get
     * INPUT_DEFAULT_BUFSIZE bytes at a time. */
    p_input->i_bufsize = p_rawdv->frame_size;

    /* Create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate = p_rawdv->frame_size * p_rawdv->f_rate;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Add video stream */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_rawdv->p_video_es = input_AddES( p_input,
                                       p_input->stream.p_selected_program,
                                       1, 0 );
    p_rawdv->p_video_es->i_stream_id = 0;
    p_rawdv->p_video_es->i_fourcc = VLC_FOURCC( 'd','v','s','d' );
    p_rawdv->p_video_es->i_cat = VIDEO_ES;
    p_rawdv->p_video_es->p_bitmapinfoheader = (void *)p_rawdv->p_bih;
    input_SelectES( p_input, p_rawdv->p_video_es );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Add audio stream */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_rawdv->p_audio_es = input_AddES( p_input,
                                       p_input->stream.p_selected_program,
                                       2, 0 );
    p_rawdv->p_audio_es->i_stream_id = 1;
    p_rawdv->p_audio_es->i_fourcc = VLC_FOURCC( 'd','v','a','u' );
    p_rawdv->p_audio_es->i_cat = AUDIO_ES;
    p_rawdv->p_audio_es->p_waveformatex = (void *)p_rawdv->p_wf;
    input_SelectES( p_input, p_rawdv->p_audio_es );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Init pcr */
    p_rawdv->i_pcr = 0;

    return 0;

 error:
    if( p_rawdv->p_bih ) free( p_rawdv->p_bih );
    if( p_rawdv->p_wf ) free( p_rawdv->p_wf );
    Deactivate( (vlc_object_t *)p_input );
    return -1;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *p_rawdv = (demux_sys_t *)p_input->p_demux_data;

    free( p_rawdv );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t    *p_rawdv = (demux_sys_t *)p_input->p_demux_data;
    pes_packet_t   *p_pes;
    pes_packet_t   *p_audio_pes;
    data_packet_t  *p_data;
    ssize_t        i_read;

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        off_t i_pos;

        msg_Warn( p_input, "synchro reinit" );

        /* If the user tried to seek in the stream, we need to make sure
         * the new position is at a DIF block boundary. */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        i_pos= p_input->stream.p_selected_area->i_tell;
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        if( (i_pos % p_rawdv->frame_size) &&
            p_input->stream.b_seekable &&
            p_input->stream.i_method == INPUT_METHOD_FILE )
        {
            p_input->pf_seek( p_input, (i_pos / p_rawdv->frame_size)
                              * p_rawdv->frame_size );
            input_AccessReinit( p_input );
        }
    }

    /* Call the pace control */
    input_ClockManageRef( p_input, p_input->stream.p_selected_program,
                          p_rawdv->i_pcr );

    i_read = input_SplitBuffer( p_input, &p_data, p_rawdv->frame_size );
    if( i_read <= 0 )
    {
        return i_read;
    }

    /* Build video PES packet */
    p_pes = input_NewPES( p_input->p_method_data );
    if( p_pes == NULL )
    {
        msg_Err( p_input, "out of memory" );
        input_DeletePacket( p_input->p_method_data, p_data );
        return -1;
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_pes_size = i_read;
    p_pes->i_nb_data = 1;
    p_pes->i_pts =
        input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                          p_rawdv->i_pcr );

    /* Do the same for audio */
    p_audio_pes = input_NewPES( p_input->p_method_data );
    if( p_pes == NULL )
    {
        msg_Err( p_input, "out of memory" );
        input_DeletePacket( p_input->p_method_data, p_data );
        return -1;
    }
    p_audio_pes->i_rate = p_input->stream.control.i_rate;
    p_audio_pes->p_first = p_audio_pes->p_last =
        input_ShareBuffer( p_input->p_method_data, p_data->p_buffer );
    p_audio_pes->p_first->p_next = p_data->p_next;
    p_audio_pes->p_first->p_payload_start = p_data->p_payload_start;
    p_audio_pes->p_first->p_payload_end = p_data->p_payload_end;
    p_audio_pes->i_pes_size = i_read;
    p_audio_pes->i_nb_data = 1;
    p_audio_pes->i_pts = p_pes->i_pts;

    /* Decode PES packets */
    input_DecodePES( p_rawdv->p_video_es->p_decoder_fifo, p_pes );
    input_DecodePES( p_rawdv->p_audio_es->p_decoder_fifo, p_audio_pes );

    p_rawdv->i_pcr += ( 90000 / p_rawdv->f_rate );

    return 1;
}
