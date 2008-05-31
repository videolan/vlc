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
#import <stdlib.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>

#import <ApplicationServices/ApplicationServices.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

typedef int CGSConnectionRef;
extern CGError CGSNewConnection( void *, CGSConnectionRef * );
extern CGError CGSReleaseConnection( CGSConnectionRef );
extern CGError CGSGetGlobalCursorDataSize( CGSConnectionRef, int * );
extern CGError CGSGetGlobalCursorData( CGSConnectionRef, unsigned char *,
                                       int *, int *, CGRect *, CGPoint *,
                                       int *, int *, int * );
extern CGError CGSGetCurrentCursorLocation( CGSConnectionRef, CGPoint * );

#import "screen.h"

struct screen_data_t
{
  CGLContextObj screen;
  
  CGLContextObj scaled;
  char *scaled_image;
  
  GLuint texture;
  char *texture_image;
  
  GLuint cursor_texture;
  
  int left;
  int top;
  int src_width;
  int src_height;
  
  int dest_width;
  int dest_height;
  
  int screen_width;
  int screen_height;
  
  CGSConnectionRef connection;
};

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t   *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    CGLPixelFormatAttribute attribs[4];
    CGLPixelFormatObj pix;
    GLint npix;
    GLint viewport[4];
    
    p_sys->p_data = p_data =
        ( screen_data_t * )malloc( sizeof( screen_data_t ) );
    
    attribs[0] = kCGLPFAFullScreen;
    attribs[1] = kCGLPFADisplayMask;
    attribs[2] = CGDisplayIDToOpenGLDisplayMask( CGMainDisplayID() );
    attribs[3] = 0;
    
    CGLChoosePixelFormat( attribs, &pix, &npix );
    CGLCreateContext( pix, NULL, &( p_data->screen ) );
    CGLDestroyPixelFormat( pix );

    CGLSetCurrentContext( p_data->screen );
    CGLSetFullScreen( p_data->screen );
    
    glGetIntegerv( GL_VIEWPORT, viewport );
    
    p_data->screen_width = viewport[2];
    p_data->screen_height = viewport[3];
    
    p_data->left = 0;
    p_data->top = 0;
    p_data->src_width = p_data->screen_width;
    p_data->src_height = p_data->screen_height;
    p_data->dest_width = p_data->src_width;
    p_data->dest_height = p_data->src_height;
    
    attribs [0] = kCGLPFAOffScreen;
    attribs [1] = kCGLPFAColorSize;
    attribs [2] = 32;
    attribs [3] = 0;
    
    CGLChoosePixelFormat( attribs, &pix, &npix );
    CGLCreateContext( pix, NULL, &( p_data->scaled ) );
    CGLDestroyPixelFormat( pix );

    CGLSetCurrentContext( p_data->scaled );
    p_data->scaled_image = ( char * )malloc( p_data->dest_width
                                          * p_data->dest_height * 4 );
    CGLSetOffScreen( p_data->scaled, p_data->dest_width, p_data->dest_height,
                     p_data->dest_width * 4, p_data->scaled_image );
    
    es_format_Init( &p_sys->fmt, VIDEO_ES, VLC_FOURCC( 'R','V','3','2' ) );
    
    p_sys->fmt.video.i_width = p_data->dest_width;
    p_sys->fmt.video.i_visible_width = p_data->dest_width;
    p_sys->fmt.video.i_height = p_data->dest_height;
    p_sys->fmt.video.i_bits_per_pixel = 32;
    
    glGenTextures( 1, &( p_data->texture ) );
    glBindTexture( GL_TEXTURE_2D, p_data->texture );
    
    p_data->texture_image
      = ( char * )malloc( p_data->src_width * p_data->src_height * 4 );
    
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

    glGenTextures( 1, &( p_data->cursor_texture ) );
    glBindTexture( GL_TEXTURE_2D, p_data->cursor_texture );
    
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    
    CGSNewConnection( NULL, &( p_data->connection ) );
    
    return VLC_SUCCESS;
}

