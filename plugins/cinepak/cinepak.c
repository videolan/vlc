/*****************************************************************************
 * cinepak.c: cinepak video decoder 
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: cinepak.c,v 1.5 2002/07/31 20:56:51 sam Exp $
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
#include "cinepak.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      OpenDecoder     ( vlc_object_t * );
static int      RunDecoder      ( decoder_fifo_t * );
static int      InitThread      ( videodec_thread_t * );
static void     EndThread       ( videodec_thread_t * );
static void     DecodeThread    ( videodec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( "Cinepak video decoder" );
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
    
    switch( p_fifo->i_fourcc )
    {
        case VLC_FOURCC('c','v','i','d'):
        case VLC_FOURCC('C','V','I','D'):
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

static inline u16 GetWBE( u8 *p_buff )
{
    return( (p_buff[0]<<8) + p_buff[1] );
}

static inline u32 GetDWBE( u8 *p_buff )
{
    return( (p_buff[0] << 24) + ( p_buff[1] <<16 ) +
            ( p_buff[2] <<8 ) + p_buff[3] );
}

#define GET2BYTES( p ) \
    GetWBE( p ); p+= 2;
/* FIXME */
#define GET3BYTES( p ) \
    (GetDWBE( p ) >> 8); p+= 3;

#define GET4BYTES( p ) \
    GetDWBE( p ); p+= 4;

#define FREE( p ) \
    if( p ) free( p )

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

static int cinepak_CheckVout( vout_thread_t *p_vout,
                              int i_width,
                              int i_height )
{
    if( !p_vout )
    {
        return( 0 );
    }
    
    if( ( p_vout->render.i_width != i_width )||
        ( p_vout->render.i_height != i_height )||
        ( p_vout->render.i_chroma != VLC_FOURCC('I','4','2','0') )||
        ( p_vout->render.i_aspect != VOUT_ASPECT_FACTOR * i_width / i_height) )
    {
        return( 0 );
    }
    else
    {
        return( 1 );
    }
}

/* Return a Vout */

static vout_thread_t *cinepak_CreateVout( videodec_thread_t *p_vdec,
                                         int i_width,
                                         int i_height )
{
    vout_thread_t *p_vout;

    if( (!i_width)||(!i_height) )
    {
        return( NULL ); /* Can't create a new vout without display size */
    }

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
        if( !cinepak_CheckVout( p_vout, i_width, i_height ) )
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
                                    VLC_FOURCC('I','4','2','0'),
                                    VOUT_ASPECT_FACTOR * i_width / i_height );
    }

    return( p_vout );
}

void cinepak_LoadCodebook( cinepak_codebook_t *p_codebook,
                           u8 *p_data,
                           int b_grayscale )
{
    int i, i_y[4], i_u, i_v, i_Cb, i_Cr;
    int i_uv;
#define SCALEBITS 12
#define FIX( x ) ( (int)( (x) * ( 1L << SCALEBITS ) + 0.5 ) )
    
    for( i = 0; i < 4; i++ )
    {
        i_y[i] = (u8)( *(p_data++) );
    }
    if( b_grayscale )
    {
        i_u  = (s8)( *(p_data++) );
        i_v  = (s8)( *(p_data++) );
    }
    else
    {
        i_u  = 0;
        i_v  = 0;
    }
    
    /*
          | Y  |   | 1 -0.0655  0.0110 | | CY |
          | Cb | = | 0  1.1656 -0.0062 | | CU |
          | Cr |   | 0  0.0467  1.4187 | | CV |
     */
    i_uv = ( FIX( -0.0655 ) * i_u + FIX( 0.0110 ) * i_v ) >> SCALEBITS;
    for( i = 0; i < 4; i++ )
    {
        i_y[i] += i_uv;
    }
    i_Cb  = ( FIX( 1.1656 ) * i_u + FIX( -0.0062 ) * i_v ) >> SCALEBITS;
    i_Cr  = ( FIX( 0.0467 ) * i_u + FIX(  1.4187 ) * i_v ) >> SCALEBITS;
    
    for( i = 0; i < 4; i++ )
    {
        p_codebook->i_y[i] = __MIN( __MAX( 0, i_y[i] ), 255 );
    }
    p_codebook->i_u  = __MIN( __MAX( 0, i_Cb + 128 ), 255 );
    p_codebook->i_v  = __MIN( __MAX( 0, i_Cr + 128 ), 255 );
   
#undef FIX
#undef SCALEBITS
}

