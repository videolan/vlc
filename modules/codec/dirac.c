/*****************************************************************************
 * dirac.c: Dirac decoder/encoder module making use of libdirac.
 *          (http://www.bbc.co.uk/rd/projects/dirac/index.shtml)
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_sout.h>
#include <vlc_vout.h>

#include <libdirac_decoder/dirac_parser.h>
#include <libdirac_encoder/dirac_encoder.h>

/*****************************************************************************
 * decoder_sys_t : theora decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Dirac properties
     */
    dirac_decoder_t *p_dirac;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int        OpenDecoder  ( vlc_object_t * );
static void       CloseDecoder ( vlc_object_t * );
static picture_t *DecodeBlock  ( decoder_t *p_dec, block_t **pp_block );

static int  OpenEncoder( vlc_object_t *p_this );
static void CloseEncoder( vlc_object_t *p_this );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );

#define ENC_CFG_PREFIX "sout-dirac-"

static const char *const ppsz_enc_options[] = {
    "quality", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
  "Quality of the encoding between 1.0 (low) and 10.0 (high)." )

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_description( N_("Dirac video decoder") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "dirac" );

    add_submodule();
    set_description( N_("Dirac video encoder") );
    set_capability( "encoder", 100 );
    set_callbacks( OpenEncoder, CloseEncoder );
    add_float( ENC_CFG_PREFIX "quality", 7.0, NULL, ENC_QUALITY_TEXT,
               ENC_QUALITY_LONGTEXT, false );

vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    dirac_decoder_t *p_dirac;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('d','r','a','c') )
    {
        return VLC_EGENERIC;
    }

    /* Initialise the dirac decoder */
    if( !(p_dirac = dirac_decoder_init(0)) ) return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    p_sys->p_dirac = p_dirac;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0');

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
}

static void FreeFrameBuffer( dirac_decoder_t *p_dirac )
{
    if( p_dirac->fbuf )
    {
        int i;
        for( i = 0; i < 3; i++ )
        {
            free( p_dirac->fbuf->buf[i] );
            p_dirac->fbuf->buf[i] = 0;
        }
    }
}

/*****************************************************************************
 * GetNewPicture: Get a new picture from the vout and copy the decoder output
 *****************************************************************************/
