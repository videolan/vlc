/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ffmpeg.c,v 1.10 2002/05/18 17:47:46 sam Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <errno.h>
#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "video.h"
#include "video_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"


#include "vdec_ext-plugins.h"
#include "avcodec.h"                                            /* ffmpeg */
#include "ffmpeg.h"

/*
 * Local prototypes
 */
static int      decoder_Probe   ( u8 * );
static int      decoder_Run     ( decoder_config_t * );
static int      InitThread      ( videodec_thread_t * );
static void     EndThread       ( videodec_thread_t * );
static void     DecodeThread    ( videodec_thread_t * );


static int      b_ffmpeginit = 0;

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( vdec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.dec.pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run   = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/

MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "ffmpeg video decoder (MSMPEG4v123,MPEG4)" )
    ADD_CAPABILITY( DECODER, 70 )
    ADD_SHORTCUT( "ffmpeg" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vdec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP


static inline u16 __GetWordLittleEndianFromBuff( byte_t *p_buff )
{
    u16 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 );
    return ( i );
}

static inline u32 __GetDoubleWordLittleEndianFromBuff( byte_t *p_buff )
{
    u32 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 ) + 
                ( *(p_buff + 2) <<16 ) + ( *(p_buff + 3) <<24 );
    return ( i );
}

/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int decoder_Probe( u8 *pi_type )
{
    switch( *pi_type )
    {
#if LIBAVCODEC_BUILD >= 4608
        case( MSMPEG4v1_VIDEO_ES):
        case( MSMPEG4v2_VIDEO_ES):
#endif
        case( MSMPEG4v3_VIDEO_ES):
        case( MPEG4_VIDEO_ES ):
            return( 0 );
        default:
            return( -1 );
    }
}

/*****************************************************************************
 * Functions locales 
 *****************************************************************************/

