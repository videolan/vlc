/*****************************************************************************
 * vaapi.c: VAAPI helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright (C) 2009-2010 Laurent Aimar
 * Copyright (C) 2012-2014 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>

#ifdef VLC_VA_BACKEND_XLIB
# include <vlc_xlib.h>
# include <va/va_x11.h>
#endif
#ifdef VLC_VA_BACKEND_DRM
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <vlc_fs.h>
# include <va/va_drm.h>
#endif
#include <libavcodec/avcodec.h>
#include <libavcodec/vaapi.h>

#include "avcodec.h"
#include "va.h"
#include "../../video_chroma/copy.h"

#ifndef VA_SURFACE_ATTRIB_SETTABLE
#define vaCreateSurfaces(d, f, w, h, s, ns, a, na) \
    vaCreateSurfaces(d, w, h, f, ns, s)
#endif

struct vlc_va_sys_t
{
#ifdef VLC_VA_BACKEND_XLIB
        Display  *p_display_x11;
#endif
#ifdef VLC_VA_BACKEND_DRM
        int       drm_fd;
#endif
    struct vaapi_context hw_ctx;

    /* */
    vlc_mutex_t  lock;
    int          i_surface_width;
    int          i_surface_height;
    vlc_fourcc_t i_surface_chroma;

    VAImage      image;
    copy_cache_t image_cache;

    bool b_supports_derive;

    uint8_t      count;
    uint32_t     available;
    VASurfaceID  surfaces[32];
};

static void DestroySurfaces( vlc_va_sys_t *sys )
{
    if( sys->image.image_id != VA_INVALID_ID )
    {
        CopyCleanCache( &sys->image_cache );
        vaDestroyImage(sys->hw_ctx.display, sys->image.image_id);
    }
    else if(sys->b_supports_derive)
    {
        CopyCleanCache( &sys->image_cache );
    }

    /* */
    sys->image.image_id = VA_INVALID_ID;
    sys->i_surface_width = 0;
    sys->i_surface_height = 0;
    vlc_mutex_destroy(&sys->lock);
}

static int CreateSurfaces( vlc_va_sys_t *sys, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                           int i_width, int i_height )
{
    assert( i_width > 0 && i_height > 0 );

    /* */
    sys->image.image_id = VA_INVALID_ID;

    /* Find and create a supported image chroma */
    int i_fmt_count = vaMaxNumImageFormats(sys->hw_ctx.display);
    VAImageFormat *p_fmt = calloc( i_fmt_count, sizeof(*p_fmt) );
    if( !p_fmt )
        goto error;

    if (vaQueryImageFormats(sys->hw_ctx.display, p_fmt, &i_fmt_count))
    {
        free( p_fmt );
        goto error;
    }

    VAImage test_image;
    vlc_fourcc_t  deriveImageFormat = 0;
    if (vaDeriveImage(sys->hw_ctx.display, sys->surfaces[0], &test_image) == VA_STATUS_SUCCESS)
    {
        sys->b_supports_derive = true;
        deriveImageFormat = test_image.format.fourcc;
        vaDestroyImage(sys->hw_ctx.display, test_image.image_id);
    }

    vlc_fourcc_t  i_chroma = 0;
    int nv12support = -1;
    for( int i = 0; i < i_fmt_count; i++ )
    {
        if( p_fmt[i].fourcc == VA_FOURCC_YV12 ||
            p_fmt[i].fourcc == VA_FOURCC_IYUV ||
            p_fmt[i].fourcc == VA_FOURCC_NV12 )
        {
            if (vaCreateImage(sys->hw_ctx.display, &p_fmt[i], i_width, i_height, &sys->image))
            {
                sys->image.image_id = VA_INVALID_ID;
                continue;
            }
            /* Validate that vaGetImage works with this format */
            if (vaGetImage(sys->hw_ctx.display, sys->surfaces[0],
                           0, 0, i_width, i_height, sys->image.image_id))
            {
                vaDestroyImage(sys->hw_ctx.display, sys->image.image_id);
                sys->image.image_id = VA_INVALID_ID;
                continue;
            }

            if( p_fmt[i].fourcc == VA_FOURCC_NV12 )
            {
                /* Mark NV12 as supported, but favor other formats first */
                nv12support = i;
                vaDestroyImage(sys->hw_ctx.display, sys->image.image_id);
                sys->image.image_id = VA_INVALID_ID;
                continue;
            }
            i_chroma = VLC_CODEC_YV12;
            break;
        }
    }

    if( !i_chroma && nv12support >= 0 )
    {
        /* only nv12 is supported, so use that format */
        if (vaCreateImage(sys->hw_ctx.display, &p_fmt[nv12support], i_width, i_height, &sys->image))
        {
            sys->image.image_id = VA_INVALID_ID;
        }
        i_chroma = VLC_CODEC_YV12;
    }
    else if( sys->b_supports_derive && deriveImageFormat != sys->image.format.fourcc )
    {
        /* only use vaDerive if it's giving us a format we handle natively */
        sys->b_supports_derive = false;
    }

    free( p_fmt );
    if( !i_chroma )
        goto error;
    *pi_chroma = i_chroma;

    if(sys->b_supports_derive)
    {
        vaDestroyImage(sys->hw_ctx.display, sys->image.image_id);
        sys->image.image_id = VA_INVALID_ID;
    }

    if( unlikely(CopyInitCache( &sys->image_cache, i_width )) )
        goto error;

    /* Setup the ffmpeg hardware context */
    *pp_hw_ctx = &sys->hw_ctx;

    /* */
    sys->i_surface_chroma = i_chroma;
    sys->i_surface_width = i_width;
    sys->i_surface_height = i_height;
    return VLC_SUCCESS;

error:
    DestroySurfaces( sys );
    return VLC_EGENERIC;
}

