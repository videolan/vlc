/*****************************************************************************
 * xxmc.c : HW MPEG decoder thread
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_codec.h>
#include <vlc_codec_synchro.h>

#include <unistd.h>
#ifdef __GLIBC__
    #include <mcheck.h>
#endif

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"
#include "xvmc_vld.h"

/* Aspect ratio (ISO/IEC 13818-2 section 6.3.3, table 6-3) */
#define AR_SQUARE_PICTURE       1                           /* square pixels */
#define AR_3_4_PICTURE          2                        /* 3:4 picture (TV) */
#define AR_16_9_PICTURE         3              /* 16:9 picture (wide screen) */
#define AR_221_1_PICTURE        4                  /* 2.21:1 picture (movie) */

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
    mtime_t             i_pts;
    mtime_t             i_previous_pts;
    mtime_t             i_current_pts;
    mtime_t             i_previous_dts;
    mtime_t             i_current_dts;
    int                 i_current_rate;
    picture_t *         p_picture_to_destroy;
    bool                b_garbage_pic;
    bool                b_after_sequence_header; /* is it the next frame after
                                                  * the sequence header ?    */
    bool                b_slice_i;             /* intra-slice refresh stream */

    /*
     * Output properties
     */
    decoder_synchro_t   *p_synchro;
    int                 i_aspect;
    mtime_t             i_last_frame_pts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

static picture_t *DecodeBlock( decoder_t *, block_t ** );

static picture_t *GetNewPicture( decoder_t *, uint8_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("MPEG I/II hw video decoder (using libmpeg2)") );
    set_capability( "decoder", 140 );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "xxmc" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{

    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = NULL;
    uint32_t i_accel = 0;
    FILE *f_wd_dec; 

#ifdef __GLIBC__
    mtrace();
#endif
    if( p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','v') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','1') &&
        /* Pinnacle hardware-mpeg1 */
        p_dec->fmt_in.i_codec != VLC_FOURCC('P','I','M','1') &&
        /* VIA hardware-mpeg2 */
        p_dec->fmt_in.i_codec != VLC_FOURCC('X','x','M','C') &&
        /* ATI Video */
        p_dec->fmt_in.i_codec != VLC_FOURCC('V','C','R','2') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','g','2') )
    {
        return VLC_EGENERIC;
    }

    msg_Dbg(p_dec, "OpenDecoder Entering");

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = (decoder_sys_t *)malloc(sizeof(decoder_sys_t));
    if( !p_sys )
        return VLC_ENOMEM;

    /* Initialize the thread properties */
    memset( p_sys, 0, sizeof(decoder_sys_t) );
    p_sys->p_mpeg2dec = NULL;
    p_sys->p_synchro  = NULL;
    p_sys->p_info     = NULL;
    p_sys->i_pts      = mdate() + DEFAULT_PTS_DELAY;
    p_sys->i_current_pts  = 0;
    p_sys->i_previous_pts = 0;
    p_sys->i_current_dts  = 0;
    p_sys->i_previous_dts = 0;
    p_sys->p_picture_to_destroy = NULL;
    p_sys->b_garbage_pic = 0;
    p_sys->b_slice_i  = 0;
    p_sys->b_skip     = 0;

#if defined( __i386__ )
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

#elif defined( __powerpc__ ) || defined( SYS_DARWIN )
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

    f_wd_dec = fopen("/vlc/dec_pid", "w");
    if (f_wd_dec != NULL)
    {
        fprintf(f_wd_dec, "%d\n", getpid());
        fflush(f_wd_dec);
        fclose(f_wd_dec);
    }

    msg_Dbg(p_dec, "OpenDecoder Leaving");
    return VLC_SUCCESS;
}

