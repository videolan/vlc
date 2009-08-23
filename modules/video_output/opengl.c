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

#include <errno.h>                                                 /* ENOMEM */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>

#ifdef __APPLE__
# include <OpenGL/gl.h>
# include <OpenGL/glext.h>
#else
# include <GL/gl.h>
#endif

#ifndef YCBCR_MESA
# define YCBCR_MESA 0x8757
#endif
#ifndef UNSIGNED_SHORT_8_8_MESA
# define UNSIGNED_SHORT_8_8_MESA 0x85BA
#endif
/* RV16 */
#ifndef GL_UNSIGNED_SHORT_5_6_5
# define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifdef __APPLE__
/* On OS X, use GL_TEXTURE_RECTANGLE_EXT instead of GL_TEXTURE_2D.
   This allows sizes which are not powers of 2 */
# define VLCGL_TARGET GL_TEXTURE_RECTANGLE_EXT

/* OS X OpenGL supports YUV. Hehe. */
# define VLCGL_FORMAT GL_YCBCR_422_APPLE
# define VLCGL_TYPE   GL_UNSIGNED_SHORT_8_8_APPLE
#else

# define VLCGL_TARGET GL_TEXTURE_2D

/* RV32 */
# define VLCGL_RGB_FORMAT GL_RGBA
# define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE

/* YUY2 */
# define VLCGL_YUV_FORMAT YCBCR_MESA
# define VLCGL_YUV_TYPE UNSIGNED_SHORT_8_8_MESA

/* Use RGB on Win32/GLX */
# define VLCGL_FORMAT VLCGL_RGB_FORMAT
# define VLCGL_TYPE   VLCGL_RGB_TYPE
#endif


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

static inline int GetAlignedSize( int );

static int InitTextures  ( vout_thread_t * );
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

    uint8_t    *pp_buffer[2];
    int         i_index;
    int         i_tex_width;
    int         i_tex_height;
    GLuint      p_textures[2];
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

    p_sys->i_index = 0;
#ifdef __APPLE__
    p_sys->i_tex_width  = p_vout->fmt_in.i_width;
    p_sys->i_tex_height = p_vout->fmt_in.i_height;
#else
    /* A texture must have a size aligned on a power of 2 */
    p_sys->i_tex_width  = GetAlignedSize( p_vout->fmt_in.i_width );
    p_sys->i_tex_height = GetAlignedSize( p_vout->fmt_in.i_height );
#endif

    msg_Dbg( p_vout, "Texture size: %dx%d", p_sys->i_tex_width,
             p_sys->i_tex_height );

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
        vlc_object_detach( p_sys->p_vout );
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

/*****************************************************************************
 * Init: initialize the OpenGL video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_pixel_pitch;

    p_sys->p_vout->pf_init( p_sys->p_vout );

/* TODO: We use YCbCr on Mac which is Y422, but on OSX it seems to == YUY2. Verify */
#if ( defined( WORDS_BIGENDIAN ) && VLCGL_FORMAT == GL_YCBCR_422_APPLE ) || (VLCGL_FORMAT == YCBCR_MESA)
    p_vout->output.i_chroma = VLC_CODEC_YUYV;
    i_pixel_pitch = 2;

#elif defined( GL_YCBCR_422_APPLE ) && (VLCGL_FORMAT == GL_YCBCR_422_APPLE)
    p_vout->output.i_chroma = VLC_CODEC_UYVY;
    i_pixel_pitch = 2;

#elif VLCGL_FORMAT == GL_RGB
#   if VLCGL_TYPE == GL_UNSIGNED_BYTE
    p_vout->output.i_chroma = VLC_CODEC_RGB24;
#       if defined( WORDS_BIGENDIAN )
    p_vout->output.i_rmask = 0x00ff0000;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x000000ff;
#       else
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;
#       endif
    i_pixel_pitch = 3;
#   else
    p_vout->output.i_chroma = VLC_CODEC_RGB16;
#       if defined( WORDS_BIGENDIAN )
    p_vout->output.i_rmask = 0x001f;
    p_vout->output.i_gmask = 0x07e0;
    p_vout->output.i_bmask = 0xf800;
#       else
    p_vout->output.i_rmask = 0xf800;
    p_vout->output.i_gmask = 0x07e0;
    p_vout->output.i_bmask = 0x001f;
#       endif
    i_pixel_pitch = 2;
#   endif
#else
    p_vout->output.i_chroma = VLC_CODEC_RGB32;
#       if defined( WORDS_BIGENDIAN )
    p_vout->output.i_rmask = 0xff000000;
    p_vout->output.i_gmask = 0x00ff0000;
    p_vout->output.i_bmask = 0x0000ff00;
