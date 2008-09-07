/*****************************************************************************
 * libmpeg2.c: mpeg2 video decoder module making use of libmpeg2.
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <vlc_vout.h>
#include <vlc_codec.h>

#include <mpeg2.h>

#include <vlc_codec_synchro.h>

/*****************************************************************************
 * decoder_sys_t : libmpeg2 decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * libmpeg2 properties
     */
    mpeg2dec_t          *p_mpeg2dec;
    const mpeg2_info_t  *p_info;
    bool                b_skip;

    /*
     * Input properties
     */
    mtime_t          i_previous_pts;
    mtime_t          i_current_pts;
    mtime_t          i_previous_dts;
    mtime_t          i_current_dts;
    int              i_current_rate;
    picture_t *      p_picture_to_destroy;
    bool             b_garbage_pic;
    bool             b_after_sequence_header; /* is it the next frame after
                                               * the sequence header ?    */
    bool             b_slice_i;             /* intra-slice refresh stream */
    bool             b_second_field;

    bool             b_preroll;

    /*
     * Output properties
     */
    decoder_synchro_t *p_synchro;
    int            i_aspect;
    int            i_sar_num;
    int            i_sar_den;
    mtime_t        i_last_frame_pts;

};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

static picture_t *DecodeBlock( decoder_t *, block_t ** );

static picture_t *GetNewPicture( decoder_t *, uint8_t ** );
static void GetAR( decoder_t *p_dec );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("MPEG I/II video decoder (using libmpeg2)") );
    set_capability( "decoder", 150 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "libmpeg2" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    uint32_t i_accel = 0;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','v') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','1') &&
        /* Pinnacle hardware-mpeg1 */
        p_dec->fmt_in.i_codec != VLC_FOURCC('P','I','M','1') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','2','v') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','2') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('h','d','v','2') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Initialize the thread properties */
    memset( p_sys, 0, sizeof(decoder_sys_t) );
    p_sys->p_mpeg2dec = NULL;
    p_sys->p_synchro  = NULL;
    p_sys->p_info     = NULL;
    p_sys->i_current_pts  = 0;
    p_sys->i_previous_pts = 0;
    p_sys->i_current_dts  = 0;
    p_sys->i_previous_dts = 0;
    p_sys->p_picture_to_destroy = NULL;
    p_sys->b_garbage_pic = 0;
    p_sys->b_slice_i  = 0;
    p_sys->b_second_field = 0;
    p_sys->b_skip     = 0;
    p_sys->b_preroll = false;

#if defined( __i386__ ) || defined( __x86_64__ )
    if( vlc_CPU() & CPU_CAPABILITY_MMX )
    {
        i_accel |= MPEG2_ACCEL_X86_MMX;
    }

    if( vlc_CPU() & CPU_CAPABILITY_3DNOW )
    {
        i_accel |= MPEG2_ACCEL_X86_3DNOW;
    }

    if( vlc_CPU() & CPU_CAPABILITY_MMXEXT )
    {
        i_accel |= MPEG2_ACCEL_X86_MMXEXT;
    }

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
    if( vlc_CPU() & CPU_CAPABILITY_ALTIVEC )
    {
        i_accel |= MPEG2_ACCEL_PPC_ALTIVEC;
    }

#else
    /* If we do not know this CPU, trust libmpeg2's feature detection */
    i_accel = MPEG2_ACCEL_DETECT;

