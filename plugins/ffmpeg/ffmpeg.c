/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ffmpeg.c,v 1.15 2002/07/15 22:45:12 fenrir Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <errno.h>
#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "vdec_ext-plugins.h"
#include "avcodec.h"                                            /* ffmpeg */
#include "ffmpeg.h"

/*
 * Local prototypes
 */
static int      decoder_Probe   ( u8 * );
static int      decoder_Run     ( decoder_fifo_t * );
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
    SET_DESCRIPTION( "ffmpeg video decoder (MS-MPEG4,MPEG4,SVQ1,H263,H263.I)" )
    ADD_CAPABILITY( DECODER, 70 )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vdec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP



/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int decoder_Probe( u8 *pi_type )
{
    return( ffmpeg_GetFfmpegCodec( *pi_type, NULL, NULL ) ? 0 : -1 );
}

/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run ( decoder_fifo_t * p_fifo )
{
    videodec_thread_t   *p_vdec;
    int b_error;
    
    if ( !(p_vdec = (videodec_thread_t*)malloc( sizeof(videodec_thread_t))) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_vdec, 0, sizeof( videodec_thread_t ) );

    p_vdec->p_fifo = p_fifo;

    if( InitThread( p_vdec ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }
     
    while( (!p_vdec->p_fifo->b_die) && (!p_vdec->p_fifo->b_error) )
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


/*****************************************************************************
 * locales Functions
 *****************************************************************************/

#define GetWLE( p ) \
    ( *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) )

#define GetDWLE( p ) \
    (  *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) + \
      ( *((u8*)(p)+2) << 16 ) + ( *((u8*)(p)+3) << 24 ) )

