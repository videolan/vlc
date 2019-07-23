/*****************************************************************************
 * theora.c: theora decoder module making use of libtheora.
 *****************************************************************************
 * Copyright (C) 1999-2012 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include <vlc_input_item.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>

#include <theora/codec.h>
#include <theora/theoradec.h>
#include <theora/theoraenc.h>

#include <assert.h>
#include <limits.h>

/*****************************************************************************
 * decoder_sys_t : theora decoder descriptor
 *****************************************************************************/
typedef struct
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    bool b_has_headers;

    /*
     * Theora properties
     */
    th_info          ti;       /* theora bitstream settings */
    th_comment       tc;       /* theora comment information */
    th_dec_ctx       *tcx;     /* theora decoder context */

    /*
     * Decoding properties
     */
    bool b_decoded_first_keyframe;

    /*
     * Common properties
     */
    vlc_tick_t i_pts;
} decoder_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static int DecodeVideo  ( decoder_t *, block_t * );
static block_t *Packetize  ( decoder_t *, block_t ** );
static int  ProcessHeaders( decoder_t * );
static void *ProcessPacket ( decoder_t *, ogg_packet *, block_t * );
static void Flush( decoder_t * );

static picture_t *DecodePacket( decoder_t *, ogg_packet * );

static void ParseTheoraComments( decoder_t * );
static void theora_CopyPicture( picture_t *, th_ycbcr_buffer );

#ifdef ENABLE_SOUT
static int  OpenEncoder( vlc_object_t *p_this );
static void CloseEncoder( vlc_object_t *p_this );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
  "Enforce a quality between 1 (low) and 10 (high), instead " \
  "of specifying a particular bitrate. This will produce a VBR stream." )

#define ENC_POSTPROCESS_TEXT N_("Post processing quality")

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( "Theora" )
    set_description( N_("Theora video decoder") )
    set_capability( "video decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "theora" )
#   define DEC_CFG_PREFIX "theora-"
    add_integer( DEC_CFG_PREFIX "postproc", -1, ENC_POSTPROCESS_TEXT, NULL, true )

    add_submodule ()
    set_description( N_("Theora video packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )
    add_shortcut( "theora" )

#ifdef ENABLE_SOUT
    add_submodule ()
    set_description( N_("Theora video encoder") )
    set_capability( "encoder", 150 )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_shortcut( "theora" )

#   define ENC_CFG_PREFIX "sout-theora-"
    add_integer( ENC_CFG_PREFIX "quality", 2, ENC_QUALITY_TEXT,
                 ENC_QUALITY_LONGTEXT, false )
#endif
vlc_module_end ()

static const char *const ppsz_enc_options[] = {
    "quality", NULL
};

static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_THEORA )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
        return VLC_ENOMEM;
    p_sys->b_packetizer = b_packetizer;
    p_sys->b_has_headers = false;
    p_sys->i_pts = VLC_TICK_INVALID;
    p_sys->b_decoded_first_keyframe = false;
    p_sys->tcx = NULL;

    if( b_packetizer )
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_THEORA;
        p_dec->pf_packetize = Packetize;
    }
    else
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_I420;
        p_dec->pf_decode = DecodeVideo;
    }
    p_dec->pf_flush = Flush;

    /* Init supporting Theora structures needed in header parsing */
    th_comment_init( &p_sys->tc );
    th_info_init( &p_sys->ti );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenCommon( p_this, false );
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    /* Block to Ogg packet */
    oggpacket.packet = p_block->p_buffer;
    oggpacket.bytes = p_block->i_buffer;
    oggpacket.granulepos = p_block->i_dts;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Check for headers */
    if( !p_sys->b_has_headers )
    {
        if( ProcessHeaders( p_dec ) )
        {
            block_Release( p_block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &oggpacket, p_block );
}

static int DecodeVideo( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    picture_t *p_pic = DecodeBlock( p_dec, p_block );
    if( p_pic != NULL )
        decoder_QueueVideo( p_dec, p_pic );
    return VLCDEC_SUCCESS;
}

static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    if( pp_block == NULL ) /* No Drain */
        return NULL;
    block_t *p_block = *pp_block; *pp_block = NULL;
    if( p_block == NULL )
        return NULL;
    return DecodeBlock( p_dec, p_block );
}

