/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ffmpeg.c,v 1.3 2002/04/27 16:13:23 fenrir Exp $
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

/* FIXME make this variable global */
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
    SET_DESCRIPTION( "ffmpeg video decoder module (MSMPEG4,MPEG4)" )
    ADD_CAPABILITY( DECODER, 50 )
    ADD_SHORTCUT( "ffmpeg" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vdec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP


static __inline__ u16 __GetWordLittleEndianFromBuff( byte_t *p_buff )
{
    u16 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 );
    return ( i );
}

static __inline__ u32 __GetDoubleWordLittleEndianFromBuff( byte_t *p_buff )
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
/*        case( MPEG1_VIDEO_ES ): marche pas pr le moment 
        case( MPEG2_VIDEO_ES ): */
        case( MSMPEG4_VIDEO_ES ):
        case( MPEG4_VIDEO_ES ):
            return( 0 );
        default:
            return( -1 );
    }
}

/*****************************************************************************
 * Functions locales 
 *****************************************************************************/

static int __ParseBitMapInfoHeader( bitmapinfoheader_t *h, byte_t *p_data )
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
    return( 0 );
}
/* get the first pes from fifo */
static pes_packet_t *__PES_GET( decoder_fifo_t *p_fifo )
{
    pes_packet_t *p_pes;

    vlc_mutex_lock( &p_fifo->data_lock );

    /* if fifo is emty wait */
    while( p_fifo->p_first == NULL )
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

    if( p_fifo->p_first == NULL )
    {
        /* No PES in the fifo */
        /* pp_last no longer valid */
        p_fifo->pp_last = &p_fifo->p_first;
        while( p_fifo->p_first == NULL )
        {
            vlc_cond_signal( &p_fifo->data_wait );
            vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
        }
    }
    vlc_mutex_unlock( &p_fifo->data_lock );
}

static void __PACKET_REINIT( videodec_thread_t *p_vdec )
{
    pes_packet_t *p_pes;

    p_pes = __PES_GET( p_vdec->p_fifo );
    if( p_vdec->p_fifo->b_die )
    {
        return;
    }
    p_vdec->p_data = p_pes->p_first;
    p_vdec->p_buff = p_vdec->p_data->p_payload_start;
    p_vdec->i_data_size = p_vdec->p_data->p_payload_end - 
                                p_vdec->p_data->p_payload_start;
}

static void __PACKET_NEXT( videodec_thread_t *p_vdec )
{
    do
    {
        p_vdec->p_data = p_vdec->p_data->p_next;
        if( p_vdec->p_data == NULL )
        {
            __PES_NEXT( p_vdec->p_fifo );
            if( p_vdec->p_fifo->b_die )
            {
                return;
            }
            __PACKET_REINIT( p_vdec ); 
        }
        else
        {
            p_vdec->p_buff = p_vdec->p_data->p_payload_start;
            p_vdec->i_data_size = p_vdec->p_data->p_payload_end -
                                    p_vdec->p_data->p_payload_start;
        }
        
    } while( p_vdec->i_data_size <= 0 );
}

static void __PACKET_FILL( videodec_thread_t *p_vdec ) 
{
    if( p_vdec->i_data_size <= 0 )
    {
        __PACKET_NEXT( p_vdec );
    }
}
/* call only two times so inline for faster */
static __inline__ void __ConvertAVPictureToPicture( AVPicture *p_avpicture, 
                                                    picture_t *p_picture )
{
    int i_plane, i_line;
    u8 *p_dest,*p_src;
    
    for( i_plane = 0; i_plane < p_picture->i_planes; i_plane++ )
    {
        p_dest = p_picture->p[i_plane].p_pixels;
        p_src  = p_avpicture->data[i_plane];
        if( (p_dest == NULL)||( p_src == NULL)||(i_plane >= 3) ) { return; }
        for( i_line = 0; i_line < p_picture->p[i_plane].i_lines; i_line++ )
        {
            FAST_MEMCPY( p_dest, 
                         p_src, 
                         __MIN( p_picture->p[i_plane].i_pitch,
                                p_avpicture->linesize[i_plane] ) );
            p_dest += p_picture->p[i_plane].i_pitch;
            p_src  += p_avpicture->linesize[i_plane];
        }
    }
}