static picture_t *GetNewPicture( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;
    int i_plane;

    switch( p_sys->p_dirac->src_params.chroma )
    {
    case format420: p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0'); break;
    case format422: p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','2'); break;
    case format444: p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','4','4'); break;    // XXX 0.6 ?
    default:
        p_dec->fmt_out.i_codec = 0;
        break;
    }

    p_dec->fmt_out.video.i_visible_width =
    p_dec->fmt_out.video.i_width = p_sys->p_dirac->src_params.width;
    p_dec->fmt_out.video.i_visible_height =
    p_dec->fmt_out.video.i_height = p_sys->p_dirac->src_params.height;
    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;

    p_dec->fmt_out.video.i_frame_rate =
        p_sys->p_dirac->src_params.frame_rate.numerator;
    p_dec->fmt_out.video.i_frame_rate_base =
        p_sys->p_dirac->src_params.frame_rate.denominator;

    /* Get a new picture */
    p_pic = p_dec->pf_vout_buffer_new( p_dec );

    if( p_pic == NULL ) return NULL;
    p_pic->b_progressive = !p_sys->p_dirac->src_params.source_sampling;
    p_pic->b_top_field_first = p_sys->p_dirac->src_params.topfieldfirst;

    p_pic->i_nb_fields = 2;

    /* Copy picture stride by stride */
    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        int i_line, i_width, i_dst_stride;
        uint8_t *p_src = p_sys->p_dirac->fbuf->buf[i_plane];
        uint8_t *p_dst = p_pic->p[i_plane].p_pixels;

        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_dst_stride = p_pic->p[i_plane].i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            vlc_memcpy( p_dst, p_src, i_width );
            p_src += i_width;
            p_dst += i_dst_stride;
        }
    }

    return p_pic;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    FreeFrameBuffer( p_sys->p_dirac );
    dirac_decoder_close( p_sys->p_dirac );
    free( p_sys );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete frames.
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dirac_decoder_state_t state;
    picture_t *p_pic;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    while( 1 )
    {
        state = dirac_parse( p_sys->p_dirac );

        switch( state )
        {
        case STATE_BUFFER:
            if( !p_block->i_buffer )
            {
                block_Release( p_block );
                return NULL;
            }

            msg_Dbg( p_dec, "STATE_BUFFER" );
            dirac_buffer( p_sys->p_dirac, p_block->p_buffer,
                          p_block->p_buffer + p_block->i_buffer );

            p_block->i_buffer = 0;
            break;

        case STATE_SEQUENCE:
        {
            /* Initialize video output */
            uint8_t *buf[3];

            msg_Dbg( p_dec, "%dx%d, chroma %i, %f fps",
                     p_sys->p_dirac->src_params.width,
                     p_sys->p_dirac->src_params.height,
                     p_sys->p_dirac->src_params.chroma,
                     (float)p_sys->p_dirac->src_params.frame_rate.numerator/
                     p_sys->p_dirac->src_params.frame_rate.denominator );

            FreeFrameBuffer( p_sys->p_dirac );
            buf[0] = malloc( p_sys->p_dirac->src_params.width *
                             p_sys->p_dirac->src_params.height );
            buf[1] = malloc( p_sys->p_dirac->src_params.chroma_width *
                             p_sys->p_dirac->src_params.chroma_height );
            buf[2] = malloc( p_sys->p_dirac->src_params.chroma_width *
                             p_sys->p_dirac->src_params.chroma_height );

            dirac_set_buf( p_sys->p_dirac, buf, NULL );
            break;
        }

        case STATE_SEQUENCE_END:
            msg_Dbg( p_dec, "SEQUENCE_END" );
            FreeFrameBuffer( p_sys->p_dirac );
            break;

        case STATE_PICTURE_AVAIL:
            msg_Dbg( p_dec, "PICTURE_AVAIL : frame_num=%d",
                     p_sys->p_dirac->frame_num );

            /* Picture available for display */
            p_pic = GetNewPicture( p_dec );
            p_pic->date = p_block->i_pts > 0 ? p_block->i_pts : p_block->i_dts;
            p_pic->b_force = 1; // HACK
            return p_pic;
            break;

        case STATE_INVALID:
            msg_Dbg( p_dec, "STATE_INVALID" );
            break;

        default:
            break;
        }
    }

    /* Never reached */
    return NULL;
}

/*****************************************************************************
 * encoder_sys_t : dirac encoder descriptor
 *****************************************************************************/
#define ENC_BUFSIZE 1024*1024
struct encoder_sys_t
{
    /*
     * Dirac properties
     */
    dirac_encoder_t *p_dirac;
    dirac_encoder_context_t ctx;

    uint8_t *p_buffer_in;
    int i_buffer_in;

