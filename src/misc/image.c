/*****************************************************************************
 * image.c : wrapper for image reading/writing facilities
 *****************************************************************************
 * Copyright (C) 2004-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

/**
 * \file
 * This file contains the functions to handle the image_handler_t type
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_meta.h>
#include <vlc_filter.h>
#include <vlc_es.h>
#include <vlc_image.h>
#include <vlc_stream.h>
#include <vlc_fs.h>
#include <vlc_sout.h>
#include <libvlc.h>
#include <vlc_modules.h>

static picture_t *ImageRead( image_handler_t *, block_t *,
                             video_format_t *, video_format_t * );
static picture_t *ImageReadUrl( image_handler_t *, const char *,
                                video_format_t *, video_format_t * );
static block_t *ImageWrite( image_handler_t *, picture_t *,
                            video_format_t *, video_format_t * );
static int ImageWriteUrl( image_handler_t *, picture_t *,
                          video_format_t *, video_format_t *, const char * );

static picture_t *ImageConvert( image_handler_t *, picture_t *,
                                video_format_t *, video_format_t * );
static picture_t *ImageFilter( image_handler_t *, picture_t *,
                               video_format_t *, const char *psz_module );

static decoder_t *CreateDecoder( vlc_object_t *, video_format_t * );
static void DeleteDecoder( decoder_t * );
static encoder_t *CreateEncoder( vlc_object_t *, video_format_t *,
                                 video_format_t * );
static void DeleteEncoder( encoder_t * );
static filter_t *CreateFilter( vlc_object_t *, es_format_t *,
                               video_format_t *, const char * );
static void DeleteFilter( filter_t * );

vlc_fourcc_t image_Type2Fourcc( const char * );
vlc_fourcc_t image_Ext2Fourcc( const char * );
/*static const char *Fourcc2Ext( vlc_fourcc_t );*/

#undef image_HandlerCreate
/**
 * Create an image_handler_t instance
 *
 */
image_handler_t *image_HandlerCreate( vlc_object_t *p_this )
{
    image_handler_t *p_image = calloc( 1, sizeof(image_handler_t) );
    if( !p_image )
        return NULL;

    p_image->p_parent = p_this;

    p_image->pf_read = ImageRead;
    p_image->pf_read_url = ImageReadUrl;
    p_image->pf_write = ImageWrite;
    p_image->pf_write_url = ImageWriteUrl;
    p_image->pf_convert = ImageConvert;
    p_image->pf_filter = ImageFilter;

    return p_image;
}

/**
 * Delete the image_handler_t instance
 *
 */
void image_HandlerDelete( image_handler_t *p_image )
{
    if( !p_image ) return;

    if( p_image->p_dec ) DeleteDecoder( p_image->p_dec );
    if( p_image->p_enc ) DeleteEncoder( p_image->p_enc );
    if( p_image->p_filter ) DeleteFilter( p_image->p_filter );

    free( p_image );
    p_image = NULL;
}

/**
 * Read an image
 *
 */

