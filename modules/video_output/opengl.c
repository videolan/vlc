/*****************************************************************************
 * opengl.c: OpenGL video output
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifndef SYS_DARWIN
#include <GL/gl.h>
#else
/* Mac OS X < 10.3 does not have GL/gl.h */
#include <OpenGL/gl.h>
#endif

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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define EFFECT_TEXT N_("Select effect")
#define EFFECT_LONGTEXT N_( \
    "Allows you to select different visual effects.")

vlc_module_begin();
    set_description( _("OpenGL video output") );
    set_capability( "video output", 20 );
    add_shortcut( "opengl" );
    set_callbacks( CreateVout, DestroyVout );

    add_string( "opengl-effect", "none", NULL, EFFECT_TEXT,
                 EFFECT_LONGTEXT, VLC_TRUE );
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

    uint8_t     *p_buffer;
    int         i_index;
    int         i_tex_width;
    int         i_tex_height;
    GLuint      texture;

    int         i_effect;
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

    /* A texture must have a size aligned on a power of 2 */
    p_sys->i_tex_width  = GetAlignedSize( p_vout->render.i_width );
    p_sys->i_tex_height = GetAlignedSize( p_vout->render.i_height );

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
        msg_Err( p_vout, "No OpenGL provider found" );
        vlc_object_detach( p_sys->p_vout );
        vlc_object_destroy( p_sys->p_vout );
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
    vlc_value_t val;

    p_sys->p_vout->pf_init( p_sys->p_vout );

    /* No YUV textures :( */

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

    /* Since OpenGL can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_vout->p_picture[0].i_planes = 1;
    p_sys->p_buffer =
        malloc( p_sys->i_tex_width * p_sys->i_tex_height * i_pixel_pitch );
    if( !p_sys->p_buffer )
    {
        msg_Err( p_vout, "Out of memory" );
        return -1;
    }

    p_vout->p_picture[0].p->p_pixels = p_sys->p_buffer;
    p_vout->p_picture[0].p->i_lines = p_vout->output.i_height;
    p_vout->p_picture[0].p->i_pixel_pitch = i_pixel_pitch;
    p_vout->p_picture[0].p->i_pitch = p_sys->i_tex_width *
        p_vout->p_picture[0].p->i_pixel_pitch;
    p_vout->p_picture[0].p->i_visible_pitch = p_vout->output.i_width *
        p_vout->p_picture[0].p->i_pixel_pitch;

    p_vout->p_picture[0].i_status = DESTROYED_PICTURE;
    p_vout->p_picture[0].i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ 0 ] = &p_vout->p_picture[0];
    I_OUTPUTPICTURES = 1;

    /* Set the texture parameters */
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0 );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

    glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

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

    return 0;
}

/*****************************************************************************
 * End: terminate GLX video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    glFinish();
    glFlush();
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
    if( p_sys->p_buffer ) free( p_sys->p_buffer );

    free( p_sys );
}

/*****************************************************************************
 * Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occured.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    return p_sys->p_vout->pf_manage( p_sys->p_vout );
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    float f_width = (float)p_vout->output.i_width / p_sys->i_tex_width;
    float f_height = (float)p_vout->output.i_height / p_sys->i_tex_height;

    glClear( GL_COLOR_BUFFER_BIT );

    glTexImage2D( GL_TEXTURE_2D, 0, 3,
                  p_sys->i_tex_width, p_sys->i_tex_height , 0,
                  VLCGL_RGB_FORMAT, VLCGL_RGB_TYPE, p_sys->p_buffer );

    if( p_sys->i_effect == OPENGL_EFFECT_NONE )
    {
        glEnable( GL_TEXTURE_2D );
        glBegin( GL_POLYGON );
        glTexCoord2f( 0.0, 0.0 ); glVertex2f( -1.0, 1.0 );
        glTexCoord2f( f_width, 0.0 ); glVertex2f( 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex2f( 1.0, -1.0 );
        glTexCoord2f( 0.0, f_height ); glVertex2f( -1.0, -1.0 );
        glEnd();
    }
    else
    {
        glRotatef( 1.0, 0.3, 0.5, 0.7 );

        glEnable( GL_TEXTURE_2D );
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

    glDisable( GL_TEXTURE_2D);
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    p_sys->p_vout->pf_swap( p_sys->p_vout );
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
    else
        return vout_vaControlDefault( p_vout, i_query, args );
}
