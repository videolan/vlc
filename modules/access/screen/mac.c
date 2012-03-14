/*****************************************************************************
 * mac.c: Screen capture module for the Mac.
 *****************************************************************************
 * Copyright (C) 2004, 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          arai <arai_a@mac.com>
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
# import "config.h"
#endif

#import <vlc_common.h>

// Fix ourselves ColorSync headers that gets included in ApplicationServices.
#define DisposeCMProfileIterateUPP(a) DisposeCMProfileIterateUPP(CMProfileIterateUPP userUPP __attribute__((unused)))
#define DisposeCMMIterateUPP(a) DisposeCMMIterateUPP(CMProfileIterateUPP userUPP __attribute__((unused)))
#define __MACHINEEXCEPTIONS__
#import <ApplicationServices/ApplicationServices.h>

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <stdlib.h>

typedef int CGSConnectionRef;
extern CGError CGSNewConnection( void *, CGSConnectionRef * );
extern CGError CGSReleaseConnection( CGSConnectionRef );
extern CGError CGSGetGlobalCursorDataSize( CGSConnectionRef, int * );
extern CGError CGSGetGlobalCursorData( CGSConnectionRef, unsigned char *,
                                       int *, int *, CGRect *, CGPoint *,
                                       int *, int *, int * );
extern CGError CGSGetCurrentCursorLocation( CGSConnectionRef, CGPoint * );
extern int CGSCurrentCursorSeed( void );

typedef CGImageRef ( *typeofCGDisplayCreateImageForRect )( CGDirectDisplayID displayID, CGRect rect );

#import "screen.h"

struct screen_data_t
{
    CGLContextObj screen;
    char *screen_image;

    CGLContextObj clipped;
    char *clipped_image;

    GLuint cursor_texture;
    int cursor_seed;
    bool cursor_need_update;
    CGRect cursor_rect;
    CGPoint cursor_hot;
    double cursor_texture_map_u, cursor_texture_map_v;

    int width;
    int height;

    int screen_top;
    int screen_left;
    int screen_width;
    int screen_height;

    CGSConnectionRef connection;

    CFBundleRef bundle;

    typeofCGDisplayCreateImageForRect myCGDisplayCreateImageForRect;

    CGDirectDisplayID display_id;
};

CGLError screen_CreateContext( CGLContextObj *context,
                               CGLPixelFormatAttribute a0,
                               CGLPixelFormatAttribute a1,
                               CGLPixelFormatAttribute a2,
                               CGLPixelFormatAttribute a3 );
int screen_DrawCursor( demux_sys_t *p_sys, CGPoint *cursor_pos );
int screen_CaptureScreen( demux_sys_t *p_sys );

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t   *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    CGLError returnedError;
    unsigned int i;

    p_sys->p_data = p_data =
        ( screen_data_t * )calloc( 1, sizeof( screen_data_t ) );

    p_data->display_id = kCGDirectMainDisplay;

    unsigned int displayCount;
    displayCount = 0;
    returnedError = CGGetOnlineDisplayList( 0, NULL, &displayCount );
    if( !returnedError )
    {
        CGDirectDisplayID *ids;
        ids = ( CGDirectDisplayID * )malloc( displayCount * sizeof( CGDirectDisplayID ) );
        returnedError = CGGetOnlineDisplayList( displayCount, ids, &displayCount );
        if( !returnedError )
        {
            if ( p_sys->i_display_id > 0 )
            {
                for( i = 0; i < displayCount; i ++ )
                {
                    if( p_sys->i_display_id == ids[i] )
                    {
                        p_data->display_id = ids[i];
                        break;
                    }
                }
            }
            else if ( p_sys->i_screen_index > 0 && p_sys->i_screen_index <= displayCount )
            {
                p_data->display_id = ids[p_sys->i_screen_index - 1];
            }
        }
        free( ids );
    }

    /* CGImage Function
     *   CGDisplayCreateImageForRect is available in Mac OS X v10.6 and later */

    p_data->myCGDisplayCreateImageForRect = NULL;

    CFURLRef frameworkURL = NULL;
    CFStringRef path = CFSTR( "file://localhost/System/Library/Frameworks/ApplicationServices.framework/Frameworks/CoreGraphics.framework" );
    frameworkURL = CFURLCreateWithString( kCFAllocatorDefault, path, NULL );
    if( frameworkURL != NULL )
    {
        p_data->bundle = CFBundleCreate( kCFAllocatorDefault, frameworkURL );
        if( p_data->bundle != NULL )
        {
            p_data->myCGDisplayCreateImageForRect =
                ( typeofCGDisplayCreateImageForRect )CFBundleGetFunctionPointerForName
                ( p_data->bundle, CFSTR( "CGDisplayCreateImageForRect" ) );
        }

        CFRelease( frameworkURL );
    }

    /* Screen Size */

    CGRect rect = CGDisplayBounds( p_data->display_id );
    p_data->screen_left = rect.origin.x;
    p_data->screen_top = rect.origin.y;
    p_data->screen_width = rect.size.width;
    p_data->screen_height = rect.size.height;

    p_data->width = p_sys->i_width;
    p_data->height = p_sys->i_height;
    if( p_data->width <= 0 || p_data->height <= 0 )
    {
        p_data->width = p_data->screen_width;
        p_data->height = p_data->screen_height;
    }

    /* Screen Context */

    if( p_data->myCGDisplayCreateImageForRect == NULL )
    {
        returnedError =
            screen_CreateContext( &p_data->screen,
                                  kCGLPFAFullScreen,
                                  kCGLPFADisplayMask,
                                  ( CGLPixelFormatAttribute )CGDisplayIDToOpenGLDisplayMask( p_data->display_id ),
                                  ( CGLPixelFormatAttribute )0 );
        if( returnedError )
            goto errorHandling;

        returnedError = CGLSetCurrentContext( p_data->screen );
        if( returnedError )
            goto errorHandling;

        returnedError = CGLSetFullScreen( p_data->screen );
        if( returnedError )
            goto errorHandling;
    }

    /* Clipped Context */

    returnedError =
        screen_CreateContext( &p_data->clipped,
                              kCGLPFAOffScreen,
                              kCGLPFAColorSize,
                              ( CGLPixelFormatAttribute )32,
                              ( CGLPixelFormatAttribute )0 );
    if( returnedError )
        goto errorHandling;

    returnedError = CGLSetCurrentContext( p_data->clipped );
    if( returnedError )
        goto errorHandling;

    /* Clipped Image */

    p_data->clipped_image =
        ( char * )malloc( p_data->width * p_data->height * 4 );

    returnedError = CGLSetOffScreen( p_data->clipped, p_data->width, p_data->height, p_data->width * 4, p_data->clipped_image );
    if( returnedError )
        goto errorHandling;

    /* Screen Image */

    if( p_data->myCGDisplayCreateImageForRect != NULL )
    {
        p_data->screen_image =
            ( char * )malloc( p_data->screen_width * p_data->screen_height * 4 );
    }
    else
    {
        p_data->screen_image =
            ( char * )malloc( p_data->width * p_data->height * 4 );
    }

    /* Cursor */

    CGSNewConnection( NULL, &( p_data->connection ) );

    p_data->cursor_need_update = 1;
    p_data->cursor_seed = 0;

    glGenTextures( 1, &( p_data->cursor_texture ) );
    glBindTexture( GL_TEXTURE_2D, p_data->cursor_texture );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

    /* System */

    es_format_Init( &p_sys->fmt, VIDEO_ES, VLC_CODEC_RGB32 );

    /* p_sys->fmt.video.i_* must set to screen size, not subscreen size */
    p_sys->fmt.video.i_width = p_data->screen_width;
    p_sys->fmt.video.i_visible_width = p_data->screen_width;
    p_sys->fmt.video.i_height = p_data->screen_height;
    p_sys->fmt.video.i_bits_per_pixel = 32;

    return VLC_SUCCESS;

    errorHandling:
    msg_Err( p_demux, "Core OpenGL failure: %s", CGLErrorString( returnedError ) );
    return VLC_EGENERIC;
}