static void __ParseBitMapInfoHeader( bitmapinfoheader_t *h, byte_t *p_data )
{
    h->i_size          = GetDWLE( p_data );
    h->i_width         = GetDWLE( p_data + 4 );
    h->i_height        = GetDWLE( p_data + 8 );
    h->i_planes        = GetWLE( p_data + 12 );
    h->i_bitcount      = GetWLE( p_data + 14 );
    h->i_compression   = GetDWLE( p_data + 16 );
    h->i_sizeimage     = GetDWLE( p_data + 20 );
    h->i_xpelspermeter = GetDWLE( p_data + 24 );
    h->i_ypelspermeter = GetDWLE( p_data + 28 );
    h->i_clrused       = GetDWLE( p_data + 32 );
    h->i_clrimportant  = GetDWLE( p_data + 36 );
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
        p_vdec->p_fifo->p_vlc->pf_memcpy( p_buffer, p_data->p_payload_start, 
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

/* FIXME FIXME some of them may be wrong */
static int i_ffmpeg_PixFmtToChroma[] = 
{
/* PIX_FMT_ANY = -1,PIX_FMT_YUV420P, PIX_FMT_YUV422,
   PIX_FMT_RGB24, PIX_FMT_BGR24, PIX_FMT_YUV422P, 
   PIX_FMT_YUV444P, PIX_FMT_YUV410P */

    0, FOURCC_I420, FOURCC_I420, 
    FOURCC_RV24, 0, FOURCC_Y422, 
    FOURCC_I444, 0 
};

static inline u32 ffmpeg_PixFmtToChroma( int i_ffmpegchroma )
{
    if( ++i_ffmpegchroma > 7 )
    {
        return( 0 );
    }
    else
    {
        return( i_ffmpeg_PixFmtToChroma[i_ffmpegchroma] );
    }
}

static inline int ffmpeg_FfAspect( int i_width, int i_height, int i_ffaspect )
{
    switch( i_ffaspect )
    {
        case( FF_ASPECT_4_3_625 ):
        case( FF_ASPECT_4_3_525 ):
            return( VOUT_ASPECT_FACTOR * 4 / 3);
        case( FF_ASPECT_16_9_625 ):
        case( FF_ASPECT_16_9_525 ):
            return( VOUT_ASPECT_FACTOR * 16 / 9 );
        case( FF_ASPECT_SQUARE ):
        default:
            return( VOUT_ASPECT_FACTOR * i_width / i_height );
    }
}

static int ffmpeg_CheckVout( vout_thread_t *p_vout,
                             int i_width,
                             int i_height,
                             int i_aspect,
                             int i_chroma )
{
    if( !p_vout )
    {
        return( 0 );
    }
    if( !i_chroma )
    {
        i_chroma = FOURCC_I420; /* we will try to make conversion */
    } 
    
    if( ( p_vout->render.i_width != i_width )||
        ( p_vout->render.i_height != i_height )||
        ( p_vout->render.i_chroma != i_chroma )||
        ( p_vout->render.i_aspect != 
                ffmpeg_FfAspect( i_width, i_height, i_aspect ) ) )
    {
        return( 0 );
    }
    else
    {
        return( 1 );
    }
}

/* Return a Vout */

static vout_thread_t *ffmpeg_CreateVout( videodec_thread_t *p_vdec,
                                         int i_width,
                                         int i_height,
                                         int i_aspect,
                                         int i_chroma )
{
    vout_thread_t *p_vout;

    if( (!i_width)||(!i_height) )
    {
        return( NULL ); /* Can't create a new vout without display size */
    }

    if( !i_chroma )
    {
        i_chroma = FOURCC_I420; /* we make conversion if possible*/
        msg_Warn( p_vdec->p_fifo, "Internal chroma conversion (FIXME)");
        /* It's mainly for I410 -> I420 conversion that I've made,
           it's buggy and very slow */
    } 

    i_aspect = ffmpeg_FfAspect( i_width, i_height, i_aspect );
    
    /* Spawn a video output if there is none. First we look for our children,
     * then we look for any other vout that might be available. */
    p_vout = vlc_object_find( p_vdec->p_fifo, VLC_OBJECT_VOUT,
                                              FIND_CHILD );
    if( !p_vout )
    {
        p_vout = vlc_object_find( p_vdec->p_fifo, VLC_OBJECT_VOUT,
                                                  FIND_ANYWHERE );
    }

    if( p_vout )
    {
        if( !ffmpeg_CheckVout( p_vout, 
                               i_width, i_height, i_aspect,i_chroma ) )
        {
            /* We are not interested in this format, close this vout */
            vlc_object_detach_all( p_vout );
            vlc_object_release( p_vout );
            vout_DestroyThread( p_vout );
            p_vout = NULL;
        }
        else
        {
            /* This video output is cool! Hijack it. */
            vlc_object_detach_all( p_vout );
            vlc_object_attach( p_vout, p_vdec->p_fifo );
            vlc_object_release( p_vout );
        }
    }

    if( p_vout == NULL )
    {
        msg_Dbg( p_vdec->p_fifo, "no vout present, spawning one" );
    
        p_vout = vout_CreateThread( p_vdec->p_fifo,
                                    i_width,
                                    i_height,
                                    i_chroma,
                                    i_aspect );
    }

    return( p_vout );
}

#if 0
/* segfault some^Wevery times*/
static void ffmpeg_ConvertPictureI410toI420( picture_t *p_pic,
                                             AVPicture *p_avpicture,
                                             videodec_thread_t   *p_vdec )
{
    int i_plane; 
    
    int i_x, i_y;
    int i_x_max, i_y_max;

    int i_width;
    int i_height; 

    int i_dst_stride;
    int i_src_stride;
    
    u8  *p_dest;
    u8  *p_src;

    i_width = p_vdec->p_context->width;
    i_height= p_vdec->p_context->height;
    

    p_dest = p_pic->p[0].p_pixels;
    p_src  = p_avpicture->data[0];

    i_src_stride = p_avpicture->linesize[0];
    i_dst_stride = p_pic->p[0].i_pitch;
    
    if( i_src_stride == i_dst_stride )
    {
        p_vdec->p_fifo->p_vlc->pf_memcpy( p_dest, p_src, i_src_stride * i_height  );
    }
    else
    {
        for( i_y = 0; i_y < i_height; i_y++ )
        {
            p_vdec->p_fifo->p_vlc->pf_memcpy( p_dest, p_src, i_width );
            p_dest += i_dst_stride;
            p_src  += i_src_stride;
        }
    }

    for( i_plane = 1; i_plane < 3; i_plane++ )
    {
        i_y_max = p_pic->p[i_plane].i_lines;
        
        p_src  = p_avpicture->data[i_plane];
        p_dest = p_pic->p[i_plane].p_pixels;
        i_dst_stride = p_pic->p[i_plane].i_pitch;
        i_src_stride = p_avpicture->linesize[i_plane];

        i_x_max = __MIN( i_dst_stride / 2, i_src_stride );

        for( i_y = 0; i_y <( i_y_max + 1 ) / 2 ; i_y++ )
        {
            for( i_x = 0; i_x < i_x_max - 1; i_x++ )
            {
                p_dest[2 * i_x    ] = p_src[i_x];
                p_dest[2 * i_x + 1] = ( p_src[i_x] + p_src[i_x + 1] ) / 2;
            }
            p_dest[2 * i_x_max - 2] = p_src[i_x];
            p_dest[2 * i_x_max - 1] = p_src[i_x];

            p_dest += 2 * i_dst_stride;
            p_src  += i_src_stride;
        }
        
        p_src  = p_pic->p[i_plane].p_pixels;
        p_dest = p_src + i_dst_stride;

        for( i_y = 0; i_y < ( i_y_max + 1 ) / 2 ; i_y++ )
        {
            p_vdec->p_fifo->p_vlc->pf_memcpy( p_dest, p_src, i_dst_stride ); 
            p_dest += 2*i_dst_stride;
            p_src  += 2*i_dst_stride;
        }

    }
}
#endif

/* FIXME FIXME FIXME this is a big shit
   does someone want to rewrite this function ? 
   or said to me how write a better thing
   FIXME FIXME FIXME
*/

static void ffmpeg_ConvertPictureI410toI420( picture_t *p_pic,
                                             AVPicture *p_avpicture,
                                             videodec_thread_t   *p_vdec )
{
    u8 *p_src, *p_dst;
    u8 *p_plane[3];
    int i_plane;
    
    int i_stride, i_lines;
    int i_height, i_width;
    int i_y, i_x;
    
    i_height = p_vdec->p_context->height;
    i_width  = p_vdec->p_context->width;
    
    p_dst = p_pic->p[0].p_pixels;
    p_src  = p_avpicture->data[0];

    /* copy first plane */
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_width);
        p_dst += p_pic->p[0].i_pitch;
        p_src += p_avpicture->linesize[0];
    }
    
    /* process each plane in a temporary buffer */
    for( i_plane = 1; i_plane < 3; i_plane++ )
    {
        i_stride = p_avpicture->linesize[i_plane];
        i_lines = i_height / 4;

        p_dst = p_plane[i_plane] = malloc( i_lines * i_stride * 2 * 2 );
        p_src  = p_avpicture->data[i_plane];

        /* for each source line */
        for( i_y = 0; i_y < i_lines; i_y++ )
        {
            for( i_x = 0; i_x < i_stride - 1; i_x++ )
            {
                p_dst[2 * i_x    ] = p_src[i_x];
                p_dst[2 * i_x + 1] = ( p_src[i_x] + p_src[i_x + 1]) / 2;

            }
            p_dst[2 * i_stride - 2] = p_src[i_x];
            p_dst[2 * i_stride - 1] = p_src[i_x];
                           
            p_dst += 4 * i_stride; /* process the next even lines */
            p_src += i_stride;
        }


    }

    for( i_plane = 1; i_plane < 3; i_plane++ )
    {
        i_stride = p_avpicture->linesize[i_plane];
        i_lines = i_height / 4;

        p_dst = p_plane[i_plane] + 2*i_stride;
        p_src  = p_plane[i_plane];

        for( i_y = 0; i_y < i_lines - 1; i_y++ )
        {
            for( i_x = 0; i_x <  2 * i_stride ; i_x++ )
            {
                p_dst[i_x] = ( p_src[i_x] + p_src[i_x + 4*i_stride])/2;
            }
                           
            p_dst += 4 * i_stride; /* process the next odd lines */
            p_src += 4 * i_stride;
        }
        /* last line */
        p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, 2*i_stride );
    }
    /* copy to p_pic, by block
       if I do pixel per pixel it segfault. It's why I use 
       temporaries buffers */
    for( i_plane = 1; i_plane < 3; i_plane++ )
    {

        int i_size; 
        p_src  = p_plane[i_plane];
        p_dst = p_pic->p[i_plane].p_pixels;

        i_size = __MIN( 2*i_stride, p_pic->p[i_plane].i_pitch);
        for( i_y = 0; i_y < __MIN(p_pic->p[i_plane].i_lines, 2 * i_lines); i_y++ )
        {
            p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_size );
            p_src += 2 * i_stride;
            p_dst += p_pic->p[i_plane].i_pitch;
        }
        free( p_plane[i_plane] );
    }

}

