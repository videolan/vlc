/*****************************************************************************
 * libmpeg2.c: mpeg2 video decoder module making use of libmpeg2.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: libmpeg2.c,v 1.3 2003/03/20 21:45:01 gbazin Exp $
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

    /*
     * Input properties
     */
    decoder_fifo_t   *p_fifo;                  /* stores the PES stream data */
    pes_packet_t     *p_pes;                  /* current PES we are decoding */
    mtime_t          i_pts;
    mtime_t          i_previous_pts;
    mtime_t          i_current_pts;

    /*
     * Output properties
     */
    vout_thread_t *p_vout;

} dec_thread_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder  ( vlc_object_t * );
static int  RunDecoder   ( decoder_fifo_t * );
static void CloseDecoder ( dec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("libmpeg2 decoder module") );
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
    p_dec->p_info     = NULL;
    p_dec->i_pts      = mdate() + DEFAULT_PTS_DELAY;
    p_dec->i_current_pts  = 0;
    p_dec->i_previous_pts = 0;

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

                if( p_dec->p_pes->i_pts )
                {
                    mpeg2_pts( p_dec->p_mpeg2dec,
                               (uint32_t)p_dec->p_pes->i_pts );
                    p_dec->i_previous_pts = p_dec->i_current_pts;
                    p_dec->i_current_pts = p_dec->p_pes->i_pts;
                }
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
            /* Initialize video output */
            i_aspect = ((uint64_t)p_dec->p_info->sequence->width) *
                p_dec->p_info->sequence->pixel_width * VOUT_ASPECT_FACTOR /
                p_dec->p_info->sequence->height /
                p_dec->p_info->sequence->pixel_height;

            i_chroma = VLC_FOURCC('Y','V','1','2');

            p_dec->p_vout = vout_Request( p_dec->p_fifo, p_dec->p_vout,
                                          p_dec->p_info->sequence->width,
                                          p_dec->p_info->sequence->height,
                                          i_chroma, i_aspect );
            break;

        case STATE_PICTURE:
        {
            uint8_t *buf[3];

            /* Get a new picture */
            while( !(p_pic = vout_CreatePicture( p_dec->p_vout, 0, 0, 0 ) ) )
            {
                if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
                    break;

                msleep( VOUT_OUTMEM_SLEEP );
            }
            if( p_pic == NULL )
                break;

            buf[0] = p_pic->p[0].p_pixels;
            buf[1] = p_pic->p[1].p_pixels;
            buf[2] = p_pic->p[2].p_pixels;
            mpeg2_set_buf( p_dec->p_mpeg2dec, buf, p_pic );

            /* Store the date for the picture */
            if( p_dec->p_info->current_picture->flags & PIC_FLAG_PTS )
            {
                p_pic->date = ( p_dec->p_info->current_picture->pts ==
                                (uint32_t)p_dec->i_current_pts ) ?
                              p_dec->i_current_pts : p_dec->i_previous_pts;
            }
        }
        break;

        case STATE_END:
        case STATE_SLICE:
            if( p_dec->p_info->display_fbuf
                && p_dec->p_info->display_fbuf->id )
            {
                p_pic = (picture_t *)p_dec->p_info->display_fbuf->id;

                /* Date the new picture */
                if( p_dec->p_info->display_picture->flags & PIC_FLAG_PTS )
                {
                    p_dec->i_pts = p_pic->date;
                }
                else
                {
                    p_dec->i_pts += (p_dec->p_info->sequence->frame_period/27);
                }
                vout_DatePicture( p_dec->p_vout, p_pic, p_dec->i_pts );

                vout_DisplayPicture( p_dec->p_vout, p_pic );
            }
            break;

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
        if( p_dec->p_pes )
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_dec->p_pes );

        vout_Request( p_dec->p_fifo, p_dec->p_vout, 0, 0, 0, 0 );

        if( p_dec->p_mpeg2dec ) mpeg2_close( p_dec->p_mpeg2dec );

        free( p_dec );
    }
}
