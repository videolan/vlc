/*****************************************************************************
 * opengl.c: OpenGL video output
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Eric Petit <titer@m0k.org>
 *          Cedric Cocquebert <cedric.cocquebert@supelec.fr>
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
#include "opengl.h"

/*****************************************************************************
 * Vout interface
 *****************************************************************************/
static int  CreateVout   ( vlc_object_t * );
static void DestroyVout  ( vlc_object_t * );
static int  Init         ( vout_thread_t * );
static void End          ( vout_thread_t * );
static int  Manage       ( vout_thread_t * );
static void Render       ( vout_thread_t *, picture_t * );
static void DisplayVideo ( vout_thread_t *, picture_t * );
static int  Control      ( vout_thread_t *, int, va_list );

static int SendEvents    ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

#define PROVIDER_TEXT N_("OpenGL Provider")
#define PROVIDER_LONGTEXT N_("Allows you to modify what OpenGL provider should be used")

vlc_module_begin ()
    set_shortname( "OpenGL" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_description( N_("OpenGL video output") )
#ifdef __APPLE__
    set_capability( "video output", 200 )
#else
    set_capability( "video output", 20 )
#endif
    add_shortcut( "opengl" )
    /* Allow opengl provider plugin selection */
    add_module( "opengl-provider", "opengl provider", NULL, NULL,
                PROVIDER_TEXT, PROVIDER_LONGTEXT, true )
    set_callbacks( CreateVout, DestroyVout )
vlc_module_end ()

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the OpenGL specific properties of the output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;
    vout_opengl_t gl;
    vout_display_opengl_t vgl;

    picture_pool_t *p_pool;
    picture_t *p_current;
};

/*****************************************************************************
 * CreateVout: This function allocates and initializes the OpenGL vout method.
 *****************************************************************************/
