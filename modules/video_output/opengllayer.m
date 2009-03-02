/*****************************************************************************
 * opengl.c: CAOpenGLLayer (Mac OS X) video output. Display a video output in
 * a layer. The layer will register itself to the drawable object stored in 
 * the "drawable" variable. 
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Eric Petit <titer@m0k.org>
 *          Cedric Cocquebert <cedric.cocquebert@supelec.fr>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <errno.h>                                                 /* ENOMEM */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>

#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>

/* On OS X, use GL_TEXTURE_RECTANGLE_EXT instead of GL_TEXTURE_2D.
   This allows sizes which are not powers of 2 */
#define VLCGL_TARGET GL_TEXTURE_RECTANGLE_EXT

/* OS X OpenGL supports YUV. Hehe. */
#define VLCGL_FORMAT GL_YCBCR_422_APPLE
#define VLCGL_TYPE   GL_UNSIGNED_SHORT_8_8_APPLE

/* RV32 */
#define VLCGL_RGB_FORMAT GL_RGBA
#define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE

/* YUY2 */
#ifndef YCBCR_MESA
#define YCBCR_MESA 0x8757
#endif
#ifndef UNSIGNED_SHORT_8_8_MESA
#define UNSIGNED_SHORT_8_8_MESA 0x85BA
#endif
#define VLCGL_YUV_FORMAT YCBCR_MESA
#define VLCGL_YUV_TYPE UNSIGNED_SHORT_8_8_MESA


#ifndef GL_CLAMP_TO_EDGE
#   define GL_CLAMP_TO_EDGE 0x812F
#endif

@interface VLCVideoView : NSObject
- (void)addVoutLayer:(CALayer *)layer;
@end

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

static int InitTextures  ( vout_thread_t * );

vlc_module_begin ()
    set_shortname( "OpenGLLayer" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_description( N_("Core Animation OpenGL Layer (Mac OS X)") )
    set_capability( "video output", 20 )
    add_shortcut( "opengllayer" )
    set_callbacks( CreateVout, DestroyVout )
vlc_module_end ()

@interface VLCVoutLayer : CAOpenGLLayer {
    vout_thread_t * p_vout;
}
+ (id)layerWithVout:(vout_thread_t*)_p_vout; 
@end

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the OpenGL specific properties of the output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t * p_vout;

    uint8_t    *pp_buffer[2]; /* one last rendered, one to be rendered */
    int         i_index;
    bool  b_frame_available;
    
    CGLContextObj glContext;

    int         i_tex_width;
    int         i_tex_height;
    GLuint      p_textures[2];

    NSAutoreleasePool *autorealease_pool;
    VLCVoutLayer * o_layer;
    id          o_cocoa_container;
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
    p_vout->p_sys = p_sys = calloc( 1, sizeof( vout_sys_t ) );
    if( p_sys == NULL )
        return VLC_EGENERIC;

    p_sys->i_tex_width  = p_vout->fmt_in.i_width;
    p_sys->i_tex_height = p_vout->fmt_in.i_height;

    msg_Dbg( p_vout, "Texture size: %dx%d", p_sys->i_tex_width,
             p_sys->i_tex_height );

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

#if ( defined( WORDS_BIGENDIAN ) && VLCGL_FORMAT == GL_YCBCR_422_APPLE ) || (VLCGL_FORMAT == YCBCR_MESA)
    p_vout->output.i_chroma = VLC_FOURCC('Y','U','Y','2');
    i_pixel_pitch = 2;
#elif (VLCGL_FORMAT == GL_YCBCR_422_APPLE)
    p_vout->output.i_chroma = VLC_FOURCC('U','Y','V','Y');
    i_pixel_pitch = 2;
#endif

    /* Since OpenGL can do rescaling for us, stick to the default
     * coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* We do need a drawable to work properly */
    vlc_value_t value_drawable;
    var_Create( p_vout, "drawable-gl", VLC_VAR_DOINHERIT );
    var_Get( p_vout, "drawable-gl", &value_drawable );

    p_vout->p_sys->o_cocoa_container = (id) value_drawable.i_int;
    
    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    /* We know the chroma, allocate two buffer which will be used
     * directly by the decoder */
    int i;
    for( i = 0; i < 2; i++ )
    {
        p_sys->pp_buffer[i] =
            malloc( p_sys->i_tex_width * p_sys->i_tex_height * i_pixel_pitch );
        if( !p_sys->pp_buffer[i] )
            return VLC_EGENERIC;
    }
    p_sys->b_frame_available = false;
    p_sys->i_index = 0;

    p_vout->p_picture[0].i_planes = 1;
    p_vout->p_picture[0].p->p_pixels = p_sys->pp_buffer[p_sys->i_index];
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
    p_sys->autorealease_pool = [[NSAutoreleasePool alloc] init];

    [VLCVoutLayer performSelectorOnMainThread:@selector(autoinitInVout:)
                             withObject:[NSValue valueWithPointer:p_vout]
                             waitUntilDone:YES];

    return 0;
}

