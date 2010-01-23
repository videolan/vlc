/*****************************************************************************
 * wrapper.c: a "video filter2/splitter" with mouse to "video filter" wrapper.
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_filter.h>
#include <vlc_video_splitter.h>

#include "filter_common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *, const char *psz_name, bool b_filter );
static void Close( vlc_object_t * );

#define DECLARE_OPEN(name,filter) \
    static int  Open##name ( vlc_object_t *p_this ) { return Open( p_this, #name, filter ); }

DECLARE_OPEN(magnify, true)
DECLARE_OPEN(puzzle, true)
DECLARE_OPEN(logo, true)

DECLARE_OPEN(clone, false)
DECLARE_OPEN(wall, false)
DECLARE_OPEN(panoramix, false)

#undef DECLARE_OPEN

#define DECLARE_MODULE(name)                            \
    set_description( "Video filter "#name" wrapper" )   \
    set_shortname( "Video filter"#name" wrapper" )      \
    set_capability( "video filter", 0 )                 \
    set_callbacks( Open##name, Close )                  \
    add_shortcut( #name )

vlc_module_begin()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    DECLARE_MODULE(magnify)

    add_submodule()
    DECLARE_MODULE(puzzle)

    add_submodule()
    DECLARE_MODULE(logo)

    add_submodule()
    DECLARE_MODULE(clone)

    add_submodule()
    DECLARE_MODULE(wall)

    add_submodule()
    DECLARE_MODULE(panoramix)

vlc_module_end()

#undef DECLARE_MODULE

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
static int Control    ( vout_thread_t *, int, va_list );

struct vout_sys_t
{
    int              i_vout;
    vout_thread_t    **pp_vout;

    es_format_t      fmt;

    vlc_mutex_t      lock;
    filter_chain_t   *p_chain;
    video_splitter_t *p_splitter;

    vlc_mouse_t      *p_mouse_src;
    vlc_mouse_t      mouse;
};

/* */
static int  FilterAllocationInit ( filter_t *, void * );
static void FilterAllocationClean( filter_t * );

/* */
static int  FullscreenEventUp( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int  FullscreenEventDown( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );
static int  SplitterPictureNew( video_splitter_t *, picture_t *pp_picture[] );
static void SplitterPictureDel( video_splitter_t *, picture_t *pp_picture[] );

/* */
static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static void VoutsClean( vout_thread_t *p_vout, int i_count );

/**
 * Open our wrapper instance.
 */
static int Open( vlc_object_t *p_this, const char *psz_name, bool b_filter )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;

    msg_Err( p_vout, "Opening video %s wrapper for %s",
             b_filter ? "filter" : "splitter", psz_name );

    /* */
    es_format_t fmt;
    es_format_Init( &fmt, VIDEO_ES, p_vout->render.i_chroma );
    video_format_Setup( &fmt.video, p_vout->render.i_chroma,
                        p_vout->render.i_width, p_vout->render.i_height,
                        p_vout->render.i_aspect * p_vout->render.i_height,
                        VOUT_ASPECT_FACTOR      * p_vout->render.i_width );

    /* Try to open our real module */
    filter_chain_t   *p_chain = NULL;
    video_splitter_t *p_splitter = NULL;
    if( b_filter )
    {
        p_chain = filter_chain_New( p_vout, "video filter2", false,
                                    FilterAllocationInit, FilterAllocationClean, p_vout );
        if( !p_chain )
            return VLC_ENOMEM;

        filter_chain_Reset( p_chain, &fmt, &fmt );

        filter_t *p_filter =
            filter_chain_AppendFilter( p_chain, psz_name, p_vout->p_cfg, &fmt, &fmt );

        if( !p_filter )
        {
            msg_Err( p_vout, "Failed to open filter '%s'", psz_name );
            filter_chain_Delete( p_chain );
            return VLC_EGENERIC;
        }
    }
    else
    {
        p_splitter = video_splitter_New( VLC_OBJECT(p_vout), psz_name, &fmt.video );
        if( !p_splitter )
        {
            msg_Err( p_vout, "Failed to open splitter '%s'", psz_name );
            return VLC_EGENERIC;
        }
        assert( p_splitter->i_output > 0 );

        p_splitter->p_owner = (video_splitter_owner_t*)p_vout;
        p_splitter->pf_picture_new = SplitterPictureNew;
        p_splitter->pf_picture_del = SplitterPictureDel;
    }

    /* */
    p_vout->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        goto error;

    p_sys->i_vout  = p_chain ? 1 : p_splitter->i_output;
    p_sys->pp_vout = calloc( p_sys->i_vout, sizeof(*p_sys->pp_vout) );;
    p_sys->p_mouse_src = calloc( p_sys->i_vout, sizeof(*p_sys->p_mouse_src) );

    p_sys->fmt = fmt;
    vlc_mutex_init( &p_sys->lock );
    p_sys->p_chain    = p_chain;
    p_sys->p_splitter = p_splitter;
    vlc_mouse_Init( &p_sys->mouse );
    for( int i = 0; i < p_sys->i_vout; i++ )
        vlc_mouse_Init( &p_sys->p_mouse_src[i] );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    return VLC_SUCCESS;

error:
    if( p_chain )
        filter_chain_Delete( p_chain );
    if( p_splitter )
        video_splitter_Delete( p_splitter );
    return VLC_ENOMEM;
}

