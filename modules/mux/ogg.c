/*****************************************************************************
 * ogg.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ogg.c,v 1.2 2003/02/25 17:17:43 fenrir Exp $
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
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

#include <ogg/ogg.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int Capability( sout_instance_t *, int, void *, void * );
static int AddStream( sout_instance_t *, sout_input_t * );
static int DelStream( sout_instance_t *, sout_input_t * );
static int Mux      ( sout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Ogg/ogm muxer") );
    set_capability( "sout mux", 10 );
    add_shortcut( "ogg" );
    add_shortcut( "ogm" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 *
 *****************************************************************************/
#define FREE( p ) if( p ) { free( p ); (p) = NULL; }

#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_COMMENT  0x03

#define PACKET_IS_SYNCPOINT      0x08

typedef struct __attribute__((__packed__))
{
    int32_t i_width;
    int32_t i_height;
} ogg_stream_header_video_t;

typedef struct __attribute__((__packed__))
{
    int16_t i_channels;
    int16_t i_block_align;
    int32_t i_avgbytespersec;
} ogg_stream_header_audio_t;

typedef struct __attribute__((__packed__))
{
    uint8_t i_packet_type;

    char stream_type[8];
    char sub_type[4];

    int32_t i_size;

    int64_t i_time_unit;
    int64_t i_samples_per_unit;
    int32_t i_default_len;

    int32_t i_buffer_size;
    int16_t i_bits_per_sample;
    int16_t i_padding_0;            // hum hum
    union
    {
        ogg_stream_header_video_t video;
        ogg_stream_header_audio_t audio;
    } header;

} ogg_stream_header_t;


typedef struct
{
    int i_cat;
    int i_fourcc;

    ogg_stream_header_t header;

    int i_packet_no;

    mtime_t             i_dts;
    mtime_t             i_length;
    ogg_stream_state    os;

} ogg_stream_t;

typedef struct sout_mux_s
{
    int b_write_header;
    int i_streams;

} sout_mux_t;

#define SetWLE( p, v ) _SetWLE( (uint8_t*)p, v)
static void _SetWLE( uint8_t *p, uint16_t i_dw )
{
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

#define SetDWLE( p, v ) _SetDWLE( (uint8_t*)p, v)
static void _SetDWLE( uint8_t *p, uint32_t i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}
#define SetQWLE( p, v ) _SetQWLE( (uint8_t*)p, v)
static void _SetQWLE( uint8_t *p, uint64_t i_qw )
{
    SetDWLE( p,   i_qw&0xffffffff );
    SetDWLE( p+4, ( i_qw >> 32)&0xffffffff );
}

static void OggSetDate( sout_buffer_t *, mtime_t , mtime_t  );
static sout_buffer_t *OggStreamFlush( sout_instance_t *, ogg_stream_state *, mtime_t );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux;

    msg_Info( p_sout, "Open" );

    p_mux                 = malloc( sizeof( sout_mux_t ) );
    p_mux->i_streams      = 0;
    p_mux->b_write_header = VLC_TRUE;

    p_sout->p_mux_data       = (void*)p_mux;
    p_sout->pf_mux_capacity  = Capability;
    p_sout->pf_mux_addstream = AddStream;
    p_sout->pf_mux_delstream = DelStream;
    p_sout->pf_mux           = Mux;
    p_sout->i_mux_preheader  = 1;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux  = (sout_mux_t*)p_sout->p_mux_data;

    msg_Info( p_sout, "Close" );

    free( p_mux );
}

