/*****************************************************************************
 * vaapi.c: VAAPI helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
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
#include <vlc_xlib.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/vaapi.h>
#include <X11/Xlib.h>
#include <va/va_x11.h>

#include "avcodec.h"
#include "va.h"
#include "copy.h"

static int Create( vlc_va_t *, int, const es_format_t * );
static void Delete( vlc_va_t * );

vlc_module_begin ()
    set_description( N_("Video Acceleration (VA) API") )
    set_capability( "hw decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Create, Delete )
vlc_module_end ()

typedef struct
{
    VASurfaceID  i_id;
    int          i_refcount;
    unsigned int i_order;

} vlc_va_surface_t;

struct vlc_va_sys_t
{
    Display      *p_display_x11;
    VADisplay     p_display;

    VAConfigID    i_config_id;
    VAContextID   i_context_id;

    struct vaapi_context hw_ctx;

    /* */
    int i_version_major;
    int i_version_minor;

    /* */
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
static int Open( vlc_va_t *p_external, int i_codec_id )
{
    vlc_va_sys_t *p_va = calloc( 1, sizeof(*p_va) );
    if ( unlikely(p_va == NULL) )
       return VLC_ENOMEM;

    VAProfile i_profile, *p_profiles_list;
    bool b_supported_profile = false;
    int i_profiles_nb = 0;
    int i_surface_count;

    /* */
    switch( i_codec_id )
    {
    case CODEC_ID_MPEG1VIDEO:
    case CODEC_ID_MPEG2VIDEO:
        i_profile = VAProfileMPEG2Main;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_MPEG4:
        i_profile = VAProfileMPEG4AdvancedSimple;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_WMV3:
        i_profile = VAProfileVC1Main;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_VC1:
        i_profile = VAProfileVC1Advanced;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_H264:
        i_profile = VAProfileH264High;
        i_surface_count = 16+1;
        break;
    default:
        return VLC_EGENERIC;
    }

    /* */
    p_va->i_config_id  = VA_INVALID_ID;
    p_va->i_context_id = VA_INVALID_ID;
    p_va->image.image_id = VA_INVALID_ID;

    /* Create a VA display */
    p_va->p_display_x11 = XOpenDisplay(NULL);
    if( !p_va->p_display_x11 )
    {
        msg_Err( p_external, "Could not connect to X server" );
        goto error;
    }

    p_va->p_display = vaGetDisplay( p_va->p_display_x11 );
    if( !p_va->p_display )
    {
        msg_Err( p_external, "Could not get a VAAPI device" );
        goto error;
    }

    if( vaInitialize( p_va->p_display, &p_va->i_version_major, &p_va->i_version_minor ) )
    {
        msg_Err( p_external, "Failed to initialize the VAAPI device" );
        goto error;
    }

    /* Check if the selected profile is supported */
    i_profiles_nb = vaMaxNumProfiles( p_va->p_display );
    p_profiles_list = calloc( i_profiles_nb, sizeof( VAProfile ) );
    if( !p_profiles_list )
        goto error;

    VAStatus i_status = vaQueryConfigProfiles( p_va->p_display, p_profiles_list, &i_profiles_nb );
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
        msg_Dbg( p_external, "Codec and profile not supported by the hardware" );
        goto error;
    }

    /* Create a VA configuration */
    VAConfigAttrib attrib;
    memset( &attrib, 0, sizeof(attrib) );
    attrib.type = VAConfigAttribRTFormat;
    if( vaGetConfigAttributes( p_va->p_display,
                               i_profile, VAEntrypointVLD, &attrib, 1 ) )
        goto error;

    /* Not sure what to do if not, I don't have a way to test */
    if( (attrib.value & VA_RT_FORMAT_YUV420) == 0 )
        goto error;
    if( vaCreateConfig( p_va->p_display,
                        i_profile, VAEntrypointVLD, &attrib, 1, &p_va->i_config_id ) )
    {
        p_va->i_config_id = VA_INVALID_ID;
        goto error;
    }

    p_va->i_surface_count = i_surface_count;

    p_va->b_supports_derive = false;

    if( asprintf( &p_external->description, "VA API version %d.%d",
                  p_va->i_version_major, p_va->i_version_minor ) < 0 )
        p_external->description = NULL;

    p_external->sys = p_va;
    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

