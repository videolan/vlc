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

static int Create( vlc_va_t *, AVCodecContext *, const es_format_t * );
static void Delete( vlc_va_t * );

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

typedef struct
{
    VASurfaceID  i_id;
    int          i_refcount;
    unsigned int i_order;
    vlc_mutex_t *p_lock;
} vlc_va_surface_t;

struct vlc_va_sys_t
{
#ifdef VLC_VA_BACKEND_XLIB
        Display  *p_display_x11;
#endif
#ifdef VLC_VA_BACKEND_DRM
        int       drm_fd;
#endif
    VADisplay     p_display;

    VAConfigID    i_config_id;
    VAContextID   i_context_id;

    struct vaapi_context hw_ctx;

    /* */
    vlc_mutex_t  lock;
    int          i_surface_count;
    unsigned int i_surface_order;
    int          i_surface_width;
    int          i_surface_height;
    vlc_fourcc_t i_surface_chroma;

    vlc_va_surface_t *p_surface;

    VAImage      image;
    copy_cache_t image_cache;

    bool b_supports_derive;
};

/* */
static int Open( vlc_va_t *va, int i_codec_id, int i_thread_count )
{
    vlc_va_sys_t *sys = calloc( 1, sizeof(*sys) );
    if ( unlikely(sys == NULL) )
       return VLC_ENOMEM;

    VAProfile i_profile, *p_profiles_list;
    bool b_supported_profile = false;
    int i_profiles_nb = 0;
    int i_surface_count;

    /* */
    switch( i_codec_id )
    {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
        i_profile = VAProfileMPEG2Main;
        i_surface_count = 2 + 2;
        break;
    case AV_CODEC_ID_MPEG4:
        i_profile = VAProfileMPEG4AdvancedSimple;
        i_surface_count = 2+1;
        break;
    case AV_CODEC_ID_WMV3:
        i_profile = VAProfileVC1Main;
        i_surface_count = 2+1;
        break;
    case AV_CODEC_ID_VC1:
        i_profile = VAProfileVC1Advanced;
        i_surface_count = 2+1;
        break;
    case AV_CODEC_ID_H264:
        i_profile = VAProfileH264High;
        i_surface_count = 16 + i_thread_count + 2;
        break;;
    default:
        free( sys );
        return VLC_EGENERIC;
    }

    /* */
    sys->i_config_id  = VA_INVALID_ID;
    sys->i_context_id = VA_INVALID_ID;
    sys->image.image_id = VA_INVALID_ID;

    /* Create a VA display */
#ifdef VLC_VA_BACKEND_XLIB
    sys->p_display_x11 = XOpenDisplay(NULL);
    if( !sys->p_display_x11 )
    {
        msg_Err( va, "Could not connect to X server" );
        goto error;
    }

    sys->p_display = vaGetDisplay( sys->p_display_x11 );
#endif
#ifdef VLC_VA_BACKEND_DRM
    sys->drm_fd = vlc_open("/dev/dri/card0", O_RDWR);
    if( sys->drm_fd == -1 )
    {
        msg_Err( va, "Could not access rendering device: %m" );
        goto error;
    }

    sys->p_display = vaGetDisplayDRM( sys->drm_fd );
#endif
    if( !sys->p_display )
    {
        msg_Err( va, "Could not get a VAAPI device" );
        goto error;
    }

    int major, minor;

    if( vaInitialize( sys->p_display, &major, &minor ) )
    {
        msg_Err( va, "Failed to initialize the VAAPI device" );
        goto error;
    }

    /* Check if the selected profile is supported */
    i_profiles_nb = vaMaxNumProfiles( sys->p_display );
    p_profiles_list = calloc( i_profiles_nb, sizeof( VAProfile ) );
    if( !p_profiles_list )
        goto error;

    VAStatus i_status = vaQueryConfigProfiles( sys->p_display, p_profiles_list, &i_profiles_nb );
    if ( i_status == VA_STATUS_SUCCESS )
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
    if( vaGetConfigAttributes( sys->p_display,
                               i_profile, VAEntrypointVLD, &attrib, 1 ) )
        goto error;

    /* Not sure what to do if not, I don't have a way to test */
    if( (attrib.value & VA_RT_FORMAT_YUV420) == 0 )
        goto error;
    if( vaCreateConfig( sys->p_display,
                        i_profile, VAEntrypointVLD, &attrib, 1, &sys->i_config_id ) )
    {
        sys->i_config_id = VA_INVALID_ID;
        goto error;
    }

    sys->i_surface_count = i_surface_count;

    sys->b_supports_derive = false;

    vlc_mutex_init(&sys->lock);

    va->sys = sys;
    va->description = vaQueryVendorString( sys->p_display );
    return VLC_SUCCESS;

error:
    if( sys->p_display != NULL )
        vaTerminate( sys->p_display );
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

static void DestroySurfaces( vlc_va_sys_t *sys )
{
    if( sys->image.image_id != VA_INVALID_ID )
    {
        CopyCleanCache( &sys->image_cache );
        vaDestroyImage( sys->p_display, sys->image.image_id );
    }
    else if(sys->b_supports_derive)
    {
        CopyCleanCache( &sys->image_cache );
    }

    if( sys->i_context_id != VA_INVALID_ID )
        vaDestroyContext( sys->p_display, sys->i_context_id );

    for( int i = 0; i < sys->i_surface_count && sys->p_surface; i++ )
    {
        vlc_va_surface_t *p_surface = &sys->p_surface[i];

        if( p_surface->i_id != VA_INVALID_SURFACE )
            vaDestroySurfaces( sys->p_display, &p_surface->i_id, 1 );
    }
    free( sys->p_surface );

    /* */
    sys->image.image_id = VA_INVALID_ID;
    sys->i_context_id = VA_INVALID_ID;
    sys->p_surface = NULL;
    sys->i_surface_width = 0;
    sys->i_surface_height = 0;
    vlc_mutex_destroy(&sys->lock);
}

static int CreateSurfaces( vlc_va_sys_t *sys, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                           int i_width, int i_height )
{
    assert( i_width > 0 && i_height > 0 );

    /* */
    sys->p_surface = calloc( sys->i_surface_count, sizeof(*sys->p_surface) );
    if( !sys->p_surface )
        return VLC_EGENERIC;
    sys->image.image_id = VA_INVALID_ID;
    sys->i_context_id   = VA_INVALID_ID;

    /* Create surfaces */
    VASurfaceID pi_surface_id[sys->i_surface_count];
    if( vaCreateSurfaces( sys->p_display, VA_RT_FORMAT_YUV420, i_width, i_height,
                          pi_surface_id, sys->i_surface_count, NULL, 0 ) )
    {
        for( int i = 0; i < sys->i_surface_count; i++ )
            sys->p_surface[i].i_id = VA_INVALID_SURFACE;
        goto error;
    }

    for( int i = 0; i < sys->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &sys->p_surface[i];

        p_surface->i_id = pi_surface_id[i];
        p_surface->i_refcount = 0;
        p_surface->i_order = 0;
        p_surface->p_lock = &sys->lock;
    }

    /* Create a context */
    if( vaCreateContext( sys->p_display, sys->i_config_id,
                         i_width, i_height, VA_PROGRESSIVE,
                         pi_surface_id, sys->i_surface_count, &sys->i_context_id ) )
    {
        sys->i_context_id = VA_INVALID_ID;
        goto error;
    }

    /* Find and create a supported image chroma */
    int i_fmt_count = vaMaxNumImageFormats( sys->p_display );
    VAImageFormat *p_fmt = calloc( i_fmt_count, sizeof(*p_fmt) );
    if( !p_fmt )
        goto error;

    if( vaQueryImageFormats( sys->p_display, p_fmt, &i_fmt_count ) )
    {
        free( p_fmt );
        goto error;
    }

    VAImage test_image;
    if(vaDeriveImage(sys->p_display, pi_surface_id[0], &test_image) == VA_STATUS_SUCCESS)
    {
        sys->b_supports_derive = true;
        vaDestroyImage(sys->p_display, test_image.image_id);
    }

    vlc_fourcc_t  i_chroma = 0;
    for( int i = 0; i < i_fmt_count; i++ )
    {
        if( p_fmt[i].fourcc == VA_FOURCC_YV12 ||
            p_fmt[i].fourcc == VA_FOURCC_IYUV ||
            p_fmt[i].fourcc == VA_FOURCC_NV12 )
        {
            if( vaCreateImage(  sys->p_display, &p_fmt[i], i_width, i_height, &sys->image ) )
            {
                sys->image.image_id = VA_INVALID_ID;
                continue;
            }
            /* Validate that vaGetImage works with this format */
            if( vaGetImage( sys->p_display, pi_surface_id[0],
                            0, 0, i_width, i_height,
                            sys->image.image_id) )
            {
                vaDestroyImage( sys->p_display, sys->image.image_id );
                sys->image.image_id = VA_INVALID_ID;
                continue;
            }

            i_chroma = VLC_CODEC_YV12;
            break;
        }
    }
    free( p_fmt );
    if( !i_chroma )
        goto error;
    *pi_chroma = i_chroma;

    if(sys->b_supports_derive)
    {
        vaDestroyImage( sys->p_display, sys->image.image_id );
        sys->image.image_id = VA_INVALID_ID;
    }

    if( unlikely(CopyInitCache( &sys->image_cache, i_width )) )
        goto error;

    /* Setup the ffmpeg hardware context */
    *pp_hw_ctx = &sys->hw_ctx;

    memset( &sys->hw_ctx, 0, sizeof(sys->hw_ctx) );
    sys->hw_ctx.display    = sys->p_display;
    sys->hw_ctx.config_id  = sys->i_config_id;
    sys->hw_ctx.context_id = sys->i_context_id;

    /* */
    sys->i_surface_chroma = i_chroma;
    sys->i_surface_width = i_width;
    sys->i_surface_height = i_height;
    return VLC_SUCCESS;

error:
    DestroySurfaces( sys );
    return VLC_EGENERIC;
}

static int Setup( vlc_va_t *va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                  int i_width, int i_height )
{
    vlc_va_sys_t *sys = va->sys;

    if( sys->i_surface_width == i_width &&
        sys->i_surface_height == i_height )
    {
        *pp_hw_ctx = &sys->hw_ctx;
        *pi_chroma = sys->i_surface_chroma;
        return VLC_SUCCESS;
    }

    *pp_hw_ctx = NULL;
    *pi_chroma = 0;
    if( sys->i_surface_width || sys->i_surface_height )
        DestroySurfaces( sys );

    if( i_width > 0 && i_height > 0 )
        return CreateSurfaces( sys, pp_hw_ctx, pi_chroma, i_width, i_height );

    return VLC_EGENERIC;
}

static int Extract( vlc_va_t *va, picture_t *p_picture, void *opaque,
                    uint8_t *data )
{
    vlc_va_sys_t *sys = va->sys;
    VASurfaceID i_surface_id = (VASurfaceID)(uintptr_t)data;

#if VA_CHECK_VERSION(0,31,0)
    if( vaSyncSurface( sys->p_display, i_surface_id ) )
#else
    if( vaSyncSurface( sys->p_display, sys->i_context_id, i_surface_id ) )
#endif
        return VLC_EGENERIC;

    if(sys->b_supports_derive)
    {
        if(vaDeriveImage(sys->p_display, i_surface_id, &(sys->image)) != VA_STATUS_SUCCESS)
            return VLC_EGENERIC;
    }
    else
    {
        if( vaGetImage( sys->p_display, i_surface_id,
                        0, 0, sys->i_surface_width, sys->i_surface_height,
                        sys->image.image_id) )
            return VLC_EGENERIC;
    }

    void *p_base;
    if( vaMapBuffer( sys->p_display, sys->image.buf, &p_base ) )
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

    if( vaUnmapBuffer( sys->p_display, sys->image.buf ) )
        return VLC_EGENERIC;

    if(sys->b_supports_derive)
    {
        vaDestroyImage( sys->p_display, sys->image.image_id );
        sys->image.image_id = VA_INVALID_ID;
    }
    (void) opaque;
    return VLC_SUCCESS;
}

static int Get( vlc_va_t *va, void **opaque, uint8_t **data )
{
    vlc_va_sys_t *sys = va->sys;
    int i_old;
    int i;

    vlc_mutex_lock( &sys->lock );
    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with ffmpeg */
    for( i = 0, i_old = 0; i < sys->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &sys->p_surface[i];

        if( !p_surface->i_refcount )
            break;

        if( p_surface->i_order < sys->p_surface[i_old].i_order )
            i_old = i;
    }
    if( i >= sys->i_surface_count )
        i = i_old;
    vlc_mutex_unlock( &sys->lock );

    vlc_va_surface_t *p_surface = &sys->p_surface[i];

    p_surface->i_refcount = 1;
    p_surface->i_order = sys->i_surface_order++;
    *data = (void *)(uintptr_t)p_surface->i_id;
    *opaque = p_surface;
    return VLC_SUCCESS;
}

static void Release( void *opaque, uint8_t *data )
{
    vlc_va_surface_t *p_surface = opaque;

    vlc_mutex_lock( p_surface->p_lock );
    p_surface->i_refcount--;
    vlc_mutex_unlock( p_surface->p_lock );
    (void) data;
}

static void Close( vlc_va_sys_t *sys )
{
    if( sys->i_surface_width || sys->i_surface_height )
        DestroySurfaces( sys );

    if( sys->i_config_id != VA_INVALID_ID )
        vaDestroyConfig( sys->p_display, sys->i_config_id );
    vaTerminate( sys->p_display );
#ifdef VLC_VA_BACKEND_XLIB
    XCloseDisplay( sys->p_display_x11 );
#endif
#ifdef VLC_VA_BACKEND_DRM
    close( sys->drm_fd );
#endif
}

static void Delete( vlc_va_t *va )
{
    vlc_va_sys_t *sys = va->sys;
    Close( sys );
    free( sys );
}

static int Create( vlc_va_t *p_va, AVCodecContext *ctx,
                   const es_format_t *fmt )
{
#ifdef VLC_VA_BACKEND_XLIB
    if( !vlc_xlib_init( VLC_OBJECT(p_va) ) )
    {
        msg_Warn( p_va, "Ignoring VA API" );
        return VLC_EGENERIC;
    }
#endif

    (void) fmt;

    int err = Open( p_va, ctx->codec_id, ctx->thread_count );
    if( err )
        return err;

    /* Only VLD supported */
    p_va->pix_fmt = PIX_FMT_VAAPI_VLD;
    p_va->setup = Setup;
    p_va->get = Get;
    p_va->release = Release;
    p_va->extract = Extract;
    return VLC_SUCCESS;
}