static int Extract( vlc_va_t *va, picture_t *p_picture, uint8_t *data )
{
    vlc_va_sys_t *sys = va->sys;
    VASurfaceID i_surface_id = (VASurfaceID)(uintptr_t)data;

#if VA_CHECK_VERSION(0,31,0)
    if (vaSyncSurface(sys->hw_ctx.display, i_surface_id))
#else
    if (vaSyncSurface(sys->hw_ctx.display, sys->hw_ctx.context_id,
                      i_surface_id))
#endif
        return VLC_EGENERIC;

    if(sys->b_supports_derive)
    {
        if (vaDeriveImage(sys->hw_ctx.display, i_surface_id, &(sys->image)) != VA_STATUS_SUCCESS)
            return VLC_EGENERIC;
    }
    else
    {
        if (vaGetImage(sys->hw_ctx.display, i_surface_id, 0, 0,
                       sys->i_surface_width, sys->i_surface_height,
                       sys->image.image_id))
            return VLC_EGENERIC;
    }

    void *p_base;
    if (vaMapBuffer(sys->hw_ctx.display, sys->image.buf, &p_base))
        return VLC_EGENERIC;

    const uint32_t i_fourcc = sys->image.format.fourcc;
    if( i_fourcc == VA_FOURCC_YV12 ||
        i_fourcc == VA_FOURCC_IYUV )
    {
        bool b_swap_uv = i_fourcc == VA_FOURCC_IYUV;
        uint8_t *pp_plane[3];
        size_t  pi_pitch[3];

        for( int i = 0; i < 3; i++ )
        {
            const int i_src_plane = (b_swap_uv && i != 0) ?  (3 - i) : i;
            pp_plane[i] = (uint8_t*)p_base + sys->image.offsets[i_src_plane];
            pi_pitch[i] = sys->image.pitches[i_src_plane];
        }
        CopyFromYv12( p_picture, pp_plane, pi_pitch,
                      sys->i_surface_width,
                      sys->i_surface_height,
                      &sys->image_cache );
    }
    else
    {
        assert( i_fourcc == VA_FOURCC_NV12 );
        uint8_t *pp_plane[2];
        size_t  pi_pitch[2];

        for( int i = 0; i < 2; i++ )
        {
            pp_plane[i] = (uint8_t*)p_base + sys->image.offsets[i];
            pi_pitch[i] = sys->image.pitches[i];
        }
        CopyFromNv12( p_picture, pp_plane, pi_pitch,
                      sys->i_surface_width,
                      sys->i_surface_height,
                      &sys->image_cache );
    }

    if (vaUnmapBuffer(sys->hw_ctx.display, sys->image.buf))
        return VLC_EGENERIC;

    if(sys->b_supports_derive)
    {
        vaDestroyImage(sys->hw_ctx.display, sys->image.image_id);
        sys->image.image_id = VA_INVALID_ID;
    }
    return VLC_SUCCESS;
}

static int Get( vlc_va_t *va, picture_t *pic, uint8_t **data )
{
    vlc_va_sys_t *sys = va->sys;
    unsigned i = sys->count;

    vlc_mutex_lock( &sys->lock );
    if (sys->available)
    {
        i = ctz(sys->available);
        sys->available &= ~(1 << i);
    }
    vlc_mutex_unlock( &sys->lock );

    if( i >= sys->count )
        return VLC_ENOMEM;

    VASurfaceID *surface = &sys->surfaces[i];

    pic->context = surface;
    *data = (void *)(uintptr_t)*surface;
    return VLC_SUCCESS;
}

