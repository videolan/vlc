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

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_description( N_("Schroedinger video decoder") )
    set_capability( "decoder", 200 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "schroedinger" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *DecodeBlock  ( decoder_t *p_dec, block_t **pp_block );

struct picture_free_t
{
   picture_t *p_pic;
   decoder_t *p_dec;
};

/*****************************************************************************
 * decoder_sys_t : Schroedinger decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Dirac properties
     */
    mtime_t i_lastpts;
    mtime_t i_frame_pts_delta;
    SchroDecoder *p_schro;
    SchroVideoFormat *p_format;
};

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

    p_dec->fmt_out.video.i_visible_width = p_sys->p_format->clean_width;
    p_dec->fmt_out.video.i_x_offset = p_sys->p_format->left_offset;
    p_dec->fmt_out.video.i_width = p_sys->p_format->width;

    p_dec->fmt_out.video.i_visible_height = p_sys->p_format->clean_height;
    p_dec->fmt_out.video.i_y_offset = p_sys->p_format->top_offset;
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
 * SchroFrameFree: schro_frame callback to release the associated picture_t
 * When schro_decoder_reset() is called there will be pictures in the
 * decoding pipeline that need to be released rather than displayed.
 *****************************************************************************/
static void SchroFrameFree( SchroFrame *frame, void *priv)
{
    struct picture_free_t *p_free = priv;

    if( !p_free )
        return;

    decoder_DeletePicture( p_free->p_dec, p_free->p_pic );
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

    p_pic = decoder_NewPicture( p_dec );

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
 * Blocks need not be Dirac dataunit aligned.
 * If a block has a PTS signaled, it applies to the first picture at or after p_block
 *
 * If this function returns a picture (!NULL), it is called again and the
 * same block is resubmitted.  To avoid this, set *pp_block to NULL;
 * If this function returns NULL, the *pp_block is lost (and leaked).
 * This function must free all blocks when finished with them.
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !pp_block ) return NULL;

    if ( *pp_block ) {
        block_t *p_block = *pp_block;

        /* reset the decoder when seeking as the decode in progress is invalid */
        /* discard the block as it is just a null magic block */
        if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY ) {
            schro_decoder_reset( p_sys->p_schro );

            p_sys->i_lastpts = -1;
            block_Release( p_block );
            *pp_block = NULL;
            return NULL;
        }

        SchroBuffer *p_schrobuffer;
        p_schrobuffer = schro_buffer_new_with_data( p_block->p_buffer, p_block->i_buffer );
        p_schrobuffer->free = SchroBufferFree;
        p_schrobuffer->priv = p_block;
        if( p_block->i_pts != VLC_TS_INVALID ) {
            mtime_t *p_pts = malloc( sizeof(*p_pts) );
            if( p_pts ) {
                *p_pts = p_block->i_pts;
                /* if this call fails, p_pts is freed automatically */
                p_schrobuffer->tag = schro_tag_new( p_pts, free );
            }
        }

        /* this stops the same block being fed back into this function if
         * we were on the next iteration of this loop to output a picture */
        *pp_block = NULL;
        schro_decoder_autoparse_push( p_sys->p_schro, p_schrobuffer );
        /* DO NOT refer to p_block after this point, it may have been freed */
    }

    while( 1 )
    {
        SchroFrame *p_schroframe;
        picture_t *p_pic;
        int state = schro_decoder_autoparse_wait( p_sys->p_schro );

        switch( state )
        {
        case SCHRO_DECODER_FIRST_ACCESS_UNIT:
            SetVideoFormat( p_dec );
            break;

        case SCHRO_DECODER_NEED_BITS:
            return NULL;

        case SCHRO_DECODER_NEED_FRAME:
            p_schroframe = CreateSchroFrameFromPic( p_dec );

            if( !p_schroframe )
            {
                msg_Err( p_dec, "Could not allocate picture for decoder");
                return NULL;
            }

            schro_decoder_add_output_picture( p_sys->p_schro, p_schroframe);
            break;

        case SCHRO_DECODER_OK: {
            SchroTag *p_tag = schro_decoder_get_picture_tag( p_sys->p_schro );
            p_schroframe = schro_decoder_pull( p_sys->p_schro );
            if( !p_schroframe || !p_schroframe->priv )
            {
                /* frame can't be one that was allocated by us
                 *   -- no private data: discard */
                if( p_tag ) schro_tag_free( p_tag );
                if( p_schroframe ) schro_frame_unref( p_schroframe );
                break;
            }
            p_pic = ((struct picture_free_t*) p_schroframe->priv)->p_pic;
            p_schroframe->priv = NULL;

            if( p_tag )
            {
                /* free is handled by schro_frame_unref */
                p_pic->date = *(mtime_t*) p_tag->value;
                schro_tag_free( p_tag );
            }
            else if( p_sys->i_lastpts >= 0 )
            {
                /* NB, this shouldn't happen since the packetizer does a
                 * very thorough job of inventing timestamps.  The
                 * following is just a very rough fall back incase packetizer
                 * is missing. */
                /* maybe it would be better to set p_pic->b_force ? */
                p_pic->date = p_sys->i_lastpts + p_sys->i_frame_pts_delta;
            }
            p_sys->i_lastpts = p_pic->date;

            schro_frame_unref( p_schroframe );
            return p_pic;
        }
        case SCHRO_DECODER_EOS:
            /* NB, the new api will not emit _EOS, it handles the reset internally */
            break;

        case SCHRO_DECODER_ERROR:
            msg_Err( p_dec, "SCHRO_DECODER_ERROR");
            return NULL;
        }
    }
}

