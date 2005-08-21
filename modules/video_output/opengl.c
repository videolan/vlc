/*****************************************************************************
 * opengl.c: OpenGL video output
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Eric Petit <titer@m0k.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#ifdef SYS_DARWIN
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

/* On OS X, use GL_TEXTURE_RECTANGLE_EXT instead of GL_TEXTURE_2D.
   This allows sizes which are not powers of 2 */
#define VLCGL_TARGET GL_TEXTURE_RECTANGLE_EXT

/* OS X OpenGL supports YUV. Hehe. */
#define VLCGL_FORMAT GL_YCBCR_422_APPLE
#define VLCGL_TYPE   GL_UNSIGNED_SHORT_8_8_APPLE
#else

#include <GL/gl.h>
#define VLCGL_TARGET GL_TEXTURE_2D

/* RV16 */
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
//#define VLCGL_RGB_FORMAT GL_RGB
//#define VLCGL_RGB_TYPE GL_UNSIGNED_SHORT_5_6_5

/* RV24 */
//#define VLCGL_RGB_FORMAT GL_RGB
//#define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE

/* RV32 */
#define VLCGL_RGB_FORMAT GL_RGBA
#define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE

/* Use RGB on Win32/GLX */
#define VLCGL_FORMAT VLCGL_RGB_FORMAT
#define VLCGL_TYPE   VLCGL_RGB_TYPE
#endif

#ifndef GL_CLAMP_TO_EDGE
#   define GL_CLAMP_TO_EDGE 0x812F
#endif

/* OpenGL effects */
#define OPENGL_EFFECT_NONE             1
#define OPENGL_EFFECT_CUBE             2
#define OPENGL_EFFECT_TRANSPARENT_CUBE 4

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

static int InitTextures( vout_thread_t * );
static int SendEvents( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SPEED_TEXT N_( "OpenGL cube rotation speed" )
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SPEED_TEXT N_( "OpenGL cube rotation speed" )
#define SPEED_LONGTEXT N_( "If the OpenGL cube effect is enabled, this " \
                           "controls its rotation speed." )

#define EFFECT_TEXT N_("Select effect")
#define EFFECT_LONGTEXT N_( \
    "Allows you to select different visual effects.")

static char *ppsz_effects[] = {
        "none", "cube", "transparent-cube" };
static char *ppsz_effects_text[] = {
        N_("None"), N_("Cube"), N_("Transparent Cube") };

vlc_module_begin();
    set_shortname( "OpenGL" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    set_description( _("OpenGL video output") );
#ifdef SYS_DARWIN
    set_capability( "video output", 200 );
#else
    set_capability( "video output", 20 );
#endif
    add_shortcut( "opengl" );
    add_float( "opengl-cube-speed", 2.0, NULL, SPEED_TEXT,
                    SPEED_LONGTEXT, VLC_TRUE );
    set_callbacks( CreateVout, DestroyVout );
    add_string( "opengl-effect", "none", NULL, EFFECT_TEXT,
                 EFFECT_LONGTEXT, VLC_FALSE );
        change_string_list( ppsz_effects, ppsz_effects_text, 0 );
vlc_module_end();

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

    int         i_effect;

    float       f_speed;
};

/*****************************************************************************
 * CreateVout: This function allocates and initializes the OpenGL vout method.
 *****************************************************************************/