static void Release( void *opaque, uint8_t *data )
{
    picture_t *pic = opaque;
    VASurfaceID *surface = pic->context;
    vlc_va_sys_t *sys = (void *)((((uintptr_t)surface)
        - offsetof(vlc_va_sys_t, surfaces)) & ~(sizeof (sys->surfaces) - 1));
    unsigned i = surface - sys->surfaces;

    vlc_mutex_lock( &sys->lock );
    assert(((sys->available >> i) & 1) == 0);
    sys->available |= 1 << i;
    vlc_mutex_unlock( &sys->lock );

    pic->context = NULL;
    picture_Release(pic);
    (void) data;
}

static int Setup( vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *pi_chroma )
{
    vlc_va_sys_t *sys = va->sys;

    if( sys->i_surface_width == avctx->coded_width &&
        sys->i_surface_height == avctx->coded_height )
    {
        avctx->hwaccel_context = &sys->hw_ctx;
        *pi_chroma = sys->i_surface_chroma;
        return VLC_SUCCESS;
    }

    avctx->hwaccel_context = NULL;
    *pi_chroma = 0;
    if( sys->i_surface_width || sys->i_surface_height )
        DestroySurfaces( sys );

    if( avctx->coded_width <= 0 && avctx->coded_height <= 0 )
        return VLC_EGENERIC;

    return CreateSurfaces( sys, &avctx->hwaccel_context, pi_chroma,
                           avctx->coded_width, avctx->coded_height );
}

static void Delete( vlc_va_t *va, AVCodecContext *avctx )
{
    vlc_va_sys_t *sys = va->sys;

    (void) avctx;

    vaDestroyContext(sys->hw_ctx.display, sys->hw_ctx.context_id);
    vaDestroySurfaces(sys->hw_ctx.display, sys->surfaces, sys->count);
    vaDestroyConfig(sys->hw_ctx.display, sys->hw_ctx.config_id);
    vaTerminate(sys->hw_ctx.display);
#ifdef VLC_VA_BACKEND_XLIB
    XCloseDisplay( sys->p_display_x11 );
#endif
#ifdef VLC_VA_BACKEND_DRM
    close( sys->drm_fd );
#endif
    free( sys );
}