static int Capability( sout_instance_t *p_sout, int i_query, void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_FALSE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

static int AddStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    sout_mux_t          *p_mux  = (sout_mux_t*)p_sout->p_mux_data;
    ogg_stream_t        *p_stream;

    BITMAPINFOHEADER    *p_bih;
    WAVEFORMATEX        *p_wf;

    msg_Dbg( p_sout, "adding input" );
    p_input->p_mux_data = (void*)p_stream = malloc( sizeof( ogg_stream_t ) );

    p_stream->i_cat       = p_input->input_format.i_cat;
    p_stream->i_fourcc    = p_input->input_format.i_fourcc;
    p_stream->i_packet_no = 0;

    p_stream->header.i_packet_type = PACKET_TYPE_HEADER;
    switch( p_input->input_format.i_cat )
    {
        case VIDEO_ES:
            p_bih = (BITMAPINFOHEADER*)p_input->input_format.p_format;
            switch( p_input->input_format.i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p','4', 'v' ):
                case VLC_FOURCC( 'D', 'I','V', '3' ):
                    memcpy( p_stream->header.stream_type,
                            "video    ",
                            8 );
                    if( p_input->input_format.i_fourcc == VLC_FOURCC( 'm', 'p','4', 'v' ) )
                    {
                        memcpy( p_stream->header.sub_type, "XVID", 4 );
                    }
                    else if( p_input->input_format.i_fourcc == VLC_FOURCC( 'D', 'I','V', '3' ) )
                    {
                        memcpy( p_stream->header.sub_type, "DIV3", 4 );
                    }
                    SetDWLE( &p_stream->header.i_size, sizeof( ogg_stream_header_t ) - 1);
                    /* XXX this won't make mplayer happy, but vlc can read that without any problem so...*/
                    SetQWLE( &p_stream->header.i_time_unit, 10*1000 );//(int64_t)10*1000*1000/(int64_t)25 );  // FIXME (25fps)
                    SetQWLE( &p_stream->header.i_samples_per_unit, 1 );
                    SetDWLE( &p_stream->header.i_default_len, 0 );      /* ??? */
                    SetDWLE( &p_stream->header.i_buffer_size, 1024*1024 );
                    SetWLE( &p_stream->header.i_bits_per_sample, 0 );
                    if( p_bih )
                    {
                        SetDWLE( &p_stream->header.header.video.i_width, p_bih->biWidth );
                        SetDWLE( &p_stream->header.header.video.i_height, p_bih->biHeight );
                    }
                    else
                    {
                        SetDWLE( &p_stream->header.header.video.i_width,  0 );
                        SetDWLE( &p_stream->header.header.video.i_height, 0 );
                    }
                    break;
                default:
                    FREE( p_input->p_mux_data );
                    return( VLC_EGENERIC );
            }
            break;
        case AUDIO_ES:
            p_wf = (WAVEFORMATEX*)p_input->input_format.p_format;
            switch( p_input->input_format.i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p','g', 'a' ):
                case VLC_FOURCC( 'a', '5','2', ' ' ):
                    memcpy( p_stream->header.stream_type,
                            "audio    ",
                            8 );
                    if( p_input->input_format.i_fourcc == VLC_FOURCC( 'm', 'p','g', 'a' ) )
                    {
                        memcpy( p_stream->header.sub_type, "55  ", 4 );
                    }
                    else if( p_input->input_format.i_fourcc == VLC_FOURCC( 'a', '5','2', ' ' ) )
                    {
                        memcpy( p_stream->header.sub_type, "2000", 4 );
                    }
                    SetDWLE( &p_stream->header.i_size, sizeof( ogg_stream_header_t ) - 1);
                    SetQWLE( &p_stream->header.i_time_unit, 1000000 );  /* is it used ? */
                    SetDWLE( &p_stream->header.i_default_len, 0 );      /* ??? */
                    SetDWLE( &p_stream->header.i_buffer_size, 30*1024 );
                    if( p_wf )
                    {
                        SetQWLE( &p_stream->header.i_samples_per_unit, p_wf->nSamplesPerSec );
                        SetWLE( &p_stream->header.i_bits_per_sample, p_wf->wBitsPerSample );
                        SetDWLE( &p_stream->header.header.audio.i_channels, p_wf->nChannels );
                        SetDWLE( &p_stream->header.header.audio.i_block_align, p_wf->nBlockAlign );
                        SetDWLE( &p_stream->header.header.audio.i_avgbytespersec, p_wf->nAvgBytesPerSec );
                    }
                    else
                    {
                        /* perhaps it's better to fail */
                        SetQWLE( &p_stream->header.i_samples_per_unit, 44100 );
                        SetWLE( &p_stream->header.i_bits_per_sample, 0 );
                        SetDWLE( &p_stream->header.header.audio.i_channels, 2 );
                        SetDWLE( &p_stream->header.header.audio.i_block_align, 0 );
                        SetDWLE( &p_stream->header.header.audio.i_avgbytespersec, 0 );
                    }
                    break;
                case VLC_FOURCC( 'v', 'o', 'r', 'b' ):
                default:
                    FREE( p_input->p_mux_data );
                    return( VLC_EGENERIC );
            }
            break;
        default:
            FREE( p_input->p_mux_data );
            return( VLC_EGENERIC );
    }

    ogg_stream_init (&p_stream->os, rand ());

    p_mux->i_streams++;
    return( VLC_SUCCESS );
}