static picture_t *ImageRead( image_handler_t *p_image, block_t *p_block,
                             video_format_t *p_fmt_in,
                             video_format_t *p_fmt_out )
{
    picture_t *p_pic = NULL, *p_tmp;

    /* Check if we can reuse the current decoder */
    if( p_image->p_dec &&
        p_image->p_dec->fmt_in.i_codec != p_fmt_in->i_chroma )
    {
        DeleteDecoder( p_image->p_dec );
        p_image->p_dec = 0;
    }

    /* Start a decoder */
    if( !p_image->p_dec )
    {
        p_image->p_dec = CreateDecoder( p_image->p_parent, p_fmt_in );
        if( !p_image->p_dec ) return NULL;
    }

    p_block->i_pts = p_block->i_dts = mdate();
    while( (p_tmp = p_image->p_dec->pf_decode_video( p_image->p_dec, &p_block ))
             != NULL )
    {
        if( p_pic != NULL )
            picture_Release( p_pic );
        p_pic = p_tmp;
    }

    if( p_pic == NULL )
    {
        msg_Warn( p_image->p_parent, "no image decoded" );
        return 0;
    }

    if( !p_fmt_out->i_chroma )
        p_fmt_out->i_chroma = p_image->p_dec->fmt_out.video.i_chroma;
    if( !p_fmt_out->i_width && p_fmt_out->i_height )
        p_fmt_out->i_width = (int64_t)p_image->p_dec->fmt_out.video.i_width *
                             p_image->p_dec->fmt_out.video.i_sar_num *
                             p_fmt_out->i_height /
                             p_image->p_dec->fmt_out.video.i_height /
                             p_image->p_dec->fmt_out.video.i_sar_den;

    if( !p_fmt_out->i_height && p_fmt_out->i_width )
        p_fmt_out->i_height = (int64_t)p_image->p_dec->fmt_out.video.i_height *
                              p_image->p_dec->fmt_out.video.i_sar_den *
                              p_fmt_out->i_width /
                              p_image->p_dec->fmt_out.video.i_width /
                              p_image->p_dec->fmt_out.video.i_sar_num;
    if( !p_fmt_out->i_width )
        p_fmt_out->i_width = p_image->p_dec->fmt_out.video.i_width;
    if( !p_fmt_out->i_height )
        p_fmt_out->i_height = p_image->p_dec->fmt_out.video.i_height;

    /* Check if we need chroma conversion or resizing */
    if( p_image->p_dec->fmt_out.video.i_chroma != p_fmt_out->i_chroma ||
        p_image->p_dec->fmt_out.video.i_width != p_fmt_out->i_width ||
        p_image->p_dec->fmt_out.video.i_height != p_fmt_out->i_height )
    {
        if( p_image->p_filter )
        if( p_image->p_filter->fmt_in.video.i_chroma !=
            p_image->p_dec->fmt_out.video.i_chroma ||
            p_image->p_filter->fmt_out.video.i_chroma != p_fmt_out->i_chroma )
        {
            /* We need to restart a new filter */
            DeleteFilter( p_image->p_filter );
            p_image->p_filter = 0;
        }

        /* Start a filter */
        if( !p_image->p_filter )
        {
            p_image->p_filter =
                CreateFilter( p_image->p_parent, &p_image->p_dec->fmt_out,
                              p_fmt_out, NULL );

            if( !p_image->p_filter )
            {
                picture_Release( p_pic );
                return NULL;
            }
        }
        else
        {
            /* Filters should handle on-the-fly size changes */
            p_image->p_filter->fmt_in = p_image->p_dec->fmt_out;
            p_image->p_filter->fmt_out = p_image->p_dec->fmt_out;
            p_image->p_filter->fmt_out.i_codec = p_fmt_out->i_chroma;
            p_image->p_filter->fmt_out.video = *p_fmt_out;
        }

        p_pic = p_image->p_filter->pf_video_filter( p_image->p_filter, p_pic );
        *p_fmt_out = p_image->p_filter->fmt_out.video;
    }
    else *p_fmt_out = p_image->p_dec->fmt_out.video;

    return p_pic;
}

static picture_t *ImageReadUrl( image_handler_t *p_image, const char *psz_url,
                                video_format_t *p_fmt_in,
                                video_format_t *p_fmt_out )
{
    block_t *p_block;
    picture_t *p_pic;
    stream_t *p_stream = NULL;
    int i_size;

    p_stream = stream_UrlNew( p_image->p_parent, psz_url );

    if( !p_stream )
    {
        msg_Dbg( p_image->p_parent, "could not open %s for reading",
                 psz_url );
        return NULL;
    }

    i_size = stream_Size( p_stream );

    p_block = block_Alloc( i_size );

    stream_Read( p_stream, p_block->p_buffer, i_size );

    if( !p_fmt_in->i_chroma )
    {
        char *psz_mime = NULL;
        stream_Control( p_stream, STREAM_GET_CONTENT_TYPE, &psz_mime );
        if( psz_mime )
            p_fmt_in->i_chroma = image_Mime2Fourcc( psz_mime );
        free( psz_mime );
    }
    stream_Delete( p_stream );

    if( !p_fmt_in->i_chroma )
    {
        /* Try to guess format from file name */
        p_fmt_in->i_chroma = image_Ext2Fourcc( psz_url );
    }

    p_pic = ImageRead( p_image, p_block, p_fmt_in, p_fmt_out );

    return p_pic;
}