int screen_CloseCapture( demux_t *p_demux )
{
    screen_data_t *p_data = ( screen_data_t * )p_demux->p_sys->p_data;
    
    CGSReleaseConnection( p_data->connection );
    
    CGLSetCurrentContext( NULL );
    CGLClearDrawable( p_data->screen );
    CGLDestroyContext( p_data->screen );
    
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
        return 0;
    }
    
    CGLSetCurrentContext( p_data->screen );
    glReadPixels( p_data->left,
                  p_data->screen_height - p_data->top - p_data->src_height,
                  p_data->src_width,
                  p_data->src_height,
                  GL_RGBA, GL_UNSIGNED_BYTE,
                  p_data->texture_image );
    
    CGLSetCurrentContext( p_data->scaled );
    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, p_data->texture );
    glTexImage2D( GL_TEXTURE_2D, 0,
                  GL_RGBA8, p_data->src_width, p_data->src_height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, p_data->texture_image );
    
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear( GL_COLOR_BUFFER_BIT );
    glColor3f( 1.0f, 1.0f, 1.0f );
    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, p_data->texture );
    glBegin( GL_POLYGON );
    glTexCoord2f( 0.0, 1.0 ); glVertex2f( -1.0, -1.0 );
    glTexCoord2f( 1.0, 1.0 ); glVertex2f( 1.0, -1.0 );
    glTexCoord2f( 1.0, 0.0 ); glVertex2f( 1.0, 1.0 );
    glTexCoord2f( 0.0, 0.0 ); glVertex2f( -1.0, 1.0 );
    glEnd();
    glDisable( GL_TEXTURE_2D );
    
    CGPoint cursor_pos;
    int size;
    int tmp1, tmp2, tmp3, tmp4;
    unsigned char *cursor_image;
    CGRect cursor_rect;
    CGPoint cursor_hot;
    
    cursor_pos.x = 0;
    cursor_pos.y = 0;
    
    if( CGSGetCurrentCursorLocation( p_data->connection, &cursor_pos )
        == kCGErrorSuccess
        && CGSGetGlobalCursorDataSize( p_data->connection, &size )
        == kCGErrorSuccess )
    {
        cursor_image = ( unsigned char* )malloc( size );
        if( CGSGetGlobalCursorData( p_data->connection,
                                    cursor_image, &size,
                                    &tmp1,
                                    &cursor_rect, &cursor_hot,
                                    &tmp2, &tmp3, &tmp4 )
            == kCGErrorSuccess )
        {
            glEnable( GL_TEXTURE_2D );
            glBindTexture( GL_TEXTURE_2D, p_data->cursor_texture );
            glTexImage2D( GL_TEXTURE_2D, 0,
                          GL_RGBA8,
                          ( int )( cursor_rect.size.width ),
                          ( int )( cursor_rect.size.height ), 0,
                          GL_RGBA, GL_UNSIGNED_BYTE,
                          ( char * )cursor_image );
            
            cursor_rect.origin.x = cursor_pos.x - p_data->left - cursor_hot.x;
            cursor_rect.origin.y = cursor_pos.y - p_data->top - cursor_hot.y;
            
            cursor_rect.origin.x
              = 2.0 * cursor_rect.origin.x / p_data->src_width - 1.0;
            cursor_rect.origin.y
              = 2.0 * cursor_rect.origin.y / p_data->src_height - 1.0;
            cursor_rect.size.width
              = 2.0 * cursor_rect.size.width / p_data->src_width;
            cursor_rect.size.height
              = 2.0 * cursor_rect.size.height / p_data->src_height;
            
            glColor3f( 1.0f, 1.0f, 1.0f );
            glEnable( GL_TEXTURE_2D );
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            glBindTexture( GL_TEXTURE_2D, p_data->cursor_texture );
            glBegin( GL_POLYGON );
            glTexCoord2f( 0.0, 0.0 ); glVertex2f( cursor_rect.origin.x,
                                                  cursor_rect.origin.y );
            glTexCoord2f( 1.0, 0.0 ); glVertex2f( cursor_rect.origin.x
                                                  + cursor_rect.size.width,
                                                  cursor_rect.origin.y );
            glTexCoord2f( 1.0, 1.0 ); glVertex2f( cursor_rect.origin.x
                                                  + cursor_rect.size.width,
                                                  cursor_rect.origin.y
                                                  + cursor_rect.size.height );
            glTexCoord2f( 0.0, 1.0 ); glVertex2f( cursor_rect.origin.x,
                                                  cursor_rect.origin.y
                                                  + cursor_rect.size.height );
            glEnd();
            glDisable( GL_BLEND );
            glDisable( GL_TEXTURE_2D );
        }
        free( cursor_image );
    }
    
    glReadPixels( 0, 0, 
                  p_data->dest_width,
                  p_data->dest_height,
                  GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                  p_block->p_buffer );
    
    return p_block;
}