    uint8_t p_buffer_out[ENC_BUFSIZE];
};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    vlc_value_t val;
    float f_quality;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC('d','r','a','c') &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof(encoder_sys_t) );
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_video = Encode;
    p_enc->fmt_in.i_codec = VLC_FOURCC('I','4','2','0');
    p_enc->fmt_in.video.i_bits_per_pixel = 12;
    p_enc->fmt_out.i_codec = VLC_FOURCC('d','r','a','c');

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    dirac_encoder_context_init( &p_sys->ctx, VIDEO_FORMAT_CUSTOM );
    /* */
    p_sys->ctx.src_params.width = p_enc->fmt_in.video.i_width;
    p_sys->ctx.src_params.height = p_enc->fmt_in.video.i_height;
    p_sys->ctx.src_params.chroma = format420;
    /* */
    p_sys->ctx.src_params.frame_rate.numerator =
        p_enc->fmt_in.video.i_frame_rate;
    p_sys->ctx.src_params.frame_rate.denominator =
        p_enc->fmt_in.video.i_frame_rate_base;
    p_sys->ctx.src_params.source_sampling = 0;
    p_sys->ctx.src_params.topfieldfirst = 0;

    var_Get( p_enc, ENC_CFG_PREFIX "quality", &val );
    f_quality = val.f_float;
    if( f_quality > 10 ) f_quality = 10;
    if( f_quality < 1 ) f_quality = 1;
    p_sys->ctx.enc_params.qf = f_quality;

    /* Initialise the encoder with the encoder context */
    p_sys->p_dirac = dirac_encoder_init( &p_sys->ctx, 0 );

    /* Set the buffer size for the encoded picture */
    p_sys->i_buffer_in = p_enc->fmt_in.video.i_width *
        p_enc->fmt_in.video.i_height * 3 / 2;
    p_sys->p_buffer_in = malloc( p_sys->i_buffer_in );

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block, *p_chain = NULL;
    int i_plane, i_line, i_width, i_src_stride;
    uint8_t *p_dst;

    /* Copy input picture in encoder input buffer (stride by stride) */
    p_dst = p_sys->p_buffer_in;
    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        uint8_t *p_src = p_pic->p[i_plane].p_pixels;
        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_src_stride = p_pic->p[i_plane].i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            vlc_memcpy( p_dst, p_src, i_width );
            p_dst += i_width;
            p_src += i_src_stride;
        }
    }

    /* Load one frame of data into encoder */
    if( dirac_encoder_load( p_sys->p_dirac, p_sys->p_buffer_in,
                            p_sys->i_buffer_in ) >= 0 )
    {
        dirac_encoder_state_t state;

        msg_Dbg( p_enc, "dirac_encoder_load" );

        /* Retrieve encoded frames from encoder */
        do
        {
            p_sys->p_dirac->enc_buf.buffer = p_sys->p_buffer_out;
            p_sys->p_dirac->enc_buf.size = ENC_BUFSIZE;
            state = dirac_encoder_output( p_sys->p_dirac );
            msg_Dbg( p_enc, "dirac_encoder_output: %i", state );
            switch( state )
            {
            case ENC_STATE_AVAIL:
                 // Encoded frame available in encoder->enc_buf
                 // Encoded frame params available in enccoder->enc_fparams
                 // Encoded frame stats available in enccoder->enc_fstats
                 p_block = block_New( p_enc, p_sys->p_dirac->enc_buf.size );
                 memcpy( p_block->p_buffer, p_sys->p_dirac->enc_buf.buffer,
                         p_sys->p_dirac->enc_buf.size );
                 p_block->i_dts = p_block->i_pts = p_pic->date;
                 block_ChainAppend( &p_chain, p_block );

                 break;
            case ENC_STATE_BUFFER:
                break;
            case ENC_STATE_INVALID:
            default:
                break;
            }
            if( p_sys->p_dirac->decoded_frame_avail )
            {
                //locally decoded frame is available in
                //encoder->dec_buf
                //locally decoded frame parameters available
                //in encoder->dec_fparams
            }
            if( p_sys->p_dirac->instr_data_avail )
            {
                //Instrumentation data (motion vectors etc.)
                //available in encoder->instr
            }

        } while( state == ENC_STATE_AVAIL );
    }
    else
    {
        msg_Dbg( p_enc, "dirac_encoder_load() error" );
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: dirac encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    msg_Dbg( p_enc, "resulting bit-rate: %lld bits/sec",
             p_sys->p_dirac->enc_seqstats.bit_rate );

    /* Free the encoder resources */
    dirac_encoder_close( p_sys->p_dirac );
 
    free( p_sys->p_buffer_in );
    free( p_sys );
}
