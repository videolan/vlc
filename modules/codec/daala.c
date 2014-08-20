/*****************************************************************************
 * daala.c: daala codec module making use of libdaala.
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Tristan Matthews <le.businessman@gmail.com>
 *   * Based on theora.c by: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_input.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>

#include <daala/codec.h>
#include <daala/daaladec.h>

#include <limits.h>

/*****************************************************************************
 * decoder_sys_t : daala decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    bool b_has_headers;

    /*
     * Daala properties
     */
    daala_info          di;       /* daala bitstream settings */
    daala_comment       dc;       /* daala comment information */
    daala_dec_ctx       *dcx;     /* daala decoder context */

    /*
     * Decoding properties
     */
    bool b_decoded_first_keyframe;

    /*
     * Common properties
     */
    mtime_t i_pts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static void *DecodeBlock  ( decoder_t *, block_t ** );
static int  ProcessHeaders( decoder_t * );
static void *ProcessPacket ( decoder_t *, ogg_packet *, block_t ** );

static picture_t *DecodePacket( decoder_t *, ogg_packet * );

static void ParseDaalaComments( decoder_t * );
static void daala_CopyPicture( picture_t *, od_img * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( "Daala" )
    set_description( N_("Daala video decoder") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "daala" )
    add_submodule ()
    set_description( N_("Daala video packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )
    add_shortcut( "daala" )

vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_DAALA )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = malloc(sizeof(*p_sys));
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_dec->p_sys = p_sys;
    p_dec->p_sys->b_packetizer = false;
    p_sys->b_has_headers = false;
    p_sys->i_pts = VLC_TS_INVALID;
    p_sys->b_decoded_first_keyframe = false;
    p_sys->dcx = NULL;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_I420;

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    /* Init supporting Daala structures needed in header parsing */
    daala_comment_init( &p_sys->dc );
    daala_info_init( &p_sys->di );

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS )
    {
        p_dec->p_sys->b_packetizer = true;
        p_dec->fmt_out.i_codec = VLC_CODEC_DAALA;
    }

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    ogg_packet oggpacket;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

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

    /* If we haven't seen a single keyframe yet, set to preroll,
     * otherwise we'll get display artifacts.  (This is impossible
     * in the general case, but can happen if e.g. we play a network stream
     * using a timed URL, such that the server doesn't start the video with a
     * keyframe). */
    if( !p_sys->b_decoded_first_keyframe )
        p_block->i_flags |= BLOCK_FLAG_PREROLL; /* Wait until we've decoded the first keyframe */

    return ProcessPacket( p_dec, &oggpacket, pp_block );
}

/*****************************************************************************
 * ProcessHeaders: process Daala headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    int ret = VLC_SUCCESS;
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;
    daala_setup_info *ds = NULL; /* daala setup information */

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    void     *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;
    if( i_count < 3 )
        return VLC_EGENERIC;

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial info header */
    oggpacket.b_o_s  = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = pp_data[0];
    if( daala_decode_header_in( &p_sys->di, &p_sys->dc, &ds, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Daala video data" );
        ret = VLC_EGENERIC;
        goto cleanup;
    }

    /* Set output properties */
    if( !p_sys->b_packetizer )
    {
        if( p_sys->di.plane_info[0].xdec == 0 && p_sys->di.plane_info[0].ydec == 0 &&
            p_sys->di.plane_info[1].xdec == 1 && p_sys->di.plane_info[1].ydec == 1 &&
            p_sys->di.plane_info[2].xdec == 1 && p_sys->di.plane_info[2].ydec == 1 )
        {
            p_dec->fmt_out.i_codec = VLC_CODEC_I420;
        }
        else if( p_sys->di.plane_info[0].xdec == 0 && p_sys->di.plane_info[0].ydec == 0 &&
                 p_sys->di.plane_info[1].xdec == 0 && p_sys->di.plane_info[1].ydec == 0 &&
                 p_sys->di.plane_info[2].xdec == 0 && p_sys->di.plane_info[2].ydec == 0 )
        {
            p_dec->fmt_out.i_codec = VLC_CODEC_I444;
        }
        else
        {
            msg_Err( p_dec, "unknown chroma in daala sample" );
        }
    }

    p_dec->fmt_out.video.i_width = p_sys->di.pic_width;
    p_dec->fmt_out.video.i_height = p_sys->di.pic_height;
    if( p_sys->di.pic_width && p_sys->di.pic_height )
    {
        p_dec->fmt_out.video.i_visible_width = p_sys->di.pic_width;
        p_dec->fmt_out.video.i_visible_height = p_sys->di.pic_height;
    }

    if( p_sys->di.pixel_aspect_denominator && p_sys->di.pixel_aspect_numerator )
    {
        p_dec->fmt_out.video.i_sar_num = p_sys->di.pixel_aspect_numerator;
        p_dec->fmt_out.video.i_sar_den = p_sys->di.pixel_aspect_denominator;
    }
    else
    {
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
    }

    if( p_sys->di.timebase_numerator > 0 && p_sys->di.timebase_denominator > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate = p_sys->di.timebase_numerator;
        p_dec->fmt_out.video.i_frame_rate_base = p_sys->di.timebase_denominator;
    }

    msg_Dbg( p_dec, "%dx%d %.02f fps video, frame content ",
             p_sys->di.pic_width, p_sys->di.pic_height,
             (double)p_sys->di.timebase_numerator/p_sys->di.timebase_denominator );

    /* The next packet in order is the comments header */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[1];
    oggpacket.packet = pp_data[1];

    if( daala_decode_header_in( &p_sys->di, &p_sys->dc, &ds, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "Daala comment header is corrupted" );
        ret = VLC_EGENERIC;
        goto cleanup;
    }

    ParseDaalaComments( p_dec );

    /* The next packet in order is the setup header
     * We need to watch out that this packet is not missing as a
     * missing or corrupted header is fatal. */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[2];
    oggpacket.packet = pp_data[2];
    if( daala_decode_header_in( &p_sys->di, &p_sys->dc, &ds, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "Daala setup header is corrupted" );
        ret = VLC_EGENERIC;
        goto cleanup;
    }

    if( !p_sys->b_packetizer )
    {
        /* We have all the headers, initialize decoder */
        if ( ( p_sys->dcx = daala_decode_alloc( &p_sys->di, ds ) ) == NULL )
        {
            msg_Err( p_dec, "Could not allocate Daala decoder" );
            ret = VLC_EGENERIC;
            goto cleanup;
        }
    }
    else
    {
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = xrealloc( p_dec->fmt_out.p_extra,
                p_dec->fmt_out.i_extra );
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }

cleanup:
    /* Clean up the decoder setup info... we're done with it */
    daala_setup_free( ds );

    return ret;
}