static void ffmpeg_ConvertPicture( picture_t *p_pic,
                                   AVPicture *p_avpicture,
                                   videodec_thread_t   *p_vdec )
{
    int i_plane; 
    int i_size;
    int i_line;

    u8  *p_dst;
    u8  *p_src;
    int i_src_stride;
    int i_dst_stride;
    
    if( ffmpeg_PixFmtToChroma( p_vdec->p_context->pix_fmt ) )
    {
        /* convert ffmpeg picture to our format */
        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_src  = p_avpicture->data[i_plane];
            p_dst = p_pic->p[i_plane].p_pixels;
            i_src_stride = p_avpicture->linesize[i_plane];
            i_dst_stride = p_pic->p[i_plane].i_pitch;
            
            i_size = __MIN( i_src_stride, i_dst_stride );
            for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
            {
                p_vdec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_size );
                p_src += i_src_stride;
                p_dst += i_dst_stride;
            }
        }
        return;
    }

    /* we need to convert to I420 */
    switch( p_vdec->p_context->pix_fmt )
    {
#if LIBAVCODEC_BUILD >= 4615
        case( PIX_FMT_YUV410P ):
            ffmpeg_ConvertPictureI410toI420( p_pic, p_avpicture, p_vdec );
            break;
#endif            
        default:
            p_vdec->p_fifo->b_error =1;
            break;
    }
}