/**
 * Write an image
 *
 */

static block_t *ImageWrite( image_handler_t *p_image, picture_t *p_pic,
                            video_format_t *p_fmt_in,
                            video_format_t *p_fmt_out )
{
    block_t *p_block;

    /* Check if we can reuse the current encoder */
    if( p_image->p_enc &&
        ( p_image->p_enc->fmt_out.i_codec != p_fmt_out->i_chroma ||
          p_image->p_enc->fmt_out.video.i_width != p_fmt_out->i_width ||
          p_image->p_enc->fmt_out.video.i_height != p_fmt_out->i_height ) )
    {
        DeleteEncoder( p_image->p_enc );
        p_image->p_enc = 0;
    }

    /* Start an encoder */
    if( !p_image->p_enc )
    {
        p_image->p_enc = CreateEncoder( p_image->p_parent,
                                        p_fmt_in, p_fmt_out );
        if( !p_image->p_enc ) return NULL;
    }

    /* Check if we need chroma conversion or resizing */
    if( p_image->p_enc->fmt_in.video.i_chroma != p_fmt_in->i_chroma ||
        p_image->p_enc->fmt_in.video.i_width != p_fmt_in->i_width ||
        p_image->p_enc->fmt_in.video.i_height != p_fmt_in->i_height )
    {
        picture_t *p_tmp_pic;

        if( p_image->p_filter )
        if( p_image->p_filter->fmt_in.video.i_chroma != p_fmt_in->i_chroma ||
            p_image->p_filter->fmt_out.video.i_chroma !=
            p_image->p_enc->fmt_in.video.i_chroma )
        {
            /* We need to restart a new filter */
            DeleteFilter( p_image->p_filter );
            p_image->p_filter = 0;
        }

        /* Start a filter */
        if( !p_image->p_filter )
        {
            es_format_t fmt_in;
            es_format_Init( &fmt_in, VIDEO_ES, p_fmt_in->i_chroma );
            fmt_in.video = *p_fmt_in;

            p_image->p_filter =
                CreateFilter( p_image->p_parent, &fmt_in,
                              &p_image->p_enc->fmt_in.video, NULL );

            if( !p_image->p_filter )
            {
                return NULL;
            }
        }
        else
        {
            /* Filters should handle on-the-fly size changes */
            p_image->p_filter->fmt_in.i_codec = p_fmt_in->i_chroma;
            p_image->p_filter->fmt_out.video = *p_fmt_in;
            p_image->p_filter->fmt_out.i_codec =p_image->p_enc->fmt_in.i_codec;
            p_image->p_filter->fmt_out.video = p_image->p_enc->fmt_in.video;
        }

        picture_Hold( p_pic );

        p_tmp_pic =
            p_image->p_filter->pf_video_filter( p_image->p_filter, p_pic );

        if( likely(p_tmp_pic != NULL) )
        {
            p_block = p_image->p_enc->pf_encode_video( p_image->p_enc,
                                                       p_tmp_pic );
            p_image->p_filter->pf_video_buffer_del( p_image->p_filter,
                                                    p_tmp_pic );
        }
        else
            p_block = NULL;
    }
    else
    {
        p_block = p_image->p_enc->pf_encode_video( p_image->p_enc, p_pic );
    }

    if( !p_block )
    {
        msg_Dbg( p_image->p_parent, "no image encoded" );
        return 0;
    }

    return p_block;
}