/*****************************************************************************
 * ProcessHeaders: process Theora headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;
    th_setup_info *ts = NULL; /* theora setup information */
    int i_max_pp, i_pp;

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    const void *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;
    if( i_count < 3 )
        return VLC_EGENERIC;

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial Vorbis header */
    oggpacket.b_o_s  = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = (void *)pp_data[0];
    if( th_decode_headerin( &p_sys->ti, &p_sys->tc, &ts, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Theora video data" );
        goto error;
    }

    /* Set output properties */
    if( !p_sys->b_packetizer )

    switch( p_sys->ti.pixel_fmt )
    {
      case TH_PF_420:
        p_dec->fmt_out.i_codec = VLC_CODEC_I420;
        break;
      case TH_PF_422:
        p_dec->fmt_out.i_codec = VLC_CODEC_I422;
        break;
      case TH_PF_444:
        p_dec->fmt_out.i_codec = VLC_CODEC_I444;
        break;
      case TH_PF_RSVD:
      default:
        msg_Err( p_dec, "unknown chroma in theora sample" );
        break;
    }

    p_dec->fmt_out.video.i_width = p_sys->ti.frame_width;
    p_dec->fmt_out.video.i_height = p_sys->ti.frame_height;
    if( p_sys->ti.pic_width && p_sys->ti.pic_height )
    {
        p_dec->fmt_out.video.i_visible_width = p_sys->ti.pic_width;
        p_dec->fmt_out.video.i_visible_height = p_sys->ti.pic_height;

        p_dec->fmt_out.video.i_x_offset = p_sys->ti.pic_x;
        p_dec->fmt_out.video.i_y_offset = p_sys->ti.pic_y;
    }

    if( p_sys->ti.aspect_denominator && p_sys->ti.aspect_numerator )
    {
        p_dec->fmt_out.video.i_sar_num = p_sys->ti.aspect_numerator;
        p_dec->fmt_out.video.i_sar_den = p_sys->ti.aspect_denominator;
    }
    else
    {
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
    }

    if( p_sys->ti.fps_numerator > 0 && p_sys->ti.fps_denominator > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate = p_sys->ti.fps_numerator;
        p_dec->fmt_out.video.i_frame_rate_base = p_sys->ti.fps_denominator;
    }

    msg_Dbg( p_dec, "%dx%d %u/%u fps video, frame content "
             "is %dx%d with offset (%d,%d)",
             p_sys->ti.frame_width, p_sys->ti.frame_height,
             p_sys->ti.fps_numerator, p_sys->ti.fps_denominator,
             p_sys->ti.pic_width, p_sys->ti.pic_height,
             p_sys->ti.pic_x, p_sys->ti.pic_y );

    /* Some assertions based on the documentation.  These are mandatory restrictions. */
    assert( p_sys->ti.frame_height % 16 == 0 && p_sys->ti.frame_height < 1048576 );
    assert( p_sys->ti.frame_width % 16 == 0 && p_sys->ti.frame_width < 1048576 );
    assert( p_sys->ti.keyframe_granule_shift >= 0 && p_sys->ti.keyframe_granule_shift <= 31 );
    assert( p_sys->ti.pic_x <= __MIN( p_sys->ti.frame_width - p_sys->ti.pic_width, 255 ) );
    assert( p_sys->ti.pic_y <= p_sys->ti.frame_height - p_sys->ti.pic_height);
    assert( p_sys->ti.frame_height - p_sys->ti.pic_height - p_sys->ti.pic_y <= 255 );

    /* Sanity check that seems necessary for some corrupted files */
    if( p_sys->ti.frame_width < p_sys->ti.pic_width ||
        p_sys->ti.frame_height < p_sys->ti.pic_height )
    {
        msg_Warn( p_dec, "trying to correct invalid theora header "
                  "(frame size (%dx%d) is smaller than frame content (%d,%d))",
                  p_sys->ti.frame_width, p_sys->ti.frame_height,
                  p_sys->ti.pic_width, p_sys->ti.pic_height );

        if( p_sys->ti.frame_width < p_sys->ti.pic_width )
          p_sys->ti.frame_width = p_sys->ti.pic_width;
        if( p_sys->ti.frame_height < p_sys->ti.pic_height )
            p_sys->ti.frame_height = p_sys->ti.pic_height;
    }

    /* The next packet in order is the comments header */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[1];
    oggpacket.packet = (void *)pp_data[1];

    if( th_decode_headerin( &p_sys->ti, &p_sys->tc, &ts, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "2nd Theora header is corrupted" );
        goto error;
    }

    ParseTheoraComments( p_dec );

    /* The next packet in order is the codebooks header
     * We need to watch out that this packet is not missing as a
     * missing or corrupted header is fatal. */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[2];
    oggpacket.packet = (void *)pp_data[2];
    if( th_decode_headerin( &p_sys->ti, &p_sys->tc, &ts, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "3rd Theora header is corrupted" );
        goto error;
    }

    if( !p_sys->b_packetizer )
    {
        /* We have all the headers, initialize decoder */
        if ( ( p_sys->tcx = th_decode_alloc( &p_sys->ti, ts ) ) == NULL )
        {
            msg_Err( p_dec, "Could not allocate Theora decoder" );
            goto error;
        }

        i_pp = var_InheritInteger( p_dec, DEC_CFG_PREFIX "postproc" );
        if ( i_pp >= 0 && !th_decode_ctl( p_sys->tcx,
                    TH_DECCTL_GET_PPLEVEL_MAX, &i_max_pp, sizeof(int) ) )
        {
            i_pp = __MIN( i_pp, i_max_pp );
            if ( th_decode_ctl( p_sys->tcx, TH_DECCTL_SET_PPLEVEL,
                                &i_pp, sizeof(int) ) )
                msg_Err( p_dec, "Failed to set post processing level to %d",
                         i_pp );
            else
                msg_Dbg( p_dec, "Set post processing level to %d / %d",
                         i_pp, i_max_pp );
        }

    }
    else
    {
        void* p_extra = realloc( p_dec->fmt_out.p_extra,
                                 p_dec->fmt_in.i_extra );
        if( unlikely( p_extra == NULL ) )
        {
            /* Clean up the decoder setup info... we're done with it */
            th_setup_free( ts );
            return VLC_ENOMEM;
        }
        p_dec->fmt_out.p_extra = p_extra;
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }

    /* Clean up the decoder setup info... we're done with it */
    th_setup_free( ts );
    return VLC_SUCCESS;

error:
    /* Clean up the decoder setup info... we're done with it */
    th_setup_free( ts );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->i_pts = VLC_TICK_INVALID;
}