/**
 * Close our wrapper instance
 */
static void Close( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    if( p_sys->p_chain )
        filter_chain_Delete( p_sys->p_chain );
    if( p_sys->p_splitter )
        video_splitter_Delete( p_sys->p_splitter );

    vlc_mutex_destroy( &p_sys->lock );
    es_format_Clean( &p_sys->fmt );
    free( p_sys->p_mouse_src );
    free( p_sys->pp_vout );

    free( p_vout->p_sys );
}

/**
 * Initialise our wrapper
 */
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    assert( p_vout->render.i_chroma == p_sys->fmt.video.i_chroma &&
            p_vout->render.i_width  == p_sys->fmt.video.i_width &&
            p_vout->render.i_height == p_sys->fmt.video.i_height );

    /* Initialize the output structure */
    I_OUTPUTPICTURES = 0;
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output(s)" );

    video_format_t fmt = p_vout->fmt_out;

    if( p_sys->p_chain )
    {
        p_sys->pp_vout[0] = vout_Create( p_vout, &fmt );
        if( !p_sys->pp_vout[0] )
        {
            msg_Err( p_vout, "cannot open vout, aborting" );
            return VLC_EGENERIC;
        }
        vout_filter_AddChild( p_vout, p_sys->pp_vout[0], MouseEvent );
    }
    else
    {
        video_splitter_t *p_splitter = p_sys->p_splitter;

        /* */
        const int i_org_align = var_CreateGetInteger( p_vout, "align" );
        const int i_org_x = var_CreateGetInteger( p_vout, "video-x" );
        const int i_org_y = var_CreateGetInteger( p_vout, "video-y" );
        const char *psz_org_vout = var_CreateGetNonEmptyString( p_vout, "vout" );

        /* */
        for( int i = 0; i < p_splitter->i_output; i++ )
        {
            const video_splitter_output_t *p_cfg = &p_splitter->p_output[i];

            /* */
            var_SetInteger( p_vout, "align", p_cfg->window.i_align);

            var_SetInteger( p_vout, "video-x", i_org_x + p_cfg->window.i_x );
            var_SetInteger( p_vout, "video-y", i_org_y + p_cfg->window.i_y );

            if( p_cfg->psz_module )
                var_SetString( p_vout, "vout", p_cfg->psz_module );

            /* */
            video_format_t fmt = p_cfg->fmt;
            p_sys->pp_vout[i] = vout_Create( p_vout, &fmt );
            if( !p_sys->pp_vout[i] )
            {
                msg_Err( p_vout, "cannot open vout, aborting" );
                VoutsClean( p_vout, i );
                return VLC_EGENERIC;
            }
        }

        /* Attach once pp_vout is completly field to avoid race conditions */
        for( int i = 0; i < p_splitter->i_output; i++ )
            vout_filter_SetupChild( p_vout, p_sys->pp_vout[i],
                                    MouseEvent,
                                    FullscreenEventUp, FullscreenEventDown, true );
        /* Restore settings */
        var_SetInteger( p_vout, "align", i_org_align );
        var_SetInteger( p_vout, "video-x", i_org_x );
        var_SetInteger( p_vout, "video-y", i_org_y );
        var_SetString( p_vout, "vout", psz_org_vout ? psz_org_vout : "" );
    }

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    return VLC_SUCCESS;
}