void cinepak_Getv4( cinepak_context_t *p_context,
                    int i_strip,
                    int i_x,  int i_y,
                    int i_x2, int i_y2,
                    u8 *p_data )
{
    u8 i_index[4];
    int i,j;
    
    u8 *p_dst_y, *p_dst_u, *p_dst_v;
#define PIX_SET_Y( x, y, v ) \
    p_dst_y[(x) + (y)* p_context->i_stride[0]] = (v);
    
#define PIX_SET_UV( i, p, x, y, v ) \
    p[(x) + (y)* (p_context->i_stride[i])] = (v);
   
    for( i = 0; i < 4; i++ )
    {
        i_index[i] = *(p_data++);
    }

    /* y plane */
    p_dst_y = p_context->p_pix[0] + p_context->i_stride[0] * i_y + i_x;
    p_dst_u = p_context->p_pix[1] + p_context->i_stride[1] * (i_y/2) + (i_x/2);
    p_dst_v = p_context->p_pix[2] + p_context->i_stride[2] * (i_y/2) + (i_x/2);
    
    for( i = 0; i < 2; i++ )
    {
        for( j = 0; j < 2; j ++ )
        {
            PIX_SET_Y( 2*j + 0, 2*i + 0, 
                       p_context->codebook_v4[i_strip][i_index[j+2*i]].i_y[0]);
            PIX_SET_Y( 2*j + 1, 2*i + 0, 
                       p_context->codebook_v4[i_strip][i_index[j+2*i]].i_y[1]);
            PIX_SET_Y( 2*j + 0, 2*i + 1, 
                       p_context->codebook_v4[i_strip][i_index[j+2*i]].i_y[2]);
            PIX_SET_Y( 2*j + 1, 2*i + 1, 
                       p_context->codebook_v4[i_strip][i_index[j+2*i]].i_y[3]);

            PIX_SET_UV( 1, p_dst_u, j, i, 
                        p_context->codebook_v4[i_strip][i_index[j+2*i]].i_u );
            PIX_SET_UV( 2, p_dst_v, j, i, 
                        p_context->codebook_v4[i_strip][i_index[j+2*i]].i_v );
        }
    }
#undef PIX_SET_Y
#undef PIX_SET_UV
}

void cinepak_Getv1( cinepak_context_t *p_context,
                    int i_strip,
                    int i_x,  int i_y,
                    int i_x2, int i_y2,
                    u8 *p_data )
{
    u8 i_index;
    int i,j;
    
    u8 *p_dst_y, *p_dst_u, *p_dst_v;
#define PIX_SET_Y( x, y, v ) \
    p_dst_y[(x) + (y)* p_context->i_stride[0]] = (v);
    
#define PIX_SET_UV( i,p, x, y, v ) \
    p[(x) + (y)* (p_context->i_stride[i])] = (v);
   
    i_index = *(p_data++);

    /* y plane */
    p_dst_y = p_context->p_pix[0] + p_context->i_stride[0] * i_y + i_x;
    p_dst_u = p_context->p_pix[1] + p_context->i_stride[1] * (i_y/2) + (i_x/2);
    p_dst_v = p_context->p_pix[2] + p_context->i_stride[2] * (i_y/2) + (i_x/2);

    for( i = 0; i < 2; i++ )
    {
        for( j = 0; j < 2; j ++ )
        {
            PIX_SET_Y( 2*j + 0, 2*i + 0, 
                       p_context->codebook_v1[i_strip][i_index].i_y[0] );
            PIX_SET_Y( 2*j + 1, 2*i + 0, 
                       p_context->codebook_v1[i_strip][i_index].i_y[1] );
            PIX_SET_Y( 2*j + 0, 2*i + 1, 
                       p_context->codebook_v1[i_strip][i_index].i_y[2] );
            PIX_SET_Y( 2*j + 1, 2*i + 1, 
                       p_context->codebook_v1[i_strip][i_index].i_y[3] );

            PIX_SET_UV( 1,p_dst_u, j, i, 
                        p_context->codebook_v1[i_strip][i_index].i_u );
            PIX_SET_UV( 2,p_dst_v, j, i, 
                        p_context->codebook_v1[i_strip][i_index].i_v );
        }
    }

#undef PIX_SET_Y
#undef PIX_SET_UV
}

/*****************************************************************************
 * The function that decode one frame
 *****************************************************************************/
