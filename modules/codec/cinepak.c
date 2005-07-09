/*****************************************************************************
 * cinepak.c: cinepak video decoder
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder ( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

vlc_module_begin();
    set_description( _("Cinepak video decoder") );
    set_capability( "decoder", 100 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_callbacks( OpenDecoder, CloseDecoder );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define CINEPAK_MAXSTRIP 32

typedef struct
{
    uint8_t i_y[4];
    uint8_t i_u, i_v;

} cinepak_codebook_t;

typedef struct
{
    int b_grayscale; /* force to grayscale */

    int i_width;
    int i_height;

    int i_stride_x;
    int i_stride_y;

    uint8_t *p_y, *p_u, *p_v;

    int i_stride[3]; /* our 3 planes */
    int i_lines[3];
    uint8_t *p_pix[3];

    cinepak_codebook_t codebook_v1[CINEPAK_MAXSTRIP][256];
    cinepak_codebook_t codebook_v4[CINEPAK_MAXSTRIP][256];

} cinepak_context_t;

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Cinepak properties
     */
    cinepak_context_t context;
};

static picture_t *DecodeBlock ( decoder_t *, block_t ** );

static int cinepak_decode_frame( cinepak_context_t *, int, uint8_t * );

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    vlc_value_t val;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('c','v','i','d') &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('C','V','I','D') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    memset( &p_sys->context, 0, sizeof( cinepak_context_t ) );

    var_Create( p_dec, "grayscale", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_dec, "grayscale", &val );
    p_sys->context.b_grayscale = val.b_bool;

    p_dec->pf_decode_video = DecodeBlock;

    msg_Dbg( p_dec, "cinepak decoder started" );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with whole frames.
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_status, i_plane;
    uint8_t *p_dst, *p_src;
    picture_t *p_pic;
    block_t *p_block;

    if( !pp_block || !*pp_block )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    i_status = cinepak_decode_frame( &p_sys->context, p_block->i_buffer,
                                     p_block->p_buffer );
    if( i_status < 0 )
    {
        msg_Warn( p_dec, "cannot decode one frame (%d bytes)",
                  p_block->i_buffer );
        block_Release( p_block );
        return NULL;
    }

    p_dec->fmt_out.video.i_width = p_sys->context.i_width;
    p_dec->fmt_out.video.i_height = p_sys->context.i_height;
    p_dec->fmt_out.video.i_aspect = p_sys->context.i_width
        * VOUT_ASPECT_FACTOR / p_sys->context.i_height;
    p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0');

    /* Get a new picture */
    if( ( p_pic = p_dec->pf_vout_buffer_new( p_dec ) ) )
    {
        for( i_plane = 0; i_plane < 3; i_plane++ )
        {
            int i_line, i_lines;

            p_dst = p_pic->p[i_plane].p_pixels;
            p_src = p_sys->context.p_pix[i_plane];

            i_lines = __MIN( p_sys->context.i_lines[i_plane],
                             p_pic->p[i_plane].i_visible_lines );
            for( i_line = 0; i_line < i_lines; i_line++ )
            {
                memcpy( p_dst, p_src,
                        __MIN( p_pic->p[i_plane].i_pitch,
                               p_sys->context.i_stride[i_plane] ) );
                p_dst += p_pic->p[i_plane].i_pitch;
                p_src += p_sys->context.i_stride[i_plane];
            }
        }

        p_pic->date = p_block->i_pts ? p_block->i_pts : p_block->i_dts;
    }

    block_Release( p_block );
    return p_pic;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i;

    msg_Dbg( p_dec, "cinepak decoder stopped" );

    for( i = 0; i < 3; i++ )
    {
        if( p_sys->context.p_pix[i] ) free( p_sys->context.p_pix[i] );
    }

    free( p_sys );
}

/*****************************************************************************
 * local Functions
 *****************************************************************************/

#define GET2BYTES( p ) \
    GetWBE( p ); p+= 2;
/* FIXME */
#define GET3BYTES( p ) \
    (GetDWBE( p ) >> 8); p+= 3;

#define GET4BYTES( p ) \
    GetDWBE( p ); p+= 4;

#define FREE( p ) \
    if( p ) free( p )