/*****************************************************************************
 * End: terminate GLX video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    p_vout->p_sys->b_frame_available = false;

    [p_vout->p_sys->o_cocoa_container performSelectorOnMainThread:@selector(removeVoutLayer:) withObject:p_vout->p_sys->o_layer waitUntilDone:YES];

    // Should be done automatically
    [p_sys->o_layer release];
    [p_sys->autorealease_pool release];

    /* Free the texture buffer*/
    free( p_sys->pp_buffer[0] );
    free( p_sys->pp_buffer[1] );
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

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    @synchronized( p_sys->o_layer ) /* Make sure the p_sys->glContext isn't edited */
    {
        if( p_sys->glContext )
        {
            CGLLockContext(p_sys->glContext);
            CGLSetCurrentContext(p_sys->glContext);
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
            CGLUnlockContext(p_sys->glContext);
            
            p_sys->b_frame_available = true;
        }
    }

    /* Give a buffer where the image will be rendered */
    p_pic->p->p_pixels = p_sys->pp_buffer[p_sys->i_index];
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    
    [p_sys->o_layer performSelectorOnMainThread:@selector(display)
                    withObject:nil waitUntilDone:YES];
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
 * InitTextures
 *****************************************************************************/
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

        /* Note: It seems that we can't bypass those, and even
         * disabled they are used. They are the cause of the flickering */

        /* Tell the driver not to make a copy of the texture but to use
           our buffer */
        glEnable( GL_UNPACK_CLIENT_STORAGE_APPLE );
        glPixelStorei( GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE );

        /* Use AGP texturing */
        glTexParameteri( VLCGL_TARGET, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE );

        /* Call glTexImage2D only once, and use glTexSubImage2D later */
        glTexImage2D( VLCGL_TARGET, 0, 4, p_sys->i_tex_width,
                      p_sys->i_tex_height, 0, VLCGL_FORMAT, VLCGL_TYPE,
                      p_sys->pp_buffer[i_index] );
    }

    return 0;
}

/*****************************************************************************
 * @implementation VLCVoutLayer
 */
@implementation VLCVoutLayer

/*****************************************************************************
 * autoinitInVout: Called from the video thread to create a layer.
 * The created layer is stored in the p_vout. We do that way because, cocoa
 * doesn't support layer creation on non-main thread.
 *****************************************************************************/
+ (void)autoinitInVout:(NSValue*)arg
{
    vout_thread_t * p_vout = [arg pointerValue];
    p_vout->p_sys->o_layer = [[VLCVoutLayer layerWithVout:p_vout] retain];
    [p_vout->p_sys->o_cocoa_container addVoutLayer:p_vout->p_sys->o_layer];
}

+ (id)layerWithVout:(vout_thread_t*)_p_vout 
{
    VLCVoutLayer* me = [[[self alloc] init] autorelease];
    if( me )
    {
        me->p_vout = _p_vout;
        me.asynchronous = NO;
        me.bounds = CGRectMake( 0.0, 0.0, 
                                (float)_p_vout->fmt_in.i_visible_width * _p_vout->fmt_in.i_sar_num,
                                (float)_p_vout->fmt_in.i_visible_height * _p_vout->fmt_in.i_sar_den );
    }
    return me;
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    /* Only draw the frame if we have a frame that was previously rendered */
 	return p_vout->p_sys->b_frame_available; // Flag is cleared by drawInCGLContext:pixelFormat:forLayerTime:displayTime:
}

- (void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    CGLLockContext( glContext );
    CGLSetCurrentContext( glContext );

    float f_width, f_height, f_x, f_y;

    f_x = (float)p_vout->fmt_out.i_x_offset;
    f_y = (float)p_vout->fmt_out.i_y_offset;
    f_width = (float)p_vout->fmt_out.i_x_offset +
              (float)p_vout->fmt_out.i_visible_width;
    f_height = (float)p_vout->fmt_out.i_y_offset +
               (float)p_vout->fmt_out.i_visible_height;

    glClear( GL_COLOR_BUFFER_BIT );

    glEnable( VLCGL_TARGET );
    glBegin( GL_POLYGON );
    glTexCoord2f( f_x, f_y ); glVertex2f( -1.0, 1.0 );
    glTexCoord2f( f_width, f_y ); glVertex2f( 1.0, 1.0 );
    glTexCoord2f( f_width, f_height ); glVertex2f( 1.0, -1.0 );
    glTexCoord2f( f_x, f_height ); glVertex2f( -1.0, -1.0 );
    glEnd();

    glDisable( VLCGL_TARGET );

    glFlush();

    CGLUnlockContext( glContext );
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    CGLContextObj context = [super copyCGLContextForPixelFormat:pixelFormat];

    CGLLockContext( context );

    CGLSetCurrentContext( context );

    /* Swap buffers only during the vertical retrace of the monitor.
    http://developer.apple.com/documentation/GraphicsImaging/
    Conceptual/OpenGL/chap5/chapter_5_section_44.html */

    GLint params = 1;
    CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval,
                     &params );

    InitTextures( p_vout );

    glDisable( GL_BLEND );
    glDisable( GL_DEPTH_TEST );
    glDepthMask( GL_FALSE );
    glDisable( GL_CULL_FACE) ;
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    CGLUnlockContext( context );
    @synchronized( self )
    {
        p_vout->p_sys->glContext = context;
    }

    return context;
}

- (void)releaseCGLContext:(CGLContextObj)glContext
{
    @synchronized( self )
    {
        p_vout->p_sys->glContext = nil;
    }

    CGLLockContext( glContext );
    CGLSetCurrentContext( glContext );

    glDeleteTextures( 2, p_vout->p_sys->p_textures );

    CGLUnlockContext( glContext );
}
@end