int cinepak_decode_frame( cinepak_context_t *p_context, 
                          int i_length, u8 *p_data )
{
    int i_strip;

    int i_frame_flags;
    int i_frame_size;
    int i_width, i_height;
    int i_frame_strips;
    int i_index;
    int i_strip_x1 =0, i_strip_y1=0;
    int i_strip_x2 =0, i_strip_y2=0;
   
    if( i_length <= 10 )
    {
        /* Broken header or no data */
        return( -1 );
    }
  
    /* get header */
    i_frame_flags  = *(p_data++);
    i_frame_size = GET3BYTES( p_data );
    i_width  = GET2BYTES( p_data );
    i_height = GET2BYTES( p_data );
    i_frame_strips = GET2BYTES( p_data );
    
    /* Check if we have a picture buffer with good size */
    if( ( p_context->i_width != i_width )||
        ( p_context->i_height != i_height ) )
    {
        int i;
        for( i = 0; i < 3; i++ )
        {
            FREE( p_context->p_pix[i] );
        }

        p_context->i_width = i_width;
        p_context->i_height = i_height;

        p_context->i_stride[0] = ( i_width + 3)&0xfffc;
        p_context->i_stride[1] = p_context->i_stride[2] = 
                p_context->i_stride[0] / 2;

        p_context->i_lines[0] = ( i_height + 3 )&0xfffc;
        p_context->i_lines[1] = p_context->i_lines[2] =
                p_context->i_lines[0] /2;
        
        for( i = 0; i < 3; i++ )
        {
            p_context->p_pix[i] = malloc( p_context->i_stride[i] * 
                                          p_context->i_lines[i] );
        }
    }

    if( i_frame_size != i_length )
    {
        i_length = __MIN( i_length, i_frame_size );
    }
    i_length -= 10;

    if( i_frame_strips >= CINEPAK_MAXSTRIP )
    {
        i_frame_strips = CINEPAK_MAXSTRIP;
    }

    /* Now decode each strip */

    for( i_strip = 0; i_strip < i_frame_strips; i_strip++ )
    {
        int i_strip_id;
        int i_strip_size;

        if( i_length <= 12 )
        {
            break;
        }
        
           
        i_strip_id   = GET2BYTES( p_data );
        i_strip_size = GET2BYTES( p_data );
        i_strip_size = __MIN( i_strip_size, i_length );
        /* FIXME I don't really understand how it's work; */
        i_strip_y1  = i_strip_y2 + GET2BYTES( p_data );
        i_strip_x1  = GET2BYTES( p_data );
        i_strip_y2  = i_strip_y2 + GET2BYTES( p_data );
        i_strip_x2  = GET2BYTES( p_data );

        i_length -= i_strip_size;

        i_strip_size -= 12;
        /* init codebook , if needed */
        if( ( i_strip > 0 )&&( !(i_frame_flags&0x01) ) )
        {
            memcpy( &p_context->codebook_v1[i_strip], 
                    &p_context->codebook_v1[i_strip-1],
                    sizeof(cinepak_codebook_t[256] ) );

            memcpy( &p_context->codebook_v4[i_strip], 
                    &p_context->codebook_v4[i_strip-1],
                    sizeof(cinepak_codebook_t[256] ) );
        }

        /* Now parse all chunk in this strip */
        while( i_strip_size > 0 )
        {
            cinepak_codebook_t (*p_codebook)[CINEPAK_MAXSTRIP][256];
            int i_mode;

            int i_chunk_id;
            int i_chunk_size;
            u32 i_vector_flags;
            int i_count;
            int i;
            int i_x, i_y; /* (0,0) begin in fact at (x1,y1) ... */

            i_chunk_id   = GET2BYTES( p_data );
            i_chunk_size = GET2BYTES( p_data );
            i_chunk_size  = __MIN( i_chunk_size, i_strip_size ); 
            i_strip_size -= i_chunk_size;
            
            i_chunk_size -= 4;

            i_x = 0; 
            i_y = 0;
            if( i_chunk_size < 0 )
            {
                break;
            }
            
            switch( i_chunk_id )
            {
                case( 0x2000 ): /* 12bits v4 Intra*/
                case( 0x2200 ): /* 12bits v1 Intra*/
                case( 0x2400 ): /* 8bits v4 Intra*/
                case( 0x2600 ): /* 8bits v1 Intra */
                    i_mode = ( ( i_chunk_id&0x0400 ) == 0 );
                    p_codebook = ( i_chunk_id&0x0200 ) ?
                                    &p_context->codebook_v1 :
                                    &p_context->codebook_v4;
                    
                    i_count = __MIN( i_chunk_size / ( i_mode ? 6 : 4 ), 256 );

                    for( i = 0; i < i_count; i++ )
                    {
                        cinepak_LoadCodebook( &((*p_codebook)[i_strip][i]), 
                                              p_data, 
                                       i_mode&~p_context->b_grayscale );
                        p_data += i_mode ? 6 : 4;
                        i_chunk_size -= i_mode ? 6 : 4;
                    }
                    break;

                case( 0x2100 ): /* selective 12bits v4 Inter*/
                case( 0x2300 ): /* selective 12bits v1 Inter*/
                case( 0x2500 ): /* selective 8bits v4 Inter*/
                case( 0x2700 ): /* selective 8bits v1 Inter*/
                    i_mode = ( ( i_chunk_id&0x0400 ) == 0 );
                    p_codebook = ( i_chunk_id&0x0200 ) ?
                                    &p_context->codebook_v1 :
                                    &p_context->codebook_v4;
                    
                    i_index = 0;
                    while( (i_chunk_size > 4)&&(i_index<256))
                    {
                        i_vector_flags = GET4BYTES( p_data );
                        i_chunk_size -= 4;
                        for( i = 0; i < 32; i++ )
                        {
                            if( ( i_chunk_size < ( i_mode ? 6 : 4 ) )||(i_index >= 256 ))
                            {
                                break;
                            }
                            if( i_vector_flags&0x80000000UL )
                            {
                                cinepak_LoadCodebook( &((*p_codebook)[i_strip][i_index]),
                                                      p_data, 
                                            i_mode&~p_context->b_grayscale );

                                p_data += i_mode ? 6 : 4;
                                i_chunk_size -= i_mode ? 6 : 4;
                            }
                            i_index++;
                            i_vector_flags <<= 1;
                        }
                    }
                    break;

                case( 0x3000 ): /* load image Intra */
                    while( (i_chunk_size >= 4 )&&(i_y<i_strip_y2-i_strip_y1) )
                    {
                        i_vector_flags = GET4BYTES( p_data );
                        i_chunk_size -= 4;
                        i_strip_size -= 4;
                        i_length     -= 4;

                        for( i = 0; i < 32; i++ )
                        {
                            if( ( i_y >= i_strip_y2 - i_strip_y1)||
                                    ( i_chunk_size<=0) )
                            {
                                break;
                            }
                            if( i_vector_flags&0x80000000UL )
                            {
                                cinepak_Getv4( p_context,
                                               i_strip,
                                               i_strip_x1 + i_x, 
                                               i_strip_y1 + i_y,
                                               i_strip_x2, i_strip_y2,
                                               p_data );
                                p_data += 4;
                                i_chunk_size -= 4;
                            }
                            else
                            {
                                cinepak_Getv1( p_context,
                                               i_strip,
                                               i_strip_x1 + i_x, 
                                               i_strip_y1 + i_y,
                                               i_strip_x2, i_strip_y2,
                                               p_data );
                                p_data++;
                                i_chunk_size--;
                            }

                            i_x += 4;
                            if( i_x >= i_strip_x2 - i_strip_x1 )
                            {
                                i_x = 0;
                                i_y += 4;
                            }
                            i_vector_flags <<= 1;
                        }
                    } 
                    break;

                case( 0x3100 ): /* load image Inter */
                    while( ( i_chunk_size > 4 )&&( i_y < i_strip_y2 - i_strip_y1) )
                    {
                        u32 i_mask;
                        i_vector_flags = GET4BYTES( p_data );
                        i_chunk_size -= 4;
                        i_mask = 0x80000000UL;

                        while((i_chunk_size > 0 )&&( i_mask )&&( i_y < i_strip_y2 - i_strip_y1 ))
                        {
                            if( i_vector_flags&i_mask)
                            {
                                i_mask >>= 1;
                                if( !i_mask )
                                {
                                    if( i_chunk_size < 4 )
                                    {
                                        break;
                                    } 
                                    i_vector_flags = GET4BYTES( p_data );
                                    i_chunk_size -= 4;
                                    i_mask = 0x80000000UL;
                                }
                                if( i_vector_flags&i_mask )
                                {
                                    if( i_chunk_size < 4 ) break;
                                    cinepak_Getv4( p_context,
                                                   i_strip,
                                                   i_strip_x1 + i_x, 
                                                   i_strip_y1 + i_y,
                                                   i_strip_x2, i_strip_y2,
                                                   p_data );
                                    p_data += 4;
                                    i_chunk_size -= 4;
                                }
                                else
                                {
                                    if( i_chunk_size < 1 ) break;
                                    cinepak_Getv1( p_context,
                                                   i_strip,
                                                   i_strip_x1 + i_x, 
                                                   i_strip_y1 + i_y,
                                                   i_strip_x2, i_strip_y2,
                                                   p_data );
                                    p_data++;
                                    i_chunk_size--;
                                }
                            }
                            i_mask >>= 1;

                            i_x += 4;
                            if( i_x >= i_strip_x2 - i_strip_x1 )
                            {
                                i_x = 0;
                                i_y += 4;
                            }
                        }
                    } 
                    break;

                case( 0x3200 ): /* load intra picture but all v1*/
                    while( ( i_chunk_size > 0 )&&
                           ( i_y < i_strip_y2 - i_strip_y1) )
                    {
                        cinepak_Getv1( p_context,
                                       i_strip,
                                       i_strip_x1 + i_x, 
                                       i_strip_y1 + i_y,
                                       i_strip_x2, i_strip_y2,
                                       p_data );
                        p_data++;
                        i_chunk_size--;

                        i_x += 4;
                        if( i_x >= i_strip_x2 - i_strip_x1 )
                        {
                            i_x = 0;
                            i_y += 4;
                        }
                        
                    } 
                    break;
                    


                default:
                    break;

            }
            p_data += i_chunk_size ; /* skip remains bytes */
            
        }
    }

    
    return( 0 );
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

    /* This will be created after the first decoded frame */
    if( !(p_vdec->p_context = malloc( sizeof( cinepak_context_t ) ) ) )
    {
        msg_Err( p_vdec->p_fifo, "out of memory" );
    }
    memset( p_vdec->p_context, 0, sizeof( cinepak_context_t ) );

    if( config_GetInt( p_vdec->p_fifo, "grayscale" ) )
    {
        p_vdec->p_context->b_grayscale = 1;
    }
    else
    {
        p_vdec->p_context->b_grayscale = 0;
    }
    
    p_vdec->p_vout = NULL;
    msg_Dbg( p_vdec->p_fifo, "cinepak decoder started" );
    return( 0 );
}