#       else
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;
#       endif
    i_pixel_pitch = 4;
#endif

    /* Since OpenGL can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    /* We know the chroma, allocate one buffer which will be used
     * directly by the decoder */
    p_sys->pp_buffer[0] =
        malloc( p_sys->i_tex_width * p_sys->i_tex_height * i_pixel_pitch );
    if( !p_sys->pp_buffer[0] )
        return -1;
    p_sys->pp_buffer[1] =
        malloc( p_sys->i_tex_width * p_sys->i_tex_height * i_pixel_pitch );
    if( !p_sys->pp_buffer[1] )
        return -1;

    p_vout->p_picture[0].i_planes = 1;
    p_vout->p_picture[0].p->p_pixels = p_sys->pp_buffer[0];
    p_vout->p_picture[0].p->i_lines = p_vout->output.i_height;
    p_vout->p_picture[0].p->i_visible_lines = p_vout->output.i_height;
    p_vout->p_picture[0].p->i_pixel_pitch = i_pixel_pitch;
    p_vout->p_picture[0].p->i_pitch = p_vout->output.i_width *
        p_vout->p_picture[0].p->i_pixel_pitch;
    p_vout->p_picture[0].p->i_visible_pitch = p_vout->output.i_width *
        p_vout->p_picture[0].p->i_pixel_pitch;

    p_vout->p_picture[0].i_status = DESTROYED_PICTURE;
    p_vout->p_picture[0].i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ 0 ] = &p_vout->p_picture[0];

    I_OUTPUTPICTURES = 1;

    if( p_sys->p_vout->pf_lock &&
        p_sys->p_vout->pf_lock( p_sys->p_vout ) )
    {
        msg_Warn( p_vout, "could not lock OpenGL provider" );
        return 0;
    }

    InitTextures( p_vout );

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
    }

    return 0;
}

/*****************************************************************************
 * End: terminate GLX video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    if( p_sys->p_vout->pf_lock &&
        p_sys->p_vout->pf_lock( p_sys->p_vout ) )
    {
        msg_Warn( p_vout, "could not lock OpenGL provider" );
        return;
    }

    glFinish();
    glFlush();

    /* Free the texture buffer*/
    glDeleteTextures( 2, p_sys->p_textures );
    free( p_sys->pp_buffer[0] );
    free( p_sys->pp_buffer[1] );

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
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
    p_vout->fmt_out.i_aspect = p_sys->p_vout->fmt_in.i_aspect =
        p_vout->fmt_in.i_aspect;
    p_vout->fmt_out.i_sar_num = p_sys->p_vout->fmt_in.i_sar_num =
        p_vout->fmt_in.i_sar_num;
    p_vout->fmt_out.i_sar_den = p_sys->p_vout->fmt_in.i_sar_den =
        p_vout->fmt_in.i_sar_den;
    p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;

    p_sys->p_vout->i_changes = p_vout->i_changes;
    i_ret = p_sys->p_vout->pf_manage( p_sys->p_vout );
    p_vout->i_changes = p_sys->p_vout->i_changes;

#ifdef __APPLE__
    if( p_sys->p_vout->pf_lock &&
        p_sys->p_vout->pf_lock( p_sys->p_vout ) )
    {
        msg_Warn( p_vout, "could not lock OpenGL provider" );
        return i_ret;
    }

    /* On OS X, we create the window and the GL view when entering
       fullscreen - the textures have to be inited again */
    if( i_fullscreen_change )
    {
        InitTextures( p_vout );
    }

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
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
    VLC_UNUSED(p_pic);
    vout_sys_t *p_sys = p_vout->p_sys;

    /* On Win32/GLX, we do this the usual way:
       + Fill the buffer with new content,
       + Reload the texture,
       + Use the texture.

       On OS X with VRAM or AGP texturing, the order has to be:
       + Reload the texture,
       + Fill the buffer with new content,
       + Use the texture.

       (Thanks to gcc from the Arstechnica forums for the tip)

       Therefore, we have to use two buffers and textures. On Win32/GLX,
       we reload the texture to be displayed and use it right away. On
       OS X, we first render, then reload the texture to be used next
       time. */

    if( p_sys->p_vout->pf_lock &&
        p_sys->p_vout->pf_lock( p_sys->p_vout ) )
    {
        msg_Warn( p_vout, "could not lock OpenGL provider" );
        return;
    }

