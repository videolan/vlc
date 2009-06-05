/*****************************************************************************
 * wrapper.c: a "video filter2" with mouse to "video filter" wrapper.
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

#include "filter_common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *, const char *psz_name );
static void Close( vlc_object_t * );

#define DECLARE_OPEN(name) \
    static int  Open##name ( vlc_object_t *p_this ) { return Open( p_this, #name ); }

DECLARE_OPEN(magnify)
DECLARE_OPEN(puzzle)
DECLARE_OPEN(logo)

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
    vout_thread_t *p_vout;

    es_format_t    fmt;

    vlc_mutex_t    lock;
    filter_chain_t *p_chain;
    vlc_mouse_t    mouse;
};

static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

static int  FilterAllocationInit ( filter_t *, void * );
static void FilterAllocationClean( filter_t * );

/**
 * Open our wrapper instance.
 */
static int Open( vlc_object_t *p_this, const char *psz_name )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;

    msg_Err( p_vout, "Opening video filter wrapper for %s", psz_name );

    /* Try to open our filter */
    filter_chain_t *p_chain =
        filter_chain_New( p_vout, "video filter2", false,
                          FilterAllocationInit, FilterAllocationClean, p_vout );
    if( !p_chain )
        return VLC_ENOMEM;

    es_format_t fmt;
    es_format_Init( &fmt, VIDEO_ES, p_vout->render.i_chroma );
    video_format_Setup( &fmt.video, p_vout->render.i_chroma,
                        p_vout->render.i_width, p_vout->render.i_height,
                        p_vout->render.i_aspect );

    filter_chain_Reset( p_chain, &fmt, &fmt );

    filter_t *p_filter =
        filter_chain_AppendFilter( p_chain, psz_name, p_vout->p_cfg, &fmt, &fmt );

    if( !p_filter )
    {
        msg_Err( p_vout, "Failed to open filter '%s'", psz_name );
        filter_chain_Delete( p_chain );
        return VLC_EGENERIC;
    }

    p_vout->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
    {
        filter_chain_Delete( p_chain );
        return VLC_ENOMEM;
    }
    p_sys->fmt = fmt;
    vlc_mutex_init( &p_sys->lock );
    p_sys->p_chain = p_chain;
    p_sys->p_vout = NULL;
    vlc_mouse_Init( &p_sys->mouse );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    return VLC_SUCCESS;
}

/**
 * Close our wrapper instance
 */
static void Close( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    filter_chain_Delete( p_sys->p_chain );
    vlc_mutex_destroy( &p_sys->lock );
    es_format_Clean( &p_sys->fmt );

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
    msg_Dbg( p_vout, "spawning the real video output" );

    video_format_t fmt = p_vout->fmt_out;
    p_sys->p_vout = vout_Create( p_vout, &fmt );
    if( !p_sys->p_vout )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );
        return VLC_EGENERIC;
    }

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    vout_filter_AddChild( p_vout, p_vout->p_sys->p_vout, MouseEvent );

    return VLC_SUCCESS;
}

/**
 * Clean up our wrapper
 */
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vout_filter_DelChild( p_vout, p_sys->p_vout, MouseEvent );
    vout_CloseAndRelease( p_sys->p_vout );

    vout_filter_ReleaseDirectBuffers( p_vout );
}

/**
 * Control the real vout
 */
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/**
 * Filter a picture
 */
static void Render( vout_thread_t *p_vout, picture_t *p_src )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    picture_t *p_dst = filter_chain_VideoFilter( p_sys->p_chain, p_src );
    if( p_dst )
        vout_DisplayPicture( p_sys->p_vout, p_dst );
    vlc_mutex_unlock( &p_sys->lock );
}

/**
 * Callback for mouse events
 */
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    vout_sys_t *p_sys = p_vout->p_sys;

    vlc_mouse_t m;
    vlc_mouse_Init( &m );
    m.i_x = var_GetInteger( p_sys->p_vout, "mouse-x" );
    m.i_y = var_GetInteger( p_sys->p_vout, "mouse-y" );
    m.i_pressed = var_GetInteger( p_sys->p_vout, "mouse-button-down" );

    vlc_mutex_lock( &p_sys->lock );

    vlc_mouse_t nmouse;
    vlc_mouse_t omouse = p_sys->mouse;

    int i_ret = filter_chain_MouseFilter( p_sys->p_chain, &nmouse, &m );
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

/* */
static picture_t *VideoBufferNew( filter_t *p_filter )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_filter->p_owner;
    vout_sys_t *p_sys = p_vout->p_sys;

    picture_t *p_picture;
    for( ;; )
    {
        p_picture = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 );
        if( p_picture )
            return p_picture;

        if( !vlc_object_alive( p_vout ) || p_vout->b_error )
            return NULL;
        msleep( VOUT_OUTMEM_SLEEP );
    }
}
static void VideoBufferDelete( filter_t *p_filter, picture_t *p_picture )
{
    VLC_UNUSED(p_filter); VLC_UNUSED(p_picture);
    /* FIXME is there anything to do ? */
}

static int FilterAllocationInit( filter_t *p_filter, void *p_data )
{
    VLC_UNUSED( p_data );

    p_filter->pf_vout_buffer_new = VideoBufferNew;
    p_filter->pf_vout_buffer_del = VideoBufferDelete;
    p_filter->p_owner = p_data;

    return VLC_SUCCESS;
}
static void FilterAllocationClean( filter_t *p_filter )
{
    p_filter->pf_vout_buffer_new = NULL;
    p_filter->pf_vout_buffer_del = NULL;
}

