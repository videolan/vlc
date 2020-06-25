/*****************************************************************************
 * chain.c : chain multiple video filter modules as a last resort solution
 *****************************************************************************
 * Copyright (C) 2007-2017 VLC authors and VideoLAN
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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
#include <vlc_filter.h>
#include <vlc_mouse.h>
#include <vlc_picture.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int       ActivateConverter  ( vlc_object_t * );
static int       ActivateFilter     ( vlc_object_t * );
static void      Destroy            ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Video filtering using a chain of video filter modules") )
    set_capability( "video converter", 1 )
    set_callbacks( ActivateConverter, Destroy )
    add_submodule ()
        set_capability( "video filter", 0 )
        set_callbacks( ActivateFilter, Destroy )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static picture_t *Chain         ( filter_t *, picture_t * );

static int BuildTransformChain( filter_t *p_filter );
static int BuildChromaResize( filter_t * );
static int BuildChromaChain( filter_t *p_filter );
static int BuildFilterChain( filter_t *p_filter );

static int CreateChain( filter_t *p_filter, const es_format_t *p_fmt_mid );
static int CreateResizeChromaChain( filter_t *p_filter, const es_format_t *p_fmt_mid );
static filter_t * AppendTransform( filter_chain_t *p_chain, const es_format_t *p_fmt_in,
                                   const es_format_t *p_fmt_out );
static void EsFormatMergeSize( es_format_t *p_dst,
                               const es_format_t *p_base,
                               const es_format_t *p_size );

#define ALLOWED_CHROMAS_YUV10 \
    VLC_CODEC_I420_10L, \
    VLC_CODEC_I420_10B, \
    VLC_CODEC_I420_16L \

static const vlc_fourcc_t pi_allowed_chromas_yuv[] = {
    VLC_CODEC_I420,
    VLC_CODEC_I422,
    ALLOWED_CHROMAS_YUV10,
    VLC_CODEC_RGB32,
    VLC_CODEC_RGB24,
    VLC_CODEC_BGRA,
    0
};

static const vlc_fourcc_t pi_allowed_chromas_yuv10[] = {
    ALLOWED_CHROMAS_YUV10,
    VLC_CODEC_I420,
    VLC_CODEC_I422,
    VLC_CODEC_RGB32,
    VLC_CODEC_RGB24,
    VLC_CODEC_BGRA,
    0
};

static const vlc_fourcc_t *get_allowed_chromas( filter_t *p_filter )
{
    switch (p_filter->fmt_out.video.i_chroma)
    {
        case VLC_CODEC_I420_10L:
        case VLC_CODEC_I420_10B:
        case VLC_CODEC_I420_16L:
        case VLC_CODEC_CVPX_P010:
        case VLC_CODEC_D3D9_OPAQUE_10B:
        case VLC_CODEC_D3D11_OPAQUE_10B:
        case VLC_CODEC_VAAPI_420_10BPP:
            return pi_allowed_chromas_yuv10;
        default:
            return pi_allowed_chromas_yuv;
    }
}

typedef struct
{
    filter_chain_t *p_chain;
    filter_t *p_video_filter;
} filter_sys_t;

/* Restart filter callback */
static int RestartFilterCallback( vlc_object_t *obj, char const *psz_name,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *p_data )
{ VLC_UNUSED(obj); VLC_UNUSED(psz_name); VLC_UNUSED(oldval);
    VLC_UNUSED(newval);

    var_TriggerCallback( (vlc_object_t *)p_data, "video-filter" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Buffer management
 *****************************************************************************/
static picture_t *BufferChainNew( filter_t *p_filter )
{
    filter_t *p_chain_parent = p_filter->owner.sys;
    // the last filter of the internal chain gets its pictures from the original
    // filter source
    return filter_NewPicture( p_chain_parent );
}

#define CHAIN_LEVEL_MAX 2

static vlc_decoder_device * HoldChainDecoderDevice(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    filter_t *p_chain_parent = sys;
    return filter_HoldDecoderDevice( p_chain_parent );
}

static const struct filter_video_callbacks filter_video_chain_cbs =
{
    BufferChainNew, HoldChainDecoderDevice,
};

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( filter_t *p_filter, int (*pf_build)(filter_t *) )
{
    filter_sys_t *p_sys;
    int i_ret = VLC_EGENERIC;

    p_sys = p_filter->p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    filter_owner_t owner = {
        .video = &filter_video_chain_cbs,
        .sys = p_filter,
    };

    p_sys->p_chain = filter_chain_NewVideo( p_filter, p_filter->b_allow_fmt_out_change, &owner );
    if( !p_sys->p_chain )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    int type = VLC_VAR_INTEGER;
    if( var_Type( vlc_object_parent(p_filter), "chain-level" ) != 0 )
        type |= VLC_VAR_DOINHERIT;

    var_Create( p_filter, "chain-level", type );
    /* Note: atomicity is not actually needed here. */
    var_IncInteger( p_filter, "chain-level" );

    int level = var_GetInteger( p_filter, "chain-level" );
    if( level < 0 || level > CHAIN_LEVEL_MAX )
        msg_Err( p_filter, "Too high level of recursion (%d)", level );
    else
        i_ret = pf_build( p_filter );

    var_Destroy( p_filter, "chain-level" );

    if( i_ret )
    {
        /* Hum ... looks like this really isn't going to work. Too bad. */
        if (p_sys->p_video_filter)
            filter_DelProxyCallbacks( p_filter, p_sys->p_video_filter,
                                      RestartFilterCallback );
        filter_chain_Delete( p_sys->p_chain );
        free( p_sys );
        return VLC_EGENERIC;
    }
    if( p_filter->b_allow_fmt_out_change )
    {
        es_format_Clean( &p_filter->fmt_out );
        es_format_Copy( &p_filter->fmt_out,
                        filter_chain_GetFmtOut( p_sys->p_chain ) );
        p_filter->vctx_out = filter_chain_GetVideoCtxOut( p_sys->p_chain );
    }
    assert(p_filter->vctx_out == filter_chain_GetVideoCtxOut( p_sys->p_chain ));
    /* */
    p_filter->pf_video_filter = Chain;
    return VLC_SUCCESS;
}

static int ActivateConverter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    const bool b_chroma = p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma;
    const bool b_resize = p_filter->fmt_in.video.i_width  != p_filter->fmt_out.video.i_width ||
                          p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height;

    const bool b_chroma_resize = b_chroma && b_resize;
    const bool b_transform = p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation;

    if( !b_chroma && !b_chroma_resize && !b_transform)
        return VLC_EGENERIC;

    return Activate( p_filter, b_transform ? BuildTransformChain :
                               b_chroma_resize ? BuildChromaResize :
                               BuildChromaChain );
}

static int ActivateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if( !p_filter->b_allow_fmt_out_change || p_filter->psz_name == NULL )
        return VLC_EGENERIC;

    if( var_Type( vlc_object_parent(p_filter), "chain-filter-level" ) != 0 )
        return VLC_EGENERIC;

    var_Create( p_filter, "chain-filter-level", VLC_VAR_INTEGER );
    int i_ret = Activate( p_filter, BuildFilterChain );
    var_Destroy( p_filter, "chain-filter-level" );

    return i_ret;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if (p_sys->p_video_filter)
        filter_DelProxyCallbacks( p_filter, p_sys->p_video_filter,
                                  RestartFilterCallback );
    filter_chain_Delete( p_sys->p_chain );
    free( p_sys );
}

