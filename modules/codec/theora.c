/*****************************************************************************
 * theora.c: theora decoder module making use of libtheora.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: theora.c,v 1.13 2003/10/25 00:49:13 sam Exp $
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include <input_ext-dec.h>

#include <ogg/ogg.h>

#include <theora/theora.h>

/*****************************************************************************
 * decoder_sys_t : theora decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    vlc_bool_t b_packetizer;

    /*
     * Input properties
     */
    int i_headers;

    /*
     * Theora properties
     */
    theora_info      ti;                        /* theora bitstream settings */
    theora_comment   tc;                            /* theora comment header */
    theora_state     td;                   /* theora bitstream user comments */

    /*
     * Output properties
     */
    vout_thread_t *p_vout;

    /*
     * Packetizer output properties
     */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           sout_format;

    /*
     * Common properties
     */
    mtime_t i_pts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int OpenDecoder   ( vlc_object_t * );
static int OpenPacketizer( vlc_object_t * );

static int InitDecoder   ( decoder_t * );
static int RunDecoder    ( decoder_t *, block_t * );
static int EndDecoder    ( decoder_t * );

static int ProcessPacket ( decoder_t *, ogg_packet *, mtime_t );
static int DecodePacket  ( decoder_t *, ogg_packet * );
static int SendPacket    ( decoder_t *, ogg_packet * );

static void ParseTheoraComments( decoder_t * );
static void theora_CopyPicture( decoder_t *, picture_t *, yuv_buffer * );