static __inline__ u32 __FfmpegChromaToFourCC( int i_ffmpegchroma )
{
    switch( i_ffmpegchroma )
    {
        case( PIX_FMT_YUV420P ):
        case( PIX_FMT_YUV422 ):
            return FOURCC_I420;
        case( PIX_FMT_RGB24 ):
            return FOURCC_RV24;
        case( PIX_FMT_BGR24 ):
            return 0; /* FIXME pas trouvé ds video.h */
        case( PIX_FMT_YUV422P ):
            return FOURCC_Y422;
        case( PIX_FMT_YUV444P ):
            return  0; /* FIXME pas trouvé FOURCC_IYU2; */
        default:
            return  0;
    }

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
        memset( &p_vdec->format, 0, sizeof( bitmapinfoheader_t ) );
    }
    /* some codec need to have height and width initialized (msmepg4,mpeg4) */
    /* we cannot create vout because we don't know what chroma */

    /*init ffmpeg */
    /* TODO: add a global variable to know if init was already done 
        in case we use it also for audio */
    if( b_ffmpeginit == 0 )
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
        case( MPEG1_VIDEO_ES ): /* marche pas pr le moment */
        case( MPEG2_VIDEO_ES ):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MPEG1VIDEO );
            p_vdec->psz_namecodec = "MPEG-1";
            break;
        case( MSMPEG4_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MSMPEG4 );
            p_vdec->psz_namecodec = "MS MPEG-4";
            break;
        case( MPEG4_VIDEO_ES):
            p_vdec->p_codec = avcodec_find_decoder( CODEC_ID_MPEG4 );
            p_vdec->psz_namecodec = "MPEG-4";
            break;
        default:
            p_vdec->p_codec = NULL;
            p_vdec->psz_namecodec = "Unknown";
    }

    if( p_vdec->p_codec == NULL )
    {
        intf_ErrMsg( "vdec error: codec not found (%s)",
                     p_vdec->psz_namecodec );
        return( -1 );
    }

    p_vdec->p_context = &p_vdec->context;
    memset( p_vdec->p_context, 0, sizeof( AVCodecContext ) );

    p_vdec->p_context->width  = p_vdec->format.i_width;
    p_vdec->p_context->height = p_vdec->format.i_height;
    p_vdec->p_context->pix_fmt = PIX_FMT_YUV420P;

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

    __PACKET_REINIT( p_vdec );
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

    if( p_vdec->p_vout != NULL )
    {
        vout_DestroyThread( p_vdec->p_vout, NULL );
    }

    free( p_vdec );
}