#endif

    /* Set CPU acceleration features */
    mpeg2_accel( i_accel );

    /* Initialize decoder */
    p_sys->p_mpeg2dec = mpeg2_init();
    if( p_sys->p_mpeg2dec == NULL)
    {
        msg_Err( p_dec, "mpeg2_init() failed" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_info = mpeg2_info( p_sys->p_mpeg2dec );

    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: the libmpeg2 decoder
 *****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t   *p_sys = p_dec->p_sys;
    mpeg2_state_t   state;
    picture_t       *p_pic;

    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    while( 1 )
    {
        state = mpeg2_parse( p_sys->p_mpeg2dec );

        switch( state )
        {
        case STATE_BUFFER:
            if( !p_block->i_buffer )
            {
                block_Release( p_block );
                return NULL;
            }

            if( (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY
                                      | BLOCK_FLAG_CORRUPTED)) &&
                p_sys->p_synchro &&
                p_sys->p_info->sequence &&
                p_sys->p_info->sequence->width != (unsigned)-1 )
            {
                decoder_SynchroReset( p_sys->p_synchro );
                if( p_sys->p_info->current_fbuf != NULL
                    && p_sys->p_info->current_fbuf->id != NULL )
                {
                    p_sys->b_garbage_pic = 1;
                    p_pic = p_sys->p_info->current_fbuf->id;
                }
                else
                {
                    uint8_t *buf[3];
                    buf[0] = buf[1] = buf[2] = NULL;
                    if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
                    {
                        p_block->i_buffer = 0;
                        break;
                    }
                    mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );
                    mpeg2_stride( p_sys->p_mpeg2dec, p_pic->p[Y_PLANE].i_pitch );
                }
                p_sys->p_picture_to_destroy = p_pic;

                if ( p_sys->b_slice_i )
                {
                    decoder_SynchroNewPicture( p_sys->p_synchro,
                        I_CODING_TYPE, 2, 0, 0, p_sys->i_current_rate,
                        p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY );
                    decoder_SynchroDecode( p_sys->p_synchro );
                    decoder_SynchroEnd( p_sys->p_synchro, I_CODING_TYPE, 0 );
                }
            }

            if( p_block->i_flags & BLOCK_FLAG_PREROLL )
            {
                p_sys->b_preroll = true;
            }
            else if( p_sys->b_preroll )
            {
                p_sys->b_preroll = false;
                /* Reset synchro */
                decoder_SynchroReset( p_sys->p_synchro );
            }

#ifdef PIC_FLAG_PTS
            if( p_block->i_pts )
            {
                mpeg2_pts( p_sys->p_mpeg2dec, (uint32_t)p_block->i_pts );

#else /* New interface */
            if( p_block->i_pts || p_block->i_dts )
            {
                mpeg2_tag_picture( p_sys->p_mpeg2dec,
                                   (uint32_t)p_block->i_pts,
                                   (uint32_t)p_block->i_dts );
#endif
                p_sys->i_previous_pts = p_sys->i_current_pts;
                p_sys->i_current_pts = p_block->i_pts;
                p_sys->i_previous_dts = p_sys->i_current_dts;
                p_sys->i_current_dts = p_block->i_dts;
            }

            p_sys->i_current_rate = p_block->i_rate;

            mpeg2_buffer( p_sys->p_mpeg2dec, p_block->p_buffer,
                          p_block->p_buffer + p_block->i_buffer );

            p_block->i_buffer = 0;
            break;

#if MPEG2_RELEASE >= MPEG2_VERSION (0, 5, 0)

        case STATE_SEQUENCE_MODIFIED:
            GetAR( p_dec );
            break;
#endif

        case STATE_SEQUENCE:
        {
            /* Initialize video output */
            uint8_t *buf[3];
            buf[0] = buf[1] = buf[2] = NULL;

            GetAR( p_dec );

            mpeg2_custom_fbuf( p_sys->p_mpeg2dec, 1 );

            /* Set the first 2 reference frames */
            mpeg2_set_buf( p_sys->p_mpeg2dec, buf, NULL );

            if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
            {
                block_Release( p_block );
                return NULL;
            }

            mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );
            mpeg2_stride( p_sys->p_mpeg2dec, p_pic->p[Y_PLANE].i_pitch );

            /* This picture will never go through display_picture. */
            p_pic->date = 0;

            /* For some reason, libmpeg2 will put this pic twice in
             * discard_picture. This can be considered a bug in libmpeg2. */
            p_dec->pf_picture_link( p_dec, p_pic );

            if( p_sys->p_synchro )
            {
                decoder_SynchroRelease( p_sys->p_synchro );
            }
            p_sys->p_synchro = decoder_SynchroInit( p_dec,
                (uint32_t)((uint64_t)1001000000 * 27 /
                p_sys->p_info->sequence->frame_period) );
            p_sys->b_after_sequence_header = 1;
        }
        break;

        case STATE_PICTURE_2ND:
            p_sys->b_second_field = 1;
            break;

        case STATE_PICTURE:
        {
            uint8_t *buf[3];
            mtime_t i_pts, i_dts;
            buf[0] = buf[1] = buf[2] = NULL;

            if ( p_sys->b_after_sequence_header &&
                 ((p_sys->p_info->current_picture->flags &
                       PIC_MASK_CODING_TYPE) == PIC_FLAG_CODING_TYPE_P) )
            {
                /* Intra-slice refresh. Simulate a blank I picture. */
                msg_Dbg( p_dec, "intra-slice refresh stream" );
                decoder_SynchroNewPicture( p_sys->p_synchro,
                    I_CODING_TYPE, 2, 0, 0, p_sys->i_current_rate,
                    p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY );
                decoder_SynchroDecode( p_sys->p_synchro );
                decoder_SynchroEnd( p_sys->p_synchro, I_CODING_TYPE, 0 );
                p_sys->b_slice_i = 1;
            }
            p_sys->b_after_sequence_header = 0;

#ifdef PIC_FLAG_PTS
            i_pts = p_sys->p_info->current_picture->flags & PIC_FLAG_PTS ?
                ( ( p_sys->p_info->current_picture->pts ==
                    (uint32_t)p_sys->i_current_pts ) ?
                  p_sys->i_current_pts : p_sys->i_previous_pts ) : 0;
            i_dts = 0;

            /* Hack to handle demuxers which only have DTS timestamps */
            if( !i_pts && !p_block->i_pts && p_block->i_dts > 0 )
            {
                if( p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY ||
                    (p_sys->p_info->current_picture->flags &
                      PIC_MASK_CODING_TYPE) == PIC_FLAG_CODING_TYPE_B )
                {
                    i_pts = p_block->i_dts;
                }
            }
            p_block->i_pts = p_block->i_dts = 0;
            /* End hack */

#else /* New interface */

            i_pts = p_sys->p_info->current_picture->flags & PIC_FLAG_TAGS ?
                ( ( p_sys->p_info->current_picture->tag ==
                    (uint32_t)p_sys->i_current_pts ) ?
                  p_sys->i_current_pts : p_sys->i_previous_pts ) : 0;
            i_dts = p_sys->p_info->current_picture->flags & PIC_FLAG_TAGS ?
                ( ( p_sys->p_info->current_picture->tag2 ==
                    (uint32_t)p_sys->i_current_dts ) ?
                  p_sys->i_current_dts : p_sys->i_previous_dts ) : 0;
#endif

            /* If nb_fields == 1, it is a field picture, and it will be
             * followed by another field picture for which we won't call
             * decoder_SynchroNewPicture() because this would have other
             * problems, so we take it into account here.
             * This kind of sucks, but I didn't think better. --Meuuh
             */
            decoder_SynchroNewPicture( p_sys->p_synchro,
                p_sys->p_info->current_picture->flags & PIC_MASK_CODING_TYPE,
                p_sys->p_info->current_picture->nb_fields == 1 ? 2 :
                p_sys->p_info->current_picture->nb_fields, i_pts, i_dts,
                p_sys->i_current_rate,
                p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY );

            if( !p_dec->b_pace_control && !p_sys->b_preroll &&
                !(p_sys->b_slice_i
                   && ((p_sys->p_info->current_picture->flags
                         & PIC_MASK_CODING_TYPE) == P_CODING_TYPE))
                   && !decoder_SynchroChoose( p_sys->p_synchro,
                              p_sys->p_info->current_picture->flags
                                & PIC_MASK_CODING_TYPE,
                              /*p_sys->p_vout->render_time*/ 0 /*FIXME*/,
                              p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY ) )
            {
                mpeg2_skip( p_sys->p_mpeg2dec, 1 );
                p_sys->b_skip = 1;
                decoder_SynchroTrash( p_sys->p_synchro );
                mpeg2_set_buf( p_sys->p_mpeg2dec, buf, NULL );
            }
            else
            {
                mpeg2_skip( p_sys->p_mpeg2dec, 0 );
                p_sys->b_skip = 0;
                decoder_SynchroDecode( p_sys->p_synchro );

                if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
                {
                    block_Release( p_block );
                    return NULL;
                }

                mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );
        mpeg2_stride( p_sys->p_mpeg2dec, p_pic->p[Y_PLANE].i_pitch );
            }
        }
        break;

        case STATE_END:
        case STATE_SLICE:
            p_pic = NULL;
            if( p_sys->p_info->display_fbuf
                && p_sys->p_info->display_fbuf->id )
            {
                p_pic = (picture_t *)p_sys->p_info->display_fbuf->id;

                decoder_SynchroEnd( p_sys->p_synchro,
                            p_sys->p_info->display_picture->flags
                             & PIC_MASK_CODING_TYPE,
                            p_sys->b_garbage_pic );
                p_sys->b_garbage_pic = 0;

                if ( p_sys->p_picture_to_destroy != p_pic )
                {
                    p_pic->date = decoder_SynchroDate( p_sys->p_synchro );
                }
                else
                {
                    p_sys->p_picture_to_destroy = NULL;
                    p_pic->date = 0;
                }
            }

            if( p_sys->p_info->discard_fbuf &&
                p_sys->p_info->discard_fbuf->id )
            {
                p_dec->pf_picture_unlink( p_dec,
                                          p_sys->p_info->discard_fbuf->id );
            }

            /* For still frames */
            if( state == STATE_END && p_pic ) p_pic->b_force = true;

            if( p_pic )
            {
                /* Avoid frames with identical timestamps.
                 * Especially needed for still frames in DVD menus. */
                if( p_sys->i_last_frame_pts == p_pic->date ) p_pic->date++;
                p_sys->i_last_frame_pts = p_pic->date;

                return p_pic;
            }

            break;

        case STATE_INVALID:
        {
            uint8_t *buf[3];
            buf[0] = buf[1] = buf[2] = NULL;

            msg_Warn( p_dec, "invalid picture encountered" );
            if ( ( p_sys->p_info->current_picture == NULL ) ||
               ( ( p_sys->p_info->current_picture->flags &
                   PIC_MASK_CODING_TYPE) != B_CODING_TYPE ) )
            {
                if( p_sys->p_synchro ) decoder_SynchroReset( p_sys->p_synchro );
            }
            mpeg2_skip( p_sys->p_mpeg2dec, 1 );
            p_sys->b_skip = 1;

            if( p_sys->p_info->current_fbuf &&
                p_sys->p_info->current_fbuf->id )
            {
                p_sys->b_garbage_pic = 1;
                p_pic = p_sys->p_info->current_fbuf->id;
            }
            else if( !p_sys->p_info->sequence )
            {
                break;
            }
            else
            {
                if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
                    break;
                mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );
                mpeg2_stride( p_sys->p_mpeg2dec, p_pic->p[Y_PLANE].i_pitch );
            }
            p_sys->p_picture_to_destroy = p_pic;

            memset( p_pic->p[0].p_pixels, 0,
                    p_sys->p_info->sequence->width
                     * p_sys->p_info->sequence->height );
            memset( p_pic->p[1].p_pixels, 0x80,
                    p_sys->p_info->sequence->width
                     * p_sys->p_info->sequence->height / 4 );
            memset( p_pic->p[2].p_pixels, 0x80,
                    p_sys->p_info->sequence->width
                     * p_sys->p_info->sequence->height / 4 );

            if( p_sys->b_slice_i )
            {
                decoder_SynchroNewPicture( p_sys->p_synchro,
                        I_CODING_TYPE, 2, 0, 0, p_sys->i_current_rate,
                        p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY );
                decoder_SynchroDecode( p_sys->p_synchro );
                decoder_SynchroEnd( p_sys->p_synchro, I_CODING_TYPE, 0 );
            }
            break;
        }

        default:
            break;
        }
    }

    /* Never reached */
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: libmpeg2 decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_synchro ) decoder_SynchroRelease( p_sys->p_synchro );

    if( p_sys->p_mpeg2dec ) mpeg2_close( p_sys->p_mpeg2dec );

    free( p_sys );
}

