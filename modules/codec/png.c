/*****************************************************************************
 * png.c: png decoder module making use of libpng.
 *****************************************************************************
 * Copyright (C) 1999-2001 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <png.h>

/* PNG_SYS_COMMON_MEMBERS:
 * members common to encoder and decoder descriptors
 */
#define PNG_SYS_COMMON_MEMBERS                              \
/**@{*/                                                     \
    bool b_error;                                           \
    vlc_object_t *p_obj;                                    \
/**@}*/                                                     \

/*
 * png common descriptor
 */
struct png_sys_t
{
    PNG_SYS_COMMON_MEMBERS
};

typedef struct png_sys_t png_sys_t;

/*****************************************************************************
 * decoder_sys_t : png decoder descriptor
 *****************************************************************************/
typedef struct
{
    PNG_SYS_COMMON_MEMBERS
} decoder_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static int DecodeBlock  ( decoder_t *, block_t * );

/*
 * png encoder descriptor
 */
typedef struct
{
    PNG_SYS_COMMON_MEMBERS
    int i_blocksize;
} encoder_sys_t;

static int  OpenEncoder(vlc_object_t *);
static void CloseEncoder(vlc_object_t *);

static block_t *EncodeBlock(encoder_t *, picture_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_description( N_("PNG video decoder") )
    set_capability( "video decoder", 1000 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "png" )

    /* encoder submodule */
    add_submodule()
    add_shortcut("png")
    set_section(N_("Encoding"), NULL)
    set_description(N_("PNG video encoder"))
    set_capability("encoder", 1000)
    set_callbacks(OpenEncoder, CloseEncoder)
vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_PNG &&
        p_dec->fmt_in.i_codec != VLC_FOURCC('M','P','N','G') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    decoder_sys_t *p_sys = malloc( sizeof(decoder_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;

    p_sys->p_obj = p_this;

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_RGBA;
    p_dec->fmt_out.video.transfer  = TRANSFER_FUNC_SRGB;
    p_dec->fmt_out.video.space     = COLOR_SPACE_SRGB;
    p_dec->fmt_out.video.primaries = COLOR_PRIMARIES_SRGB;
    p_dec->fmt_out.video.color_range = COLOR_RANGE_FULL;

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;

    return VLC_SUCCESS;
}

static void user_read( png_structp p_png, png_bytep data, png_size_t i_length )
{
    block_t *p_block = (block_t *)png_get_io_ptr( p_png );
    if( i_length > p_block->i_buffer ) {
        png_error( p_png, "not enough data" );
        return;
    }

    memcpy( data, p_block->p_buffer, i_length );
    p_block->p_buffer += i_length;
    p_block->i_buffer -= i_length;
}

static void user_flush( png_structp p_png )
{
    /* noop */
    (void) p_png;
}

static void user_write( png_structp p_png, png_bytep data, png_size_t i_length )
{
    block_t *p_block = (block_t *)png_get_io_ptr( p_png );
    if( i_length > p_block->i_buffer ) {
        char err_str[64];
        snprintf( err_str, sizeof(err_str),
                  "block size %zu too small for %zu encoded bytes",
                  p_block->i_buffer, i_length );
        png_error( p_png, err_str );
        return;
    }

    memcpy( p_block->p_buffer, data, i_length );
    p_block->p_buffer += i_length;
    p_block->i_buffer -= i_length;
}

static void user_error( png_structp p_png, png_const_charp error_msg )
{
    png_sys_t *p_sys = (png_sys_t *)png_get_error_ptr( p_png );
    p_sys->b_error = true;
    msg_Err( p_sys->p_obj, "%s", error_msg );
}

static void user_warning( png_structp p_png, png_const_charp warning_msg )
{
    png_sys_t *p_sys = (png_sys_t *)png_get_error_ptr( p_png );
    msg_Warn( p_sys->p_obj, "%s", warning_msg );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with a complete compressed frame.
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = 0;

    png_uint_32 i_width, i_height;
    int i_color_type, i_interlace_type, i_compression_type, i_filter_type;
    int i_bit_depth, i;

    png_structp p_png;
    png_infop p_info, p_end_info;
    png_bytep *volatile p_row_pointers = NULL;

    if( !p_block ) /* No Drain */
        return VLCDEC_SUCCESS;

    p_sys->b_error = false;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    p_png = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    if( p_png == NULL )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    p_info = png_create_info_struct( p_png );
    if( p_info == NULL )
    {
        png_destroy_read_struct( &p_png, NULL, NULL );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    p_end_info = png_create_info_struct( p_png );
    if( p_end_info == NULL )
    {
        png_destroy_read_struct( &p_png, &p_info, NULL );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    /* libpng longjmp's there in case of error */
    if( setjmp( png_jmpbuf( p_png ) ) )
        goto error;

    png_set_read_fn( p_png, p_block, user_read );
    png_set_error_fn( p_png, p_sys, user_error, user_warning );

    png_read_info( p_png, p_info );
    if( p_sys->b_error ) goto error;

    png_get_IHDR( p_png, p_info, &i_width, &i_height,
                  &i_bit_depth, &i_color_type, &i_interlace_type,
                  &i_compression_type, &i_filter_type);
    if( p_sys->b_error ) goto error;

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_RGBA;
    p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width = i_width;
    p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height = i_height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;

    if( i_color_type == PNG_COLOR_TYPE_PALETTE )
        png_set_palette_to_rgb( p_png );

    if( i_color_type == PNG_COLOR_TYPE_GRAY ||
        i_color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
          png_set_gray_to_rgb( p_png );
    if( i_color_type & PNG_COLOR_MASK_ALPHA )
        png_set_alpha_mode( p_png, PNG_ALPHA_OPTIMIZED, PNG_DEFAULT_sRGB );

    /* Strip to 8 bits per channel */
    if( i_bit_depth == 16 )
    {
#if PNG_LIBPNG_VER >= 10504
        png_set_scale_16( p_png );
#else
        png_set_strip_16( p_png );
#endif
    }

    if( png_get_valid( p_png, p_info, PNG_INFO_tRNS ) )
    {
        png_set_tRNS_to_alpha( p_png );
    }
    else if( !(i_color_type & PNG_COLOR_MASK_ALPHA) )
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_RGB24;
    }

    /* Get a new picture */
    if( decoder_UpdateVideoFormat( p_dec ) )
        goto error;
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic ) goto error;

    /* Decode picture */
    p_row_pointers = vlc_alloc( i_height, sizeof(png_bytep) );
    if( !p_row_pointers )
        goto error;
    for( i = 0; i < (int)i_height; i++ )
        p_row_pointers[i] = p_pic->p->p_pixels + p_pic->p->i_pitch * i;

    png_read_image( p_png, p_row_pointers );
    if( p_sys->b_error ) goto error;
    png_read_end( p_png, p_end_info );
    if( p_sys->b_error ) goto error;

    png_destroy_read_struct( &p_png, &p_info, &p_end_info );
    free( p_row_pointers );

    p_pic->date = p_block->i_pts != VLC_TICK_INVALID ? p_block->i_pts : p_block->i_dts;

    block_Release( p_block );
    decoder_QueueVideo( p_dec, p_pic );
    return VLCDEC_SUCCESS;

 error:

    free( p_row_pointers );
    png_destroy_read_struct( &p_png, &p_info, &p_end_info );
    block_Release( p_block );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: png decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys );
}

static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *) p_this;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_PNG )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the encoder's structure */
    encoder_sys_t *p_sys = malloc( sizeof(encoder_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_sys->p_obj = p_this;

    p_sys->i_blocksize = 3 * p_enc->fmt_in.video.i_visible_width *
        p_enc->fmt_in.video.i_visible_height;

    p_enc->fmt_in.i_codec = VLC_CODEC_RGB24;
    p_enc->fmt_in.video.i_bmask = 0;
    video_format_FixRgb( &p_enc->fmt_in.video );

    p_enc->pf_encode_video = EncodeBlock;

    return VLC_SUCCESS;
}

/*
 * EncodeBlock
 */
static block_t *EncodeBlock(encoder_t *p_enc, picture_t *p_pic)
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    if( unlikely( !p_pic ) )
    {
        return NULL;
    }

    block_t *p_block = block_Alloc( p_sys->i_blocksize );
    if( p_block == NULL )
    {
        return NULL;
    }

    png_structp p_png = png_create_write_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    if( p_png == NULL )
    {
        block_Release( p_block );
        return NULL;
    }

    /* Disable filtering to speed-up encoding */
    png_set_filter( p_png, 0, PNG_NO_FILTERS );
    /* 1 == best speed */
    png_set_compression_level( p_png, 1 );

    /* save buffer start */
    uint8_t *p_start = p_block->p_buffer;
    size_t i_start = p_block->i_buffer;

    p_sys->b_error = false;
    png_infop p_info = NULL;

    /* libpng longjmp's there in case of error */
    if( setjmp( png_jmpbuf( p_png ) ) )
        goto error;

    png_set_write_fn( p_png, p_block, user_write, user_flush );
    png_set_error_fn( p_png, p_sys, user_error, user_warning );

    p_info = png_create_info_struct( p_png );
    if( p_info == NULL )
        goto error;

    png_set_IHDR( p_png, p_info,
            p_enc->fmt_in.video.i_visible_width,
            p_enc->fmt_in.video.i_visible_height,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
    if( p_sys->b_error ) goto error;

    png_write_info( p_png, p_info );
    if( p_sys->b_error ) goto error;

    /* Encode picture */

    for( int i = 0; i < p_pic->p->i_visible_lines; i++ )
    {
        png_write_row( p_png, p_pic->p->p_pixels + (i * p_pic->p->i_pitch) );
        if( p_sys->b_error ) goto error;
    }

    png_write_end( p_png, p_info );
    if( p_sys->b_error ) goto error;

    png_destroy_write_struct( &p_png, &p_info );

    /* restore original buffer position */
    p_block->p_buffer = p_start;
    p_block->i_buffer = i_start - p_block->i_buffer;

    p_block->i_dts = p_block->i_pts = p_pic->date;

    return p_block;

 error:

    png_destroy_write_struct( &p_png, p_info ? &p_info : NULL );

    block_Release(p_block);
    return NULL;
}

/*****************************************************************************
 * CloseEncoder: png encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    free( p_sys );
}