static void  DecodeThread( videodec_thread_t *p_vdec )
{
    int     i_len;
    int     b_gotpicture;
    int     b_convert;
    mtime_t i_pts; 
    pes_packet_t  *p_pes;
    AVPicture avpicture;  /* ffmpeg picture */
    u32 i_chroma;
    picture_t *p_picture; /* videolan picture */
    /* we have to get a frame stored in a pes 
       give it to ffmpeg decoder 
       and send the image to the output */ 
    /* when we have the first image we create the video output */
/* int avcodec_decode_video(AVCodecContext *avctx, AVPicture *picture, 
                            int *got_picture_ptr,
                            UINT8 *buf, int buf_size);
 typedef struct AVPicture 
 {
     UINT8 *data[3];
     int linesize[3];
 } AVPicture;
 */
    i_pts = -1 ;
    do
    {
        __PACKET_FILL( p_vdec );
        if( (p_vdec->p_fifo->b_die)||(p_vdec->p_fifo->b_error) )
        {
            return;
        }
        /* save pts */
        if( i_pts < 0 ) {i_pts =  __PES_GET( p_vdec->p_fifo )->i_pts;}

        i_len = avcodec_decode_video( p_vdec->p_context,
                                      &avpicture,
                                      &b_gotpicture,
                                      p_vdec->p_buff,
                                      p_vdec->i_data_size);
                                      
        if( i_len < 0 )
        {
            intf_WarnMsg( 1, "vdec error: cannot decode one frame (%d bytes)",
                        p_vdec->i_data_size );
            __PES_NEXT( p_vdec->p_fifo );
            __PACKET_REINIT( p_vdec );
            return;
        }
        p_vdec->i_data_size -= i_len;
        p_vdec->p_buff += i_len;
    } while( !b_gotpicture );

    i_chroma =__FfmpegChromaToFourCC( p_vdec->p_context->pix_fmt );
    if( i_chroma == 0 )
    {
        b_convert = 1;
        i_chroma = FOURCC_I420;
    }
    else
    {
        b_convert = 0;
    }
    
    /* Send decoded frame to vout */
    if( p_vdec->p_vout == NULL )
    {
        /* FIXME FIXME faire ca comme il faut avec :
         * pp_vout_bank
         * bon aspect, ds avi pas definie mais pour le reste a voir ...
         */

        /* create vout */

        /* ffmpeg set it for our with some codec */  
        if( (p_vdec->format.i_width == 0)||(p_vdec->format.i_height == 0) )
        {
            p_vdec->format.i_width  = p_vdec->p_context->width;
            p_vdec->format.i_height = p_vdec->p_context->height; 
        }
        /* calculate i_aspect */
        p_vdec->i_aspect = VOUT_ASPECT_FACTOR * p_vdec->format.i_width /
                                p_vdec->format.i_height;
        p_vdec->i_chroma = i_chroma;

        intf_WarnMsg( 1, "vdec info: creating vout %dx%d chroma %4.4s %s",
                         p_vdec->format.i_width,
                         p_vdec->format.i_height,
                         (char*)&p_vdec->i_chroma,
                         b_convert ? "(with convertion)" : "" );

        p_vdec->p_vout = vout_CreateThread( 
                                NULL,
                                p_vdec->format.i_width,
                                p_vdec->format.i_height,
                                p_vdec->i_chroma,
                                p_vdec->i_aspect );

        if( p_vdec->p_vout == NULL )
        {
            intf_ErrMsg( "vdec error: can't open vout, aborting" );
            p_vdec->p_fifo->b_error = 1;
            return;
        }
    }

    while( (p_picture = vout_CreatePicture( p_vdec->p_vout,
                                    0,  /* ??? */
                                    0,  /* ??? */
                                    0) ) /* ??? */
                    == NULL )
    {
        if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    if( b_convert == 1 )
    {
        /* we convert in a supported format */
        int i_status;
        u8 *p_buff;
        AVPicture avpicture_tmp;
        
        p_buff = malloc( avpicture_get_size( PIX_FMT_YUV420P,
                                             p_vdec->p_context->width,
                                             p_vdec->p_context->height) );
        avpicture_fill(  &avpicture_tmp,
                         p_buff,
                         PIX_FMT_YUV420P,
                         p_vdec->p_context->width,
                         p_vdec->p_context->height );

        i_status = img_convert( &avpicture_tmp,
                                PIX_FMT_YUV420P,
                                &avpicture,
                                p_vdec->p_context->pix_fmt,
                                p_vdec->p_context->width,
                                p_vdec->p_context->height );
       if( i_status < 0 )
       {
            intf_ErrMsg( "vdec error: cannot convert picture in known chroma" );
            return;
       }
        __ConvertAVPictureToPicture( &avpicture_tmp, p_picture );
        free( p_buff ); /* FIXME try to alloc only one time */
    }
    else
    {
        __ConvertAVPictureToPicture( &avpicture, p_picture );
    }

    vout_DatePicture( p_vdec->p_vout, p_picture, i_pts );
    vout_DisplayPicture( p_vdec->p_vout, p_picture );
    
    return;
}   