static int DelStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    ogg_stream_t        *p_stream = (ogg_stream_t*)p_input->p_mux_data;
    sout_buffer_t       *p_og;

    msg_Dbg( p_sout, "removing input" );

    /* flush all remaining data */

    p_og = OggStreamFlush( p_sout, &p_stream->os, 0 );
    if( p_og )
    {
        OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );

        sout_AccessOutWrite( p_sout->p_access, p_og );
    }

    ogg_stream_clear( &p_stream->os );

    FREE( p_input->p_mux_data );
    return( 0 );
}

/*
 * TODO  move this function to src/stream_output.c (used by nearly all muxers)
 */
static int MuxGetStream( sout_instance_t *p_sout,
                         int        *pi_stream,
                         mtime_t    *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_sout->i_nb_inputs; i++ )
    {
        sout_fifo_t  *p_fifo;

        p_fifo = p_sout->pp_inputs[i]->p_fifo;

        if( p_fifo->i_depth > 1 )
        {
            sout_buffer_t *p_buf;

            p_buf = sout_FifoShow( p_fifo );
            if( i_stream < 0 || p_buf->i_dts < i_dts )
            {
                i_dts = p_buf->i_dts;
                i_stream = i;
            }
        }
        else
        {
            return( -1 ); // wait that all fifo have at least 2 packets
        }
    }
    if( pi_stream )
    {
        *pi_stream = i_stream;
    }
    if( pi_dts )
    {
        *pi_dts = i_dts;
    }
    return( i_stream );
}


static sout_buffer_t *OggStreamFlush( sout_instance_t *p_sout,
                                      ogg_stream_state *p_os,
                                      mtime_t i_pts )
{
    sout_buffer_t       *p_og, *p_og_first = NULL;
    ogg_page            og;

    for( ;; )
    {
        /* flush all data */
        int i_result;
        int i_size;
        if( ( i_result = ogg_stream_flush( p_os, &og ) ) == 0 )
        {
            break;
        }
        i_size = og.header_len + og.body_len;
        p_og = sout_BufferNew( p_sout, i_size);

        memcpy( p_og->p_buffer,
                og.header,
                og.header_len );
        memcpy( p_og->p_buffer + og.header_len,
                og.body,
                og.body_len );
        p_og->i_size    = i_size;
        p_og->i_dts     = 0;
        p_og->i_pts     = i_pts;
        p_og->i_length  = 0;

        i_pts   = 0; // write it only once

        sout_BufferChain( &p_og_first, p_og );
    }

    return( p_og_first );
}
static sout_buffer_t *OggStreamPageOut( sout_instance_t *p_sout,
                                        ogg_stream_state *p_os,
                                        mtime_t i_pts )
{
    sout_buffer_t       *p_og, *p_og_first = NULL;
    ogg_page            og;

    for( ;; )
    {
        /* flush all data */
        int i_result;
        int i_size;
        if( ( i_result = ogg_stream_pageout( p_os, &og ) ) == 0 )
        {
            break;
        }
        i_size = og.header_len + og.body_len;
        p_og = sout_BufferNew( p_sout, i_size);

        memcpy( p_og->p_buffer,
                og.header,
                og.header_len );
        memcpy( p_og->p_buffer + og.header_len,
                og.body,
                og.body_len );
        p_og->i_size    = i_size;
        p_og->i_dts     = 0;
        p_og->i_pts     = i_pts;
        p_og->i_length  = 0;

        i_pts   = 0; // write them only once

        sout_BufferChain( &p_og_first, p_og );
    }

    return( p_og_first );
}
static sout_buffer_t *OggCreateHeader( sout_instance_t *p_sout, mtime_t i_dts )
{
    sout_buffer_t *p_hdr = NULL;
    sout_buffer_t *p_og;
    ogg_packet    op;
    int i;

    /* write header for each stream */
    for( i = 0; i < p_sout->i_nb_inputs; i++ )
    {
        ogg_stream_t *p_stream;

        p_stream = (ogg_stream_t*)p_sout->pp_inputs[i]->p_mux_data;

        if( p_stream->i_fourcc == VLC_FOURCC( 'v', 'o', 'r', 'b' ) )
        {
            /* special case */
        }
        else
        {
            /* ds header */
#if 0
            uint8_t com[128];
            int     i_com;
#endif

            /* header */
            op.packet = (uint8_t*)&p_stream->header;
            op.bytes  = sizeof( ogg_stream_t );
            op.b_o_s  = 1;
            op.e_o_s  = 0;
            op.granulepos = 0;
            op.packetno = p_stream->i_packet_no++;
            ogg_stream_packetin( &p_stream->os, &op );
#if 0
            /* I don't know why but this produce broken file */
            /* comment */
            com[0] = PACKET_TYPE_COMMENT;
            i_com = snprintf( &com[1], 128, "VLC 0.5.x stream output" ) + 1;
            op.packet = com;
            op.bytes  = i_com;
            op.b_o_s  = 0;
            op.e_o_s  = 0;
            op.granulepos = 0;
            op.packetno = p_stream->i_packet_no++;
            ogg_stream_packetin( &p_stream->os, &op );
#endif
        }

        p_og = OggStreamFlush( p_sout, &p_stream->os, 0 );
        sout_BufferChain( &p_hdr, p_og );
    }

    /* set HEADER flag */
    for( p_og = p_hdr; p_og != NULL; p_og = p_og->p_next )
    {
        p_og->i_flags |= SOUT_BUFFER_FLAGS_HEADER;
    }
    return( p_hdr );
}