static void DestroySurfaces( vlc_va_sys_t *p_va )
{
    if( p_va->image.image_id != VA_INVALID_ID )
    {
        CopyCleanCache( &p_va->image_cache );
        vaDestroyImage( p_va->p_display, p_va->image.image_id );
    }
    else if(p_va->b_supports_derive)
    {
        CopyCleanCache( &p_va->image_cache );
    }

    if( p_va->i_context_id != VA_INVALID_ID )
        vaDestroyContext( p_va->p_display, p_va->i_context_id );

    for( int i = 0; i < p_va->i_surface_count && p_va->p_surface; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        if( p_surface->i_id != VA_INVALID_SURFACE )
            vaDestroySurfaces( p_va->p_display, &p_surface->i_id, 1 );
    }
    free( p_va->p_surface );

    /* */
    p_va->image.image_id = VA_INVALID_ID;
    p_va->i_context_id = VA_INVALID_ID;
    p_va->p_surface = NULL;
    p_va->i_surface_width = 0;
    p_va->i_surface_height = 0;
}
static int CreateSurfaces( vlc_va_sys_t *p_va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                           int i_width, int i_height )
{
    assert( i_width > 0 && i_height > 0 );

    /* */
    p_va->p_surface = calloc( p_va->i_surface_count, sizeof(*p_va->p_surface) );
    if( !p_va->p_surface )
        return VLC_EGENERIC;
    p_va->image.image_id = VA_INVALID_ID;
    p_va->i_context_id   = VA_INVALID_ID;

    /* Create surfaces */
    VASurfaceID pi_surface_id[p_va->i_surface_count];
    if( vaCreateSurfaces( p_va->p_display, i_width, i_height, VA_RT_FORMAT_YUV420,
                          p_va->i_surface_count, pi_surface_id ) )
    {
        for( int i = 0; i < p_va->i_surface_count; i++ )
            p_va->p_surface[i].i_id = VA_INVALID_SURFACE;
        goto error;
    }

    for( int i = 0; i < p_va->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        p_surface->i_id = pi_surface_id[i];
        p_surface->i_refcount = 0;
        p_surface->i_order = 0;
    }

    /* Create a context */
    if( vaCreateContext( p_va->p_display, p_va->i_config_id,
                         i_width, i_height, VA_PROGRESSIVE,
                         pi_surface_id, p_va->i_surface_count, &p_va->i_context_id ) )
    {
        p_va->i_context_id = VA_INVALID_ID;
        goto error;
    }

    /* Find and create a supported image chroma */
    int i_fmt_count = vaMaxNumImageFormats( p_va->p_display );
    VAImageFormat *p_fmt = calloc( i_fmt_count, sizeof(*p_fmt) );
    if( !p_fmt )
        goto error;

    if( vaQueryImageFormats( p_va->p_display, p_fmt, &i_fmt_count ) )
    {
        free( p_fmt );
        goto error;
    }

    VAImage test_image;
    if(vaDeriveImage(p_va->p_display, pi_surface_id[0], &test_image) == VA_STATUS_SUCCESS)
    {
        p_va->b_supports_derive = true;
        vaDestroyImage(p_va->p_display, test_image.image_id);
    }

    vlc_fourcc_t  i_chroma = 0;
    VAImageFormat fmt;
    for( int i = 0; i < i_fmt_count; i++ )
    {
        if( p_fmt[i].fourcc == VA_FOURCC( 'Y', 'V', '1', '2' ) ||
            p_fmt[i].fourcc == VA_FOURCC( 'I', '4', '2', '0' ) ||
            p_fmt[i].fourcc == VA_FOURCC( 'N', 'V', '1', '2' ) )
        {
            if( vaCreateImage(  p_va->p_display, &p_fmt[i], i_width, i_height, &p_va->image ) )
            {
                p_va->image.image_id = VA_INVALID_ID;
                continue;
            }
            /* Validate that vaGetImage works with this format */
            if( vaGetImage( p_va->p_display, pi_surface_id[0],
                            0, 0, i_width, i_height,
                            p_va->image.image_id) )
            {
                vaDestroyImage( p_va->p_display, p_va->image.image_id );
                p_va->image.image_id = VA_INVALID_ID;
                continue;
            }

            i_chroma = VLC_CODEC_YV12;
            fmt = p_fmt[i];
            break;
        }
    }
    free( p_fmt );
    if( !i_chroma )
        goto error;
    *pi_chroma = i_chroma;

    if(p_va->b_supports_derive)
    {
        vaDestroyImage( p_va->p_display, p_va->image.image_id );
        p_va->image.image_id = VA_INVALID_ID;
    }

    if( unlikely(CopyInitCache( &p_va->image_cache, i_width )) )
        goto error;

    /* Setup the ffmpeg hardware context */
    *pp_hw_ctx = &p_va->hw_ctx;

    memset( &p_va->hw_ctx, 0, sizeof(p_va->hw_ctx) );
    p_va->hw_ctx.display    = p_va->p_display;
    p_va->hw_ctx.config_id  = p_va->i_config_id;
    p_va->hw_ctx.context_id = p_va->i_context_id;

    /* */
    p_va->i_surface_chroma = i_chroma;
    p_va->i_surface_width = i_width;
    p_va->i_surface_height = i_height;
    return VLC_SUCCESS;

error:
    DestroySurfaces( p_va );
    return VLC_EGENERIC;
}