/*****************************************************************************
 * ProcessPacket: processes a theora packet.
 *****************************************************************************/
static void *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                            block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    void *p_buf;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            /* Don't send the a corrupted packet to
             * theora_decode, otherwise we get purple/green display artifacts
             * appearing in the video output */
            block_Release(p_block);
            return NULL;
        }
    }

    /* Date management */
    if( p_block->i_pts != VLC_TICK_INVALID && p_block->i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = p_block->i_pts;
    }

    if( p_sys->b_packetizer )
    {
        /* Date management */
        p_block->i_dts = p_block->i_pts = p_sys->i_pts;

        p_block->i_length = p_sys->i_pts - p_block->i_pts;

        p_buf = p_block;
    }
    else
    {
        p_buf = DecodePacket( p_dec, p_oggpacket );
        block_Release( p_block );
    }

    /* Date management */
    p_sys->i_pts += vlc_tick_from_samples( p_sys->ti.fps_denominator,
                      p_sys->ti.fps_numerator ); /* 1 frame per packet */

    return p_buf;
}

/*****************************************************************************
 * DecodePacket: decodes a Theora packet.
 *****************************************************************************/
static picture_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;
    th_ycbcr_buffer ycbcr;

    /* TODO: Implement _granpos (3rd parameter here) and add the
     * call to TH_DECCTL_SET_GRANDPOS after seek */
    /* TODO: If the return is TH_DUPFRAME, we don't need to display a new
     * frame, but we do need to keep displaying the previous one. */
    if (th_decode_packetin( p_sys->tcx, p_oggpacket, NULL ) < 0)
        return NULL; /* bad packet */

    /* Check for keyframe */
    if( !(p_oggpacket->packet[0] & 0x80) /* data packet */ &&
        !(p_oggpacket->packet[0] & 0x40) /* intra frame */ )
        p_sys->b_decoded_first_keyframe = true;

    /* If we haven't seen a single keyframe yet, don't let Theora decode
     * anything, otherwise we'll get display artifacts.  (This is impossible
     * in the general case, but can happen if e.g. we play a network stream
     * using a timed URL, such that the server doesn't start the video with a
     * keyframe). */
    if( !p_sys->b_decoded_first_keyframe )
        return NULL; /* Wait until we've decoded the first keyframe */

    if( th_decode_ycbcr_out( p_sys->tcx, ycbcr ) ) /* returns 0 on success */
        return NULL;

    /* Get a new picture */
    if( decoder_UpdateVideoFormat( p_dec ) )
        return NULL;
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic ) return NULL;

    theora_CopyPicture( p_pic, ycbcr );

    p_pic->date = p_sys->i_pts;
    p_pic->b_progressive = true;

    return p_pic;
}