/*****************************************************************************
 * ProcessPacket: processes a daala packet.
 *****************************************************************************/
static void *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                            block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;
    void *p_buf;

    if( ( p_block->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) != 0 )
    {
        /* Don't send the the first packet after a discontinuity to
         * daala_decode, otherwise we get purple/green display artifacts
         * appearing in the video output */
        block_Release(p_block);
        return NULL;
    }

    /* Date management */
    if( p_block->i_pts > VLC_TS_INVALID && p_block->i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = p_block->i_pts;
    }

    *pp_block = NULL; /* To avoid being fed the same packet again */

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
    p_sys->i_pts += ( CLOCK_FREQ * p_sys->di.timebase_denominator /
                      p_sys->di.timebase_numerator ); /* 1 frame per packet */

    return p_buf;
}

/*****************************************************************************
 * DecodePacket: decodes a Daala packet.
 *****************************************************************************/
static picture_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;
    od_img ycbcr;

    if (daala_decode_packet_in( p_sys->dcx, &ycbcr, p_oggpacket ) < 0)
        return NULL; /* bad packet */

    /* Check for keyframe */
    if( daala_packet_iskeyframe( p_oggpacket->packet, p_oggpacket->bytes ) )
        p_sys->b_decoded_first_keyframe = true;

    /* Get a new picture */
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic ) return NULL;

    daala_CopyPicture( p_pic, &ycbcr );

    p_pic->date = p_sys->i_pts;

    return p_pic;
}

/*****************************************************************************
 * ParseDaalaComments:
 *****************************************************************************/
static void ParseDaalaComments( decoder_t *p_dec )
{
    char *psz_name, *psz_value, *psz_comment;
    /* Regarding the daala_comment structure: */

    /* The metadata is stored as a series of (tag, value) pairs, in
       length-encoded string vectors. The first occurrence of the '='
       character delimits the tag and value. A particular tag may
       occur more than once, and order is significant. The character
       set encoding for the strings is always UTF-8, but the tag names
       are limited to ASCII, and treated as case-insensitive. See the
       Theora specification, Section 6.3.3 for details. */

    /* In filling in this structure, daala_decode_header_in() will
       null-terminate the user_comment strings for safety. However,
       the bitstream format itself treats them as 8-bit clean vectors,
       possibly containing null characters, and so the length array
       should be treated as their authoritative length. */
    for ( int i = 0; i < p_dec->p_sys->dc.comments; i++ )
    {
        int clen = p_dec->p_sys->dc.comment_lengths[i];
        if ( clen <= 0 || clen >= INT_MAX ) { continue; }
        psz_comment = malloc( clen + 1 );
        if( !psz_comment )
            break;
        memcpy( (void*)psz_comment, (void*)p_dec->p_sys->dc.user_comments[i], clen + 1 );
        psz_comment[clen] = '\0';

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
    }
}

/*****************************************************************************
 * CloseDecoder: daala decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    daala_info_clear(&p_sys->di);
    daala_comment_clear(&p_sys->dc);
    daala_decode_free(p_sys->dcx);
    free( p_sys );
}

/*****************************************************************************
 * daala_CopyPicture: copy a picture from daala internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void daala_CopyPicture( picture_t *p_pic,
                               od_img *ycbcr )
{
    const int i_planes = p_pic->i_planes < 3 ? p_pic->i_planes : 3;
    for( int i_plane = 0; i_plane < i_planes; i_plane++ )
    {
        const int i_total_lines = __MIN(p_pic->p[i_plane].i_lines,
                ycbcr->height >> ycbcr->planes[i_plane].ydec);
        uint8_t *p_dst = p_pic->p[i_plane].p_pixels;
        uint8_t *p_src = ycbcr->planes[i_plane].data;
        const int i_dst_stride  = p_pic->p[i_plane].i_pitch;
        const int i_src_stride  = ycbcr->planes[i_plane].ystride;
        for( int i_line = 0; i_line < i_total_lines; i_line++ )
        {
            memcpy( p_dst, p_src, i_src_stride );
            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}