static int Setup( vlc_va_t *p_external, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                  int i_width, int i_height )
{
    vlc_va_sys_t *p_va = p_external->sys;

    if( p_va->i_surface_width == i_width &&
        p_va->i_surface_height == i_height )
    {
        *pp_hw_ctx = &p_va->hw_ctx;
        *pi_chroma = p_va->i_surface_chroma;
        return VLC_SUCCESS;
    }

    *pp_hw_ctx = NULL;
    *pi_chroma = 0;
    if( p_va->i_surface_width || p_va->i_surface_height )
        DestroySurfaces( p_va );

    if( i_width > 0 && i_height > 0 )
        return CreateSurfaces( p_va, pp_hw_ctx, pi_chroma, i_width, i_height );

    return VLC_EGENERIC;
}
static int Extract( vlc_va_t *p_external, picture_t *p_picture, AVFrame *p_ff )
{
    vlc_va_sys_t *p_va = p_external->sys;

    VASurfaceID i_surface_id = (VASurfaceID)(uintptr_t)p_ff->data[3];

#if VA_CHECK_VERSION(0,31,0)
    if( vaSyncSurface( p_va->p_display, i_surface_id ) )
#else
    if( vaSyncSurface( p_va->p_display, p_va->i_context_id, i_surface_id ) )
#endif
        return VLC_EGENERIC;

    if(p_va->b_supports_derive)
    {
        if(vaDeriveImage(p_va->p_display, i_surface_id, &(p_va->image)) != VA_STATUS_SUCCESS)
            return VLC_EGENERIC;
    }
    else
    {
        if( vaGetImage( p_va->p_display, i_surface_id,
                        0, 0, p_va->i_surface_width, p_va->i_surface_height,
                        p_va->image.image_id) )
            return VLC_EGENERIC;
    }

    void *p_base;
    if( vaMapBuffer( p_va->p_display, p_va->image.buf, &p_base ) )
        return VLC_EGENERIC;

    const uint32_t i_fourcc = p_va->image.format.fourcc;
    if( i_fourcc == VA_FOURCC('Y','V','1','2') ||
        i_fourcc == VA_FOURCC('I','4','2','0') )
    {
        bool b_swap_uv = i_fourcc == VA_FOURCC('I','4','2','0');
        uint8_t *pp_plane[3];
        size_t  pi_pitch[3];

        for( int i = 0; i < 3; i++ )
        {
            const int i_src_plane = (b_swap_uv && i != 0) ?  (3 - i) : i;
            pp_plane[i] = (uint8_t*)p_base + p_va->image.offsets[i_src_plane];
            pi_pitch[i] = p_va->image.pitches[i_src_plane];
        }
        CopyFromYv12( p_picture, pp_plane, pi_pitch,
                      p_va->i_surface_width,
                      p_va->i_surface_height,
                      &p_va->image_cache );
    }
    else
    {
        assert( i_fourcc == VA_FOURCC('N','V','1','2') );
        uint8_t *pp_plane[2];
        size_t  pi_pitch[2];

        for( int i = 0; i < 2; i++ )
        {
            pp_plane[i] = (uint8_t*)p_base + p_va->image.offsets[i];
            pi_pitch[i] = p_va->image.pitches[i];
        }
        CopyFromNv12( p_picture, pp_plane, pi_pitch,
                      p_va->i_surface_width,
                      p_va->i_surface_height,
                      &p_va->image_cache );
    }

    if( vaUnmapBuffer( p_va->p_display, p_va->image.buf ) )
        return VLC_EGENERIC;

    if(p_va->b_supports_derive)
    {
        vaDestroyImage( p_va->p_display, p_va->image.image_id );
        p_va->image.image_id = VA_INVALID_ID;
    }

    return VLC_SUCCESS;
}
static int Get( vlc_va_t *p_external, AVFrame *p_ff )
{
    vlc_va_sys_t *p_va = p_external->sys;
    int i_old;
    int i;

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with ffmpeg */
    for( i = 0, i_old = 0; i < p_va->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        if( !p_surface->i_refcount )
            break;

        if( p_surface->i_order < p_va->p_surface[i_old].i_order )
            i_old = i;
    }
    if( i >= p_va->i_surface_count )
        i = i_old;

    vlc_va_surface_t *p_surface = &p_va->p_surface[i];

    p_surface->i_refcount = 1;
    p_surface->i_order = p_va->i_surface_order++;

    /* */
    for( int i = 0; i < 4; i++ )
    {
        p_ff->data[i] = NULL;
        p_ff->linesize[i] = 0;

        if( i == 0 || i == 3 )
            p_ff->data[i] = (void*)(uintptr_t)p_surface->i_id;/* Yummie */
    }
    return VLC_SUCCESS;
}
static void Release( vlc_va_t *p_external, AVFrame *p_ff )
{
    vlc_va_sys_t *p_va = p_external->sys;

    VASurfaceID i_surface_id = (VASurfaceID)(uintptr_t)p_ff->data[3];

    for( int i = 0; i < p_va->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        if( p_surface->i_id == i_surface_id )
            p_surface->i_refcount--;
    }
}

static void Close( vlc_va_sys_t *p_va )
{
    if( p_va->i_surface_width || p_va->i_surface_height )
        DestroySurfaces( p_va );

    if( p_va->i_config_id != VA_INVALID_ID )
        vaDestroyConfig( p_va->p_display, p_va->i_config_id );
    if( p_va->p_display )
        vaTerminate( p_va->p_display );
    if( p_va->p_display_x11 )
        XCloseDisplay( p_va->p_display_x11 );
}

static void Delete( vlc_va_t *p_external )
{
    vlc_va_sys_t *p_va = p_external->sys;
    Close( p_va );
    free( p_external->description );
    free( p_va );
}

static int Create( vlc_va_t *p_va, int i_codec_id, const es_format_t *fmt )
{
    if( !vlc_xlib_init( VLC_OBJECT(p_va) ) )
    {
        msg_Warn( p_va, "Ignoring VA API" );
        return VLC_EGENERIC;
    }

    (void) fmt;

    int err = Open( p_va, i_codec_id );
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