/*****************************************************************************
 * ParseTheoraComments:
 *****************************************************************************/
static void ParseTheoraComments( decoder_t *p_dec )
{
    char *psz_name, *psz_value, *psz_comment;
    int i = 0;

    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Regarding the th_comment structure: */

    /* The metadata is stored as a series of (tag, value) pairs, in
       length-encoded string vectors. The first occurrence of the '='
       character delimits the tag and value. A particular tag may
       occur more than once, and order is significant. The character
       set encoding for the strings is always UTF-8, but the tag names
       are limited to ASCII, and treated as case-insensitive. See the
       Theora specification, Section 6.3.3 for details. */

    /* In filling in this structure, th_decode_headerin() will
       null-terminate the user_comment strings for safety. However,
       the bitstream format itself treats them as 8-bit clean vectors,
       possibly containing null characters, and so the length array
       should be treated as their authoritative length. */
    while ( i < p_sys->tc.comments )
    {
        int clen = p_sys->tc.comment_lengths[i];
        if ( clen <= 0 || clen >= INT_MAX ) { i++; continue; }
        psz_comment = (char *)malloc( clen + 1 );
        if( !psz_comment )
            break;
        memcpy( (void*)psz_comment, (void*)p_sys->tc.user_comments[i], clen + 1 );
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;

            if( !p_dec->p_description )
                p_dec->p_description = vlc_meta_New();
            /* TODO:  Since psz_value can contain NULLs see if there is an
             * instance where we need to preserve the full length of this string */
            if( p_dec->p_description )
                vlc_meta_AddExtra( p_dec->p_description, psz_name, psz_value );
        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * CloseDecoder: theora decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    th_info_clear(&p_sys->ti);
    th_comment_clear(&p_sys->tc);
    th_decode_free(p_sys->tcx);
    free( p_sys );
}

/*****************************************************************************
 * theora_CopyPicture: copy a picture from theora internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void theora_CopyPicture( picture_t *p_pic,
                                th_ycbcr_buffer ycbcr )
{
    int i_plane, i_planes;
    /* th_img_plane
       int  width   The width of this plane.
       int  height  The height of this plane.
       int  stride  The offset in bytes between successive rows.
       unsigned char *data  A pointer to the beginning of the first row.

       Detailed Description

       A buffer for a single color plane in an uncompressed image.

       This contains the image data in a left-to-right, top-down
       format. Each row of pixels is stored contiguously in memory,
       but successive rows need not be. Use stride to compute the
       offset of the next row. The encoder accepts both positive
       stride values (top-down in memory) and negative (bottom-up in
       memory). The decoder currently always generates images with
       positive strides.

       typedef th_img_plane th_ycbcr_buffer[3]
    */

    i_planes = __MIN(p_pic->i_planes, 3);
    for( i_plane = 0; i_plane < i_planes; i_plane++ )
    {
        plane_t src;
        src.i_lines = ycbcr[i_plane].height;
        src.p_pixels = ycbcr[i_plane].data;
        src.i_pitch = ycbcr[i_plane].stride;
        src.i_visible_pitch = src.i_pitch;
        src.i_visible_lines = src.i_lines;
        plane_CopyPixels( &p_pic->p[i_plane], &src );
    }
}

