/*****************************************************************************
 * schroedinger.c: Dirac decoder module making use of libschroedinger.
 *          (http://www.bbc.co.uk/rd/projects/dirac/index.shtml)
 *          (http://diracvideo.org)
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jonathan Rosser <jonathan.rosser@gmail.com>
 *          David Flynn <davidf at rd dot bbc.co.uk>
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

#include <schroedinger/schro.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int        OpenDecoder  ( vlc_object_t * );
static void       CloseDecoder ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_description( N_("Schroedinger video decoder") );
    set_capability( "decoder", 200 );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "schroedinger" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *DecodeBlock  ( decoder_t *p_dec, block_t **pp_block );

/*****************************************************************************
 * picture_pts_t : store pts alongside picture number, not carried through
 * decoder
 *****************************************************************************/
struct picture_pts_t
{
   int i_empty;      //not in use
   uint32_t u_pnum;  //picture number from dirac header
   mtime_t i_pts;    //pts for this picture
};

struct picture_free_t
{
   picture_t *p_pic;
   decoder_t *p_dec;
};

/*****************************************************************************
 * decoder_sys_t : Schroedinger decoder descriptor
 *****************************************************************************/
#define PTS_TLB_SIZE 16
struct decoder_sys_t
{
    /*
     * Dirac properties
     */
    mtime_t i_lastpts;
    mtime_t i_frame_pts_delta;
    SchroDecoder *p_schro;
    SchroVideoFormat *p_format;
    struct picture_pts_t pts_tlb[PTS_TLB_SIZE];
    int i_ts_resync_hack;
};

//#define TRACE

/*****************************************************************************
 * ResetPTStlb: Purge all entries in @p_dec@'s PTS-tlb
 *****************************************************************************/