/*****************************************************************************
 * Chain
 *****************************************************************************/
static picture_t *Chain( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    return filter_chain_VideoFilter( p_sys->p_chain, p_pic );
}

/*****************************************************************************
 * Builders
 *****************************************************************************/

static int BuildTransformChain( filter_t *p_filter )
{

    es_format_t fmt_mid;
    int i_ret;

    /* Lets try transform first, then (potentially) resize+chroma */
    msg_Dbg( p_filter, "Trying to build transform, then chroma+resize" );
    es_format_Copy( &fmt_mid, &p_filter->fmt_in );
    video_format_TransformTo(&fmt_mid.video, p_filter->fmt_out.video.orientation);
    i_ret = CreateChain( p_filter, &fmt_mid );
    es_format_Clean( &fmt_mid );
    if( i_ret == VLC_SUCCESS )
        return VLC_SUCCESS;

    /* Lets try resize+chroma first, then transform */
    msg_Dbg( p_filter, "Trying to build chroma+resize" );
    EsFormatMergeSize( &fmt_mid, &p_filter->fmt_out, &p_filter->fmt_in );
    i_ret = CreateChain( p_filter, &fmt_mid );
    es_format_Clean( &fmt_mid );
    return i_ret;
}

static int BuildChromaResize( filter_t *p_filter )
{
    es_format_t fmt_mid;
    int i_ret;

    /* Lets try resizing and then doing the chroma conversion */
    msg_Dbg( p_filter, "Trying to build resize+chroma" );
    EsFormatMergeSize( &fmt_mid, &p_filter->fmt_in, &p_filter->fmt_out );
    i_ret = CreateResizeChromaChain( p_filter, &fmt_mid );
    es_format_Clean( &fmt_mid );

    if( i_ret == VLC_SUCCESS )
        return VLC_SUCCESS;

    /* Lets try it the other way arround (chroma and then resize) */
    msg_Dbg( p_filter, "Trying to build chroma+resize" );
    EsFormatMergeSize( &fmt_mid, &p_filter->fmt_out, &p_filter->fmt_in );
    i_ret = CreateChain( p_filter, &fmt_mid );
    es_format_Clean( &fmt_mid );
    if( i_ret == VLC_SUCCESS )
        return VLC_SUCCESS;

    return VLC_EGENERIC;
}