static int ImageWriteUrl( image_handler_t *p_image, picture_t *p_pic,
                          video_format_t *p_fmt_in, video_format_t *p_fmt_out,
                          const char *psz_url )
{
    block_t *p_block;
    FILE *file;

    if( !p_fmt_out->i_chroma )
    {
        /* Try to guess format from file name */
        p_fmt_out->i_chroma = image_Ext2Fourcc( psz_url );
    }

    file = vlc_fopen( psz_url, "wb" );
    if( !file )
    {
        msg_Err( p_image->p_parent, "%s: %m", psz_url );
        return VLC_EGENERIC;
    }

    p_block = ImageWrite( p_image, p_pic, p_fmt_in, p_fmt_out );

    int err = 0;
    if( p_block )
    {
        if( fwrite( p_block->p_buffer, p_block->i_buffer, 1, file ) != 1 )
            err = errno;
        block_Release( p_block );
    }

    if( fclose( file ) && !err )
        err = errno;

    if( err )
    {
       errno = err;
       msg_Err( p_image->p_parent, "%s: %m", psz_url );
    }

    return err ? VLC_EGENERIC : VLC_SUCCESS;
}

/**
 * Convert an image to a different format
 *
 */

static picture_t *ImageConvert( image_handler_t *p_image, picture_t *p_pic,
                                video_format_t *p_fmt_in,
                                video_format_t *p_fmt_out )
{
    picture_t *p_pif;

    if( !p_fmt_out->i_width && !p_fmt_out->i_height &&
        p_fmt_out->i_sar_num && p_fmt_out->i_sar_den &&
        p_fmt_out->i_sar_num * p_fmt_in->i_sar_den !=
        p_fmt_out->i_sar_den * p_fmt_in->i_sar_num )
    {
        p_fmt_out->i_width =
            p_fmt_in->i_sar_num * (int64_t)p_fmt_out->i_sar_den *
            p_fmt_in->i_width / p_fmt_in->i_sar_den / p_fmt_out->i_sar_num;
        p_fmt_out->i_visible_width =
            p_fmt_in->i_sar_num * (int64_t)p_fmt_out->i_sar_den *
            p_fmt_in->i_visible_width / p_fmt_in->i_sar_den /
            p_fmt_out->i_sar_num;
    }

    if( !p_fmt_out->i_chroma ) p_fmt_out->i_chroma = p_fmt_in->i_chroma;
    if( !p_fmt_out->i_width )
        p_fmt_out->i_width = p_fmt_out->i_visible_width = p_fmt_in->i_width;
    if( !p_fmt_out->i_height )
        p_fmt_out->i_height = p_fmt_out->i_visible_height = p_fmt_in->i_height;
    if( !p_fmt_out->i_sar_num ) p_fmt_out->i_sar_num = p_fmt_in->i_sar_num;
    if( !p_fmt_out->i_sar_den ) p_fmt_out->i_sar_den = p_fmt_in->i_sar_den;

    if( p_image->p_filter )
    if( p_image->p_filter->fmt_in.video.i_chroma != p_fmt_in->i_chroma ||
        p_image->p_filter->fmt_out.video.i_chroma != p_fmt_out->i_chroma )
    {
        /* We need to restart a new filter */
        DeleteFilter( p_image->p_filter );
        p_image->p_filter = NULL;
    }

    /* Start a filter */
    if( !p_image->p_filter )
    {
        es_format_t fmt_in;
        es_format_Init( &fmt_in, VIDEO_ES, p_fmt_in->i_chroma );
        fmt_in.video = *p_fmt_in;

        p_image->p_filter =
            CreateFilter( p_image->p_parent, &fmt_in, p_fmt_out, NULL );

        if( !p_image->p_filter )
        {
            return NULL;
        }
    }
    else
    {
        /* Filters should handle on-the-fly size changes */
        p_image->p_filter->fmt_in.video = *p_fmt_in;
        p_image->p_filter->fmt_out.video = *p_fmt_out;
    }

    picture_Hold( p_pic );

    p_pif = p_image->p_filter->pf_video_filter( p_image->p_filter, p_pic );

    if( p_fmt_in->i_chroma == p_fmt_out->i_chroma &&
        p_fmt_in->i_width == p_fmt_out->i_width &&
        p_fmt_in->i_height == p_fmt_out->i_height )
    {
        /* Duplicate image */
        picture_Release( p_pif ); /* XXX: Better fix must be possible */
        p_pif = p_image->p_filter->pf_video_buffer_new( p_image->p_filter );
        if( p_pif )
            picture_Copy( p_pif, p_pic );
    }

    return p_pif;
}