static void __ParseBitMapInfoHeader( bitmapinfoheader_t *h, byte_t *p_data )
{
    h->i_size          = __GetDoubleWordLittleEndianFromBuff( p_data );
    h->i_width         = __GetDoubleWordLittleEndianFromBuff( p_data + 4 );
    h->i_height        = __GetDoubleWordLittleEndianFromBuff( p_data + 8 );
    h->i_planes        = __GetWordLittleEndianFromBuff( p_data + 12 );
    h->i_bitcount      = __GetWordLittleEndianFromBuff( p_data + 14 );
    h->i_compression   = __GetDoubleWordLittleEndianFromBuff( p_data + 16 );
    h->i_sizeimage     = __GetDoubleWordLittleEndianFromBuff( p_data + 20 );
    h->i_xpelspermeter = __GetDoubleWordLittleEndianFromBuff( p_data + 24 );
    h->i_ypelspermeter = __GetDoubleWordLittleEndianFromBuff( p_data + 28 );
    h->i_clrused       = __GetDoubleWordLittleEndianFromBuff( p_data + 32 );
    h->i_clrimportant  = __GetDoubleWordLittleEndianFromBuff( p_data + 36 );
}
/* get the first pes from fifo */
static pes_packet_t *__PES_GET( decoder_fifo_t *p_fifo )
{
    pes_packet_t *p_pes;

    vlc_mutex_lock( &p_fifo->data_lock );

    /* if fifo is emty wait */
    while( !p_fifo->p_first )
    {
        if( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return( NULL );
        }
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    p_pes = p_fifo->p_first;

    vlc_mutex_unlock( &p_fifo->data_lock );

    return( p_pes );
}
/* free the first pes and go to next */
static void __PES_NEXT( decoder_fifo_t *p_fifo )
{
    pes_packet_t *p_next;

    vlc_mutex_lock( &p_fifo->data_lock );
    
    p_next = p_fifo->p_first->p_next;
    p_fifo->p_first->p_next = NULL;
    input_DeletePES( p_fifo->p_packets_mgt, p_fifo->p_first );
    p_fifo->p_first = p_next;
    p_fifo->i_depth--;

    if( !p_fifo->p_first )
    {
        /* No PES in the fifo */
        /* pp_last no longer valid */
        p_fifo->pp_last = &p_fifo->p_first;
        while( !p_fifo->p_first )
        {
            vlc_cond_signal( &p_fifo->data_wait );
            vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
        }
    }
    vlc_mutex_unlock( &p_fifo->data_lock );
}

static inline void __GetFrame( videodec_thread_t *p_vdec )
{
    pes_packet_t  *p_pes;
    data_packet_t *p_data;
    byte_t        *p_buffer;

    p_pes = __PES_GET( p_vdec->p_fifo );
    p_vdec->i_pts = p_pes->i_pts;

    while( ( !p_pes->i_nb_data )||( !p_pes->i_pes_size ) )
    {
        __PES_NEXT( p_vdec->p_fifo );
        p_pes = __PES_GET( p_vdec->p_fifo );
    }
    p_vdec->i_framesize = p_pes->i_pes_size;
    if( p_pes->i_nb_data == 1 )
    {
        p_vdec->p_framedata = p_pes->p_first->p_payload_start;
        return;    
    }
    /* get a buffer and gather all data packet */
    p_vdec->p_framedata = p_buffer = malloc( p_pes->i_pes_size );
    p_data = p_pes->p_first;
    do
    {
        FAST_MEMCPY( p_buffer, 
                     p_data->p_payload_start, 
                     p_data->p_payload_end - p_data->p_payload_start );
        p_buffer += p_data->p_payload_end - p_data->p_payload_start;
        p_data = p_data->p_next;
    } while( p_data );
}

static inline void __NextFrame( videodec_thread_t *p_vdec )
{
    pes_packet_t  *p_pes;

    p_pes = __PES_GET( p_vdec->p_fifo );
    if( p_pes->i_nb_data != 1 )
    {
        free( p_vdec->p_framedata ); /* FIXME keep this buffer */
    }
    __PES_NEXT( p_vdec->p_fifo );
}



/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run ( decoder_config_t * p_config )
{
    videodec_thread_t   *p_vdec;
    int b_error;
    
    if ( (p_vdec = (videodec_thread_t*)malloc( sizeof(videodec_thread_t))) 
                    == NULL )
    {
        intf_ErrMsg( "vdec error: not enough memory "
                     "for vdec_CreateThread() to create the new thread");
        DecoderError( p_config->p_decoder_fifo );
        return( -1 );
    }
    memset( p_vdec, 0, sizeof( videodec_thread_t ) );

    p_vdec->p_fifo = p_config->p_decoder_fifo;
    p_vdec->p_config = p_config;

    if( InitThread( p_vdec ) != 0 )
    {
        DecoderError( p_config->p_decoder_fifo );
        return( -1 );
    }
     
    while( (!p_vdec->p_fifo->b_die) && (!p_vdec->p_fifo->b_error) )
    {
        /* decode a picture */
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

/*****************************************************************************
 * InitThread: initialize vdec output thread
 *****************************************************************************
 * This function is called from decoder_Run and performs the second step 
 * of the initialization. It returns 0 on success. Note that the thread's 
 * flag are not modified inside this function.
 *****************************************************************************/
static int InitThread( videodec_thread_t *p_vdec )
{
    
    if( p_vdec->p_config->p_demux_data != NULL )
    {
        __ParseBitMapInfoHeader( &p_vdec->format, 
                                (byte_t*)p_vdec->p_config->p_demux_data );
    }
    else
    {
        intf_ErrMsg( "vdec error: cannot get informations" );
        return( -1 );
    }

    /*init ffmpeg */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        b_ffmpeginit = 1;
        intf_WarnMsg( 1, "vdec init: library ffmpeg initialised" );
   }
   else
   {
        intf_WarnMsg( 1, "vdec init: library ffmpeg already initialised" );
   }

    switch( p_vdec->p_config->i_type)
    {
#if LIBAVCODEC_BUILD >= 4608 /* what is the true version */
        case( MSMPEG4v1_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MSMPEG4V1 );
            p_vdec->psz_namecodec = "MS MPEG-4 v1";
            break;
        case( MSMPEG4v2_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MSMPEG4V2 );
            p_vdec->psz_namecodec = "MS MPEG-4 v2";
            break;
        case( MSMPEG4v3_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MSMPEG4V3 );
            p_vdec->psz_namecodec = "MS MPEG-4 v3";
            break;
#else            
            /* fallback on this */
            case( MSMPEG4v3_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MSMPEG4 );
            p_vdec->psz_namecodec = "MS MPEG-4";
            break;
#endif
        case( MPEG4_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MPEG4 );
            p_vdec->psz_namecodec = "MPEG-4";
            break;
        default:
            p_vdec->p_codec = NULL;
            p_vdec->psz_namecodec = "Unknown";
    }

    if( !p_vdec->p_codec )
    {
        intf_ErrMsg( "vdec error: codec not found (%s)",
                     p_vdec->psz_namecodec );
        return( -1 );
    }

    p_vdec->p_context = &p_vdec->context;
    memset( p_vdec->p_context, 0, sizeof( AVCodecContext ) );

    p_vdec->p_context->width  = p_vdec->format.i_width;
    p_vdec->p_context->height = p_vdec->format.i_height;
    p_vdec->p_context->pix_fmt = PIX_FMT_YUV420P; /* I420 */

    if (avcodec_open(p_vdec->p_context, p_vdec->p_codec) < 0)
    {
        intf_ErrMsg( "vdec error: cannot open codec (%s)",
                     p_vdec->psz_namecodec );
        return( -1 );
    }
    else
    {
        intf_WarnMsg( 1, "vdec info: ffmpeg codec (%s) started",
                         p_vdec->psz_namecodec );
    }
    /* create vout */

     p_vdec->p_vout = vout_CreateThread( 
                                NULL,
                                p_vdec->format.i_width,
                                p_vdec->format.i_height,
                                FOURCC_I420,
                                VOUT_ASPECT_FACTOR * p_vdec->format.i_width /
                                    p_vdec->format.i_height );

    if( !p_vdec->p_vout )
    {
        intf_ErrMsg( "vdec error: can't open vout, aborting" );
        avcodec_close( p_vdec->p_context );
        intf_WarnMsg(1, "vdec info: ffmpeg codec (%s) stopped",
                            p_vdec->psz_namecodec);
        return( -1 );
    }

    vlc_mutex_lock( &p_vout_bank->lock );
    if( p_vout_bank->i_count != 0 )
    {
        vlc_mutex_unlock( &p_vout_bank->lock );
        vout_DestroyThread( p_vout_bank->pp_vout[ 0 ], NULL );
        vlc_mutex_lock( &p_vout_bank->lock );
        p_vout_bank->i_count--;
    }
    p_vout_bank->i_count++;
    p_vout_bank->pp_vout[0] = p_vdec->p_vout;
    vlc_mutex_unlock( &p_vout_bank->lock );

    return( 0 );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( videodec_thread_t *p_vdec )
{
    if( p_vdec == NULL )
    {
        intf_ErrMsg( "vdec error: cannot free structures" );
        return;
    }

    if( p_vdec->p_context != NULL)
    {
        avcodec_close( p_vdec->p_context );
        intf_WarnMsg(1, "vdec info: ffmpeg codec (%s) stopped",
                        p_vdec->psz_namecodec);
    }

    vlc_mutex_lock( &p_vout_bank->lock );
    if( p_vout_bank->i_count != 0 )
    {
        vlc_mutex_unlock( &p_vout_bank->lock );
        vout_DestroyThread( p_vout_bank->pp_vout[ 0 ], NULL );
        vlc_mutex_lock( &p_vout_bank->lock );
        p_vout_bank->i_count--; 
        p_vout_bank->pp_vout[ 0 ] = NULL;
    }
    vlc_mutex_unlock( &p_vout_bank->lock );

    free( p_vdec );
}

static void  DecodeThread( videodec_thread_t *p_vdec )
{
    int     i_plane;
    int     i_status;
    int     b_gotpicture;
    AVPicture avpicture;  /* ffmpeg picture */
    picture_t *p_pic; /* videolan picture */
    /* we have to get a frame stored in a pes 
       give it to ffmpeg decoder 
       and send the image to the output */ 

    __GetFrame( p_vdec );

    i_status = avcodec_decode_video( p_vdec->p_context,
                                     &avpicture,
                                     &b_gotpicture,
                                     p_vdec->p_framedata,
                                     p_vdec->i_framesize);
    __NextFrame( p_vdec );
                                         
    if( i_status < 0 )
    {
        intf_WarnMsg( 2, "vdec error: cannot decode one frame (%d bytes)",
                         p_vdec->i_framesize );
        return;
    }
    if( !b_gotpicture )
    {
        return;
    }    

    /* Send decoded frame to vout */

    while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
    {
        if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        int i_size;
        int i_line;
        byte_t *p_dest = p_pic->p[i_plane].p_pixels;
        byte_t *p_src  = avpicture.data[i_plane];
        if( ( !p_dest )||( !p_src )) 
        { 
            break; 
        }
        i_size = __MIN( p_pic->p[i_plane].i_pitch,
                                 avpicture.linesize[i_plane] );
        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            FAST_MEMCPY( p_dest, p_src, i_size );
            p_dest += p_pic->p[i_plane].i_pitch;
            p_src  += avpicture.linesize[i_plane];
        }
    }

    vout_DatePicture( p_vdec->p_vout, p_pic, p_vdec->i_pts);
    vout_DisplayPicture( p_vdec->p_vout, p_pic );
    
    return;
}

