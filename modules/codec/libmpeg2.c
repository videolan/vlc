/*****************************************************************************
 * libmpeg2.c: mpeg2 video decoder module making use of libmpeg2.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: libmpeg2.c,v 1.22 2003/06/10 23:01:40 massiot Exp $
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <mpeg2dec/mpeg2.h>

#include "vout_synchro.h"

/* Aspect ratio (ISO/IEC 13818-2 section 6.3.3, table 6-3) */
#define AR_SQUARE_PICTURE       1                           /* square pixels */
#define AR_3_4_PICTURE          2                        /* 3:4 picture (TV) */
#define AR_16_9_PICTURE         3              /* 16:9 picture (wide screen) */
#define AR_221_1_PICTURE        4                  /* 2.21:1 picture (movie) */

/*****************************************************************************
 * dec_thread_t : libmpeg2 decoder thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * libmpeg2 properties
     */
    mpeg2dec_t          *p_mpeg2dec;
    const mpeg2_info_t  *p_info;
    vlc_bool_t          b_skip;

    /*
     * Input properties
     */
    decoder_fifo_t   *p_fifo;                  /* stores the PES stream data */
    pes_packet_t     *p_pes;                  /* current PES we are decoding */
    mtime_t          i_pts;
    mtime_t          i_previous_pts;
    mtime_t          i_current_pts;
    mtime_t          i_period_remainder;
    int              i_current_rate;
    picture_t *      p_picture_to_destroy;
    vlc_bool_t       b_garbage_pic;
    vlc_bool_t       b_after_sequence_header; /* is it the next frame after
                                               * the sequence header ?    */
    vlc_bool_t       b_slice_i;             /* intra-slice refresh stream */

    /*
     * Output properties
     */
    vout_thread_t *p_vout;
    vout_synchro_t *p_synchro;

} dec_thread_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder  ( vlc_object_t * );
static int  RunDecoder   ( decoder_fifo_t * );
static void CloseDecoder ( dec_thread_t * );