static void OggSetDate( sout_buffer_t *p_og, mtime_t i_dts, mtime_t i_length )
{
    int i_count;
    sout_buffer_t *p_tmp;
    mtime_t i_delta;

    for( p_tmp = p_og, i_count = 0; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        i_count++;
    }
    i_delta = i_length / i_count;

    for( p_tmp = p_og; p_tmp != NULL; p_tmp = p_tmp->p_next )
    {
        p_tmp->i_dts    = i_dts;
        p_tmp->i_length = i_delta;

        i_dts += i_delta;
    }
}

static int Mux      ( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux  = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_og = NULL;
    int i_stream;
    mtime_t             i_dts;

    if( p_mux->b_write_header )
    {
        if( MuxGetStream( p_sout, &i_stream, &i_dts) < 0 )
        {
            return( VLC_SUCCESS );
        }
        sout_BufferChain( &p_og, OggCreateHeader( p_sout, i_dts ) );
        p_mux->b_write_header = VLC_FALSE;
    }

    for( ;; )
    {
        sout_input_t    *p_input;
        ogg_stream_t    *p_stream;
        sout_buffer_t   *p_data;
        ogg_packet          op;

        if( MuxGetStream( p_sout, &i_stream, &i_dts) < 0 )
        {
            return( VLC_SUCCESS );
        }

        p_input  = p_sout->pp_inputs[i_stream];
        p_stream = (ogg_stream_t*)p_input->p_mux_data;

        p_data  = sout_FifoGet( p_input->p_fifo );

        sout_BufferReallocFromPreHeader( p_sout, p_data, 1 );
        p_data->p_buffer[0] = PACKET_IS_SYNCPOINT;      // FIXME

        op.packet = p_data->p_buffer;
        op.bytes  = p_data->i_size;
        op.b_o_s  = 0;
        op.e_o_s  = 0;
        if( p_stream->i_cat == AUDIO_ES )
        {
            /* number of sample from begining */
            op.granulepos = i_dts * p_stream->header.i_samples_per_unit / (int64_t)1000000;
        }
        else if( p_stream->i_cat == VIDEO_ES )
        {
            op.granulepos = i_dts/1000;//p_stream->i_packet_no;
        }
        op.packetno = p_stream->i_packet_no++;
        ogg_stream_packetin( &p_stream->os, &op );

        sout_BufferChain( &p_og,
                          OggStreamPageOut( p_sout,
                                            &p_stream->os,
                                            p_data->i_pts ) );

        if( p_og )
        {
            OggSetDate( p_og, p_stream->i_dts, p_stream->i_length );
            p_stream->i_dts = -1;
            p_stream->i_length = 0;;
            sout_AccessOutWrite( p_sout->p_access, p_og );

            p_og = NULL;
        }
        else
        {
            if( p_stream->i_dts < 0 )
            {
                p_stream->i_dts     = p_data->i_dts;
            }
            p_stream->i_length += p_data->i_length;
        }

        sout_BufferDelete( p_sout, p_data );
    }

    return( VLC_SUCCESS );
}