int screen_CloseCapture( demux_t *p_demux )
{
    screen_data_t *p_data = ( screen_data_t * )p_demux->p_sys->p_data;

    CGLSetCurrentContext( NULL );

    /* Cursor */

    glBindTexture( GL_TEXTURE_2D, 0 );

    glDeleteTextures( 1, &( p_data->cursor_texture ) );

    CGSReleaseConnection( p_data->connection );

    /* Screen Image */
    if( p_data->screen_image != NULL )
    {
        free( p_data->screen_image );
        p_data->screen_image = NULL;
    }

    /* Clipped Image */

    if( p_data->clipped_image != NULL )
    {
        free( p_data->clipped_image );
        p_data->clipped_image = NULL;
    }

    /* Clipped Context */

    CGLClearDrawable( p_data->clipped );
    CGLDestroyContext( p_data->clipped );

    /* Screen Context */

    if( p_data->myCGDisplayCreateImageForRect == NULL )
    {
        CGLClearDrawable( p_data->screen );
        CGLDestroyContext( p_data->screen );
    }

    /* CGImage */

    CFRelease( p_data->bundle );

    free( p_data );

    return VLC_SUCCESS;
}

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = ( screen_data_t * )p_sys->p_data;
    block_t *p_block;
    int i_size;

    i_size = p_sys->fmt.video.i_height * p_sys->fmt.video.i_width * 4;

    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "cannot get block" );
        return NULL;
    }

    CGPoint cursor_pos;
    CGError cursor_result;

    cursor_pos.x = 0;
    cursor_pos.y = 0;

    cursor_result =
        CGSGetCurrentCursorLocation( p_data->connection, &cursor_pos );

    cursor_pos.x -= p_data->screen_left;
    cursor_pos.y -= p_data->screen_top;

    if( p_sys->b_follow_mouse
        && cursor_result == kCGErrorSuccess )
    {
        FollowMouse( p_sys, cursor_pos.x, cursor_pos.y );
    }

    screen_CaptureScreen( p_sys );

    CGLSetCurrentContext( p_data->clipped );

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );

    glDrawPixels( p_data->width,
                  p_data->height,
                  GL_RGBA, GL_UNSIGNED_BYTE, p_data->screen_image );

    if( cursor_result == kCGErrorSuccess )
    {
        screen_DrawCursor( p_sys, &cursor_pos );
    }

    glReadPixels( 0, 0,
                  p_data->width,
                  p_data->height,
                  GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                  p_block->p_buffer );

    return p_block;
}