/*****************************************************************************
 *
 * Functions that initialize, decode and end the decoding process
 *
 *****************************************************************************/

/*****************************************************************************
 * InitThread: initialize vdec output thread
 *****************************************************************************
 * This function is called from decoder_Run and performs the second step 
 * of the initialization. It returns 0 on success. Note that the thread's 
 * flag are not modified inside this function.
 *****************************************************************************/

static int InitThread( videodec_thread_t *p_vdec )
{
    int i_ffmpeg_codec; 
    
    if( p_vdec->p_fifo->p_demux_data )
    {
        __ParseBitMapInfoHeader( &p_vdec->format, 
                                (byte_t*)p_vdec->p_fifo->p_demux_data );
    }
    else
    {
        msg_Warn( p_vdec->p_fifo, "display informations missing" );
    }

    /*init ffmpeg */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        b_ffmpeginit = 1;
        msg_Dbg( p_vdec->p_fifo, "library ffmpeg initialized" );
    }
    else
    {
        msg_Dbg( p_vdec->p_fifo, "library ffmpeg already initialized" );
    }
    ffmpeg_GetFfmpegCodec( p_vdec->p_fifo->i_type,
                           &i_ffmpeg_codec,
                           &p_vdec->psz_namecodec );
    p_vdec->p_codec = 
        avcodec_find_decoder( i_ffmpeg_codec );
    
    if( !p_vdec->p_codec )
    {
        msg_Err( p_vdec->p_fifo, "codec not found (%s)",
                                 p_vdec->psz_namecodec );
        return( -1 );
    }

    p_vdec->p_context = &p_vdec->context;
    memset( p_vdec->p_context, 0, sizeof( AVCodecContext ) );

    p_vdec->p_context->width  = p_vdec->format.i_width;
    p_vdec->p_context->height = p_vdec->format.i_height;
    
/*  XXX see them and search what that means 
    p_vdec->p_context->workaround_bugs 
    p_vdec->p_context->strict_std_compliance
*/

        if (avcodec_open(p_vdec->p_context, p_vdec->p_codec) < 0)
    {
        msg_Err( p_vdec->p_fifo, "cannot open codec (%s)",
                                 p_vdec->psz_namecodec );
        return( -1 );
    }
    else
    {
        msg_Dbg( p_vdec->p_fifo, "ffmpeg codec (%s) started",
                                 p_vdec->psz_namecodec );
    }

    /* This will be created after the first decoded frame */
    p_vdec->p_vout = NULL;

    return( 0 );
}

/*****************************************************************************
 * DecodeThread: Called for decode one frame
 *****************************************************************************/
static void  DecodeThread( videodec_thread_t *p_vdec )
{
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
        msg_Warn( p_vdec->p_fifo, "cannot decode one frame (%d bytes)",
                                  p_vdec->i_framesize );
        return;
    }
    if( !b_gotpicture )
    {
        return;
    }
    /* Check our vout */
    if( !ffmpeg_CheckVout( p_vdec->p_vout,
                           p_vdec->p_context->width,
                           p_vdec->p_context->height,
                           p_vdec->p_context->aspect_ratio_info,
                           ffmpeg_PixFmtToChroma(p_vdec->p_context->pix_fmt)) )
    {
        p_vdec->p_vout = 
          ffmpeg_CreateVout( p_vdec,
                             p_vdec->p_context->width,
                             p_vdec->p_context->height,
                             p_vdec->p_context->aspect_ratio_info,
                             ffmpeg_PixFmtToChroma(p_vdec->p_context->pix_fmt));
        if( !p_vdec->p_vout )
        {
            msg_Err( p_vdec->p_fifo, "cannot create vout" );
            p_vdec->p_fifo->b_error = 1; /* abort */
            return;
        }
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
    
    ffmpeg_ConvertPicture( p_pic, 
                           &avpicture, 
                           p_vdec );
    

    /* FIXME correct avi and use i_dts */
    vout_DatePicture( p_vdec->p_vout, p_pic, p_vdec->i_pts);
    vout_DisplayPicture( p_vdec->p_vout, p_pic );
    
    return;
}


/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( videodec_thread_t *p_vdec )
{
    if( !p_vdec )
    {
        return;
    }

    if( p_vdec->p_context != NULL)
    {
        avcodec_close( p_vdec->p_context );
        msg_Dbg( p_vdec->p_fifo, "ffmpeg codec (%s) stopped",
                                 p_vdec->psz_namecodec );
    }

    if( p_vdec->p_vout != NULL )
    {
        /* We are about to die. Reattach video output to p_vlc. */
        vlc_object_detach( p_vdec->p_vout, p_vdec->p_fifo );
        vlc_object_attach( p_vdec->p_vout, p_vdec->p_fifo->p_vlc );
    }
    
    free( p_vdec );
}
