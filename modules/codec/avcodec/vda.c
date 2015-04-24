/*****************************************************************************
 * vda.c: VDA helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright (C) 2012-2014 VLC authors VideoLAN
 *
 * Authors: Sebastien Zwickert <dilaroga@free.fr>
 *          Rémi Denis-Courmont <remi # remlab : net>
 *          Felix Paul Kühne <fkuehne # videolan org>
 *          David Fuhrmann <david.fuhrmann # googlemail com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_plugin.h>

#include <libavcodec/avcodec.h>

#include "avcodec.h"
#include "va.h"
#include "../../video_chroma/copy.h"

#include <libavcodec/vda.h>
#include <VideoDecodeAcceleration/VDADecoder.h>

#pragma mark prototypes and definitions

static int Open( vlc_va_t *, AVCodecContext *, enum PixelFormat,
                 const es_format_t * );
static void Close( vlc_va_t * , AVCodecContext *);
static int Setup( vlc_va_t *, AVCodecContext *, vlc_fourcc_t *);
static int Get( vlc_va_t *, picture_t *, uint8_t ** );
static int Extract( vlc_va_t *, picture_t *, uint8_t * );

static void vda_Copy422YpCbCr8( picture_t *p_pic,
                                CVPixelBufferRef buffer )
{
    int i_dst_stride, i_src_stride;
    uint8_t *p_dst, *p_src;

    CVPixelBufferLockBaseAddress( buffer, 0 );

    for( int i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        p_src = CVPixelBufferGetBaseAddressOfPlane( buffer, i_plane );
        i_dst_stride  = p_pic->p[i_plane].i_pitch;
        i_src_stride  = CVPixelBufferGetBytesPerRowOfPlane( buffer, i_plane );

        for( int i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines ; i_line++ )
        {
            memcpy( p_dst, p_src, i_src_stride );

            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }

    CVPixelBufferUnlockBaseAddress( buffer, 0 );
}

#ifndef HAVE_AV_VDA_ALLOC_CONTEXT

static const int  nvda_pix_fmt_list[] = { 0, 1 };
static const char *const nvda_pix_fmt_list_text[] =
  { N_("420YpCbCr8Planar"), N_("422YpCbCr8") };

vlc_module_begin ()
    set_description( N_("Video Decode Acceleration Framework (VDA)") )
    set_capability( "hw decoder", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Open, Close )
    add_integer ( "avcodec-vda-pix-fmt", 0, VDA_PIX_FMT_TEXT,
                  VDA_PIX_FMT_LONGTEXT, false)
        change_integer_list( nvda_pix_fmt_list, nvda_pix_fmt_list_text )
vlc_module_end ()

struct vlc_va_sys_t
{
    struct vda_context  hw_ctx;

    uint8_t             *p_extradata;
    int                 i_extradata;

    vlc_fourcc_t        i_chroma;

    copy_cache_t        image_cache;
};

typedef struct vlc_va_sys_t vlc_va_vda_t;

static vlc_va_vda_t *vlc_va_vda_Get( vlc_va_t *va )
{
    return va->sys;
}

#pragma mark - module handling

static int Open( vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                 const es_format_t *fmt )
{
    if( pix_fmt != AV_PIX_FMT_VDA_VLD )
        return VLC_EGENERIC;

    msg_Dbg( va, "opening VDA module" );
    if( ctx->codec_id != AV_CODEC_ID_H264 )
    {
        msg_Warn( va, "input codec isn't H264, canceling VDA decoding" );
        return VLC_EGENERIC;
    }

    if( fmt->p_extra == NULL || fmt->i_extra < 7 )
    {
        msg_Warn( va, "VDA requires extradata." );
        return VLC_EGENERIC;
    }

    vlc_va_vda_t *p_vda = calloc( 1, sizeof(*p_vda) );
    if( unlikely(p_vda == NULL) )
        return VLC_EGENERIC;

    p_vda->p_extradata = fmt->p_extra;
    p_vda->i_extradata = fmt->i_extra;

    va->sys = p_vda;
    va->description = "VDA";
    va->setup = Setup;
    va->get = Get;
    va->release = NULL;
    va->extract = Extract;

    return VLC_SUCCESS;
}

static void Close( vlc_va_t *va, AVCodecContext *ctx )
{
    vlc_va_vda_t *p_vda = vlc_va_vda_Get( va );

    msg_Dbg(va, "destroying VDA decoder");

    ff_vda_destroy_decoder( &p_vda->hw_ctx ) ;

    if( p_vda->hw_ctx.cv_pix_fmt_type == kCVPixelFormatType_420YpCbCr8Planar )
        CopyCleanCache( &p_vda->image_cache );

    free( p_vda );
    (void) ctx;
}

static int Setup( vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *pi_chroma )
{
    vlc_va_vda_t *p_vda = vlc_va_vda_Get( va );

    if( p_vda->hw_ctx.width == avctx->coded_width
       && p_vda->hw_ctx.height == avctx->coded_height
       && p_vda->hw_ctx.decoder )
    {
        avctx->hwaccel_context = &p_vda->hw_ctx;
        *pi_chroma = p_vda->i_chroma;
        return VLC_SUCCESS;
    }

    if( p_vda->hw_ctx.decoder )
    {
        ff_vda_destroy_decoder( &p_vda->hw_ctx );
        goto ok;
    }

    memset( &p_vda->hw_ctx, 0, sizeof(p_vda->hw_ctx) );
    p_vda->hw_ctx.format = 'avc1';
    p_vda->hw_ctx.use_ref_buffer = 1;

    int i_pix_fmt = var_CreateGetInteger( va, "avcodec-vda-pix-fmt" );

    switch( i_pix_fmt )
    {
        case 1 :
            p_vda->hw_ctx.cv_pix_fmt_type = kCVPixelFormatType_422YpCbCr8;
            p_vda->i_chroma = VLC_CODEC_UYVY;
            msg_Dbg(va, "using pixel format 422YpCbCr8");
            break;
        case 0 :
        default :
            p_vda->hw_ctx.cv_pix_fmt_type = kCVPixelFormatType_420YpCbCr8Planar;
            p_vda->i_chroma = VLC_CODEC_I420;
            CopyInitCache( &p_vda->image_cache, avctx->coded_width );
            msg_Dbg(va, "using pixel format 420YpCbCr8Planar");
    }

ok:
    /* Setup the libavcodec hardware context */
    avctx->hwaccel_context = &p_vda->hw_ctx;
    *pi_chroma = p_vda->i_chroma;

    p_vda->hw_ctx.width = avctx->coded_width;
    p_vda->hw_ctx.height = avctx->coded_height;

    /* create the decoder */
    int status = ff_vda_create_decoder( &p_vda->hw_ctx,
                                       p_vda->p_extradata,
                                       p_vda->i_extradata );
    if( status )
    {
        msg_Err( va, "Failed to create decoder: %i", status );
        return VLC_EGENERIC;
    }
    else
        msg_Dbg( va, "VDA decoder created");

    return VLC_SUCCESS;
}