CGLError screen_CreateContext( CGLContextObj *context,
                               CGLPixelFormatAttribute a0,
                               CGLPixelFormatAttribute a1,
                               CGLPixelFormatAttribute a2,
                               CGLPixelFormatAttribute a3 )
{
    CGLPixelFormatAttribute attribs[4];
    CGLPixelFormatObj pix;
    GLint npix;
    CGLError returnedError;

    attribs[0] = a0;
    attribs[1] = a1;
    attribs[2] = a2;
    attribs[3] = a3;

    returnedError = CGLChoosePixelFormat( attribs, &pix, &npix );
    if( returnedError )
    {
        return returnedError;
    }

    returnedError = CGLCreateContext( pix, NULL, context );
    if( returnedError )
    {
        return returnedError;
    }

    returnedError = CGLDestroyPixelFormat( pix );
    if( returnedError )
    {
        return returnedError;
    }

    return kCGLNoError;
}

#define POT(V,N) V = 1; while( V < N ) { V <<= 1; }

int screen_DrawCursor( demux_sys_t *p_sys, CGPoint *cursor_pos )
{
    int size;
    int tmp1, tmp2, tmp3, tmp4;
    unsigned char *cursor_image;

    screen_data_t *p_data = p_sys->p_data;

    int seed = CGSCurrentCursorSeed();
    if( seed != p_data->cursor_seed )
    {
        p_data->cursor_need_update = 1;

        if( CGSGetGlobalCursorDataSize( p_data->connection, &size )
            != kCGErrorSuccess)
        {
            return VLC_EGENERIC;
        }

        cursor_image = ( unsigned char * )malloc( size );

        if( CGSGetGlobalCursorData( p_data->connection,
                                    cursor_image, &size,
                                    &tmp1,
                                    &p_data->cursor_rect, &p_data->cursor_hot,
                                    &tmp2, &tmp3, &tmp4 )
            != kCGErrorSuccess )
        {
            free( cursor_image );

            return VLC_EGENERIC;
        }

        long int pot_width, pot_height;

        POT( pot_width, p_data->cursor_rect.size.width );
        POT( pot_height, p_data->cursor_rect.size.height );

        p_data->cursor_texture_map_u =
            p_data->cursor_rect.size.width / ( double )pot_width;
        p_data->cursor_texture_map_v =
            p_data->cursor_rect.size.height / ( double )pot_height;

        /* We need transparent image larger than original,
         * use calloc to clear alpha value to 0. */
        char *pot_cursor_image = ( char * )calloc( pot_width * pot_height * 4, sizeof( char ) );
        int width, height;
        char *from, *to;

        width = p_data->cursor_rect.size.width;
        height = p_data->cursor_rect.size.height;

        from = ( char * )cursor_image;
        to = pot_cursor_image;

#ifdef __LITTLE_ENDIAN__
        int y, fromwidth, towidth;

        fromwidth = width * 4;
        towidth = pot_width * 4;

        for( y = height; y; y -- )
        {
            memcpy( to, from, fromwidth );
            to += towidth;
            from += fromwidth;
        }
#else
        int x, y, diff;
        diff = ( pot_width - width ) * 4;
        for( y = height; y; y -- )
        {
            for( x = width; x; x -- )
            {
                to[0] = from[3];
                to[1] = from[2];
                to[2] = from[1];
                to[3] = from[0];

                to += 4;
                from += 4;
            }

            to += diff;
        }
#endif

        glEnable( GL_TEXTURE_2D );
        glBindTexture( GL_TEXTURE_2D, p_data->cursor_texture );
        glTexImage2D( GL_TEXTURE_2D, 0,
                      GL_RGBA8,
                      pot_width, pot_height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      pot_cursor_image );

        p_data->cursor_need_update = 0;
        p_data->cursor_seed = seed;

        free( pot_cursor_image );
        free( cursor_image );
    }
    else if( p_data->cursor_need_update )
    {
        return VLC_EGENERIC;
    }

    double x, y;
    double x1, y1, x2, y2;

    x = cursor_pos->x - p_sys->i_left - p_data->cursor_hot.x;
    y = cursor_pos->y - p_sys->i_top - p_data->cursor_hot.y;

    x1 = 2.0 * x / p_data->width - 1.0;
    y1 = 2.0 * y / p_data->height - 1.0;
    x2 = 2.0 * ( x + p_data->cursor_rect.size.width ) / p_data->width - 1.0;
    y2 = 2.0 * ( y + p_data->cursor_rect.size.height ) / p_data->height - 1.0;

    glColor3f( 1.0f, 1.0f, 1.0f );
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glBindTexture( GL_TEXTURE_2D, p_data->cursor_texture );
    glBegin( GL_POLYGON );
    glTexCoord2f( 0.0, 0.0 );
    glVertex2f( x1, y1 );
    glTexCoord2f( p_data->cursor_texture_map_u, 0.0 );
    glVertex2f( x2, y1 );
    glTexCoord2f( p_data->cursor_texture_map_u, p_data->cursor_texture_map_v );
    glVertex2f( x2, y2 );
    glTexCoord2f( 0.0, p_data->cursor_texture_map_v );
    glVertex2f( x1, y2 );
    glEnd();
    glDisable( GL_BLEND );
    glDisable( GL_TEXTURE_2D );

    return VLC_SUCCESS;
}

