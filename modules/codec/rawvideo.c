/*****************************************************************************
 * rawvideo.c: Pseudo audio decoder; for raw video data
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: rawvideo.c,v 1.1 2003/03/31 03:46:11 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/input.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include "codecs.h"
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    /* Input properties */
    decoder_fifo_t *p_fifo;
    int            i_raw_size;

    /* Output properties */

    mtime_t             pts;

    vout_thread_t       *p_vout;

} vdec_thread_t;

static int  OpenDecoder    ( vlc_object_t * );

static int  RunDecoder     ( decoder_fifo_t * );
static int  InitThread     ( vdec_thread_t * );
static void DecodeThread   ( vdec_thread_t * );
static void EndThread      ( vdec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("Pseudo Raw Video decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();


/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    switch( p_fifo->i_fourcc )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','4','2','2'):
            p_fifo->pf_run = RunDecoder;
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }

}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    vdec_thread_t *p_vdec;
    int b_error;

    if( !( p_vdec = malloc( sizeof( vdec_thread_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_vdec, 0, sizeof( vdec_thread_t ) );

    p_vdec->p_fifo = p_fifo;

    if( InitThread( p_vdec ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_vdec->p_fifo->b_die )&&( !p_vdec->p_fifo->b_error ) )
    {
        DecodeThread( p_vdec );
    }


    if( ( b_error = p_vdec->p_fifo->b_error ) )
    {
        DecoderError( p_vdec->p_fifo );
    }

    EndThread( p_vdec );
    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}


#define FREE( p ) if( p ) { free( p ); p = NULL; }


/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( vdec_thread_t * p_vdec )
{
    vlc_fourcc_t i_chroma;

#define bih ((BITMAPINFOHEADER*)p_vdec->p_fifo->p_bitmapinfoheader)
    if( bih == NULL )
    {
        msg_Err( p_vdec->p_fifo,
                 "info missing, fatal" );
        return( VLC_EGENERIC );
    }
    if( bih->biWidth <= 0 || bih->biHeight <= 0 )
    {
        msg_Err( p_vdec->p_fifo,
                 "invalid display size %dx%d",
                 bih->biWidth, bih->biHeight );
        return( VLC_EGENERIC );
    }

    switch( p_vdec->p_fifo->i_fourcc )
    {
        case VLC_FOURCC( 'I', '4', '2', '0' ):
            i_chroma = VLC_FOURCC( 'I', '4', '2', '0' );
            p_vdec->i_raw_size = bih->biWidth * bih->biHeight * 3 / 2;
            break;
        default:
            msg_Err( p_vdec->p_fifo, "invalid codec=%4.4s", (char*)&p_vdec->p_fifo->i_fourcc );
            return( VLC_EGENERIC );
    }

    p_vdec->p_vout = vout_Request( p_vdec->p_fifo, NULL,
                                   bih->biWidth, bih->biHeight,
                                   i_chroma,
                                   VOUT_ASPECT_FACTOR * bih->biWidth / bih->biHeight );

    if( p_vdec->p_vout == NULL )
    {
        msg_Err( p_vdec->p_fifo, "failled created vout" );
        return( VLC_EGENERIC );
    }

    return( VLC_SUCCESS );
#undef bih
}


static void FillPicture( pes_packet_t *p_pes, picture_t *p_pic )
{
    int i_plane;

    data_packet_t   *p_data;
    uint8_t *p_src;
    int     i_src;

    p_data = p_pes->p_first;
    p_src  = p_data->p_payload_start;
    i_src = p_data->p_payload_end - p_data->p_payload_start;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {

        uint8_t *p_dst;
        int     i_dst;

        p_dst = p_pic->p[i_plane].p_pixels;
        i_dst = p_pic->p[i_plane].i_pitch * p_pic->p[i_plane].i_lines;

        while( i_dst > 0 )
        {
            int i_copy;

            i_copy = __MIN( i_src, i_dst );
            if( i_copy > 0 )
            {
                memcpy( p_dst, p_src, i_copy );
            }
            i_dst -= i_copy;

            i_src -= i_copy;
            if( i_src <= 0 )
            {
                do
                {
                    p_data = p_data->p_next;
                    if( p_data == NULL )
                    {
                        return;
                    }
                    p_src  = p_data->p_payload_start;
                    i_src = p_data->p_payload_end - p_data->p_payload_start;
                } while( i_src <= 0 );
            }
        }
    }
}

/*****************************************************************************
 * DecodeThread: decodes a frame
 *****************************************************************************/
static void DecodeThread( vdec_thread_t *p_vdec )
{
    int             i_size;
    pes_packet_t    *p_pes;
    picture_t       *p_pic;

    /* **** get frame **** */
    input_ExtractPES( p_vdec->p_fifo, &p_pes );
    if( !p_pes )
    {
        p_vdec->p_fifo->b_error = 1;
        return;
    }
    i_size = p_pes->i_pes_size;

    if( i_size < p_vdec->i_raw_size )
    {
        msg_Warn( p_vdec->p_fifo, "invalid frame size (%d < %d)", i_size, p_vdec->i_raw_size );
        input_DeletePES( p_vdec->p_fifo->p_packets_mgt, p_pes );
        return;
    }

    /* **** get video picture **** */
    while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
    {
        if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }


    /* **** fill p_pic **** */
    FillPicture( p_pes, p_pic );

    /* **** display p_pic **** */
    vout_DatePicture( p_vdec->p_vout, p_pic, p_pes->i_pts);
    vout_DisplayPicture( p_vdec->p_vout, p_pic );


    input_DeletePES( p_vdec->p_fifo->p_packets_mgt, p_pes );
}


/*****************************************************************************
 * EndThread : faad decoder thread destruction
 *****************************************************************************/
static void EndThread (vdec_thread_t *p_vdec)
{
    if( p_vdec->p_vout )
    {
        vout_Request( p_vdec->p_fifo, p_vdec->p_vout, 0, 0, 0, 0 );
    }

    msg_Dbg( p_vdec->p_fifo, "raw video decoder closed" );

    free( p_vdec );
}