static void WriteDecodeFile(int value)
{
    FILE *f_wd_ok;

    f_wd_ok = fopen("/vlc/dec_ok", "w");
    if (f_wd_ok != NULL)
    {
        fprintf(f_wd_ok, "%d", value);
        fflush(f_wd_ok);
        fclose(f_wd_ok);
    }
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

    if( !pp_block || !*pp_block )
        return NULL;

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
                if( (p_block->i_flags&BLOCK_FLAG_DISCONTINUITY) &&
                    p_sys->p_synchro &&
                    p_sys->p_info->sequence &&
                    p_sys->p_info->sequence->width != (unsigned int)-1 )
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
                            break;
                        mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );
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

            case STATE_SEQUENCE:
            {
                /* Initialize video output */
                uint8_t *buf[3];
                buf[0] = buf[1] = buf[2] = NULL;

                /* Check whether the input gave a particular aspect ratio */
                if( p_dec->fmt_in.video.i_aspect )
                {
                    p_sys->i_aspect = p_dec->fmt_in.video.i_aspect;
                    if( p_sys->i_aspect <= AR_221_1_PICTURE )
                    switch( p_sys->i_aspect )
                    {
                    case AR_3_4_PICTURE:
                        p_sys->i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
                        break;
                    case AR_16_9_PICTURE:
                        p_sys->i_aspect = VOUT_ASPECT_FACTOR * 16 / 9;
                        break;
                    case AR_221_1_PICTURE:
                        p_sys->i_aspect = VOUT_ASPECT_FACTOR * 221 / 100;
                        break;
                    case AR_SQUARE_PICTURE:
                        p_sys->i_aspect = VOUT_ASPECT_FACTOR *
                                    p_sys->p_info->sequence->width /
                                    p_sys->p_info->sequence->height;
                        break;
                    }
                }
                else
                {
                    /* Use the value provided in the MPEG sequence header */
                    if( p_sys->p_info->sequence->pixel_height > 0 )
                    {
                        p_sys->i_aspect =
                            ((uint64_t)p_sys->p_info->sequence->display_width) *
                            p_sys->p_info->sequence->pixel_width *
                            VOUT_ASPECT_FACTOR /
                            p_sys->p_info->sequence->display_height /
                            p_sys->p_info->sequence->pixel_height;
                    }
                    else
                    {
                        /* Invalid aspect, assume 4:3.
                        * This shouldn't happen and if it does it is a bug
                        * in libmpeg2 (likely triggered by an invalid stream) */
                        p_sys->i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
                    }
                }

                msg_Dbg( p_dec, "%dx%d, aspect %d, %u.%03u fps",
                        p_sys->p_info->sequence->width,
                        p_sys->p_info->sequence->height, p_sys->i_aspect,
                        (uint32_t)((uint64_t)1001000000 * 27 /
                            p_sys->p_info->sequence->frame_period / 1001),
                        (uint32_t)((uint64_t)1001000000 * 27 /
                            p_sys->p_info->sequence->frame_period % 1001) );

                mpeg2_custom_fbuf( p_sys->p_mpeg2dec, 1 );

                /* Set the first 2 reference frames */
                mpeg2_set_buf( p_sys->p_mpeg2dec, buf, NULL );

                if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
                {
                    block_Release( p_block );
                    return NULL;
                }
                mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );

                p_pic->date = 0;
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
                decoder_SynchroNewPicture( p_sys->p_synchro,
                        p_sys->p_info->current_picture->flags & PIC_MASK_CODING_TYPE,
                        p_sys->p_info->current_picture->nb_fields,
                        0, 0, p_sys->i_current_rate,
                        p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY );

                if( p_sys->b_skip )
                {
                    decoder_SynchroTrash( p_sys->p_synchro );
                }
                else
                {
                    decoder_SynchroDecode( p_sys->p_synchro );
                }
                break;

            case STATE_PICTURE:
            {
                uint8_t *buf[3];
                mtime_t i_pts;
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
#else /* New interface */
                i_pts = p_sys->p_info->current_picture->flags & PIC_FLAG_TAGS ?
                    ( ( p_sys->p_info->current_picture->tag ==
                        (uint32_t)p_sys->i_current_pts ) ?
                    p_sys->i_current_pts : p_sys->i_previous_pts ) : 0;
#endif
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

                decoder_SynchroNewPicture( p_sys->p_synchro,
                    p_sys->p_info->current_picture->flags & PIC_MASK_CODING_TYPE,
                    p_sys->p_info->current_picture->nb_fields, i_pts,
                    0, p_sys->i_current_rate,
                    p_sys->p_info->sequence->flags & SEQ_FLAG_LOW_DELAY );

                if ( !(p_sys->b_slice_i
                    && ((p_sys->p_info->current_picture->flags
                            & PIC_MASK_CODING_TYPE) == P_CODING_TYPE))
                    && !decoder_SynchroChoose( p_sys->p_synchro,
                                p_sys->p_info->current_picture->flags
                                    & PIC_MASK_CODING_TYPE,
                                /*FindVout(p_dec)->render_time*/ 0 /*FIXME*/,
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

                    p_sys->p_mpeg2dec->ptr_forward_ref_picture = p_sys->p_mpeg2dec->fbuf[2]->id;
                    p_sys->p_mpeg2dec->ptr_backward_ref_picture = p_sys->p_mpeg2dec->fbuf[1]->id;

                    if ((p_sys->p_info->current_picture->flags & PIC_MASK_CODING_TYPE) != B_CODING_TYPE)
                    {
                        //if (p_sys->p_mpeg2dec->ptr_forward_ref_picture &&
                        //    p_sys->p_mpeg2dec->ptr_forward_ref_picture != picture->backward_reference_frame)
                        //    p_pic->forward_reference_frame->free (p_pic->forward_reference_frame);

                        p_sys->p_mpeg2dec->ptr_forward_ref_picture =
                                    p_sys->p_mpeg2dec->ptr_backward_ref_picture;
                        p_sys->p_mpeg2dec->ptr_backward_ref_picture = (void *)p_pic;
                    }
                    mpeg2_set_buf( p_sys->p_mpeg2dec, buf, p_pic );
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
                    p_dec->pf_picture_unlink( p_dec, p_sys->p_info->discard_fbuf->id );
                }
                /* For still frames */
                //if( state == STATE_END && p_pic ) p_pic->b_force = true;

                if( p_pic )
                {
#if 0
                    /* Avoid frames with identical timestamps.
                     * Especially needed for still frames in DVD menus. */
                     if( p_sys->i_last_frame_pts == p_pic->date ) p_pic->date++;
                        p_sys->i_last_frame_pts = p_pic->date;
#endif
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
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: libmpeg2 decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    FILE *f_wd_dec;

    if( p_sys->p_synchro ) decoder_SynchroRelease( p_sys->p_synchro );
    if( p_sys->p_mpeg2dec ) mpeg2_close( p_sys->p_mpeg2dec );

    f_wd_dec = fopen("/vlc/dec_pid", "w");
    if (f_wd_dec != NULL)
    {
        fprintf(f_wd_dec, "0\n");
        fflush(f_wd_dec);
        fclose(f_wd_dec);
    }
    free( p_sys );
}

static double get_aspect_ratio( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    double ratio;
    double mpeg1_pel_ratio[16] = {1.0 /* forbidden */,
        1.0, 0.6735, 0.7031, 0.7615, 0.8055, 0.8437, 0.8935, 0.9157,
        0.9815, 1.0255, 1.0695, 1.0950, 1.1575, 1.2015, 1.0 /*reserved*/ };

    if( !p_sys->p_mpeg2dec->decoder.mpeg1 )
    {
        /* these hardcoded values are defined on mpeg2 standard for
        * aspect ratio. other values are reserved or forbidden.  */
        switch( p_sys->p_mpeg2dec->decoder.aspect_ratio_information )
        {
            case 2:
                ratio = 4.0/3.0;
                break;
            case 3:
                ratio = 16.0/9.0;
                break;
            case 4:
                ratio = 2.11/1.0;
                break;
            case 1:
                default:
                ratio = (double)p_sys->p_mpeg2dec->decoder.width/(double)p_sys->p_mpeg2dec->decoder.height;
            break;
        }
    }
    else
    {
        /* mpeg1 constants refer to pixel aspect ratio */
        ratio = (double)p_sys->p_mpeg2dec->decoder.width/(double)p_sys->p_mpeg2dec->decoder.height;
        ratio /= mpeg1_pel_ratio[p_sys->p_mpeg2dec->decoder.aspect_ratio_information];
    }
    return ratio;
}
/*****************************************************************************
 * GetNewPicture: Get a new picture from the vout and set the buf struct
 *****************************************************************************/
static picture_t *GetNewPicture( decoder_t *p_dec, uint8_t **pp_buf )
{
    //msg_Dbg(p_dec, "GetNewPicture Entering");
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    p_dec->fmt_out.video.i_width = p_sys->p_info->sequence->width;
    p_dec->fmt_out.video.i_height = p_sys->p_info->sequence->height;
    p_dec->fmt_out.video.i_aspect = p_sys->i_aspect;

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

#if 0
    p_sys->f_wd_nb = fopen("/vlc/dec_nb", "w");
    if (p_sys->f_wd_nb != NULL)
    {
//      fprintf(p_sys->f_wd_nb, "%d\n", mdate());
        fprintf(p_sys->f_wd_nb, "%s\n", mdate());
        fflush(p_sys->f_wd_nb);
    }
#endif
    p_pic = p_dec->pf_vout_buffer_new( p_dec );

    if( p_pic == NULL ) return NULL;

    p_pic->b_progressive = p_sys->p_info->current_picture != NULL ?
        p_sys->p_info->current_picture->flags & PIC_FLAG_PROGRESSIVE_FRAME : 1;
    p_pic->b_top_field_first = p_sys->p_info->current_picture != NULL ?
        p_sys->p_info->current_picture->flags & PIC_FLAG_TOP_FIELD_FIRST : 1;
    p_pic->i_nb_fields = p_sys->p_info->current_picture != NULL ?
        p_sys->p_info->current_picture->nb_fields : 2;
    p_pic->format.i_frame_rate = p_dec->fmt_out.video.i_frame_rate;
    p_pic->format.i_frame_rate_base = p_dec->fmt_out.video.i_frame_rate_base;

    p_dec->pf_picture_link( p_dec, p_pic );

    pp_buf[0] = p_pic->p[0].p_pixels;
    pp_buf[1] = p_pic->p[1].p_pixels;
    pp_buf[2] = p_pic->p[2].p_pixels;

    /* let driver ensure this image has the right format */
#if 0
    this->driver->update_frame_format( p_pic->p_sys->p_vout, p_pic,
                                       p_dec->fmt_out.video.i_width,
                                       p_dec->fmt_out.video.i_height,
                                       p_dec->fmt_out.video.i_aspect,
                                       format, flags);
#endif
    mpeg2_xxmc_choose_coding( p_dec, &p_sys->p_mpeg2dec->decoder, p_pic,
                              get_aspect_ratio(p_dec), 0 );
    return p_pic;
}