/**
 * Filter an image with a psz_module filter
 *
 */

static picture_t *ImageFilter( image_handler_t *p_image, picture_t *p_pic,
                               video_format_t *p_fmt, const char *psz_module )
{
    /* Start a filter */
    if( !p_image->p_filter )
    {
        es_format_t fmt;
        es_format_Init( &fmt, VIDEO_ES, p_fmt->i_chroma );
        fmt.video = *p_fmt;

        p_image->p_filter =
            CreateFilter( p_image->p_parent, &fmt, &fmt.video, psz_module );

        if( !p_image->p_filter )
        {
            return NULL;
        }
    }
    else
    {
        /* Filters should handle on-the-fly size changes */
        p_image->p_filter->fmt_in.video = *p_fmt;
        p_image->p_filter->fmt_out.video = *p_fmt;
    }

    picture_Hold( p_pic );

    return p_image->p_filter->pf_video_filter( p_image->p_filter, p_pic );
}

/**
 * Misc functions
 *
 */
static const struct
{
    vlc_fourcc_t i_codec;
    const char psz_ext[7];

} ext_table[] =
{
    { VLC_CODEC_JPEG,              "jpeg" },
    { VLC_CODEC_JPEG,              "jpg"  },
    { VLC_CODEC_JPEGLS,            "ljpg" },
    { VLC_CODEC_PNG,               "png" },
    { VLC_CODEC_PGM,               "pgm" },
    { VLC_CODEC_PGMYUV,            "pgmyuv" },
    { VLC_FOURCC('p','b','m',' '), "pbm" },
    { VLC_FOURCC('p','a','m',' '), "pam" },
    { VLC_CODEC_TARGA,             "tga" },
    { VLC_CODEC_BMP,               "bmp" },
    { VLC_CODEC_PNM,               "pnm" },
    { VLC_FOURCC('x','p','m',' '), "xpm" },
    { VLC_FOURCC('x','c','f',' '), "xcf" },
    { VLC_CODEC_PCX,               "pcx" },
    { VLC_CODEC_GIF,               "gif" },
    { VLC_CODEC_TIFF,              "tif" },
    { VLC_CODEC_TIFF,              "tiff" },
    { VLC_FOURCC('l','b','m',' '), "lbm" },
    { VLC_CODEC_PPM,               "ppm" },
};

vlc_fourcc_t image_Type2Fourcc( const char *psz_type )
{
    for( unsigned i = 0; i < ARRAY_SIZE(ext_table); i++ )
        if( !strcasecmp( ext_table[i].psz_ext, psz_type ) )
            return ext_table[i].i_codec;

    return 0;
}

vlc_fourcc_t image_Ext2Fourcc( const char *psz_name )
{
    psz_name = strrchr( psz_name, '.' );
    if( !psz_name ) return 0;
    psz_name++;

    return image_Type2Fourcc( psz_name );
}

/*
static const char *Fourcc2Ext( vlc_fourcc_t i_codec )
{
    for( unsigned i = 0; i < ARRAY_SIZE(ext_table); i++ )
        if( ext_table[i].i_codec == i_codec )
            return ext_table[i].psz_ext;

    return NULL;
}
*/

