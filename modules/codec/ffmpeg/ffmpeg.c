/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ffmpeg.c,v 1.6 2002/08/26 09:12:46 sam Exp $
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

#include "avcodec.h"                                            /* ffmpeg */

#include "postprocessing/postprocessing.h"

#include "ffmpeg.h"

/*
 * Local prototypes
 */
static int      OpenDecoder     ( vlc_object_t * );
static int      RunDecoder      ( decoder_fifo_t * );
static int      InitThread      ( videodec_thread_t * );
static void     EndThread       ( videodec_thread_t * );
static void     DecodeThread    ( videodec_thread_t * );


static int      b_ffmpeginit = 0;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ERROR_RESILIENCE_LONGTEXT \
    "ffmpeg can make errors resiliences.          \n"\
    "Nevertheless, with buggy encoder (like ISO MPEG-4 encoder from M$) " \
    "this will produce a lot of errors.\n" \
    "Valid range is -1 to 99 (-1 disable all errors resiliences)."

#define HURRY_UP_LONGTEXT \
    "Allow the decoder to partially decode or skip frame(s) " \
    "when there not enough time.\n It's usefull with low CPU power " \
    "but it could produce broken pictures."

#define POSTPROCESSING_Q_LONGTEXT \
    "Quality of post processing\n"\
    "Valid range is 0 to 6\n" \
    "( Overridden by others setting)"
    
#define POSTPROCESSING_AQ_LONGTEXT \
    "Post processing quality is selected upon time left" \
    "but no more than requested quality\n" \
    "Not yet implemented !"

vlc_module_begin();
    add_category_hint( N_("Ffmpeg"), NULL );
#if LIBAVCODEC_BUILD >= 4611
    add_integer ( "ffmpeg-error-resilience", -1, NULL, 
                  "error resilience", ERROR_RESILIENCE_LONGTEXT );
    add_integer ( "ffmpeg-workaround-bugs", 0, NULL, 
                  "workaround bugs", "0-99, seems to be for msmpeg v3\n"  );