static int BuildChromaChain( filter_t *p_filter )
{
    es_format_t fmt_mid;
    int i_ret = VLC_EGENERIC;

    /* Now try chroma format list */
    const vlc_fourcc_t *pi_allowed_chromas = get_allowed_chromas( p_filter );
    for( int i = 0; pi_allowed_chromas[i]; i++ )
    {
        const vlc_fourcc_t i_chroma = pi_allowed_chromas[i];
        if( i_chroma == p_filter->fmt_in.i_codec ||
            i_chroma == p_filter->fmt_out.i_codec )
            continue;

        msg_Dbg( p_filter, "Trying to use chroma %4.4s as middle man",
                 (char*)&i_chroma );

        es_format_Copy( &fmt_mid, &p_filter->fmt_in );
        fmt_mid.i_codec        =
        fmt_mid.video.i_chroma = i_chroma;
        fmt_mid.video.i_rmask  = 0;
        fmt_mid.video.i_gmask  = 0;
        fmt_mid.video.i_bmask  = 0;
        video_format_FixRgb(&fmt_mid.video);

        i_ret = CreateChain( p_filter, &fmt_mid );
        es_format_Clean( &fmt_mid );

        if( i_ret == VLC_SUCCESS )
            break;
    }

    return i_ret;
}

static int ChainMouse( filter_t *p_filter, vlc_mouse_t *p_mouse,
                       const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    (void) p_old;
    filter_sys_t *p_sys = p_filter->p_sys;
    return filter_chain_MouseFilter( p_sys->p_chain, p_mouse, p_new );
}

static int BuildFilterChain( filter_t *p_filter )
{
    es_format_t fmt_mid;
    int i_ret = VLC_EGENERIC;

    filter_sys_t *p_sys = p_filter->p_sys;

    /* Now try chroma format list */
    const vlc_fourcc_t *pi_allowed_chromas = get_allowed_chromas( p_filter );
    for( int i = 0; pi_allowed_chromas[i]; i++ )
    {
        filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, p_filter->vctx_in, &p_filter->fmt_out );

        const vlc_fourcc_t i_chroma = pi_allowed_chromas[i];
        if( i_chroma == p_filter->fmt_in.i_codec ||
            i_chroma == p_filter->fmt_out.i_codec )
            continue;

        msg_Dbg( p_filter, "Trying to use chroma %4.4s as middle man",
                 (char*)&i_chroma );

        es_format_Copy( &fmt_mid, &p_filter->fmt_in );
        fmt_mid.i_codec        =
        fmt_mid.video.i_chroma = i_chroma;
        fmt_mid.video.i_rmask  = 0;
        fmt_mid.video.i_gmask  = 0;
        fmt_mid.video.i_bmask  = 0;
        video_format_FixRgb(&fmt_mid.video);

        if( filter_chain_AppendConverter( p_sys->p_chain,
                                          &fmt_mid ) == VLC_SUCCESS )
        {
            p_sys->p_video_filter =
                filter_chain_AppendFilter( p_sys->p_chain,
                                           p_filter->psz_name, p_filter->p_cfg,
                                           &fmt_mid );
            if( p_sys->p_video_filter )
            {
                filter_AddProxyCallbacks( p_filter,
                                          p_sys->p_video_filter,
                                          RestartFilterCallback );
                if (p_sys->p_video_filter->pf_video_mouse != NULL)
                    p_filter->pf_video_mouse = ChainMouse;
                es_format_Clean( &fmt_mid );
                i_ret = VLC_SUCCESS;
                p_filter->vctx_out = filter_chain_GetVideoCtxOut( p_sys->p_chain );
                break;
            }
        }
        es_format_Clean( &fmt_mid );
    }
    if( i_ret != VLC_SUCCESS )
        filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, p_filter->vctx_in, &p_filter->fmt_out );

    return i_ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int CreateChain( filter_t *p_filter, const es_format_t *p_fmt_mid )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, p_filter->vctx_in, &p_filter->fmt_out );

    if( p_filter->fmt_in.video.orientation != p_fmt_mid->video.orientation)
    {
        filter_t *p_transform = AppendTransform( p_sys->p_chain, &p_filter->fmt_in, p_fmt_mid );
        // Check if filter was enough:
        if( p_transform == NULL )
            return VLC_EGENERIC;
        if( es_format_IsSimilar(&p_transform->fmt_out, &p_filter->fmt_out ) )
        {
            p_filter->vctx_out = p_transform->vctx_out;
            return VLC_SUCCESS;
        }
    }
    else
    {
        if( filter_chain_AppendConverter( p_sys->p_chain, p_fmt_mid ) )
            return VLC_EGENERIC;
    }

    if( p_fmt_mid->video.orientation != p_filter->fmt_out.video.orientation)
    {
        if( AppendTransform( p_sys->p_chain, p_fmt_mid,
                             &p_filter->fmt_out ) == NULL )
            goto error;
    }
    else
    {
        if( filter_chain_AppendConverter( p_sys->p_chain, &p_filter->fmt_out ) )
            goto error;
    }
    p_filter->vctx_out = filter_chain_GetVideoCtxOut( p_sys->p_chain );
    return VLC_SUCCESS;