/*****************************************************************************
 * DecodeThread: Called for decode one frame
 *****************************************************************************/
static void  DecodeThread( videodec_thread_t *p_vdec )
{
    int     i_status;
    
    int i_plane;
    u8 *p_dst, *p_src;
    picture_t *p_pic; /* videolan picture */

    __GetFrame( p_vdec );

    i_status = cinepak_decode_frame( p_vdec->p_context,
                                     p_vdec->i_framesize,
                                     p_vdec->p_framedata );
    __NextFrame( p_vdec );
                                         
    if( i_status < 0 )
    {
        msg_Warn( p_vdec->p_fifo, "cannot decode one frame (%d bytes)",
                                  p_vdec->i_framesize );
        return;
    }
    
    /* Check our vout */
    if( !cinepak_CheckVout( p_vdec->p_vout,
                            p_vdec->p_context->i_width,
                            p_vdec->p_context->i_height ) )
    {
        p_vdec->p_vout = 
          cinepak_CreateVout( p_vdec,
                              p_vdec->p_context->i_width,
                              p_vdec->p_context->i_height );

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
    
    for( i_plane = 0; i_plane < 3; i_plane++ )
    {
        int i_line, i_lines;

        p_dst = p_pic->p[i_plane].p_pixels;
        p_src = p_vdec->p_context->p_pix[i_plane];

        i_lines = __MIN( p_vdec->p_context->i_lines[i_plane],
                         p_pic->p[i_plane].i_lines );
        for( i_line = 0; i_line < i_lines; i_line++ )
        {
            memcpy( p_dst, 
                    p_src, 
                    __MIN( p_pic->p[i_plane].i_pitch,
                           p_vdec->p_context->i_stride[i_plane] ) );
            p_dst += p_pic->p[i_plane].i_pitch;
            p_src += p_vdec->p_context->i_stride[i_plane];
        }
    }

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
    int i;
    
    if( !p_vdec )
    {
        return;
    }
    msg_Dbg( p_vdec->p_fifo, "cinepak decoder stopped" );

    for( i = 0; i < 3; i++ )
    {
        FREE( p_vdec->p_context->p_pix[i] );
    }
    
    free( p_vdec->p_context );
    
    if( p_vdec->p_vout != NULL )
    {
        /* We are about to die. Reattach video output to p_vlc. */
        vlc_object_detach( p_vdec->p_vout, p_vdec->p_fifo );
        vlc_object_attach( p_vdec->p_vout, p_vdec->p_fifo->p_vlc );
    }
    
    free( p_vdec );
}