static int Create( vlc_va_t *va, AVCodecContext *ctx, enum PixelFormat pix_fmt,
                   const es_format_t *fmt )
{
    if( pix_fmt != AV_PIX_FMT_VAAPI_VLD )
        return VLC_EGENERIC;

    (void) fmt;
#ifdef VLC_VA_BACKEND_XLIB
    if( !vlc_xlib_init( VLC_OBJECT(va) ) )
    {
        msg_Warn( va, "Ignoring VA-X11 API" );
        return VLC_EGENERIC;
    }
#endif

    VAProfile i_profile, *p_profiles_list;
    bool b_supported_profile = false;
    int i_profiles_nb = 0;
    unsigned count = 3;

    /* */
    switch( ctx->codec_id )
    {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
        i_profile = VAProfileMPEG2Main;
        count = 4;
        break;
    case AV_CODEC_ID_MPEG4:
        i_profile = VAProfileMPEG4AdvancedSimple;
        break;
    case AV_CODEC_ID_WMV3:
        i_profile = VAProfileVC1Main;
        break;
    case AV_CODEC_ID_VC1:
        i_profile = VAProfileVC1Advanced;
        break;
    case AV_CODEC_ID_H264:
        i_profile = VAProfileH264High;
        count = 18;
        break;;
    default:
        return VLC_EGENERIC;
    }
    count += ctx->thread_count;

    vlc_va_sys_t *sys;
    void *mem;

    assert(popcount(sizeof (sys->surfaces)) == 1);
    if (unlikely(posix_memalign(&mem, sizeof (sys->surfaces), sizeof (*sys))))
       return VLC_ENOMEM;

    sys = mem;
    memset(sys, 0, sizeof (*sys));

    /* */
    sys->hw_ctx.display = NULL;
    sys->hw_ctx.config_id = VA_INVALID_ID;
    sys->hw_ctx.context_id = VA_INVALID_ID;
    sys->image.image_id = VA_INVALID_ID;
    sys->b_supports_derive = false;
    sys->count = count;
    sys->available = (1 << sys->count) - 1;
    assert(count < sizeof (sys->available) * CHAR_BIT);
    assert(count * sizeof (sys->surfaces[0]) <= sizeof (sys->surfaces));

    /* Create a VA display */
#ifdef VLC_VA_BACKEND_XLIB
    sys->p_display_x11 = XOpenDisplay(NULL);
    if( !sys->p_display_x11 )
    {
        msg_Err( va, "Could not connect to X server" );
        goto error;
    }

    sys->hw_ctx.display = vaGetDisplay(sys->p_display_x11);
#endif
#ifdef VLC_VA_BACKEND_DRM
    sys->drm_fd = vlc_open("/dev/dri/card0", O_RDWR);
    if( sys->drm_fd == -1 )
    {
        msg_Err( va, "Could not access rendering device: %m" );
        goto error;
    }

    sys->hw_ctx.display = vaGetDisplayDRM(sys->drm_fd);
#endif
    if (sys->hw_ctx.display == NULL)
    {
        msg_Err( va, "Could not get a VAAPI device" );
        goto error;
    }

    int major, minor;
    if (vaInitialize(sys->hw_ctx.display, &major, &minor))
    {
        msg_Err( va, "Failed to initialize the VAAPI device" );
        goto error;
    }

    /* Check if the selected profile is supported */
    i_profiles_nb = vaMaxNumProfiles(sys->hw_ctx.display);
    p_profiles_list = calloc( i_profiles_nb, sizeof( VAProfile ) );
    if( !p_profiles_list )
        goto error;

    if (vaQueryConfigProfiles(sys->hw_ctx.display, p_profiles_list,
                              &i_profiles_nb) == VA_STATUS_SUCCESS)
    {
        for( int i = 0; i < i_profiles_nb; i++ )
        {
            if ( p_profiles_list[i] == i_profile )
            {
                b_supported_profile = true;
                break;
            }
        }
    }
    free( p_profiles_list );
    if ( !b_supported_profile )
    {
        msg_Dbg( va, "Codec and profile not supported by the hardware" );
        goto error;
    }

    /* Create a VA configuration */
    VAConfigAttrib attrib;
    memset( &attrib, 0, sizeof(attrib) );
    attrib.type = VAConfigAttribRTFormat;
    if (vaGetConfigAttributes(sys->hw_ctx.display, i_profile, VAEntrypointVLD,
                              &attrib, 1))
        goto error;

    /* Not sure what to do if not, I don't have a way to test */
    if( (attrib.value & VA_RT_FORMAT_YUV420) == 0 )
        goto error;
    if (vaCreateConfig(sys->hw_ctx.display, i_profile, VAEntrypointVLD,
                       &attrib, 1, &sys->hw_ctx.config_id))
    {
        sys->hw_ctx.config_id = VA_INVALID_ID;
        goto error;
    }

    /* Create surfaces */
    assert(ctx->coded_width > 0 && ctx->coded_height > 0);
    if (vaCreateSurfaces(sys->hw_ctx.display, VA_RT_FORMAT_YUV420,
                         ctx->coded_width, ctx->coded_height,
                         sys->surfaces, sys->count, NULL, 0))
    {
        goto error;
    }

    /* Create a context */
    if (vaCreateContext(sys->hw_ctx.display, sys->hw_ctx.config_id,
                        ctx->coded_width, ctx->coded_height, VA_PROGRESSIVE,
                        sys->surfaces, sys->count, &sys->hw_ctx.context_id))
    {
        sys->hw_ctx.context_id = VA_INVALID_ID;
        vaDestroySurfaces(sys->hw_ctx.display, sys->surfaces, sys->count);
        goto error;
    }

    vlc_mutex_init(&sys->lock);

    va->sys = sys;
    va->description = vaQueryVendorString(sys->hw_ctx.display);
    va->setup = Setup;
    va->get = Get;
    va->release = Release;
    va->extract = Extract;
    return VLC_SUCCESS;

error:
    if (sys->hw_ctx.config_id != VA_INVALID_ID)
        vaDestroyConfig(sys->hw_ctx.display, sys->hw_ctx.config_id);
    if (sys->hw_ctx.display != NULL)
        vaTerminate(sys->hw_ctx.display);
#ifdef VLC_VA_BACKEND_XLIB
    if( sys->p_display_x11 != NULL )
        XCloseDisplay( sys->p_display_x11 );
#endif
#ifdef VLC_VA_BACKEND_DRM
    if( sys->drm_fd != -1 )
        close( sys->drm_fd );
#endif
    free( sys );
    return VLC_EGENERIC;
}

vlc_module_begin ()
#if defined (VLC_VA_BACKEND_XLIB)
    set_description( N_("VA-API video decoder via X11") )
#elif defined (VLC_VA_BACKEND_DRM)
    set_description( N_("VA-API video decoder via DRM") )
#endif
    set_capability( "hw decoder", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Create, Delete )
    add_shortcut( "vaapi" )
vlc_module_end ()