static const struct
{
    vlc_fourcc_t i_codec;
    const char *psz_mime;
} mime_table[] =
{
    { VLC_CODEC_BMP,               "image/bmp" },
    { VLC_CODEC_BMP,               "image/x-bmp" },
    { VLC_CODEC_BMP,               "image/x-bitmap" },
    { VLC_CODEC_BMP,               "image/x-ms-bmp" },
    { VLC_CODEC_PNM,               "image/x-portable-anymap" },
    { VLC_CODEC_PNM,               "image/x-portable-bitmap" },
    { VLC_CODEC_PNM,               "image/x-portable-graymap" },
    { VLC_CODEC_PNM,               "image/x-portable-pixmap" },
    { VLC_CODEC_GIF,               "image/gif" },
    { VLC_CODEC_JPEG,              "image/jpeg" },
    { VLC_CODEC_PCX,               "image/pcx" },
    { VLC_CODEC_PNG,               "image/png" },
    { VLC_CODEC_TIFF,              "image/tiff" },
    { VLC_CODEC_TARGA,             "image/x-tga" },
    { VLC_FOURCC('x','p','m',' '), "image/x-xpixmap" },
    { 0, NULL }
};

vlc_fourcc_t image_Mime2Fourcc( const char *psz_mime )
{
    int i;
    for( i = 0; mime_table[i].i_codec; i++ )
        if( !strcmp( psz_mime, mime_table[i].psz_mime ) )
            return mime_table[i].i_codec;
    return 0;
}


static picture_t *video_new_buffer( decoder_t *p_dec )
{
    p_dec->fmt_out.video.i_chroma = p_dec->fmt_out.i_codec;
    return picture_NewFromFormat( &p_dec->fmt_out.video );
}

static void video_del_buffer( decoder_t *p_dec, picture_t *p_pic )
{
    (void)p_dec;
    picture_Release( p_pic );
}

static void video_link_picture( decoder_t *p_dec, picture_t *p_pic )
{
    (void)p_dec;
    picture_Hold( p_pic );
}

static void video_unlink_picture( decoder_t *p_dec, picture_t *p_pic )
{
    (void)p_dec;
    picture_Release( p_pic );
}

static decoder_t *CreateDecoder( vlc_object_t *p_this, video_format_t *fmt )
{
    decoder_t *p_dec;

    p_dec = vlc_custom_create( p_this, sizeof( *p_dec ), "image decoder" );
    if( p_dec == NULL )
        return NULL;

    p_dec->p_module = NULL;
    es_format_Init( &p_dec->fmt_in, VIDEO_ES, fmt->i_chroma );
    es_format_Init( &p_dec->fmt_out, VIDEO_ES, 0 );
    p_dec->fmt_in.video = *fmt;
    p_dec->b_pace_control = true;

    p_dec->pf_vout_buffer_new = video_new_buffer;
    p_dec->pf_vout_buffer_del = video_del_buffer;
    p_dec->pf_picture_link    = video_link_picture;
    p_dec->pf_picture_unlink  = video_unlink_picture;

    /* Find a suitable decoder module */
    p_dec->p_module = module_need( p_dec, "decoder", "$codec", false );
    if( !p_dec->p_module )
    {
        msg_Err( p_dec, "no suitable decoder module for fourcc `%4.4s'. "
                 "VLC probably does not support this image format.",
                 (char*)&p_dec->fmt_in.i_codec );

        DeleteDecoder( p_dec );
        return NULL;
    }

    return p_dec;
}

static void DeleteDecoder( decoder_t * p_dec )
{
    if( p_dec->p_module ) module_unneed( p_dec, p_dec->p_module );

    es_format_Clean( &p_dec->fmt_in );
    es_format_Clean( &p_dec->fmt_out );

    if( p_dec->p_description )
        vlc_meta_Delete( p_dec->p_description );

    vlc_object_release( p_dec );
    p_dec = NULL;
}