/*****************************************************************************
 * GetNewPicture: Get a new picture from the vout and set the buf struct
 *****************************************************************************/
static picture_t *GetNewPicture( decoder_t *p_dec, uint8_t **pp_buf )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    p_dec->fmt_out.video.i_width = p_sys->p_info->sequence->width;
    p_dec->fmt_out.video.i_visible_width =
        p_sys->p_info->sequence->picture_width;
    p_dec->fmt_out.video.i_height = p_sys->p_info->sequence->height;
    p_dec->fmt_out.video.i_visible_height =
        p_sys->p_info->sequence->picture_height;
    p_dec->fmt_out.video.i_aspect = p_sys->i_aspect;
    p_dec->fmt_out.video.i_sar_num = p_sys->i_sar_num;
    p_dec->fmt_out.video.i_sar_den = p_sys->i_sar_den;

    if( p_sys->p_info->sequence->frame_period > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate =
            (uint32_t)( (uint64_t)1001000000 * 27 /
                        p_sys->p_info->sequence->frame_period );
        p_dec->fmt_out.video.i_frame_rate_base = 1001;
    }

    p_dec->fmt_out.i_codec =
        ( p_sys->p_info->sequence->chroma_height <
          p_sys->p_info->sequence->height ) ?
        VLC_FOURCC('I','4','2','0') : VLC_FOURCC('I','4','2','2');

    /* Get a new picture */
    p_pic = p_dec->pf_vout_buffer_new( p_dec );

    if( p_pic == NULL ) return NULL;

    p_pic->b_progressive = p_sys->p_info->current_picture != NULL ?
        p_sys->p_info->current_picture->flags & PIC_FLAG_PROGRESSIVE_FRAME : 1;
    p_pic->b_top_field_first = p_sys->p_info->current_picture != NULL ?
        p_sys->p_info->current_picture->flags & PIC_FLAG_TOP_FIELD_FIRST : 1;
    p_pic->i_nb_fields = p_sys->p_info->current_picture != NULL ?
        p_sys->p_info->current_picture->nb_fields : 2;

    p_dec->pf_picture_link( p_dec, p_pic );

    pp_buf[0] = p_pic->p[0].p_pixels;
    pp_buf[1] = p_pic->p[1].p_pixels;
    pp_buf[2] = p_pic->p[2].p_pixels;

    return p_pic;
}