static int  OpenEncoder( vlc_object_t *p_this );
static void CloseEncoder( vlc_object_t *p_this );
static block_t *Headers( encoder_t *p_enc );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Theora video decoder") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
    add_shortcut( "theora" );

    add_submodule();
    set_description( _("Theora video packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, NULL );
    add_shortcut( "theora" );

    add_submodule();
    set_description( _("Theora video encoder") );
    set_capability( "video encoder", 100 );
    set_callbacks( OpenEncoder, CloseEncoder );
    add_shortcut( "theora" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('t','h','e','o') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_init = InitDecoder;
    p_dec->pf_decode = RunDecoder;
    p_dec->pf_end = EndDecoder;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    p_dec->p_sys->b_packetizer = VLC_FALSE;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = VLC_TRUE;

    return i_ret;
}

/*****************************************************************************
 * InitDecoder: Initalize the decoder
 *****************************************************************************/
static int InitDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->i_pts = 0;

    p_sys->p_sout_input = NULL;
    p_sys->sout_format.i_cat = VIDEO_ES;
    p_sys->sout_format.i_fourcc = VLC_FOURCC( 't', 'h', 'e', 'o' );
    p_sys->sout_format.i_width  = 0;
    p_sys->sout_format.i_height = 0;
    p_sys->sout_format.i_bitrate     = 0;
    p_sys->sout_format.i_extra_data  = 0;
    p_sys->sout_format.p_extra_data  = NULL;

    /* Init supporting Theora structures needed in header parsing */
    theora_comment_init( &p_sys->tc );
    theora_info_init( &p_sys->ti );

    p_sys->i_headers = 0;

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static int RunDecoder( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;
    int i_ret;

    /* Block to Ogg packet */
    oggpacket.packet = p_block->p_buffer;
    oggpacket.bytes = p_block->i_buffer;
    oggpacket.granulepos = p_block->i_dts;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    if( p_sys->i_headers == 0 )
    {
        /* Take care of the initial Theora header */

        oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
        if( theora_decode_header( &p_sys->ti, &p_sys->tc, &oggpacket ) < 0 )
        {
            msg_Err( p_dec->p_fifo, "This bitstream does not contain Theora "
                     "video data" );
            block_Release( p_block );
            return VLC_EGENERIC;
        }
        p_sys->i_headers++;


        if( p_sys->b_packetizer )
        {
            /* add a input for the stream ouput */
            p_sys->sout_format.i_width  = p_sys->ti.width;
            p_sys->sout_format.i_height = p_sys->ti.height;

            p_sys->p_sout_input =
                sout_InputNew( p_dec, &p_sys->sout_format );

            if( !p_sys->p_sout_input )
            {
                msg_Err( p_dec, "cannot add a new stream" );
                block_Release( p_block );
                return VLC_EGENERIC;
            }
        }
        else
        {
            /* Initialize video output */
            int i_chroma, i_aspect;

            if( p_sys->ti.aspect_denominator )
                i_aspect = VOUT_ASPECT_FACTOR * p_sys->ti.aspect_numerator /
                    p_sys->ti.aspect_denominator;
            else
                i_aspect = VOUT_ASPECT_FACTOR *
                    p_sys->ti.frame_width / p_sys->ti.frame_height;

            i_chroma = VLC_FOURCC('Y','V','1','2');

            p_sys->p_vout =
                vout_Request( p_dec, NULL,
                              p_sys->ti.frame_width, p_sys->ti.frame_height,
                              i_chroma, i_aspect );
            if( p_sys->p_vout == NULL )
            {
                msg_Err( p_dec, "failed to create video output" );
                block_Release( p_block );
                return VLC_EGENERIC;
            }
        }

        msg_Dbg( p_dec, "%dx%d %.02f fps video, frame content "
                 "is %dx%d with offset (%d,%d)",
                 p_sys->ti.width, p_sys->ti.height,
                 (double)p_sys->ti.fps_numerator/p_sys->ti.fps_denominator,
                 p_sys->ti.frame_width, p_sys->ti.frame_height,
                 p_sys->ti.offset_x, p_sys->ti.offset_y );

        if( p_sys->b_packetizer )
        {
            i_ret = SendPacket( p_dec, &oggpacket );
            block_Release( p_block );
            return i_ret;
        }
        else
        {
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }

    if( p_sys->i_headers == 1 )
    {
        /* The next packet in order is the comments header */
        if( theora_decode_header( &p_sys->ti, &p_sys->tc, &oggpacket ) < 0 )
        {
            msg_Err( p_dec, "2nd Theora header is corrupted" );
            return VLC_EGENERIC;
        }
        p_sys->i_headers++;

        ParseTheoraComments( p_dec );

        if( p_sys->b_packetizer )
        {
            i_ret = SendPacket( p_dec, &oggpacket );
            block_Release( p_block );
            return i_ret;
        }
        else
        {
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }

    if( p_sys->i_headers == 2 )
    {
        /* The next packet in order is the codebooks header
           We need to watch out that this packet is not missing as a
           missing or corrupted header is fatal. */
        if( theora_decode_header( &p_sys->ti, &p_sys->tc, &oggpacket ) < 0 )
        {
            msg_Err( p_dec, "3rd Theora header is corrupted" );
            return VLC_EGENERIC;
        }
        p_sys->i_headers++;

        if( !p_sys->b_packetizer )
        {
            /* We have all the headers, initialize decoder */
            theora_decode_init( &p_sys->td, &p_sys->ti );
        }

        if( p_sys->b_packetizer )
        {
            i_ret = SendPacket( p_dec, &oggpacket );
            block_Release( p_block );
            return i_ret;
        }
        else
        {
            block_Release( p_block );
            return VLC_SUCCESS;
        }
    }

    i_ret = ProcessPacket( p_dec, &oggpacket, p_block->i_pts );
    block_Release( p_block );
    return i_ret;
}

/*****************************************************************************
 * ProcessPacket: processes a Vorbis packet.
 *****************************************************************************/
static int ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                          mtime_t i_pts )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Date management */
    if( i_pts > 0 && i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = i_pts;
    }

    if( p_sys->b_packetizer )
    {
        return SendPacket( p_dec, p_oggpacket );
    }
    else
    {
        return DecodePacket( p_dec, p_oggpacket );
    }
}

/*****************************************************************************
 * DecodePacket: decodes a Theora packet.
 *****************************************************************************/
static int DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    picture_t *p_pic;
    yuv_buffer yuv;

    decoder_sys_t *p_sys = p_dec->p_sys;

    theora_decode_packetin( &p_sys->td, p_oggpacket );

    /* Decode */
    theora_decode_YUVout( &p_sys->td, &yuv );

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) ) )
    {
        if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
        {
            return VLC_EGENERIC;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    if( !p_pic ) return VLC_EGENERIC;

    theora_CopyPicture( p_dec, p_pic, &yuv );

    vout_DatePicture( p_sys->p_vout, p_pic, p_sys->i_pts );
    vout_DisplayPicture( p_sys->p_vout, p_pic );

    /* Date management */
    p_sys->i_pts += ( I64C(1000000) * p_sys->ti.fps_denominator /
                      p_sys->ti.fps_numerator ); /* 1 frame per packet */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendPacket: send an ogg packet to the stream output.
 *****************************************************************************/
static int SendPacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    sout_buffer_t *p_sout_buffer =
        sout_BufferNew( p_sys->p_sout_input->p_sout, p_oggpacket->bytes );

    if( !p_sout_buffer ) return VLC_EGENERIC;

    p_dec->p_vlc->pf_memcpy( p_sout_buffer->p_buffer,
                             p_oggpacket->packet,
                             p_oggpacket->bytes );

    /* Date management */
    p_sout_buffer->i_dts = p_sout_buffer->i_pts = p_sys->i_pts;
    p_sys->i_pts += ( I64C(1000000) * p_sys->ti.fps_denominator /
                      p_sys->ti.fps_numerator ); /* 1 frame per packet */

    if( p_sys->i_headers >= 3 )
        p_sout_buffer->i_length = p_sys->i_pts - p_sout_buffer->i_pts;
    else
        p_sout_buffer->i_length = 0;

    sout_InputSendBuffer( p_sys->p_sout_input, p_sout_buffer );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseTheoraComments: FIXME should be done in demuxer
 *****************************************************************************/
static void ParseTheoraComments( decoder_t *p_dec )
{
    input_thread_t *p_input = (input_thread_t *)p_dec->p_parent;
    input_info_category_t *p_cat =
        input_InfoCategory( p_input, _("Theora Comment") );
    int i = 0;
    char *psz_name, *psz_value, *psz_comment;
    while ( i < p_dec->p_sys->tc.comments )
    {
        psz_comment = strdup( p_dec->p_sys->tc.user_comments[i] );
        if( !psz_comment )
        {
            msg_Warn( p_dec, "Out of memory" );
            break;
        }
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;
            input_AddInfo( p_cat, psz_name, psz_value );
        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * EndDecoder: theora decoder destruction
 *****************************************************************************/
static int EndDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->b_packetizer )
        vout_Request( p_dec, p_sys->p_vout, 0, 0, 0, 0 );

    if( p_sys->p_sout_input != NULL )
    {
        sout_InputDelete( p_sys->p_sout_input );
    }

    theora_info_clear( &p_sys->ti );
    theora_comment_clear( &p_sys->tc );

    free( p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * theora_CopyPicture: copy a picture from theora internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void theora_CopyPicture( decoder_t *p_dec, picture_t *p_pic,
                                yuv_buffer *yuv )
{
    int i_plane, i_line, i_width, i_dst_stride, i_src_stride;
    int i_src_xoffset, i_src_yoffset;
    uint8_t *p_dst, *p_src;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        p_src = i_plane ? (i_plane - 1 ? yuv->v : yuv->u ) : yuv->y;
        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_dst_stride  = p_pic->p[i_plane].i_pitch;
        i_src_stride  = i_plane ? yuv->uv_stride : yuv->y_stride;
        i_src_xoffset = p_dec->p_sys->ti.offset_x;
        i_src_yoffset = p_dec->p_sys->ti.offset_y;
        if( i_plane )
        {
            i_src_xoffset /= 2;
            i_src_yoffset /= 2;
        }

        p_src += (i_src_yoffset * i_src_stride + i_src_yoffset);

        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            p_dec->p_vlc->pf_memcpy( p_dst, p_src, i_width );
            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}

/*****************************************************************************
 * encoder_sys_t : theora encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Input properties
     */
    int i_headers;

    /*
     * Theora properties
     */
    theora_info      ti;                        /* theora bitstream settings */
    theora_comment   tc;                            /* theora comment header */
    theora_state     td;                   /* theora bitstream user comments */

    /*
     * Packetizer output properties
     */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           sout_format;

    /*
     * Common properties
     */
    mtime_t i_pts;
};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    if( p_enc->i_fourcc != VLC_FOURCC('t','h','e','o') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
    {
        msg_Err( p_enc, "out of memory" );
        return VLC_EGENERIC;
    }
    p_enc->p_sys = p_sys;

    p_enc->pf_header = Headers;
    p_enc->pf_encode_video = Encode;
    p_enc->format.video.i_chroma = VLC_FOURCC('I','4','2','0');

#define frame_x_offset 0
#define frame_y_offset 0
#define video_hzn 25
#define video_hzd 1
#define video_an 4
#define video_ad 3
#define video_q 5

    theora_info_init( &p_sys->ti );

    p_sys->ti.width = p_enc->format.video.i_width;
    p_sys->ti.height = p_enc->format.video.i_height;
    p_sys->ti.frame_width = p_enc->format.video.i_width;
    p_sys->ti.frame_height = p_enc->format.video.i_height;
    p_sys->ti.offset_x = frame_x_offset;
    p_sys->ti.offset_y = frame_y_offset;
    p_sys->ti.fps_numerator = video_hzn;
    p_sys->ti.fps_denominator = video_hzd;
    p_sys->ti.aspect_numerator = video_an;
    p_sys->ti.aspect_denominator = video_ad;
    p_sys->ti.colorspace = not_specified;
    p_sys->ti.target_bitrate = p_enc->i_bitrate;
    p_sys->ti.quality = video_q;

    p_sys->ti.dropframes_p = 0;
    p_sys->ti.quick_p = 1;
    p_sys->ti.keyframe_auto_p = 1;
    p_sys->ti.keyframe_frequency = 64;
    p_sys->ti.keyframe_frequency_force = 64;
    p_sys->ti.keyframe_data_target_bitrate = p_enc->i_bitrate * 1.5;
    p_sys->ti.keyframe_auto_threshold = 80;
    p_sys->ti.keyframe_mindistance = 8;
    p_sys->ti.noise_sensitivity = 1;

    theora_encode_init( &p_sys->td, &p_sys->ti );
    theora_info_clear( &p_sys->ti );
    theora_comment_init( &p_sys->tc );

    p_sys->i_headers = 0;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Headers( encoder_t *p_enc )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    ogg_packet oggpacket;
    block_t *p_block;

    /* Create theora headers */
    switch( p_sys->i_headers )
    {
    case 0:
        theora_encode_header( &p_sys->td, &oggpacket );
        break;
    case 1:
        theora_encode_comment( &p_sys->tc, &oggpacket );
        break;
    case 2:
        theora_encode_tables( &p_sys->td, &oggpacket );
        break;
    default:
        break;
    }

    p_sys->i_headers++;
    if( p_sys->i_headers > 3 ) return NULL;

    /* Ogg packet to block */
    p_block = block_New( p_enc, oggpacket.bytes );
    p_block->p_buffer = oggpacket.packet;
    p_block->i_buffer = oggpacket.bytes;
    p_block->i_dts = oggpacket.granulepos;

    return p_block;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    ogg_packet oggpacket;
    block_t *p_block;
    yuv_buffer yuv;

    /* Theora is a one-frame-in, one-frame-out system. Submit a frame
     * for compression and pull out the packet. */

    yuv.y_width  = p_pict->p[0].i_visible_pitch;
    yuv.y_height = p_pict->p[0].i_lines;
    yuv.y_stride = p_pict->p[0].i_pitch;

    yuv.uv_width  = p_pict->p[1].i_visible_pitch;
    yuv.uv_height = p_pict->p[1].i_lines;
    yuv.uv_stride = p_pict->p[1].i_pitch;

    yuv.y = p_pict->p[0].p_pixels;
    yuv.u = p_pict->p[1].p_pixels;
    yuv.v = p_pict->p[2].p_pixels;

    theora_encode_YUVin( &p_sys->td, &yuv );

    theora_encode_packetout( &p_sys->td, 0, &oggpacket );

    /* Ogg packet to block */
    p_block = block_New( p_enc, oggpacket.bytes );
    memcpy( p_block->p_buffer, oggpacket.packet, oggpacket.bytes );
    p_block->i_dts = p_block->i_pts = p_pict->date;;

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: theora encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    theora_info_clear( &p_sys->ti );
    theora_comment_clear( &p_sys->tc );

    free( p_sys );
}