/**
 * Clean up our wrapper
 */
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    VoutsClean( p_vout, p_sys->i_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );
}

/**
 * Control the real vout
 */
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_ret = VLC_SUCCESS;

    for( int i = 0; i < p_sys->i_vout; i++ )
        i_ret = vout_vaControl( p_sys->pp_vout[i], i_query, args );
    return i_ret;
}

/**
 * Filter a picture
 */
static void Render( vout_thread_t *p_vout, picture_t *p_src )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vlc_mutex_lock( &p_sys->lock );

    picture_t *pp_dst[p_sys->i_vout];

    if( p_sys->p_chain )
    {
        pp_dst[0] = filter_chain_VideoFilter( p_sys->p_chain, p_src );
    }
    else
    {
        if( video_splitter_Filter( p_sys->p_splitter, pp_dst, p_src ) )
        {
            for( int i = 0; i < p_sys->i_vout; i++ )
                pp_dst[i] = NULL;
        }
    }
    for( int i = 0; i < p_sys->i_vout; i++ )
    {
        picture_t *p_dst = pp_dst[i];
        if( p_dst )
            vout_DisplayPicture( p_sys->pp_vout[i], p_dst );
    }

    vlc_mutex_unlock( &p_sys->lock );
}

/* */
static void VoutsClean( vout_thread_t *p_vout, int i_count )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    /* Detach all vouts before destroying them */
    for( int i = 0; i < i_count; i++ )
    {
        if( p_sys->p_chain )
            vout_filter_DelChild( p_vout, p_sys->pp_vout[i], MouseEvent );
        else
             vout_filter_SetupChild( p_vout, p_sys->pp_vout[i],
                                     MouseEvent,
                                     FullscreenEventUp, FullscreenEventDown, false );
    }

    for( int i = 0; i < i_count; i++ )
        vout_CloseAndRelease( p_sys->pp_vout[i] );
}
static int VoutsNewPicture( vout_thread_t *p_vout, picture_t *pp_dst[] )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    for( int i = 0; i < p_sys->i_vout; i++ )
    {
        picture_t *p_picture = NULL;
        for( ;; )
        {
            p_picture = vout_CreatePicture( p_sys->pp_vout[i], 0, 0, 0 );
            if( p_picture )
                break;

            if( !vlc_object_alive( p_vout ) || p_vout->b_error )
                break;
            msleep( VOUT_OUTMEM_SLEEP );
        }
        /* FIXME what to do with the allocated picture ? */
        if( !p_picture )
            return VLC_EGENERIC;

        pp_dst[i] = p_picture;
    }
    return VLC_SUCCESS;
}