#endif
    add_bool( "ffmpeg-hurry-up", 0, NULL, "hurry up", HURRY_UP_LONGTEXT );
    
    add_category_hint( N_("Post processing"), NULL );
    add_module( "ffmpeg-pp", "postprocessing",NULL, NULL,
                N_( "ffmpeg postprocessing module" ), NULL ); 
    add_integer( "ffmpeg-pp-q", 0, NULL,
                 "post processing quality", POSTPROCESSING_Q_LONGTEXT );
    add_bool( "ffmpeg-pp-auto", 0, NULL,
              "auto-level Post processing quality", POSTPROCESSING_AQ_LONGTEXT );
    add_bool( "ffmpeg-db-yv", 0, NULL, 
              "force vertical luminance deblocking", 
              "force vertical luminance deblocking (override other settings)" );
    add_bool( "ffmpeg-db-yh", 0, NULL, 
              "force horizontal luminance deblocking",
              "force horizontal luminance deblocking (override other settings)" );
    add_bool( "ffmpeg-db-cv", 0, NULL, 
              "force vertical chrominance deblocking",
              "force vertical chrominance deblocking (override other settings)" );
    add_bool( "ffmpeg-db-ch", 0, NULL, 
              "force horizontal chrominance deblocking",
              "force horizontal chrominance deblocking (override other settings) " );
    add_bool( "ffmpeg-dr-y", 0, NULL,
              "force luminance deringing",
              "force luminance deringing (override other settings)" );
    add_bool( "ffmpeg-dr-c", 0, NULL,
              "force chrominance deringing",
              "force chrominance deringing (override other settings)" );
      
    set_description( _("ffmpeg video decoder((MS)MPEG4,SVQ1,H263)") );
    set_capability( "decoder", 70 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( ffmpeg_GetFfmpegCodec( p_fifo->i_fourcc, NULL, NULL ) )
    {
        p_fifo->pf_run = RunDecoder;
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
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

#define FREE( p ) if( p ) free( p ); p = NULL
    
static void ffmpeg_ParseBitMapInfoHeader( bitmapinfoheader_t *p_bh, 
                                          u8 *p_data )
{
    p_bh->i_size          = GetDWLE( p_data );
    p_bh->i_width         = GetDWLE( p_data + 4 );
    p_bh->i_height        = GetDWLE( p_data + 8 );
    p_bh->i_planes        = GetWLE( p_data + 12 );
    p_bh->i_bitcount      = GetWLE( p_data + 14 );
    p_bh->i_compression   = GetDWLE( p_data + 16 );
    p_bh->i_sizeimage     = GetDWLE( p_data + 20 );
    p_bh->i_xpelspermeter = GetDWLE( p_data + 24 );
    p_bh->i_ypelspermeter = GetDWLE( p_data + 28 );
    p_bh->i_clrused       = GetDWLE( p_data + 32 );
    p_bh->i_clrimportant  = GetDWLE( p_data + 36 );

    if( p_bh->i_size > 40 )
    {
        p_bh->i_data = p_bh->i_size - 40;
        if( ( p_bh->p_data = malloc( p_bh->i_data ) ) )
        {
            memcpy( p_bh->p_data, p_data + 40, p_bh->i_data );
        }
        else
        {
            p_bh->i_data = 0;
        }
    }
    else
    {
        p_bh->i_data = 0;
        p_bh->p_data = NULL;
    } 

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
    if( p_vdec->i_buffer_size < p_pes->i_pes_size )
    {
        if( p_vdec->p_buffer )
        {
            p_vdec->p_buffer = realloc( p_vdec->p_buffer,
                                        p_pes->i_pes_size );
        }
        else
        {
            p_vdec->p_buffer = malloc( p_pes->i_pes_size );
        }
        p_vdec->i_buffer_size = p_pes->i_pes_size;
    }
    
    p_buffer = p_vdec->p_framedata = p_vdec->p_buffer;
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
    __PES_NEXT( p_vdec->p_fifo );
}


/* Check if we have a Vout with good parameters */
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
        /* we will try to make conversion */
        i_chroma = VLC_FOURCC('I','4','2','0');
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
        /* we make conversion if possible*/
        i_chroma = VLC_FOURCC('I','4','2','0');
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
            vlc_object_detach( p_vout );
            vlc_object_release( p_vout );
            vout_DestroyThread( p_vout );
            p_vout = NULL;
        }
        else
        {
            /* This video output is cool! Hijack it. */
            vlc_object_detach( p_vout );
            vlc_object_attach( p_vout, p_vdec->p_fifo );
            vlc_object_release( p_vout );
        }
    }

    if( p_vout == NULL )
    {
        msg_Dbg( p_vdec->p_fifo, "no vout present, spawning one" );
    
        p_vout = vout_CreateThread( p_vdec->p_fifo,
                                    i_width, i_height,
                                    i_chroma, i_aspect );
    }
    
    return( p_vout );
}

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