static picture_t *GetNewPicture( dec_thread_t *, uint8_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG I/II video decoder (using libmpeg2)") );
    set_capability( "decoder", 40 );
    set_callbacks( OpenDecoder, NULL );
    add_shortcut( "libmpeg2" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('m','p','g','v') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: the libmpeg2 decoder
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    dec_thread_t    *p_dec;
    data_packet_t   *p_data = NULL;
    mpeg2_state_t   state;
    picture_t       *p_pic;
    int             i_aspect, i_chroma;

    /* Allocate the memory needed to store the thread's structure */
    if( (p_dec = (dec_thread_t *)malloc (sizeof(dec_thread_t)) )
        == NULL)
    {
        msg_Err( p_fifo, "out of memory" );
        goto error;
    }

    /* Initialize the thread properties */
    memset( p_dec, 0, sizeof(dec_thread_t) );
    p_dec->p_fifo     = p_fifo;
    p_dec->p_pes      = NULL;
    p_dec->p_vout     = NULL;
    p_dec->p_mpeg2dec = NULL;
    p_dec->p_synchro  = NULL;
    p_dec->p_info     = NULL;
    p_dec->i_pts      = mdate() + DEFAULT_PTS_DELAY;
    p_dec->i_current_pts  = 0;
    p_dec->i_previous_pts = 0;
    p_dec->i_period_remainder = 0;
    p_dec->p_picture_to_destroy = NULL;
    p_dec->b_garbage_pic = 0;
    p_dec->b_slice_i  = 0;
    p_dec->b_skip     = 0;

    /* Initialize decoder */
    p_dec->p_mpeg2dec = mpeg2_init();
    if( p_dec->p_mpeg2dec == NULL)
        goto error;

    p_dec->p_info = mpeg2_info( p_dec->p_mpeg2dec );

    /* libmpeg2 decoder thread's main loop */
    while( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        state = mpeg2_parse( p_dec->p_mpeg2dec );

        switch( state )
        {
        case STATE_BUFFER:
            /* Feed libmpeg2 a data packet at a time */
            if( p_data == NULL )
            {
                /* Get the next PES */
                if( p_dec->p_pes )
                    input_DeletePES( p_dec->p_fifo->p_packets_mgt,
                                     p_dec->p_pes );

                input_ExtractPES( p_dec->p_fifo, &p_dec->p_pes );
                if( !p_dec->p_pes )
                {
                    p_dec->p_fifo->b_error = 1;
                    break;
                }

                if( p_dec->p_pes->b_discontinuity && p_dec->p_synchro 
                     && p_dec->p_info->sequence->width != (unsigned)-1 )
                {
                    vout_SynchroReset( p_dec->p_synchro );
                    if ( p_dec->p_info->current_fbuf != NULL
                          && p_dec->p_info->current_fbuf->id != NULL )
                    {
                        p_dec->b_garbage_pic = 1;
                        p_pic = p_dec->p_info->current_fbuf->id;
                    }
                    else
                    {
                        uint8_t *buf[3];
                        buf[0] = buf[1] = buf[2] = NULL;
                        if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
                            break;
                        mpeg2_set_buf( p_dec->p_mpeg2dec, buf, p_pic );
                    }
                    p_dec->p_picture_to_destroy = p_pic;

                    memset( p_pic->p[0].p_pixels, 0,
                            p_dec->p_info->sequence->width
                             * p_dec->p_info->sequence->height );
                    memset( p_pic->p[1].p_pixels, 0x80,
                            p_dec->p_info->sequence->width
                             * p_dec->p_info->sequence->height / 4 );
                    memset( p_pic->p[2].p_pixels, 0x80,
                            p_dec->p_info->sequence->width
                             * p_dec->p_info->sequence->height / 4 );

                    if ( p_dec->b_slice_i )
                    {
                        vout_SynchroNewPicture( p_dec->p_synchro,
                            I_CODING_TYPE, 2, 0, 0, p_dec->i_current_rate );
                        vout_SynchroDecode( p_dec->p_synchro );
                        vout_SynchroEnd( p_dec->p_synchro, I_CODING_TYPE, 0 );
                    }
                }

                if( p_dec->p_pes->i_pts )
                {
                    mpeg2_pts( p_dec->p_mpeg2dec,
                               (uint32_t)p_dec->p_pes->i_pts );
                    p_dec->i_previous_pts = p_dec->i_current_pts;
                    p_dec->i_current_pts = p_dec->p_pes->i_pts;
                }

                p_dec->i_current_rate = p_dec->p_pes->i_rate;
                p_data = p_dec->p_pes->p_first;
            }

            if( p_data != NULL )
            {
                mpeg2_buffer( p_dec->p_mpeg2dec,
                              p_data->p_payload_start,
                              p_data->p_payload_end );

                p_data = p_data->p_next;
            }
            break;

        case STATE_SEQUENCE:
        {
            /* Initialize video output */
            uint8_t *buf[3];
            buf[0] = buf[1] = buf[2] = NULL;

            /* Check whether the input gives a particular aspect ratio */
            if( p_dec->p_fifo->p_demux_data
                && ( *(int*)(p_dec->p_fifo->p_demux_data) & 0x7 ) )
            {
                i_aspect = *(int*)(p_dec->p_fifo->p_demux_data);
                switch( i_aspect )
                {
                case AR_3_4_PICTURE:
                    i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
                    break;
                case AR_16_9_PICTURE:
                    i_aspect = VOUT_ASPECT_FACTOR * 16 / 9;
                    break;
                case AR_221_1_PICTURE:
                    i_aspect = VOUT_ASPECT_FACTOR * 221 / 100;
                    break;
                case AR_SQUARE_PICTURE:
                default:
                    i_aspect = VOUT_ASPECT_FACTOR *
                                   p_dec->p_info->sequence->width /
                                   p_dec->p_info->sequence->height;
                    break;
                }
            }
            else
            {
                /* Use the value provided in the MPEG sequence header */
                i_aspect = ((uint64_t)p_dec->p_info->sequence->display_width) *
                    p_dec->p_info->sequence->pixel_width * VOUT_ASPECT_FACTOR /
                    p_dec->p_info->sequence->display_height /
                    p_dec->p_info->sequence->pixel_height;
            }

            i_chroma = VLC_FOURCC('Y','V','1','2');

            p_dec->p_vout = vout_Request( p_dec->p_fifo, p_dec->p_vout,
                                          p_dec->p_info->sequence->width,
                                          p_dec->p_info->sequence->height,
                                          i_chroma, i_aspect );

            msg_Dbg( p_dec->p_fifo, "%dx%d, aspect %d, %u.%03u fps",
                     p_dec->p_info->sequence->width,
                     p_dec->p_info->sequence->height, i_aspect,
                     (u32)((u64)1001000000 * 27 / p_dec->p_info->sequence->frame_period / 1001),
                     (u32)((u64)1001000000 * 27 / p_dec->p_info->sequence->frame_period % 1001) );

            mpeg2_custom_fbuf( p_dec->p_mpeg2dec, 1 );

            /* Set the first 2 reference frames */
            mpeg2_set_buf( p_dec->p_mpeg2dec, buf, NULL );

            if( (p_pic = GetNewPicture( p_dec, buf )) == NULL ) break;
            memset( p_pic->p[0].p_pixels, 0,
                    p_dec->p_info->sequence->width
                     * p_dec->p_info->sequence->height );
            memset( p_pic->p[1].p_pixels, 0x80,
                    p_dec->p_info->sequence->width
                     * p_dec->p_info->sequence->height / 4 );
            memset( p_pic->p[2].p_pixels, 0x80,
                    p_dec->p_info->sequence->width
                     * p_dec->p_info->sequence->height / 4 );
            mpeg2_set_buf( p_dec->p_mpeg2dec, buf, p_pic );
            /* This picture will never go through display_picture. */
            vout_DatePicture( p_dec->p_vout, p_pic, 0 );
            vout_DisplayPicture( p_dec->p_vout, p_pic );
            /* For some reason, libmpeg2 will put this pic twice in
             * discard_picture. This can be considered a bug in libmpeg2. */
            vout_LinkPicture( p_dec->p_vout, p_pic );

            if ( p_dec->p_synchro )
            {
                vout_SynchroRelease( p_dec->p_synchro );
            }
            p_dec->p_synchro = vout_SynchroInit( p_dec->p_fifo, p_dec->p_vout,
                (u32)((u64)1001000000 * 27 / p_dec->p_info->sequence->frame_period) );
            p_dec->b_after_sequence_header = 1;
        }
        break;

        case STATE_PICTURE_2ND:
            vout_SynchroNewPicture( p_dec->p_synchro,
                p_dec->p_info->current_picture->flags & PIC_MASK_CODING_TYPE,
                p_dec->p_info->current_picture->nb_fields,
                0, 0,
                p_dec->i_current_rate );

            if ( p_dec->b_skip )
            {
                vout_SynchroTrash( p_dec->p_synchro );
            }
            else
            {
                vout_SynchroDecode( p_dec->p_synchro );
            }
            break;

        case STATE_PICTURE:
        {
            uint8_t *buf[3];
            buf[0] = buf[1] = buf[2] = NULL;

            if ( p_dec->b_after_sequence_header
                  && ((p_dec->p_info->current_picture->flags
                        & PIC_MASK_CODING_TYPE)
                       == PIC_FLAG_CODING_TYPE_P) )
            {
                /* Intra-slice refresh. Simulate a blank I picture. */
                msg_Dbg( p_dec->p_fifo, "intra-slice refresh stream" );
                vout_SynchroNewPicture( p_dec->p_synchro,
                    I_CODING_TYPE, 2, 0, 0, p_dec->i_current_rate );
                vout_SynchroDecode( p_dec->p_synchro );
                vout_SynchroEnd( p_dec->p_synchro, I_CODING_TYPE, 0 );
                p_dec->b_slice_i = 1;
            }
            p_dec->b_after_sequence_header = 0;

            vout_SynchroNewPicture( p_dec->p_synchro,
                p_dec->p_info->current_picture->flags & PIC_MASK_CODING_TYPE,
                p_dec->p_info->current_picture->nb_fields,
                (p_dec->p_info->current_picture->flags & PIC_FLAG_PTS) ?
                    ( (p_dec->p_info->current_picture->pts ==
                                (uint32_t)p_dec->i_current_pts) ?
                              p_dec->i_current_pts : p_dec->i_previous_pts ) : 0,
                0,
                p_dec->i_current_rate );

            if ( !(p_dec->b_slice_i
                   && ((p_dec->p_info->current_picture->flags
                         & PIC_MASK_CODING_TYPE) == P_CODING_TYPE))
                   && !vout_SynchroChoose( p_dec->p_synchro,
                              p_dec->p_info->current_picture->flags
                                & PIC_MASK_CODING_TYPE ) )
            {
                mpeg2_skip( p_dec->p_mpeg2dec, 1 );
                p_dec->b_skip = 1;
                vout_SynchroTrash( p_dec->p_synchro );
                mpeg2_set_buf( p_dec->p_mpeg2dec, buf, NULL );
            }
            else
            {
                mpeg2_skip( p_dec->p_mpeg2dec, 0 );
                p_dec->b_skip = 0;
                vout_SynchroDecode( p_dec->p_synchro );
                if( (p_pic = GetNewPicture( p_dec, buf )) == NULL ) break;
                mpeg2_set_buf( p_dec->p_mpeg2dec, buf, p_pic );
            }
        }
        break;

        case STATE_END:
        case STATE_SLICE:
            if( p_dec->p_info->display_fbuf
                && p_dec->p_info->display_fbuf->id )
            {
                p_pic = (picture_t *)p_dec->p_info->display_fbuf->id;

                vout_SynchroEnd( p_dec->p_synchro,
                            p_dec->p_info->display_picture->flags
                             & PIC_MASK_CODING_TYPE,
                            p_dec->b_garbage_pic );
                p_dec->b_garbage_pic = 0;
                vout_DisplayPicture( p_dec->p_vout, p_pic );

                if ( p_dec->p_picture_to_destroy != p_pic )
                {
                    vout_DatePicture( p_dec->p_vout, p_pic,
                        vout_SynchroDate( p_dec->p_synchro ) );
                }
                else
                {
                    p_dec->p_picture_to_destroy = NULL;
                    vout_DatePicture( p_dec->p_vout, p_pic, 0 );
                }
            }

            if( p_dec->p_info->discard_fbuf &&
                p_dec->p_info->discard_fbuf->id )
            {
                p_pic = (picture_t *)p_dec->p_info->discard_fbuf->id;
                vout_UnlinkPicture( p_dec->p_vout, p_pic );
            }
            break;

        case STATE_INVALID:
        {
            uint8_t *buf[3];
            buf[0] = buf[1] = buf[2] = NULL;

            msg_Warn( p_dec->p_fifo, "invalid picture encountered" );
            if ( ( p_dec->p_info->current_picture == NULL ) || 
               ( ( p_dec->p_info->current_picture->flags & PIC_MASK_CODING_TYPE)
                  != B_CODING_TYPE ) )
            {
                vout_SynchroReset( p_dec->p_synchro );
            }
            mpeg2_skip( p_dec->p_mpeg2dec, 1 );
            p_dec->b_skip = 1;

            if( p_dec->p_info->current_fbuf &&
                p_dec->p_info->current_fbuf->id )
            {
                p_dec->b_garbage_pic = 1;
                p_pic = p_dec->p_info->current_fbuf->id;
            }
            else
            {
                if( (p_pic = GetNewPicture( p_dec, buf )) == NULL )
                    break;
                mpeg2_set_buf( p_dec->p_mpeg2dec, buf, p_pic );
            }
            p_dec->p_picture_to_destroy = p_pic;

            memset( p_pic->p[0].p_pixels, 0,
                    p_dec->p_info->sequence->width
                     * p_dec->p_info->sequence->height );
            memset( p_pic->p[1].p_pixels, 0x80,
                    p_dec->p_info->sequence->width
                     * p_dec->p_info->sequence->height / 4 );
            memset( p_pic->p[2].p_pixels, 0x80,
                    p_dec->p_info->sequence->width
                     * p_dec->p_info->sequence->height / 4 );

            if ( p_dec->b_slice_i )
            {
                vout_SynchroNewPicture( p_dec->p_synchro,
                            I_CODING_TYPE, 2, 0, 0, p_dec->i_current_rate );
                vout_SynchroDecode( p_dec->p_synchro );
                vout_SynchroEnd( p_dec->p_synchro, I_CODING_TYPE, 0 );
            }
            break;
        }

        default:
            break;
        }
    }

    /* If b_error is set, the libmpeg2 decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the libmpeg2 decoder thread */
    CloseDecoder( p_dec );

    return 0;

 error:
    DecoderError( p_fifo );
    if( p_dec )
    {
        if( p_dec->p_fifo )
            p_dec->p_fifo->b_error = 1;

        /* End of the libmpeg2 decoder thread */
        CloseDecoder( p_dec );
    }

    return -1;
}

/*****************************************************************************
 * CloseDecoder: libmpeg2 decoder destruction
 *****************************************************************************/
static void CloseDecoder( dec_thread_t * p_dec )
{
    if( p_dec )
    {
        int i_pic;

        if( p_dec->p_pes )
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_dec->p_pes );

        if( p_dec->p_synchro )
            vout_SynchroRelease( p_dec->p_synchro );

        if( p_dec->p_vout )
        {
            /* Temporary hack to free the pictures in use by libmpeg2 */
            for( i_pic = 0; i_pic < p_dec->p_vout->render.i_pictures; i_pic++ )
            {
                if( p_dec->p_vout->render.pp_picture[i_pic]->i_status ==
                      RESERVED_PICTURE )
                    vout_DestroyPicture( p_dec->p_vout,
                                     p_dec->p_vout->render.pp_picture[i_pic] );
                if( p_dec->p_vout->render.pp_picture[i_pic]->i_refcount > 0 )
                    vout_UnlinkPicture( p_dec->p_vout,
                                     p_dec->p_vout->render.pp_picture[i_pic] );
            }

            vout_Request( p_dec->p_fifo, p_dec->p_vout, 0, 0, 0, 0 );
        }

        if( p_dec->p_mpeg2dec ) mpeg2_close( p_dec->p_mpeg2dec );

        free( p_dec );
    }
}