static void cinepak_LoadCodebook( cinepak_codebook_t *p_codebook,
                                  uint8_t *p_data, int b_grayscale )
{
    int i, i_y[4], i_u, i_v, i_Cb, i_Cr;
    int i_uv;
#define SCALEBITS 12
#define FIX( x ) ( (int)( (x) * ( 1L << SCALEBITS ) + 0.5 ) )

    for( i = 0; i < 4; i++ )
    {
        i_y[i] = (uint8_t)( *(p_data++) );
    }
    if( b_grayscale )
    {
        i_u  = (int8_t)( *(p_data++) );
        i_v  = (int8_t)( *(p_data++) );
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

static void cinepak_Getv4( cinepak_context_t *p_context,
                           int i_strip, int i_x, int i_y,
                           int i_x2, int i_y2, uint8_t *p_data )
{
    uint8_t i_index[4];
    int i,j;

    uint8_t *p_dst_y, *p_dst_u, *p_dst_v;
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

static void cinepak_Getv1( cinepak_context_t *p_context,
                           int i_strip, int i_x,  int i_y,
                           int i_x2, int i_y2, uint8_t *p_data )
{
    uint8_t i_index;
    int i,j;

    uint8_t *p_dst_y, *p_dst_u, *p_dst_v;
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
                       p_context->codebook_v1[i_strip][i_index].i_y[2*i+j] );
            PIX_SET_Y( 2*j + 1, 2*i + 0,
                       p_context->codebook_v1[i_strip][i_index].i_y[2*i+j] );
            PIX_SET_Y( 2*j + 0, 2*i + 1,
                       p_context->codebook_v1[i_strip][i_index].i_y[2*i+j] );
            PIX_SET_Y( 2*j + 1, 2*i + 1,
                       p_context->codebook_v1[i_strip][i_index].i_y[2*i+j] );

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
static int cinepak_decode_frame( cinepak_context_t *p_context,
                                 int i_length, uint8_t *p_data )
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

    if( !i_frame_size || !i_width || !i_height )
    {
        /* Broken header */
        return( -1 );
    }

    /* Check if we have a picture buffer with good size */
    if( ( p_context->i_width != i_width ) ||
        ( p_context->i_height != i_height ) )
    {
        int i;
        for( i = 0; i < 3; i++ )
        {
            FREE( p_context->p_pix[i] );
        }

        p_context->i_width = i_width;
        p_context->i_height = i_height;

        p_context->i_stride[0] = ( i_width + 3 ) & 0xfffc;
        p_context->i_stride[1] = p_context->i_stride[2] =
                p_context->i_stride[0] / 2;

        p_context->i_lines[0] = ( i_height + 3 ) & 0xfffc;
        p_context->i_lines[1] = p_context->i_lines[2] =
                p_context->i_lines[0] /2;

        for( i = 0; i < 3; i++ )
        {
            p_context->p_pix[i] = malloc( p_context->i_stride[i] *
                                          p_context->i_lines[i] );
            /* Set it to all black */
            memset( p_context->p_pix[i], ( i == 0 ) ? 0 : 128 ,
                    p_context->i_stride[i] * p_context->i_lines[i] );
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
            uint32_t i_vector_flags;
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
                        if( ( i_chunk_size < ( i_mode ? 6 : 4 ) )
                            || (i_index >= 256 ) )
                        {
                            break;
                        }
                        if( i_vector_flags&0x80000000UL )
                        {
                            cinepak_LoadCodebook(
                                &((*p_codebook)[i_strip][i_index]),
                                p_data, i_mode&~p_context->b_grayscale );

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
                        if( ( i_y >= i_strip_y2 - i_strip_y1) ||
                            ( i_chunk_size<=0 ) )
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
                    uint32_t i_mask;
                    i_vector_flags = GET4BYTES( p_data );
                    i_chunk_size -= 4;
                    i_mask = 0x80000000UL;

                    while( (i_chunk_size > 0 ) && ( i_mask )
                           && ( i_y < i_strip_y2 - i_strip_y1 ) )
                    {
                        if( i_vector_flags&i_mask )
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
                while( ( i_chunk_size > 0 ) &&
                       ( i_y < i_strip_y2 - i_strip_y1 ) )
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