#pragma mark - actual data handling

static void vda_Copy420YpCbCr8Planar( picture_t *p_pic,
                                      CVPixelBufferRef buffer,
                                      unsigned i_width,
                                      unsigned i_height,
                                      copy_cache_t *cache )
{
    uint8_t *pp_plane[3];
    size_t  pi_pitch[3];

    if (!buffer)
        return;

    CVPixelBufferLockBaseAddress( buffer, 0 );

    for( int i = 0; i < 3; i++ )
    {
        pp_plane[i] = CVPixelBufferGetBaseAddressOfPlane( buffer, i );
        pi_pitch[i] = CVPixelBufferGetBytesPerRowOfPlane( buffer, i );
    }

    CopyFromYv12( p_pic, pp_plane, pi_pitch,
                  i_width, i_height, cache );

    CVPixelBufferUnlockBaseAddress( buffer, 0 );
}

static int Get( vlc_va_t *va, picture_t *pic, uint8_t **data )
{
    VLC_UNUSED( va );

    (void) pic;
    *data = (uint8_t *)1; // dummy
    return VLC_SUCCESS;
}

static int Extract( vlc_va_t *va, picture_t *p_picture, uint8_t *data )
{
    vlc_va_vda_t *p_vda = vlc_va_vda_Get( va );
    CVPixelBufferRef cv_buffer = ( CVPixelBufferRef )data;

    if( !cv_buffer )
    {
        msg_Dbg( va, "Frame buffer is empty.");
        return VLC_EGENERIC;
    }
    if (!CVPixelBufferGetDataSize(cv_buffer) > 0)
    {
        msg_Dbg( va, "Empty frame buffer");
        return VLC_EGENERIC;
    }

    if( p_vda->hw_ctx.cv_pix_fmt_type == kCVPixelFormatType_420YpCbCr8Planar )
    {
        if( !p_vda->image_cache.buffer ) {
            CVPixelBufferRelease( cv_buffer );
            return VLC_EGENERIC;
        }

        vda_Copy420YpCbCr8Planar( p_picture,
                                  cv_buffer,
                                  p_vda->hw_ctx.width,
                                  p_vda->hw_ctx.height,
                                  &p_vda->image_cache );
    }
    else
        vda_Copy422YpCbCr8( p_picture, cv_buffer );
    return VLC_SUCCESS;
}

#else

vlc_module_begin ()
    set_description( N_("Video Decode Acceleration Framework (VDA)") )
    set_capability( "hw decoder", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Open, Close )
vlc_module_end ()

static int Open( vlc_va_t *va, AVCodecContext *avctx,
                 enum PixelFormat pix_fmt, const es_format_t *fmt )
{
    if( pix_fmt != AV_PIX_FMT_VDA )
        return VLC_EGENERIC;

    msg_Dbg( va, "VDA decoder Open");

    va->description = (char *)"VDA";
    va->setup = Setup;
    va->get = Get;
    va->release = NULL;
    va->extract = Extract;
    msg_Dbg( va, "VDA decoder Open success!");

    (void) fmt;
    (void) avctx;

    return VLC_SUCCESS;
}

static void Close( vlc_va_t *va, AVCodecContext *avctx )
{
    av_vda_default_free(avctx);
    (void) va;
}

static int Setup( vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *p_chroma )
{
    av_vda_default_free(avctx);

    (void) va;
    *p_chroma = VLC_CODEC_UYVY;

    return (av_vda_default_init(avctx) < 0) ? VLC_EGENERIC : VLC_SUCCESS;
}

// Never called
static int Get( vlc_va_t *va, picture_t *p_picture, uint8_t **data )
{
    VLC_UNUSED( va );
    (void) p_picture;
    (void) data;
    return VLC_SUCCESS;
}

static int Extract( vlc_va_t *va, picture_t *p_picture, uint8_t *data )
{
    CVPixelBufferRef cv_buffer = (CVPixelBufferRef)data;

    if( !cv_buffer )
    {
        msg_Dbg( va, "Frame buffer is empty.");
        return VLC_EGENERIC;
    }
    if (!CVPixelBufferGetDataSize(cv_buffer) > 0)
    {
        msg_Dbg( va, "Empty frame buffer");
        return VLC_EGENERIC;
    }

    vda_Copy422YpCbCr8( p_picture, cv_buffer );

    return VLC_SUCCESS;
}
#endif