static int CreateVout( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;

    /* Allocate structure */
    p_vout->p_sys = p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_EGENERIC;
    }

    var_Create( p_vout, "opengl-effect", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    p_sys->i_index = 0;
#ifdef SYS_DARWIN
    p_sys->i_tex_width  = p_vout->render.i_width;
    p_sys->i_tex_height = p_vout->render.i_height;
#else
    /* A texture must have a size aligned on a power of 2 */
    p_sys->i_tex_width  = GetAlignedSize( p_vout->render.i_width );
    p_sys->i_tex_height = GetAlignedSize( p_vout->render.i_height );
#endif

    msg_Dbg( p_vout, "Texture size: %dx%d", p_sys->i_tex_width,
             p_sys->i_tex_height );

    /* Get window */
    p_sys->p_vout =
        (vout_thread_t *)vlc_object_create( p_this, VLC_OBJECT_OPENGL );
    if( p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }
    vlc_object_attach( p_sys->p_vout, p_this );

    p_sys->p_vout->i_window_width = p_vout->i_window_width;
    p_sys->p_vout->i_window_height = p_vout->i_window_height;
    p_sys->p_vout->b_fullscreen = p_vout->b_fullscreen;
    p_sys->p_vout->render.i_width = p_vout->render.i_width;
    p_sys->p_vout->render.i_height = p_vout->render.i_height;
    p_sys->p_vout->render.i_aspect = p_vout->render.i_aspect;
    p_sys->p_vout->b_scale = p_vout->b_scale;
    p_sys->p_vout->i_alignment = p_vout->i_alignment;

    p_sys->p_vout->p_module =
        module_Need( p_sys->p_vout, "opengl provider", NULL, 0 );
    if( p_sys->p_vout->p_module == NULL )
    {
        msg_Warn( p_vout, "No OpenGL provider found" );
        vlc_object_detach( p_sys->p_vout );
        vlc_object_destroy( p_sys->p_vout );
        return VLC_ENOOBJ;
    }

    p_sys->f_speed = var_CreateGetFloat( p_vout, "opengl-cube-speed" );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_display = DisplayVideo;
    p_vout->pf_control = Control;

    /* Forward events from the opengl provider */
    var_Create( p_sys->p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_sys->p_vout, "mouse-clicked", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_sys->p_vout, "video-on-top",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    var_AddCallback( p_sys->p_vout, "mouse-x", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-y", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-moved", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-clicked", SendEvents, p_vout );
    var_AddCallback( p_sys->p_vout, "mouse-button-down", SendEvents, p_vout );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize the OpenGL video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_pixel_pitch;
    vlc_value_t val;

    p_sys->p_vout->pf_init( p_sys->p_vout );

#ifdef SYS_DARWIN
    p_vout->output.i_chroma = VLC_FOURCC('Y','U','Y','2');
    p_vout->output.i_rmask = 0x00ff0000;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x000000ff;
    i_pixel_pitch = 2;
#else
#if VLCGL_RGB_FORMAT == GL_RGB
#   if VLCGL_RGB_TYPE == GL_UNSIGNED_BYTE
    p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4');
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;
    i_pixel_pitch = 3;
#   else
    p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
    p_vout->output.i_rmask = 0xf800;
    p_vout->output.i_gmask = 0x07e0;
    p_vout->output.i_bmask = 0x001f;
    i_pixel_pitch = 2;
#   endif
#else
    p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
    p_vout->output.i_rmask = 0x000000ff;
    p_vout->output.i_gmask = 0x0000ff00;
    p_vout->output.i_bmask = 0x00ff0000;
    i_pixel_pitch = 4;
#endif
#endif

    /* Since OpenGL can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* We know the chroma, allocate one buffer which will be used
     * directly by the decoder */
    p_sys->pp_buffer[0] =
        malloc( p_sys->i_tex_width * p_sys->i_tex_height * i_pixel_pitch );
    if( !p_sys->pp_buffer[0] )
    {
        msg_Err( p_vout, "Out of memory" );
        return -1;
    }
    p_sys->pp_buffer[1] =
        malloc( p_sys->i_tex_width * p_sys->i_tex_height * i_pixel_pitch );
    if( !p_sys->pp_buffer[1] )
    {
        msg_Err( p_vout, "Out of memory" );
        return -1;
    }

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
    glClear( GL_COLOR_BUFFER_BIT );

    /* Check if the user asked for useless visual effects */
    var_Get( p_vout, "opengl-effect", &val );
    if( !val.psz_string || !strcmp( val.psz_string, "none" ))
    {
        p_sys->i_effect = OPENGL_EFFECT_NONE;
    }
    else if( !strcmp( val.psz_string, "cube" ) )
    {
        p_sys->i_effect = OPENGL_EFFECT_CUBE;

        glEnable( GL_CULL_FACE);
        //glEnable( GL_DEPTH_TEST );
    }
    else if( !strcmp( val.psz_string, "transparent-cube" ) )
    {
        p_sys->i_effect = OPENGL_EFFECT_TRANSPARENT_CUBE;

        glDisable( GL_DEPTH_TEST );
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE );
    }
    else
    {
        msg_Warn( p_vout, "no valid opengl effect provided, using "
                  "\"none\"" );
        p_sys->i_effect = OPENGL_EFFECT_NONE;
    }
    if( val.psz_string ) free( val.psz_string );

    if( p_sys->i_effect & ( OPENGL_EFFECT_CUBE |
                OPENGL_EFFECT_TRANSPARENT_CUBE ) )
    {
        /* Set the perpective */
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity();
        glFrustum( -1.0, 1.0, -1.0, 1.0, 3.0, 20.0 );
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
        glTranslatef( 0.0, 0.0, - 5.0 );
    }

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

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
    }
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

    module_Unneed( p_sys->p_vout, p_sys->p_vout->p_module );
    vlc_object_detach( p_sys->p_vout );
    vlc_object_destroy( p_sys->p_vout );

    /* Free the texture buffer*/
    if( p_sys->pp_buffer[0] ) free( p_sys->pp_buffer[0] );
    if( p_sys->pp_buffer[1] ) free( p_sys->pp_buffer[1] );

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

    p_sys->p_vout->i_changes = p_vout->i_changes;
    i_ret = p_sys->p_vout->pf_manage( p_sys->p_vout );
    p_vout->i_changes = p_sys->p_vout->i_changes;

#ifdef SYS_DARWIN
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

        switch( p_sys->i_effect )
        {
            case OPENGL_EFFECT_CUBE:
                glEnable( GL_CULL_FACE );
                break;

            case OPENGL_EFFECT_TRANSPARENT_CUBE:
                glDisable( GL_DEPTH_TEST );
                glEnable( GL_BLEND );
                glBlendFunc( GL_SRC_ALPHA, GL_ONE );
                break;
        }

        if( p_sys->i_effect & ( OPENGL_EFFECT_CUBE |
                    OPENGL_EFFECT_TRANSPARENT_CUBE ) )
        {
            /* Set the perpective */
            glMatrixMode( GL_PROJECTION );
            glLoadIdentity();
            glFrustum( -1.0, 1.0, -1.0, 1.0, 3.0, 20.0 );
            glMatrixMode( GL_MODELVIEW );
            glLoadIdentity();
            glTranslatef( 0.0, 0.0, - 5.0 );
        }
    }

    if( p_sys->p_vout->pf_unlock )
    {
        p_sys->p_vout->pf_unlock( p_sys->p_vout );
    }