static void ffmpeg_GetPicture( picture_t *p_pic,
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
        if( ( p_vdec->i_pp_mode )&&
            ( ( p_vdec->p_vout->render.i_chroma == 
                    VLC_FOURCC( 'I','4','2','0' ) )||
              ( p_vdec->p_vout->render.i_chroma == 
                    VLC_FOURCC( 'Y','V','1','2' ) ) ) )
        {
            /* Make postproc */
#if LIBAVCODEC_BUILD > 4313
            p_vdec->p_pp->pf_postprocess( p_pic,
                                          p_vdec->p_context->quant_store, 
                                          p_vdec->p_context->qstride,
                                          p_vdec->i_pp_mode );
#endif
        }
    }
    else
    {
        /* we need to convert to I420 */
        switch( p_vdec->p_context->pix_fmt )
        {
#if LIBAVCODEC_BUILD >= 4615
            case( PIX_FMT_YUV410P ):
                ffmpeg_ConvertPictureI410toI420( p_pic, p_avpicture, p_vdec );
                break;
#endif            
            default:
                p_vdec->p_fifo->b_error = 1;
                break;
        }

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
 *
 * ffmpeg codec will be open, some memory allocated. But Vout is not yet
 *   open (done after the first decoded frame)
 *****************************************************************************/
static int InitThread( videodec_thread_t *p_vdec )
{
    int i_ffmpeg_codec; 
    int i_tmp;
    
    if( p_vdec->p_fifo->p_demux_data )
    {
        ffmpeg_ParseBitMapInfoHeader( &p_vdec->format, 
                                      (u8*)p_vdec->p_fifo->p_demux_data );
    }
    else
    {
        msg_Warn( p_vdec->p_fifo, "display informations missing" );
    }

    /* **** init ffmpeg library (libavcodec) ***** */
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

    /* ***** Search for codec ***** */
    ffmpeg_GetFfmpegCodec( p_vdec->p_fifo->i_fourcc,
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

    /* ***** Fill p_context with init values ***** */
    p_vdec->p_context = &p_vdec->context;
    memset( p_vdec->p_context, 0, sizeof( AVCodecContext ) );

    p_vdec->p_context->width  = p_vdec->format.i_width;
    p_vdec->p_context->height = p_vdec->format.i_height;
    
    /*  ***** Get configuration of ffmpeg plugin ***** */
#if LIBAVCODEC_BUILD >= 4611
    i_tmp = config_GetInt( p_vdec->p_fifo, "ffmpeg-workaround-bugs" );
    p_vdec->p_context->workaround_bugs  = __MAX( __MIN( i_tmp, 99 ), 0 );

    i_tmp = config_GetInt( p_vdec->p_fifo, "ffmpeg-error-resilience" );
    p_vdec->p_context->error_resilience = __MAX( __MIN( i_tmp, 99 ), -1 );
#endif
#if LIBAVCODEC_BUILD >= 4614
    if( config_GetInt( p_vdec->p_fifo, "grayscale" ) )
    {
        p_vdec->p_context->flags|= CODEC_FLAG_GRAY;
    }
#endif
    p_vdec->b_hurry_up = config_GetInt(p_vdec->p_fifo, "ffmpeg-hurry-up");
    

    /* ***** Open the codec ***** */ 
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

    /* ***** init this codec with special data(up to now MPEG4 only) ***** */
    if( p_vdec->format.i_data )
    {
        AVPicture avpicture;
        int b_gotpicture;
        
        switch( i_ffmpeg_codec )
        {
            case( CODEC_ID_MPEG4 ):
                avcodec_decode_video( p_vdec->p_context, &avpicture, 
                                      &b_gotpicture,
                                      p_vdec->format.p_data,
                                      p_vdec->format.i_data );
                break;
            default:
                break;
        }
    }
    
    /* ***** Load post processing ***** */

    /* get overridding settings */
    p_vdec->i_pp_mode = 0;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-yv" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_Y_V;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-yh" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_Y_H;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-cv" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_C_V;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-db-ch" ) )
        p_vdec->i_pp_mode |= PP_DEBLOCK_C_H;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-dr-y" ) )
        p_vdec->i_pp_mode |= PP_DERING_Y;
    if( config_GetInt( p_vdec->p_fifo, "ffmpeg-dr-c" ) )
        p_vdec->i_pp_mode |= PP_DERING_C;
    
    if( ( config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-q" ) > 0 )||
        ( config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-auto" )  )||
        ( p_vdec->i_pp_mode != 0 ) )
    {
        /* check if the codec support postproc. */
        switch( i_ffmpeg_codec )
        {
#if LIBAVCODEC_BUILD > 4608
            case( CODEC_ID_MSMPEG4V1 ):
            case( CODEC_ID_MSMPEG4V2 ):
            case( CODEC_ID_MSMPEG4V3 ):
#else
            case( CODEC_ID_MSMPEG4 ):
#endif
            case( CODEC_ID_MPEG4 ):
            case( CODEC_ID_H263 ):
//            case( CODEC_ID_H263P ): I don't use it up to now
            case( CODEC_ID_H263I ):
                /* Ok we can make postprocessing :)) */
               /* first try to get a postprocess module */
#if LIBAVCODEC_BUILD > 4613
                p_vdec->p_pp = vlc_object_create( p_vdec->p_fifo,
                                                  sizeof( postprocessing_t ) );
                p_vdec->p_pp->psz_object_name = "postprocessing";
                p_vdec->p_pp->p_module = 
                   module_Need( p_vdec->p_pp, "postprocessing", "$ffmpeg-pp" );

                if( !p_vdec->p_pp->p_module )
                {
                    msg_Warn( p_vdec->p_fifo, 
                              "no suitable postprocessing module" );
                    vlc_object_destroy( p_vdec->p_pp );
                    p_vdec->p_pp = NULL;
                    p_vdec->i_pp_mode = 0;
                }
                else
                {
                    /* get mode upon quality */
                    p_vdec->i_pp_mode |= 
                        p_vdec->p_pp->pf_getmode( 
                              config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-q" ),
                              config_GetInt( p_vdec->p_fifo, "ffmpeg-pp-auto" )
                                                );

                    /* allocate table for postprocess */
                    p_vdec->p_context->quant_store = 
                        malloc( sizeof( int ) * ( MBR + 1 ) * ( MBC + 1 ) );
                    p_vdec->p_context->qstride = MBC + 1;
                }
#else
                p_vdec->i_pp_mode = 0;
                msg_Warn( p_vdec->p_fifo, 
                          "post-processing not supported, upgrade ffmpeg" );
#endif
                break;
            default:
                p_vdec->i_pp_mode = 0;
                msg_Warn( p_vdec->p_fifo, 
                          "Post processing unsupported for this codec" );
                break;
        }

    }
//    memset( &p_vdec->statistic, 0, sizeof( statistic_t ) );

    return( 0 );
}

/*****************************************************************************
 * DecodeThread: Called for decode one frame
 *****************************************************************************/
static void  DecodeThread( videodec_thread_t *p_vdec )
{
    int     i_status;
    int     b_drawpicture;
    int     b_gotpicture;
    AVPicture avpicture;  /* ffmpeg picture */
    picture_t *p_pic; /* videolan picture */
    /* we have to get a frame stored in a pes 
       give it to ffmpeg decoder 
       and send the image to the output */ 

    /* TODO implement it in a better way */
    /* A good idea could be to decode all I pictures and see for the other */
    if( ( p_vdec->b_hurry_up )&&
        ( p_vdec->i_frame_late > 4 ) )
    {
#if LIBAVCODEC_BUILD > 4603
        b_drawpicture = 0;
        if( p_vdec->i_frame_late < 8 )
        {
            p_vdec->p_context->hurry_up = 2;
        }
        else
        {
            /* too much late picture, won't decode 
               but break picture until a new I, and for mpeg4 ...*/
            p_vdec->i_frame_late--; /* needed else it will never be decrease */
            __PES_NEXT( p_vdec->p_fifo );
            return;
        }
#else
        if( p_vdec->i_frame_late < 8 )
        {
            b_drawpicture = 0; /* not really good but .. UPGRADE FFMPEG !! */
        }
        else
        {
            /* too much late picture, won't decode 
               but break picture until a new I, and for mpeg4 ...*/
            p_vdec->i_frame_late--; /* needed else it will never be decrease */
            __PES_NEXT( p_vdec->p_fifo );
            return;
        }
#endif
    }
    else
    {
        b_drawpicture = 1;
#if LIBAVCODEC_BUILD > 4603
        p_vdec->p_context->hurry_up = 0;
#endif
    }

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
        p_vdec->i_frame_error++;
        return;
    }
    /* Update frame late count*/
    /* I don't make statistic on decoding time */
    if( p_vdec->i_pts <= mdate()) 
    {
        p_vdec->i_frame_late++;
    }
    else
    {
        p_vdec->i_frame_late = 0;
    }

    if( !b_gotpicture || avpicture.linesize[0] == 0 || !b_drawpicture)
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

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_vdec->p_vout, 0, 0, 0 ) ) )
    {
        if( p_vdec->p_fifo->b_die || p_vdec->p_fifo->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    /* fill p_picture_t from avpicture, do I410->I420 if needed
       and do post-processing if requested */    
    ffmpeg_GetPicture( p_pic, &avpicture, p_vdec );

    /* FIXME correct avi and use i_dts */

    /* Send decoded frame to vout */
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
    if( p_vdec->p_pp )
    {
        /* release postprocessing module */
        module_Unneed( p_vdec->p_pp, p_vdec->p_pp->p_module );
        vlc_object_destroy( p_vdec->p_pp );
        p_vdec->p_pp = NULL;
    }

    if( p_vdec->p_context != NULL)
    {
        FREE( p_vdec->p_context->quant_store );
        avcodec_close( p_vdec->p_context );
        msg_Dbg( p_vdec->p_fifo, "ffmpeg codec (%s) stopped",
                                 p_vdec->psz_namecodec );
    }

    if( p_vdec->p_vout != NULL )
    {
        /* We are about to die. Reattach video output to p_vlc. */
        vlc_object_detach( p_vdec->p_vout );
        vlc_object_attach( p_vdec->p_vout, p_vdec->p_fifo->p_vlc );
    }

    FREE( p_vdec->format.p_data );
    FREE( p_vdec->p_buffer );

    free( p_vdec );
}