static void ResetPTStlb( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    for( int i=0; i<PTS_TLB_SIZE; i++) {
        p_sys->pts_tlb[i].i_empty = 1;
    }
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    SchroDecoder *p_schro;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('d','r','a','c') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = malloc(sizeof(decoder_sys_t));
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Initialise the schroedinger (and hence liboil libraries */
    /* This does no allocation and is safe to call */
    schro_init();

    /* Initialise the schroedinger decoder */
    if( !(p_schro = schro_decoder_new()) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys;
    p_sys->p_schro = p_schro;
    p_sys->p_format = NULL;
    p_sys->i_lastpts = -1;
    p_sys->i_frame_pts_delta = 0;
    p_sys->i_ts_resync_hack = 0;

    ResetPTStlb(p_dec);

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0');

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetPictureFormat: Set the decoded picture params to the ones from the stream
 *****************************************************************************/
static void SetVideoFormat( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    double f_aspect;

    p_sys->p_format = schro_decoder_get_video_format(p_sys->p_schro);
    if( p_sys->p_format == NULL ) return;

    p_sys->i_frame_pts_delta = INT64_C(1000000)
                            * p_sys->p_format->frame_rate_denominator
                            / p_sys->p_format->frame_rate_numerator;

    switch( p_sys->p_format->chroma_format )
    {
    case SCHRO_CHROMA_420: p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0'); break;
    case SCHRO_CHROMA_422: p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','2'); break;
    case SCHRO_CHROMA_444: p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','4','4'); break;
    default:
        p_dec->fmt_out.i_codec = 0;
        break;
    }

    p_dec->fmt_out.video.i_visible_width =
    p_dec->fmt_out.video.i_width = p_sys->p_format->width;

    p_dec->fmt_out.video.i_visible_height =
    p_dec->fmt_out.video.i_height = p_sys->p_format->height;

    /* aspect_ratio_[numerator|denominator] describes the pixel aspect ratio */
    f_aspect = (double)
        ( p_sys->p_format->aspect_ratio_numerator * p_sys->p_format->width ) /
        ( p_sys->p_format->aspect_ratio_denominator * p_sys->p_format->height);

    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR * f_aspect;

    p_dec->fmt_out.video.i_frame_rate =
        p_sys->p_format->frame_rate_numerator;
    p_dec->fmt_out.video.i_frame_rate_base =
        p_sys->p_format->frame_rate_denominator;
}

/*****************************************************************************
 * StorePicturePTS: Store the PTS value for a particular picture number
 *****************************************************************************/
static void StorePicturePTS( decoder_t *p_dec, block_t *p_block, int i_pupos )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint32_t u_pnum;

    u_pnum = GetDWBE( p_block->p_buffer + i_pupos + 13 );

    for( int i=0; i<PTS_TLB_SIZE; i++ ) {
        if( p_sys->pts_tlb[i].i_empty ) {

            p_sys->pts_tlb[i].u_pnum = u_pnum;
            p_sys->pts_tlb[i].i_pts = p_block->i_pts;
            p_sys->pts_tlb[i].i_empty = 0;

            return;
        }
    }

    msg_Err( p_dec, "Could not store PTS %"PRId64" for picture %u",
             p_block->i_pts, u_pnum );
}

/*****************************************************************************
 * GetPicturePTS: Retrieve the PTS value for a particular picture number
 *****************************************************************************/
static mtime_t GetPicturePTS( decoder_t *p_dec, uint32_t u_pnum )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for( int i=0; i<PTS_TLB_SIZE; i++ ) {
        if( (!p_sys->pts_tlb[i].i_empty) &&
            (p_sys->pts_tlb[i].u_pnum == u_pnum)) {

             p_sys->pts_tlb[i].i_empty = 1;
             return p_sys->pts_tlb[i].i_pts;
        }
    }

    msg_Err( p_dec, "Could not retrieve PTS for picture %u", u_pnum );
    return 0;
}

/*****************************************************************************
 * SchroFrameFree: schro_frame callback to release the associated picture_t
 * When schro_decoder_reset() is called there will be pictures in the
 * decoding pipeline that need to be released rather than displayed.
 *****************************************************************************/
static void SchroFrameFree( SchroFrame *frame, void *priv)
{
    struct picture_free_t *p_free = priv;

    if( !p_free )
        return;

    p_free->p_dec->pf_vout_buffer_del( p_free->p_dec, p_free->p_pic );
    free(p_free);
    (void)frame;
}

/*****************************************************************************
 * CreateSchroFrameFromPic: wrap a picture_t in a SchroFrame
 *****************************************************************************/
static SchroFrame *CreateSchroFrameFromPic( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    SchroFrame *p_schroframe = schro_frame_new();
    picture_t *p_pic = NULL;
    struct picture_free_t *p_free;

    if( !p_schroframe )
        return NULL;

    p_pic = p_dec->pf_vout_buffer_new( p_dec );

    if( !p_pic )
        return NULL;

    p_schroframe->format = SCHRO_FRAME_FORMAT_U8_420;
    if( p_sys->p_format->chroma_format == SCHRO_CHROMA_422 )
    {
        p_schroframe->format = SCHRO_FRAME_FORMAT_U8_422;
    }
    else if( p_sys->p_format->chroma_format == SCHRO_CHROMA_444 )
    {
        p_schroframe->format = SCHRO_FRAME_FORMAT_U8_444;
    }

    p_schroframe->width = p_sys->p_format->width;
    p_schroframe->height = p_sys->p_format->height;

    p_free = malloc( sizeof( *p_free ) );
    p_free->p_pic = p_pic;
    p_free->p_dec = p_dec;
    schro_frame_set_free_callback( p_schroframe, SchroFrameFree, p_free );

    for( int i=0; i<3; i++ )
    {
        p_schroframe->components[i].width = p_pic->p[i].i_visible_pitch;
        p_schroframe->components[i].stride = p_pic->p[i].i_pitch;
        p_schroframe->components[i].height = p_pic->p[i].i_visible_lines;
        p_schroframe->components[i].length =
            p_pic->p[i].i_pitch * p_pic->p[i].i_lines;
        p_schroframe->components[i].data = p_pic->p[i].p_pixels;

        if(i!=0)
        {
            p_schroframe->components[i].v_shift =
                SCHRO_FRAME_FORMAT_V_SHIFT( p_schroframe->format );
            p_schroframe->components[i].h_shift =
                SCHRO_FRAME_FORMAT_H_SHIFT( p_schroframe->format );
        }
    }

    p_pic->b_progressive = !p_sys->p_format->interlaced;
    p_pic->b_top_field_first = p_sys->p_format->top_field_first;
    p_pic->i_nb_fields = 2;

    return p_schroframe;
}

/*****************************************************************************
 * SchroBufferFree: schro_buffer callback to release the associated block_t
 *****************************************************************************/
static void SchroBufferFree( SchroBuffer *buf, void *priv )
{
    block_t *p_block = priv;

    if( !p_block )
        return;

    block_Release( p_block );
    (void)buf;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    schro_decoder_free( p_sys->p_schro );
    free( p_sys );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * Blocks must start with a Dirac parse unit.
 * Blocks must contain at least one Dirac parse unit.
 * Blocks must end with a picture parse unit.
 * Blocks must not contain more than one picture parse unit.
 * If a block has a PTS signaled, it applies to the first picture in p_block
 *   - Schroedinger has no internal means to tag pictures with a PTS
 *   - In this case, the picture number is extracted and stored in a TLB
 * When a picture is extracted from schro, it is looked up in the pts_tlb
 *   - If the picture was never tagged with a PTS, a new one is calculated
 *     based upon the frame rate and last output PTS.
 *
 * If this function returns a picture (!NULL), it is called again and the
 * same block is resubmitted.  To avoid this, set *pp_block to NULL;
 * If this function returns NULL, the *pp_block is lost (and leaked).
 * This function must free all blocks when finished with them.
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int state;
    SchroBuffer *p_schrobuffer;
    SchroFrame *p_schroframe;
    picture_t *p_pic;
    block_t *p_block;
    uint32_t u_pnum;

    if( !pp_block ) return NULL;

    p_block = *pp_block;

    if ( p_block ) do {
        /* prepare block for submission */

        if (p_sys->i_ts_resync_hack && p_sys->i_ts_resync_hack--)
            return NULL;

        if( !p_block->i_buffer ) {
            msg_Err( p_dec, "block is of zero size" );
            break;
        }

        /* reset the decoder when seeking as the decode in progress is invalid */
        /* discard the block as it is just a null magic block */
        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) {
#ifdef TRACE
            msg_Dbg( p_dec, "SCHRO_DECODER_RESET" );
#endif
            schro_decoder_reset( p_sys->p_schro );

            ResetPTStlb( p_dec );

            p_sys->i_lastpts = -1;

            /* The ts layer manages to corrupt the next packet we are to receive
             * Since schro has no sync support, we need to drop it */
            p_sys->i_ts_resync_hack = 1;

            block_Release( p_block );
            *pp_block = NULL;
            return NULL;
        }

        /* Unsatisfactory, and will later be fixed in schro:
         *  - Schro can only handle a single Dirac parse unit at a time
         *  - Multiple parse units may exist in p_block
         *  - All mapping specs so far guarantee that p_block would
         *    not contain anything after a picture
         * So, we can not give the whole block to schro, but piecemeal
         */
        size_t i_bufused = 0;
        while( schro_decoder_push_ready( p_sys->p_schro )) {
            if( p_block->i_buffer - i_bufused < 13 ) {
                *pp_block = NULL;
                block_Release( p_block );
                msg_Err( p_dec, "not enough data left in block" );
                break;
            }

            int b_bail = 0;
            size_t i_pulen = GetDWBE( p_block->p_buffer + i_bufused + 5 );
            uint8_t *p_pu = p_block->p_buffer + i_bufused;

            if( 0 == i_pulen ) {
                i_pulen = 13;
            }

            /* blocks that do not start with the parse info prefix are invalid */
            if( p_pu[0] != 'B' || p_pu[1] != 'B' ||
                p_pu[2] != 'C' || p_pu[3] != 'D')
            {
                *pp_block = NULL;
                block_Release( p_block );
                msg_Err( p_dec, "block does not start with dirac parse code" );
                break;
            }

            if( i_bufused + i_pulen > p_block->i_buffer ) {
                *pp_block = NULL;
                block_Release( p_block );
                break;
            }

            if( p_pu[4] & 0x08 )
                StorePicturePTS( p_dec, p_block, i_bufused );

            p_schrobuffer = schro_buffer_new_with_data( p_pu, i_pulen );
            if( i_pulen + i_bufused < p_block->i_buffer ) {
                /* don't let schro free this block, more data still in it */
                p_schrobuffer->free = 0;
            }
            else {
                p_schrobuffer->free = SchroBufferFree;
                p_schrobuffer->priv = p_block;
                b_bail = 1;
            }

#ifdef TRACE
            msg_Dbg( p_dec, "Inserting bytes into decoder len=%zu of %zu pts=%"PRId64,
                     i_pulen, p_block->i_buffer, p_block->i_pts);
#endif
            /* this stops the same block being fed back into this function if
             * we were on the next iteration of this loop to output a picture */
            *pp_block = NULL;
            state = schro_decoder_push( p_sys->p_schro, p_schrobuffer );

            /* DO NOT refer to p_block after this point, it may have been freed */

            i_bufused += i_pulen;

            if( state == SCHRO_DECODER_FIRST_ACCESS_UNIT ) {
#ifdef TRACE
                msg_Dbg( p_dec, "SCHRO_DECODER_FIRST_ACCESS_UNIT");
#endif
                SetVideoFormat( p_dec );
                ResetPTStlb( p_dec );

                p_schroframe = CreateSchroFrameFromPic( p_dec );
                if( p_schroframe ) {
                    schro_decoder_add_output_picture( p_sys->p_schro, p_schroframe);
                }
            }

            if( b_bail )
                break;
        }
    } while( 0 );

    while( 1 )
    {
        state = schro_decoder_wait( p_sys->p_schro );

        switch( state )
        {
        case SCHRO_DECODER_NEED_BITS:
#ifdef TRACE
            msg_Dbg( p_dec, "SCHRO_DECODER_NEED_BITS" );
#endif
            return NULL;

        case SCHRO_DECODER_NEED_FRAME:
#ifdef TRACE
            msg_Dbg( p_dec, "SCHRO_DECODER_NEED_FRAME" );
#endif
            p_schroframe = CreateSchroFrameFromPic( p_dec );

            if( !p_schroframe )
            {
                msg_Err( p_dec, "Could not allocate picture for decoder");
                return NULL;
            }

            schro_decoder_add_output_picture( p_sys->p_schro, p_schroframe);
            break;

        case SCHRO_DECODER_OK:
            u_pnum = schro_decoder_get_picture_number( p_sys->p_schro );
            p_schroframe = schro_decoder_pull( p_sys->p_schro );
            p_pic = ((struct picture_free_t*) p_schroframe->priv)->p_pic;
            p_schroframe->priv = NULL;
            schro_frame_unref( p_schroframe );

            /* solve presentation time stamp for picture.  If this picture
             * was not tagged with a pts when presented to decoder, interpolate
             * one
             * This means no need to set p_pic->b_force, as we have a pts on
             * each picture */
            p_pic->date = GetPicturePTS( p_dec, u_pnum );
            if (p_sys->i_lastpts >= 0 && p_pic->date == 0)
                p_pic->date = p_sys->i_lastpts + p_sys->i_frame_pts_delta;
            p_sys->i_lastpts = p_pic->date;

#ifdef TRACE
            msg_Dbg( p_dec, "SCHRO_DECODER_OK num=%u date=%"PRId64,
                     u_pnum, p_pic->date);
#endif
            return p_pic;

        case SCHRO_DECODER_EOS:
#ifdef TRACE
            msg_Dbg( p_dec, "SCHRO_DECODER_EOS");
#endif
            /* reset the decoder -- schro doesn't do this itself automatically */
            /* there are no more pictures in the output buffer at this point */
            schro_decoder_reset( p_sys->p_schro );
            break;

        case SCHRO_DECODER_ERROR:
#ifdef TRACE
            msg_Dbg( p_dec, "SCHRO_DECODER_ERROR");
#endif
            return NULL;
        }
    }
}