/*****************************************************************************
 * GetNewPicture: Get a new picture from the vout and set the buf struct
 *****************************************************************************/
static picture_t *GetNewPicture( dec_thread_t *p_dec, uint8_t **pp_buf )
{
    picture_t *p_pic;
    vlc_bool_t b_progressive = p_dec->p_info->current_picture != NULL ?
        p_dec->p_info->current_picture->flags & PIC_FLAG_PROGRESSIVE_FRAME :
        1;
    vlc_bool_t b_top_field_first = p_dec->p_info->current_picture != NULL ?
        p_dec->p_info->current_picture->flags & PIC_FLAG_TOP_FIELD_FIRST :
        1;
    unsigned int i_nb_fields = p_dec->p_info->current_picture != NULL ?
        p_dec->p_info->current_picture->nb_fields : 2;

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_dec->p_vout,
        b_progressive, b_top_field_first, i_nb_fields )) )
    {
        if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
            break;

        msleep( VOUT_OUTMEM_SLEEP );
    }
    if( p_pic == NULL )
        return NULL;
    vout_LinkPicture( p_dec->p_vout, p_pic );

    pp_buf[0] = p_pic->p[0].p_pixels;
    pp_buf[1] = p_pic->p[1].p_pixels;
    pp_buf[2] = p_pic->p[2].p_pixels;

    return p_pic;
}