#ifdef ENABLE_SOUT
/*****************************************************************************
 * encoder_sys_t : theora encoder descriptor
 *****************************************************************************/
typedef struct
{
    /*
     * Input properties
     */
    bool b_headers;

    /*
     * Theora properties
     */
    th_info      ti;                     /* theora bitstream settings */
    th_comment   tc;                     /* theora comment header */
    th_enc_ctx   *tcx;                   /* theora context */
} encoder_sys_t;

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int i_quality;
    int t_flags;
    int max_enc_level = 0;
    int keyframe_freq_force = 64;
    ogg_packet header;
    int status;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_THEORA &&
        !p_enc->obj.force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the encoder's structure */
    if( ( p_sys = malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_video = Encode;
    p_enc->fmt_in.i_codec = VLC_CODEC_I420;
    p_enc->fmt_out.i_codec = VLC_CODEC_THEORA;

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    i_quality = var_GetInteger( p_enc, ENC_CFG_PREFIX "quality" );
    if( i_quality > 10 ) i_quality = 10;
    if( i_quality < 0 ) i_quality = 0;

    th_info_init( &p_sys->ti );

    p_sys->ti.frame_width = p_enc->fmt_in.video.i_visible_width;
    p_sys->ti.frame_height = p_enc->fmt_in.video.i_visible_height;

    if( p_sys->ti.frame_width % 16 || p_sys->ti.frame_height % 16 )
    {
        /* Pictures from the transcoder should always have a pitch
         * which is a multiple of 16 */
        p_sys->ti.frame_width = (p_sys->ti.frame_width + 15) >> 4 << 4;
        p_sys->ti.frame_height = (p_sys->ti.frame_height + 15) >> 4 << 4;

        msg_Dbg( p_enc, "padding video from %dx%d to %dx%d",
                 p_enc->fmt_in.video.i_visible_width, p_enc->fmt_in.video.i_visible_height,
                 p_sys->ti.frame_width, p_sys->ti.frame_height );
    }

    p_sys->ti.pic_width = p_enc->fmt_in.video.i_visible_width;
    p_sys->ti.pic_height = p_enc->fmt_in.video.i_visible_height;
    p_sys->ti.pic_x = 0 /*frame_x_offset*/;
    p_sys->ti.pic_y = 0 /*frame_y_offset*/;

    if( !p_enc->fmt_in.video.i_frame_rate ||
        !p_enc->fmt_in.video.i_frame_rate_base )
    {
        p_sys->ti.fps_numerator = 25;
        p_sys->ti.fps_denominator = 1;
    }
    else
    {
        p_sys->ti.fps_numerator = p_enc->fmt_in.video.i_frame_rate;
        p_sys->ti.fps_denominator = p_enc->fmt_in.video.i_frame_rate_base;
    }

    if( p_enc->fmt_in.video.i_sar_num > 0 && p_enc->fmt_in.video.i_sar_den > 0 )
    {
        unsigned i_dst_num, i_dst_den;
        vlc_ureduce( &i_dst_num, &i_dst_den,
                     p_enc->fmt_in.video.i_sar_num,
                     p_enc->fmt_in.video.i_sar_den, 0 );
        p_sys->ti.aspect_numerator = i_dst_num;
        p_sys->ti.aspect_denominator = i_dst_den;
    }
    else
    {
        p_sys->ti.aspect_numerator = 4;
        p_sys->ti.aspect_denominator = 3;
    }

    p_sys->ti.target_bitrate = p_enc->fmt_out.i_bitrate;
    p_sys->ti.quality = ((float)i_quality) * 6.3f;


    p_sys->tcx = th_encode_alloc( &p_sys->ti );
    th_comment_init( &p_sys->tc );

    /* These are no longer supported here: */
    /*
    p_sys->ti.dropframes_p = 0;
    p_sys->ti.quick_p = 1;
    p_sys->ti.keyframe_auto_p = 1;
    p_sys->ti.keyframe_frequency = 64;
    p_sys->ti.keyframe_frequency_force = 64;
    p_sys->ti.keyframe_data_target_bitrate = p_enc->fmt_out.i_bitrate * 1.5;
    p_sys->ti.keyframe_auto_threshold = 80;
    p_sys->ti.keyframe_mindistance = 8;
    p_sys->ti.noise_sensitivity = 1;
    */

    t_flags = TH_RATECTL_CAP_OVERFLOW; /* default is TH_RATECTL_CAP_OVERFLOW | TL_RATECTL_DROP_FRAMES */
    /* Turn off dropframes */
    th_encode_ctl( p_sys->tcx, TH_ENCCTL_SET_RATE_FLAGS, &t_flags, sizeof(t_flags) );

    /* turn on fast encoding */
    if ( !th_encode_ctl( p_sys->tcx, TH_ENCCTL_GET_SPLEVEL_MAX, &max_enc_level,
                sizeof(max_enc_level) ) ) /* returns 0 on success */
        th_encode_ctl( p_sys->tcx, TH_ENCCTL_SET_SPLEVEL, &max_enc_level, sizeof(max_enc_level) );

    /* Set forced distance between key frames */
    th_encode_ctl( p_sys->tcx, TH_ENCCTL_SET_KEYFRAME_FREQUENCY_FORCE,
                   &keyframe_freq_force, sizeof(keyframe_freq_force) );

    /* Create and store headers */
    while ( ( status = th_encode_flushheader( p_sys->tcx, &p_sys->tc, &header ) ) )
    {
        if ( status < 0 ) return VLC_EGENERIC;
        if( xiph_AppendHeaders( &p_enc->fmt_out.i_extra, &p_enc->fmt_out.p_extra,
                                header.bytes, header.packet ) )
        {
            p_enc->fmt_out.i_extra = 0;
            p_enc->fmt_out.p_extra = NULL;
        }
    }
    return VLC_SUCCESS;
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
    th_ycbcr_buffer ycbcr;
    unsigned i;

    if( !p_pict ) return NULL;
    /* Sanity check */
    if( p_pict->p[0].i_pitch < (int)p_sys->ti.frame_width ||
        p_pict->p[0].i_lines < (int)p_sys->ti.frame_height )
    {
        msg_Warn( p_enc, "frame is smaller than encoding size"
                  "(%ix%i->%ix%i) -> dropping frame",
                  p_pict->p[0].i_pitch, p_pict->p[0].i_lines,
                  p_sys->ti.frame_width, p_sys->ti.frame_height );
        return NULL;
    }

    /* Fill padding */
    if( p_pict->p[0].i_visible_pitch < (int)p_sys->ti.frame_width )
    {
        for( i = 0; i < p_sys->ti.frame_height; i++ )
        {
            memset( p_pict->p[0].p_pixels + i * p_pict->p[0].i_pitch +
                    p_pict->p[0].i_visible_pitch,
                    *( p_pict->p[0].p_pixels + i * p_pict->p[0].i_pitch +
                       p_pict->p[0].i_visible_pitch - 1 ),
                    p_sys->ti.frame_width - p_pict->p[0].i_visible_pitch );
        }
        for( i = 0; i < p_sys->ti.frame_height / 2; i++ )
        {
            memset( p_pict->p[1].p_pixels + i * p_pict->p[1].i_pitch +
                    p_pict->p[1].i_visible_pitch,
                    *( p_pict->p[1].p_pixels + i * p_pict->p[1].i_pitch +
                       p_pict->p[1].i_visible_pitch - 1 ),
                    p_sys->ti.frame_width / 2 - p_pict->p[1].i_visible_pitch );
            memset( p_pict->p[2].p_pixels + i * p_pict->p[2].i_pitch +
                    p_pict->p[2].i_visible_pitch,
                    *( p_pict->p[2].p_pixels + i * p_pict->p[2].i_pitch +
                       p_pict->p[2].i_visible_pitch - 1 ),
                    p_sys->ti.frame_width / 2 - p_pict->p[2].i_visible_pitch );
        }
    }

    if( p_pict->p[0].i_visible_lines < (int)p_sys->ti.frame_height )
    {
        for( i = p_pict->p[0].i_visible_lines; i < p_sys->ti.frame_height; i++ )
        {
            memset( p_pict->p[0].p_pixels + i * p_pict->p[0].i_pitch, 0,
                    p_sys->ti.frame_width );
        }
        for( i = p_pict->p[1].i_visible_lines; i < p_sys->ti.frame_height / 2; i++ )
        {
            memset( p_pict->p[1].p_pixels + i * p_pict->p[1].i_pitch, 0x80,
                    p_sys->ti.frame_width / 2 );
            memset( p_pict->p[2].p_pixels + i * p_pict->p[2].i_pitch, 0x80,
                    p_sys->ti.frame_width / 2 );
        }
    }

    /* Theora is a one-frame-in, one-frame-out system. Submit a frame
     * for compression and pull out the packet. */

    ycbcr[0].width = p_sys->ti.frame_width;
    ycbcr[0].height = p_sys->ti.frame_height;
    ycbcr[0].stride = p_pict->p[0].i_pitch;
    ycbcr[0].data = p_pict->p[0].p_pixels;

    ycbcr[1].width = p_sys->ti.frame_width / 2;
    ycbcr[1].height = p_sys->ti.frame_height / 2;
    ycbcr[1].stride = p_pict->p[1].i_pitch;
    ycbcr[1].data = p_pict->p[1].p_pixels;

    ycbcr[2].width = p_sys->ti.frame_width / 2;
    ycbcr[2].height = p_sys->ti.frame_height / 2;
    ycbcr[2].stride = p_pict->p[1].i_pitch;
    ycbcr[2].data = p_pict->p[2].p_pixels;

    if( th_encode_ycbcr_in( p_sys->tcx, ycbcr ) < 0 )
    {
        msg_Warn( p_enc, "failed encoding a frame" );
        return NULL;
    }

    th_encode_packetout( p_sys->tcx, 0, &oggpacket );

    /* Ogg packet to block */
    p_block = block_Alloc( oggpacket.bytes );
    memcpy( p_block->p_buffer, oggpacket.packet, oggpacket.bytes );
    p_block->i_dts = p_block->i_pts = p_pict->date;

    if( th_packet_iskeyframe( &oggpacket ) )
    {
        p_block->i_flags |= BLOCK_FLAG_TYPE_I;
    }

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: theora encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    th_info_clear(&p_sys->ti);
    th_comment_clear(&p_sys->tc);
    th_encode_free(p_sys->tcx);
    free( p_sys );
}
#endif