#endif

    return i_ret;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
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

#ifdef SYS_DARWIN
    int i_new_index;
    i_new_index = ( p_sys->i_index + 1 ) & 1;


    /* Update the texture */
    glBindTexture( VLCGL_TARGET, p_sys->p_textures[i_new_index] );
    glTexSubImage2D( VLCGL_TARGET, 0, 0, 0, p_sys->i_tex_width,
                     p_sys->i_tex_height, VLCGL_FORMAT, VLCGL_TYPE,
                     p_sys->pp_buffer[i_new_index] );

    /* Bind to the previous texture for drawing */
    glBindTexture( VLCGL_TARGET, p_sys->p_textures[p_sys->i_index] );

    /* Switch buffers */
    p_sys->i_index = i_new_index;
    p_pic->p->p_pixels = p_sys->pp_buffer[p_sys->i_index];

#else
    /* Update the texture */
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
                     p_vout->render.i_width, p_vout->render.i_height,
                     VLCGL_RGB_FORMAT, VLCGL_RGB_TYPE, p_sys->pp_buffer[0] );
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
    vout_sys_t *p_sys = p_vout->p_sys;
    float f_width, f_height;

    if( p_sys->p_vout->pf_lock &&
        p_sys->p_vout->pf_lock( p_sys->p_vout ) )
    {
        msg_Warn( p_vout, "could not lock OpenGL provider" );
        return;
    }

    /* glTexCoord works differently with GL_TEXTURE_2D and
       GL_TEXTURE_RECTANGLE_EXT */
#ifdef SYS_DARWIN
    f_width = (float)p_vout->output.i_width;
    f_height = (float)p_vout->output.i_height;
#else
    f_width = (float)p_vout->output.i_width / p_sys->i_tex_width;
    f_height = (float)p_vout->output.i_height / p_sys->i_tex_height;
#endif

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call pf_display to force redraw. Currently,
       the OS X provider uses it to get a smooth window resizing */

    glClear( GL_COLOR_BUFFER_BIT );

    if( p_sys->i_effect == OPENGL_EFFECT_NONE )
    {
        glEnable( VLCGL_TARGET );
        glBegin( GL_POLYGON );
        glTexCoord2f( 0.0, 0.0 ); glVertex2f( -1.0, 1.0 );
        glTexCoord2f( f_width, 0.0 ); glVertex2f( 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex2f( 1.0, -1.0 );
        glTexCoord2f( 0.0, f_height ); glVertex2f( -1.0, -1.0 );
        glEnd();
    }
    else
    {
        glRotatef( 0.5 * p_sys->f_speed , 0.3, 0.5, 0.7 );

        glEnable( VLCGL_TARGET );
        glBegin( GL_QUADS );

        /* Front */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, 1.0, 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, 1.0, 1.0 );

        /* Left */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, 1.0, - 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( - 1.0, 1.0, 1.0 );

        /* Back */
        glTexCoord2f( 0, 0 ); glVertex3f( 1.0, 1.0, - 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( - 1.0, 1.0, - 1.0 );

        /* Right */
        glTexCoord2f( 0, 0 ); glVertex3f( 1.0, 1.0, 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( 1.0, - 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, 1.0, - 1.0 );

        /* Top */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, 1.0, - 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, 1.0, 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, 1.0, - 1.0 );

        /* Bottom */
        glTexCoord2f( 0, 0 ); glVertex3f( - 1.0, - 1.0, 1.0 );
        glTexCoord2f( 0, f_height ); glVertex3f( - 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex3f( 1.0, - 1.0, - 1.0 );
        glTexCoord2f( f_width, 0 ); glVertex3f( 1.0, - 1.0, 1.0 );
        glEnd();
    }

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

    switch( i_query )
    {
    case VOUT_SNAPSHOT:
        return vout_vaControlDefault( p_vout, i_query, args );

    default:
        if( p_sys->p_vout->pf_control )
            return p_sys->p_vout->pf_control( p_sys->p_vout, i_query, args );
        else
            return vout_vaControlDefault( p_vout, i_query, args );
    }
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

#ifdef SYS_DARWIN
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
    return var_Set( (vlc_object_t *)_p_vout, psz_var, newval );
}