static encoder_t *CreateEncoder( vlc_object_t *p_this, video_format_t *fmt_in,
                                 video_format_t *fmt_out )
{
    encoder_t *p_enc;

    p_enc = sout_EncoderCreate( p_this );
    if( p_enc == NULL )
        return NULL;

    p_enc->p_module = NULL;
    es_format_Init( &p_enc->fmt_in, VIDEO_ES, fmt_in->i_chroma );
    p_enc->fmt_in.video = *fmt_in;
    if( fmt_out->i_width > 0 && fmt_out->i_height > 0 )
    {
        p_enc->fmt_in.video.i_width = fmt_out->i_width;
        p_enc->fmt_in.video.i_height = fmt_out->i_height;

        if( fmt_out->i_visible_width > 0 &&
            fmt_out->i_visible_height > 0 )
        {
            p_enc->fmt_in.video.i_visible_width = fmt_out->i_visible_width;
            p_enc->fmt_in.video.i_visible_height = fmt_out->i_visible_height;
        }
        else
        {
            p_enc->fmt_in.video.i_visible_width = fmt_out->i_width;
            p_enc->fmt_in.video.i_visible_height = fmt_out->i_height;
        }
    }
    else if( fmt_out->i_sar_num && fmt_out->i_sar_den &&
             fmt_out->i_sar_num * fmt_in->i_sar_den !=
             fmt_out->i_sar_den * fmt_in->i_sar_num )
    {
        p_enc->fmt_in.video.i_width =
            fmt_in->i_sar_num * (int64_t)fmt_out->i_sar_den * fmt_in->i_width /
            fmt_in->i_sar_den / fmt_out->i_sar_num;
        p_enc->fmt_in.video.i_visible_width =
            fmt_in->i_sar_num * (int64_t)fmt_out->i_sar_den *
            fmt_in->i_visible_width / fmt_in->i_sar_den / fmt_out->i_sar_num;
    }

    p_enc->fmt_in.video.i_frame_rate = 25;
    p_enc->fmt_in.video.i_frame_rate_base = 1;

    es_format_Init( &p_enc->fmt_out, VIDEO_ES, fmt_out->i_chroma );
    p_enc->fmt_out.video = *fmt_out;
    p_enc->fmt_out.video.i_width = p_enc->fmt_in.video.i_width;
    p_enc->fmt_out.video.i_height = p_enc->fmt_in.video.i_height;

    /* Find a suitable decoder module */
    p_enc->p_module = module_need( p_enc, "encoder", NULL, false );
    if( !p_enc->p_module )
    {
        msg_Err( p_enc, "no suitable encoder module for fourcc `%4.4s'.\n"
                 "VLC probably does not support this image format.",
                 (char*)&p_enc->fmt_out.i_codec );

        DeleteEncoder( p_enc );
        return NULL;
    }
    p_enc->fmt_in.video.i_chroma = p_enc->fmt_in.i_codec;

    return p_enc;
}

static void DeleteEncoder( encoder_t * p_enc )
{
    if( p_enc->p_module ) module_unneed( p_enc, p_enc->p_module );

    es_format_Clean( &p_enc->fmt_in );
    es_format_Clean( &p_enc->fmt_out );

    vlc_object_release( p_enc );
    p_enc = NULL;
}

static filter_t *CreateFilter( vlc_object_t *p_this, es_format_t *p_fmt_in,
                               video_format_t *p_fmt_out,
                               const char *psz_module )
{
    filter_t *p_filter;

    p_filter = vlc_custom_create( p_this, sizeof(filter_t), "filter" );
    p_filter->pf_video_buffer_new =
        (picture_t *(*)(filter_t *))video_new_buffer;
    p_filter->pf_video_buffer_del =
        (void (*)(filter_t *, picture_t *))video_del_buffer;

    p_filter->fmt_in = *p_fmt_in;
    p_filter->fmt_out = *p_fmt_in;
    p_filter->fmt_out.i_codec = p_fmt_out->i_chroma;
    p_filter->fmt_out.video = *p_fmt_out;
    p_filter->p_module = module_need( p_filter, "video filter2",
                                      psz_module, false );

    if( !p_filter->p_module )
    {
        msg_Dbg( p_filter, "no video filter found" );
        DeleteFilter( p_filter );
        return NULL;
    }

    return p_filter;
}

static void DeleteFilter( filter_t * p_filter )
{
    if( p_filter->p_module ) module_unneed( p_filter, p_filter->p_module );

    es_format_Clean( &p_filter->fmt_in );
    es_format_Clean( &p_filter->fmt_out );

    vlc_object_release( p_filter );
}
