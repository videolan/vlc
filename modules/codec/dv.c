/*****************************************************************************
 * dv.c: a decoder for DV video
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: dv.c,v 1.2 2002/11/06 09:26:25 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <vlc/decoder.h>

#undef vlc_error /*vlc_error is defined in the libdv headers, but not
                  * used in thes file */
#include <libdv/dv_types.h>
#include <libdv/dv.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int RunDecoder  ( decoder_fifo_t * );
static int OpenDecoder ( vlc_object_t * );

static u32 GetFourCC   ( dv_sample_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DV video decoder") );
    set_capability( "decoder", 70 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * The fourcc format for DV is "dvsd"
 *****************************************************************************/
static int OpenDecoder ( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('d','v','s','d') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder ( decoder_fifo_t *p_fifo )
{
    u8 *p_buffer;
    pes_packet_t *p_pes = NULL;
    int i_data = 120000;
    int i_aspect;

    bit_stream_t    bit_stream;
    dv_decoder_t *  p_decoder;
    vout_thread_t * p_vout;
    
    p_buffer = malloc( i_data );
    if( !p_buffer )
    {
        msg_Err( p_fifo, "out of memory" );
        p_fifo->b_error = 1;
        DecoderError( p_fifo );
        return -1;
    }

    p_decoder = dv_decoder_new( TRUE, FALSE, FALSE );
    if( !p_decoder )
    {
        msg_Err( p_fifo, "cannot create DV decoder" );
        free( p_buffer );
        p_fifo->b_error = 1;
        DecoderError( p_fifo );
        return -1;
    }

    if( InitBitstream( &bit_stream, p_fifo, NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_fifo, "cannot initialise bitstream" );
        free( p_buffer );
        p_fifo->b_error = 1;
        DecoderError( p_fifo );
        return -1;
    }

    /* Fill the buffer */
    GetChunk( &bit_stream, p_buffer, i_data );

    while( !p_fifo->b_die && !p_fifo->b_error )
    {
        /* Parsing the beginning of the stream */
        if( dv_parse_header( p_decoder, p_buffer ) < 0 )
        {
            fprintf(stderr, "parse error\n");
            p_fifo->b_error = 1;
            break;
        }

        if( dv_format_wide( p_decoder ) )
        {
            msg_Dbg( p_fifo, "aspect is 4:3" );
            i_aspect = VOUT_ASPECT_FACTOR * 4 / 3;
        }
        else if( dv_format_normal( p_decoder ) )
        {
            msg_Dbg( p_fifo, "aspect is 16:9" );
            i_aspect = VOUT_ASPECT_FACTOR * 4/3;//16 / 9;
        }
        else
        {
            msg_Dbg( p_fifo, "aspect is square pixels" );
            i_aspect = VOUT_ASPECT_FACTOR
                        * p_decoder->width / p_decoder->height;
        }

        if( p_decoder->frame_size <= i_data )
        {
            /* XXX: what to do? */
            i_data = p_decoder->frame_size;
        }
        else
        {
            p_buffer = realloc( p_buffer, p_decoder->frame_size );
        }
    
        /* Don't trust the sucker */
        //p_decoder->quality = p_decoder->video->quality;
        p_decoder->quality = DV_QUALITY_BEST;
        p_decoder->prev_frame_decoded = 0;

        /* Spawn a video output if there is none. First we look amongst our
         * children, then we look for any other vout that might be available */
        p_vout = vlc_object_find( p_fifo, VLC_OBJECT_VOUT, FIND_CHILD );
        if( !p_vout ) 
        {
            p_vout = vlc_object_find( p_fifo, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        }

        if( p_vout )
        {
            if( p_vout->render.i_width != p_decoder->width
             || p_vout->render.i_height != p_decoder->height
             || p_vout->render.i_chroma != GetFourCC( p_decoder->sampling )
             || p_vout->render.i_aspect != i_aspect )
            {
                /* We are not interested in this format, close this vout */
                vlc_object_detach( p_vout );
                vlc_object_release( p_vout );
                vout_DestroyThread( p_vout );
                p_vout = NULL;
            }
            else
            {
                /* This video output is cool! Hijack it. */
                vlc_object_detach( p_vout );
                vlc_object_attach( p_vout, p_fifo );
                vlc_object_release( p_vout );
            }
        }

        if( !p_vout )
        {
            msg_Dbg( p_fifo, "no vout present, spawning one" );

            p_vout = vout_CreateThread( p_fifo,
                                        p_decoder->width, p_decoder->height,
                                        GetFourCC( p_decoder->sampling ),
                                        i_aspect );
        }

        /* Main loop */
        while( !p_fifo->b_die && !p_fifo->b_error )
        {
            mtime_t i_pts = 0;

            GetChunk( &bit_stream, p_buffer + i_data,
                                   p_decoder->frame_size - i_data );
            i_data = p_decoder->frame_size;

            if( p_pes )
            {
                input_DeletePES( p_fifo->p_packets_mgt, p_pes );
            }

            input_ExtractPES( p_fifo, &p_pes );
            if( p_pes )
            {
                /* Don't trust the sucker */
                //i_pts = p_pes->i_pts + DEFAULT_PTS_DELAY;
                i_pts = mdate() + DEFAULT_PTS_DELAY;
            }

            if( p_fifo->b_die || p_fifo->b_error )
            {
                break;
            }

            if( dv_parse_header( p_decoder, p_buffer ) > 0 )
            {
                fprintf(stderr, "size changed\n");
            }

            if( p_vout && ( !p_decoder->prev_frame_decoded
                             || dv_frame_changed( p_decoder ) ) )
            {
                picture_t *p_pic;
                u8 *pixels[3];
                int pitches[3], i;

                while( !(p_pic = vout_CreatePicture( p_vout, 0, 0, 0 ) ) )
                {
                    if( p_fifo->b_die || p_fifo->b_error )
                    {
                        break;
                    } 
                    msleep( VOUT_OUTMEM_SLEEP );
                }

                if( !p_pic )
                {
                    break;
                }

                for( i = 0 ; i < p_pic->i_planes ; i++ )
                {
                    pixels[i] = p_pic->p[i].p_pixels;
                    pitches[i] = p_pic->p[i].i_pitch;
                }

                dv_decode_full_frame( p_decoder, p_buffer,
                                      e_dv_color_yuv, pixels, pitches );
                p_decoder->prev_frame_decoded = 1;

                vout_DatePicture( p_vout, p_pic, i_pts );
                vout_DisplayPicture( p_vout, p_pic );
            }

            i_data = 0;
        }

        if( p_vout )
        {
            vlc_object_detach( p_vout );
            vout_DestroyThread( p_vout );
        }
    }

    if( p_pes )
    {
        input_DeletePES( p_fifo->p_packets_mgt, p_pes );
    }

    free( p_buffer );
    CloseBitstream( &bit_stream );

    if( p_fifo->b_error )
    {
        DecoderError( p_fifo );
        return -1;
    }

    return 0;
}

static u32 GetFourCC( dv_sample_t x )
{
    switch( x )
    {
        case e_dv_sample_411: return VLC_FOURCC('Y','U','Y','2');
        case e_dv_sample_420: return VLC_FOURCC('Y','U','Y','2');
        case e_dv_sample_422: return VLC_FOURCC('Y','U','Y','2');
        default: return 0;
    }
}