#ifdef __APPLE__
    int i_new_index;
    i_new_index = ( p_sys->i_index + 1 ) & 1;


    /* Update the texture */
    glBindTexture( VLCGL_TARGET, p_sys->p_textures[i_new_index] );
    glTexSubImage2D( VLCGL_TARGET, 0, 0, 0,
                     p_vout->fmt_out.i_width,
                     p_vout->fmt_out.i_height,
                     VLCGL_FORMAT, VLCGL_TYPE, p_sys->pp_buffer[i_new_index] );

    /* Bind to the previous texture for drawing */
    glBindTexture( VLCGL_TARGET, p_sys->p_textures[p_sys->i_index] );

    /* Switch buffers */
    p_sys->i_index = i_new_index;
    p_pic->p->p_pixels = p_sys->pp_buffer[p_sys->i_index];

#else
    /* Update the texture */
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
                     p_vout->fmt_out.i_width,
                     p_vout->fmt_out.i_height,
                     VLCGL_FORMAT, VLCGL_TYPE, p_sys->pp_buffer[0] );
#endif

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
    }
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    VLC_UNUSED(p_pic);
    vout_sys_t *p_sys = p_vout->p_sys;
    float f_width, f_height, f_x, f_y;

    if( p_sys->p_vout->pf_lock &&
        p_sys->p_vout->pf_lock( p_sys->p_vout ) )
    {
        msg_Warn( p_vout, "could not lock OpenGL provider" );
        return;
    }

    /* glTexCoord works differently with GL_TEXTURE_2D and
       GL_TEXTURE_RECTANGLE_EXT */
#ifdef __APPLE__
    f_x = (float)p_vout->fmt_out.i_x_offset;
    f_y = (float)p_vout->fmt_out.i_y_offset;
    f_width = (float)p_vout->fmt_out.i_x_offset +
              (float)p_vout->fmt_out.i_visible_width;
    f_height = (float)p_vout->fmt_out.i_y_offset +
               (float)p_vout->fmt_out.i_visible_height;
#else
    f_x = (float)p_vout->fmt_out.i_x_offset / p_sys->i_tex_width;
    f_y = (float)p_vout->fmt_out.i_y_offset / p_sys->i_tex_height;
    f_width = ( (float)p_vout->fmt_out.i_x_offset +
                p_vout->fmt_out.i_visible_width ) / p_sys->i_tex_width;
    f_height = ( (float)p_vout->fmt_out.i_y_offset +
                 p_vout->fmt_out.i_visible_height ) / p_sys->i_tex_height;
#endif

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call pf_display to force redraw. Currently,
       the OS X provider uses it to get a smooth window resizing */

    glClear( GL_COLOR_BUFFER_BIT );

    glEnable( VLCGL_TARGET );
    glBegin( GL_POLYGON );
    glTexCoord2f( f_x, f_y ); glVertex2f( -1.0, 1.0 );
    glTexCoord2f( f_width, f_y ); glVertex2f( 1.0, 1.0 );
    glTexCoord2f( f_width, f_height ); glVertex2f( 1.0, -1.0 );
    glTexCoord2f( f_x, f_height ); glVertex2f( -1.0, -1.0 );
    glEnd();

    glDisable( VLCGL_TARGET );

    p_sys->p_vout->pf_swap( p_sys->p_vout );

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
    }
}

int GetAlignedSize( int i_size )
{
    /* Return the nearest power of 2 */
    int i_result = 1;
    while( i_result < i_size )
    {
        i_result *= 2;
    }
    return i_result;
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

static int InitTextures( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_index;

    glDeleteTextures( 2, p_sys->p_textures );
    glGenTextures( 2, p_sys->p_textures );

    for( i_index = 0; i_index < 2; i_index++ )
    {
        glBindTexture( VLCGL_TARGET, p_sys->p_textures[i_index] );

        /* Set the texture parameters */
        glTexParameterf( VLCGL_TARGET, GL_TEXTURE_PRIORITY, 1.0 );

        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

        glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

#ifdef __APPLE__
        /* Tell the driver not to make a copy of the texture but to use
           our buffer */
        glEnable( GL_UNPACK_CLIENT_STORAGE_APPLE );
        glPixelStorei( GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE );

#if 0
        /* Use VRAM texturing */
        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_STORAGE_HINT_APPLE,
                         GL_STORAGE_CACHED_APPLE );
#else
        /* Use AGP texturing */
        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_STORAGE_HINT_APPLE,
                         GL_STORAGE_SHARED_APPLE );
#endif
#endif

        /* Call glTexImage2D only once, and use glTexSubImage2D later */
        glTexImage2D( VLCGL_TARGET, 0, 3, p_sys->i_tex_width,
                      p_sys->i_tex_height, 0, VLCGL_FORMAT, VLCGL_TYPE,
                      p_sys->pp_buffer[i_index] );
    }

    return 0;
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