int screen_CaptureScreen( demux_sys_t *p_sys )
{
    screen_data_t *p_data = p_sys->p_data;

    if( p_data->myCGDisplayCreateImageForRect != NULL )
    {
        CGImageRef captured_image;
        CGContextRef captured_bitmap;
        CGRect capture_rect;
        CGRect draw_rect;

        capture_rect.origin.x = p_sys->i_left;
        capture_rect.origin.y = p_sys->i_top;
        capture_rect.size.width = p_data->width;
        capture_rect.size.height = p_data->height;
        captured_image =
            p_data->myCGDisplayCreateImageForRect( p_data->display_id,
                                                   capture_rect );

        captured_bitmap =
            CGBitmapContextCreate( ( unsigned char * )p_data->screen_image,
                                   capture_rect.size.width,
                                   capture_rect.size.height,
                                   8,
                                   capture_rect.size.width * 4,
                                   CGColorSpaceCreateDeviceRGB(),
                                   kCGImageAlphaPremultipliedLast );

        draw_rect.size.width = CGImageGetWidth( captured_image );
        draw_rect.size.height = CGImageGetHeight( captured_image );
        draw_rect.origin.x = 0;
        draw_rect.origin.y = capture_rect.size.height - draw_rect.size.height;
        CGContextDrawImage( captured_bitmap, draw_rect, captured_image );

        CGContextRelease( captured_bitmap );
        CGImageRelease( captured_image );
    }
    else
    {
        CGLSetCurrentContext( p_data->screen );

        glReadPixels( p_sys->i_left,
                      p_data->screen_height - p_sys->i_top - p_data->height,
                      p_data->width,
                      p_data->height,
                      GL_RGBA, GL_UNSIGNED_BYTE,
                      p_data->screen_image );
    }

    return VLC_SUCCESS;
}