/**
 * Callback for mouse events
 */
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    vout_thread_t *p_vout = p_data;
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_index;

    for( i_index = 0; i_index < p_sys->i_vout; i_index++ )
    {
        if( p_this == VLC_OBJECT(p_sys->pp_vout[i_index]) )
            break;
    }
    if( i_index >= p_sys->i_vout )
    {
        msg_Err( p_vout, "Failed to find vout source in MouseEvent" );
        return VLC_SUCCESS;
    }

    vout_thread_t *p_vout_src = p_sys->pp_vout[i_index];

    vlc_mouse_t m;
    vlc_mouse_Init( &m );
    m.i_x = var_GetInteger( p_vout_src, "mouse-x" );
    m.i_y = var_GetInteger( p_vout_src, "mouse-y" );
    m.i_pressed = var_GetInteger( p_vout_src, "mouse-button-down" );

    vlc_mutex_lock( &p_sys->lock );

    vlc_mouse_t nmouse;
    vlc_mouse_t omouse = p_sys->mouse;

    int i_ret;
    if( p_sys->p_chain )
    {
        i_ret = filter_chain_MouseFilter( p_sys->p_chain, &nmouse, &m );
    }
    else
    {
        vlc_mouse_t *p_mouse_src = &p_sys->p_mouse_src[i_index];

        i_ret = video_splitter_Mouse( p_sys->p_splitter, &nmouse, i_index, p_mouse_src, &m );
        *p_mouse_src = m;
    }

    if( !i_ret )
        p_sys->mouse = nmouse;
    vlc_mutex_unlock( &p_sys->lock );

    if( i_ret )
        return VLC_EGENERIC;

    if( vlc_mouse_HasMoved( &omouse, &nmouse ) )
    {
        var_SetInteger( p_vout, "mouse-x", nmouse.i_x );
        var_SetInteger( p_vout, "mouse-y", nmouse.i_y );
        var_SetBool( p_vout, "mouse-moved", true );
    }
    if( vlc_mouse_HasButton( &omouse, &nmouse ) )
    {
        var_SetInteger( p_vout, "mouse-button-down", nmouse.i_pressed );
        if( vlc_mouse_HasPressed( &omouse, &nmouse, MOUSE_BUTTON_LEFT ) )
            var_SetBool( p_vout, "mouse-clicked", true );
    }
    if( m.b_double_click )
    {
        /* Nothing with current API */
        msg_Warn( p_vout, "Ignoring double click" );
    }
    return VLC_SUCCESS;
}

/* -- Filter callbacks -- */

static picture_t *VideoBufferNew( filter_t *p_filter )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_filter->p_owner;

    picture_t *pp_picture[1];
    if( VoutsNewPicture( p_vout, pp_picture ) )
        return NULL;
    return pp_picture[0];
}
static void VideoBufferDelete( filter_t *p_filter, picture_t *p_picture )
{
    VLC_UNUSED(p_filter); VLC_UNUSED(p_picture);
    /* FIXME is there anything to do ? */
}

static int FilterAllocationInit( filter_t *p_filter, void *p_data )
{
    VLC_UNUSED( p_data );

    p_filter->pf_video_buffer_new = VideoBufferNew;
    p_filter->pf_video_buffer_del = VideoBufferDelete;
    p_filter->p_owner = p_data;

    return VLC_SUCCESS;
}
static void FilterAllocationClean( filter_t *p_filter )
{
    p_filter->pf_video_buffer_new = NULL;
    p_filter->pf_video_buffer_del = NULL;
}

/* -- Splitter callbacks -- */

/**
 * Forward fullscreen event to/from the childrens.
 *
 * FIXME probably unsafe (pp_vout[] content)
 */
static bool IsFullscreenActive( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    for( int i = 0; i < p_sys->i_vout; i++ )
    {
        if( var_GetBool( p_sys->pp_vout[i], "fullscreen" ) )
            return true;
    }
    return false;
}
static int FullscreenEventUp( vlc_object_t *p_this, char const *psz_var,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    VLC_UNUSED(oldval); VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(newval);

    const bool b_fullscreen = IsFullscreenActive( p_vout );
    if( !var_GetBool( p_vout, "fullscreen" ) != !b_fullscreen )
        return var_SetBool( p_vout, "fullscreen", b_fullscreen );
    return VLC_SUCCESS;
}
static int FullscreenEventDown( vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(psz_var);

    const bool b_fullscreen = IsFullscreenActive( p_vout );
    if( !b_fullscreen != !newval.b_bool )
    {
        for( int i = 0; i < p_sys->i_vout; i++ )
        {
            vout_thread_t *p_child = p_sys->pp_vout[i];
            if( !var_GetBool( p_child, "fullscreen" ) != !newval.b_bool )
            {
                var_SetBool( p_child, "fullscreen", newval.b_bool );
                if( newval.b_bool )
                    return VLC_SUCCESS;
            }
        }
    }
    return VLC_SUCCESS;
}

static int  SplitterPictureNew( video_splitter_t *p_splitter, picture_t *pp_picture[] )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_splitter->p_owner;

    return VoutsNewPicture( p_vout, pp_picture );
}
static void SplitterPictureDel( video_splitter_t *p_splitter, picture_t *pp_picture[] )
{
    VLC_UNUSED(p_splitter); VLC_UNUSED(pp_picture);
    /* FIXME is there anything to do ? */
}