/*****************************************************************************
 * GetAR: Get aspect ratio
 *****************************************************************************/
static void GetAR( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check whether the input gave a particular aspect ratio */
    if( p_dec->fmt_in.video.i_aspect )
    {
        p_sys->i_aspect = p_dec->fmt_in.video.i_aspect;
    }
    else
    {
        /* Use the value provided in the MPEG sequence header */
        if( p_sys->p_info->sequence->pixel_height > 0 )
        {
            p_sys->i_aspect =
                ((uint64_t)p_sys->p_info->sequence->picture_width) *
                p_sys->p_info->sequence->pixel_width *
                VOUT_ASPECT_FACTOR /
                p_sys->p_info->sequence->picture_height /
                p_sys->p_info->sequence->pixel_height;
            p_sys->i_sar_num = p_sys->p_info->sequence->pixel_width;
            p_sys->i_sar_den = p_sys->p_info->sequence->pixel_height;
        }
        else
        {
            /* Invalid aspect, assume 4:3.
             * This shouldn't happen and if it does it is a bug
             * in libmpeg2 (likely triggered by an invalid stream) */
            p_sys->i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
            p_sys->i_sar_num = p_sys->p_info->sequence->picture_height * 4;
            p_sys->i_sar_den = p_sys->p_info->sequence->picture_width * 3;
        }
    }

    msg_Dbg( p_dec, "%dx%d (display %d,%d), aspect %d, sar %i:%i, %u.%03u fps",
             p_sys->p_info->sequence->picture_width,
             p_sys->p_info->sequence->picture_height,
             p_sys->p_info->sequence->display_width,
             p_sys->p_info->sequence->display_height,
             p_sys->i_aspect, p_sys->i_sar_num, p_sys->i_sar_den,
             (uint32_t)((uint64_t)1001000000 * 27 /
                 p_sys->p_info->sequence->frame_period / 1001),
             (uint32_t)((uint64_t)1001000000 * 27 /
                 p_sys->p_info->sequence->frame_period % 1001) );
}