error:
    //Clean up.
    filter_chain_Clear( p_sys->p_chain );
    return VLC_EGENERIC;
}

static int CreateResizeChromaChain( filter_t *p_filter, const es_format_t *p_fmt_mid )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    filter_chain_Reset( p_sys->p_chain, &p_filter->fmt_in, p_filter->vctx_in, &p_filter->fmt_out );

    int i_ret = filter_chain_AppendConverter( p_sys->p_chain, p_fmt_mid );
    if( i_ret != VLC_SUCCESS )
        return i_ret;

    if( p_filter->b_allow_fmt_out_change )
    {
        /* XXX: Update i_sar_num/i_sar_den from last converter. Cf.
         * p_filter->b_allow_fmt_out_change comment in video_chroma/swscale.c.
         * */

        es_format_t fmt_out;
        es_format_Copy( &fmt_out,
                        filter_chain_GetFmtOut( p_sys->p_chain ) );
        fmt_out.video.i_chroma = p_filter->fmt_out.video.i_chroma;

        i_ret = filter_chain_AppendConverter( p_sys->p_chain, &fmt_out );
        es_format_Clean( &fmt_out );
    }
    else
        i_ret = filter_chain_AppendConverter( p_sys->p_chain, &p_filter->fmt_out );

    if( i_ret != VLC_SUCCESS )
        filter_chain_Clear( p_sys->p_chain );
    else
        p_filter->vctx_out = filter_chain_GetVideoCtxOut( p_sys->p_chain );

    return i_ret;
}

static filter_t * AppendTransform( filter_chain_t *p_chain, const es_format_t *p_fmt1,
                                   const es_format_t *p_fmt2 )
{
    video_transform_t transform = video_format_GetTransform(p_fmt1->video.orientation, p_fmt2->video.orientation);

    const char *type;

    switch ( transform ) {

        case TRANSFORM_R90:
            type = "90";
            break;
        case TRANSFORM_R180:
            type = "180";
            break;
        case TRANSFORM_R270:
            type = "270";
            break;
        case TRANSFORM_HFLIP:
            type = "hflip";
            break;
        case TRANSFORM_VFLIP:
            type = "vflip";
            break;
        case TRANSFORM_TRANSPOSE:
            type = "transpose";
            break;
        case TRANSFORM_ANTI_TRANSPOSE:
            type = "antitranspose";
            break;
        default:
            type = NULL;
            break;
    }

    if( !type )
        return NULL;

    config_chain_t *cfg;
    char *name;
    char config[100];
    snprintf( config, 100, "transform{type=%s}", type );
    char *next = config_ChainCreate( &name, &cfg, config );

    filter_t *p_filter = filter_chain_AppendFilter( p_chain, name, cfg, p_fmt2 );

    config_ChainDestroy(cfg);
    free(name);
    free(next);

    return p_filter;
}

static void EsFormatMergeSize( es_format_t *p_dst,
                               const es_format_t *p_base,
                               const es_format_t *p_size )
{
    es_format_Copy( p_dst, p_base );

    p_dst->video.i_width  = p_size->video.i_width;
    p_dst->video.i_height = p_size->video.i_height;

    p_dst->video.i_visible_width  = p_size->video.i_visible_width;
    p_dst->video.i_visible_height = p_size->video.i_visible_height;

    p_dst->video.i_x_offset = p_size->video.i_x_offset;
    p_dst->video.i_y_offset = p_size->video.i_y_offset;

    p_dst->video.orientation = p_size->video.orientation;
    p_dst->video.i_sar_num = p_size->video.i_sar_num;
    p_dst->video.i_sar_den = p_size->video.i_sar_den;
}