static int CreateVout( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;
    char * psz;

    /* Allocate structure */
    p_vout->p_sys = p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Get window */
    p_sys->p_vout =
        (vout_thread_t *)vlc_object_create( p_this, sizeof( vout_thread_t ) );
    if( p_sys->p_vout == NULL )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    vlc_object_attach( p_sys->p_vout, p_this );

    p_sys->p_vout->i_window_width = p_vout->i_window_width;
    p_sys->p_vout->i_window_height = p_vout->i_window_height;
    p_sys->p_vout->b_fullscreen = p_vout->b_fullscreen;
    p_sys->p_vout->render.i_width = p_vout->render.i_width;
    p_sys->p_vout->render.i_height = p_vout->render.i_height;
    p_sys->p_vout->render.i_aspect = p_vout->render.i_aspect;
    p_sys->p_vout->fmt_render = p_vout->fmt_render;
    p_sys->p_vout->fmt_in = p_vout->fmt_in;
    p_sys->p_vout->b_autoscale = p_vout->b_autoscale;
    p_sys->p_vout->i_zoom = p_vout->i_zoom;
    p_sys->p_vout->i_alignment = p_vout->i_alignment;
    var_Create( p_sys->p_vout, "video-deco",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Forward events from the opengl provider */
    var_Create( p_sys->p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_sys->p_vout, "mouse-clicked", VLC_VAR_BOOL );
    var_Create( p_sys->p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "video-on-top",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_sys->p_vout, "autoscale",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_sys->p_vout, "scale",
                VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );

    var_AddCallback( p_sys->p_vout, "mouse-x", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-y", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-moved", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-clicked", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-button-down", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "video-on-top", SendEvents, p_vout );
    var_AddCallback( p_vout, "autoscale", SendEvents, p_sys->p_vout );
    var_AddCallback( p_vout, "scale", SendEvents, p_sys->p_vout );

    psz = var_CreateGetString( p_vout, "opengl-provider" );
    p_sys->p_vout->p_module =
        module_need( p_sys->p_vout, "opengl provider", psz, false );
    free( psz );
    if( p_sys->p_vout->p_module == NULL )
    {
        msg_Warn( p_vout, "No OpenGL provider found" );
        /* no need for var_DelCallback here :-) */
        vlc_object_release( p_sys->p_vout );
        free( p_sys );
        return VLC_ENOOBJ;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_display = DisplayVideo;
    p_vout->pf_control = Control;

    return VLC_SUCCESS;
}

static int OpenglLock(vout_opengl_t *gl)
{
    vout_thread_t *p_vout = gl->sys;

    if( !p_vout->pf_lock )
        return VLC_SUCCESS;
    return p_vout->pf_lock( p_vout );
}
static void OpenglUnlock(vout_opengl_t *gl)
{
    vout_thread_t *p_vout = gl->sys;

    if( p_vout->pf_unlock )
        p_vout->pf_unlock( p_vout );
}
static void OpenglSwap(vout_opengl_t *gl)
{
    vout_thread_t *p_vout = gl->sys;
    p_vout->pf_swap( p_vout );
}

/*****************************************************************************
 * Init: initialize the OpenGL video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    p_sys->p_vout->pf_init( p_sys->p_vout );

    p_sys->gl.lock = OpenglLock;
    p_sys->gl.unlock = OpenglUnlock;
    p_sys->gl.swap = OpenglSwap;
    p_sys->gl.sys = p_sys->p_vout;

    video_format_t fmt;
    video_format_Init( &fmt, 0 );
    video_format_Setup( &fmt,
                        p_vout->render.i_chroma,
                        p_vout->render.i_width,
                        p_vout->render.i_height,
                        p_vout->render.i_aspect * p_vout->render.i_height,
                        VOUT_ASPECT_FACTOR      * p_vout->render.i_width );


    if( vout_display_opengl_Init( &p_sys->vgl, &fmt, &p_sys->gl ) )
    {
        I_OUTPUTPICTURES = 0;
        return VLC_EGENERIC;
    }
    p_sys->p_pool = vout_display_opengl_GetPool( &p_sys->vgl );
    if( !p_sys->p_pool )
    {
        vout_display_opengl_Clean( &p_sys->vgl );
        I_OUTPUTPICTURES = 0;
        return VLC_EGENERIC;
    }

    /* */
    p_vout->output.i_chroma = fmt.i_chroma;
    p_vout->output.i_rmask  = fmt.i_rmask;
    p_vout->output.i_gmask  = fmt.i_gmask;
    p_vout->output.i_bmask  = fmt.i_bmask;

    /* Since OpenGL can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    /* */
    p_sys->p_current = picture_pool_Get( p_sys->p_pool );
    p_vout->p_picture[0] = *p_sys->p_current;
    p_vout->p_picture[0].i_status = DESTROYED_PICTURE;
    p_vout->p_picture[0].i_type   = DIRECT_PICTURE;
    p_vout->p_picture[0].i_refcount = 0;
    p_vout->p_picture[0].p_sys = NULL;
    PP_OUTPUTPICTURE[0] = &p_vout->p_picture[0];

    I_OUTPUTPICTURES = 1;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate GLX video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    if( I_OUTPUTPICTURES > 0 )
    {

        if( p_sys->p_current )
            picture_Release( p_sys->p_current );
        vout_display_opengl_Clean( &p_sys->vgl );

        p_vout->p_picture[0].i_status = FREE_PICTURE;
        I_OUTPUTPICTURES = 0;
    }

    /* We must release the opengl provider here: opengl requiere init and end
       to be done in the same thread */
    module_unneed( p_sys->p_vout, p_sys->p_vout->p_module );
    vlc_object_release( p_sys->p_vout );
}

/*****************************************************************************
 * Destroy: destroy GLX video thread output method
 *****************************************************************************
 * Terminate an output method created by CreateVout
 *****************************************************************************/
static void DestroyVout( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occurred.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_ret, i_fullscreen_change;

    i_fullscreen_change = ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE );

    p_vout->fmt_out.i_x_offset = p_sys->p_vout->fmt_in.i_x_offset =
        p_vout->fmt_in.i_x_offset;
    p_vout->fmt_out.i_y_offset = p_sys->p_vout->fmt_in.i_y_offset =
        p_vout->fmt_in.i_y_offset;
    p_vout->fmt_out.i_visible_width = p_sys->p_vout->fmt_in.i_visible_width =
        p_vout->fmt_in.i_visible_width;
    p_vout->fmt_out.i_visible_height = p_sys->p_vout->fmt_in.i_visible_height =
        p_vout->fmt_in.i_visible_height;
    p_vout->fmt_out.i_sar_num = p_sys->p_vout->fmt_in.i_sar_num =
        p_vout->fmt_in.i_sar_num;
    p_vout->fmt_out.i_sar_den = p_sys->p_vout->fmt_in.i_sar_den =
        p_vout->fmt_in.i_sar_den;
    p_vout->output.i_aspect = (int64_t)p_vout->fmt_in.i_sar_num * p_vout->fmt_in.i_width * VOUT_ASPECT_FACTOR /
                              p_vout->fmt_in.i_sar_den / p_vout->fmt_in.i_height;

    p_sys->p_vout->i_changes = p_vout->i_changes;
    i_ret = p_sys->p_vout->pf_manage( p_sys->p_vout );
    p_vout->i_changes = p_sys->p_vout->i_changes;

#ifdef __APPLE__
    /* On OS X, we create the window and the GL view when entering
       fullscreen - the textures have to be inited again */
    if( i_fullscreen_change )
    {
        /* FIXME should we release p_current ? */
        vout_display_opengl_ResetTextures( &p_sys->vgl );
    }
#endif

// to align in real time in OPENGL
    if (p_sys->p_vout->i_alignment != p_vout->i_alignment)
    {
        p_vout->i_changes |= VOUT_CROP_CHANGE;        //to force change
        p_sys->p_vout->i_alignment = p_vout->i_alignment;    
    }

    /* forward signal that autoscale toggle has changed */
    if (p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_sys->p_vout->i_changes |= VOUT_SCALE_CHANGE;
    }

    /* forward signal that scale has changed */
    if (p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_sys->p_vout->i_changes |= VOUT_ZOOM_CHANGE;
    }


    return i_ret;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    picture_t *p_next = p_sys->p_current;

    if( VLCGL_TEXTURE_COUNT > 1 )
    {
        /* Get the next picture to display */
        p_next = picture_pool_Get( p_sys->p_pool );
        assert( p_next );
    }
  
    if( p_sys->p_current )
    {
        assert( p_sys->p_current->p[0].p_pixels == p_pic->p[0].p_pixels );

        /* Make sure we have the prepare after the picture_pool_Get,
         * because picture_pool_Get() will bind the new picture texture,
         * and vout_display_opengl_Prepare() bind the current rendered picture
         * texture.
         * DisplayVideo() will effectively use the last binded texture. */

        vout_display_opengl_Prepare( &p_sys->vgl, p_sys->p_current );
    }

    if( p_sys->p_current != p_next ) {
        if( p_sys->p_current )
            picture_Release( p_sys->p_current );
        
        /* Swap the picture texture on opengl vout side. */
        p_sys->p_current = p_next;

        /* Now, switch the only picture that is being used
         * to render in the backend to point to our "next"
         * picture texture */
        p_pic->p[0].p_pixels = p_sys->p_current->p[0].p_pixels;
    }

    VLC_UNUSED( p_pic );
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    vout_display_opengl_Display( &p_sys->vgl, &p_vout->fmt_out );
    VLC_UNUSED( p_pic );
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    if( p_sys->p_vout->pf_control )
        return p_sys->p_vout->pf_control( p_sys->p_vout, i_query, args );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *_p_vout )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    return var_Set( (vlc_object_t *)_p_vout, psz_var, newval );
}

