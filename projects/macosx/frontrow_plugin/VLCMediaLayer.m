/*****************************************************************************
 * VLCMediaLayer.m: Front Row plugin
 *****************************************************************************
 * Copyright (C) 2007 - 2008 the VideoLAN Team
 * $Id$
 *
 * Authors: hyei
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

#import "VLCMediaLayer.h"

@implementation VLCMediaLayer

- init
{
    self = [super init];
    
    return self;
}

- (void)dealloc
{
    [_player release];
    [super dealloc];
}

- (void)setMedia:(VLCMedia*)media
{
#if 0
    if(_videoLayer != nil) {
        [_videoLayer removeFromSuperlayer];
    }
    
    _videoLayer = [VLCVideoLayer layer];
    _videoLayer.frame = self.bounds;
    _videoLayer.autoresizingMask = kCALayerWidthSizable|kCALayerHeightSizable;
    [self addSublayer:_videoLayer];
    
    if(_player != nil) {
        [_player release];
    }
    
    _player = [[VLCMediaPlayer alloc] initWithVideoLayer:_videoLayer];
#else
    if(_videoLayer == nil) {
        _videoLayer = [VLCVideoLayer layer];
        _videoLayer.frame = self.bounds;
        _videoLayer.autoresizingMask = kCALayerWidthSizable|kCALayerHeightSizable;
        _videoLayer.fillScreen = YES;
        [self addSublayer:_videoLayer];
    }
    
    if(_player == nil) {
        _player = [[VLCMediaPlayer alloc] initWithVideoLayer:_videoLayer];
    }
#endif
    
    NSLog(@"playing media: %@", media);
    
    [_player setMedia:media];
}

- (VLCMedia *)media
{
    return [_player media];
}

- (VLCMedia *)url
{
    if(_player == nil) {
        return nil;
    }
    else {
        return [_player media];
    }
}

- (void)setPlaying:(BOOL)playing
{
    if(playing)
        [_player play];
    else
        [_player pause];
}

- (BOOL)playing
{
    return [_player isPlaying];
}

- (VLCMediaPlayer*)player
{
    return _player;
}

#define NUM_RATES 7
static float rates[NUM_RATES] = {-8.0, -4.0, -2.0, 1.0, 2.0, 4.0, 8.0};

- (void)_slideRate:(int)delta
{
    float rate = _player.rate;
    BOOL foundRate = NO;
    
    int index;
    for(index=0; index<NUM_RATES; index++) {
        if(rate == rates[index]) {
            int newIndex = index + delta;
            if(newIndex >= 0 && newIndex < NUM_RATES) {
                rate = rates[newIndex];
                foundRate = YES;
            }
        }
    }

    if(foundRate) {
        _player.rate = rate;
    }
}

- (void)goFaster
{
    [self _slideRate:+1];
}

- (void)goSlower
{
    [self _slideRate:-1];
}

- (void)playPause
{
    if(_player.rate != 1.0) {
        _player.rate = 1.0;
    }
    else {
        if([_player isPlaying]) {
            [_player pause];
        }
        else {
            [_player play];
        }
    }
}

#if 0
#define CHECK_ERR(func) \
{ \
	CGLError err = func;\
	if(err != kCGLNoError) NSLog(@"error: %s", CGLErrorString(err)); \
}

#define CHECK_GL_ERROR() \
{ \
	GLenum err = glGetError(); \
	if(err != GL_NO_ERROR) NSLog(@"glError: %d", err); \
}

- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts
{
    static int i = 0;
    CGRect bounds = self.bounds;
    //NSLog(@"draw");
    --i;

    if(i<0 && _pBuffer == NULL) {
        NSOpenGLView * openglView = nil;
        for(NSView * subview in [_videoView subviews]) {
            if([subview isKindOfClass:[NSOpenGLView class]]) {
                openglView = (NSOpenGLView*)subview;
                break;
            }
        }

        if(openglView != nil) {
            NSLog(@"Create pbuffer %@", NSStringFromRect([_videoView bounds]));
            CHECK_ERR(CGLCreatePBuffer(CGRectGetWidth(bounds), CGRectGetHeight(bounds), GL_TEXTURE_RECTANGLE_ARB, GL_RGB, 0, &_pBuffer));
            
            CGLContextObj vlcContext = (CGLContextObj)[[openglView openGLContext] CGLContextObj];
            
            CHECK_ERR(CGLLockContext(vlcContext));
            CHECK_ERR(CGLSetCurrentContext(vlcContext));
            
            GLint screen;
            CHECK_ERR(CGLGetVirtualScreen(vlcContext, &screen));
            
            CHECK_ERR(CGLSetPBuffer(vlcContext, _pBuffer, 0, 0, screen));
            
            CHECK_ERR(CGLUnlockContext(vlcContext));
            CHECK_ERR(CGLSetCurrentContext(ctx));
        }
    }
    
    if(_pBuffer != NULL) {
        glColor3f(1.0, 1.0, 1.0);
        
        GLuint texture;
        glGenTextures(1, &texture);
        CHECK_GL_ERROR();
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        CHECK_GL_ERROR();
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
        CHECK_GL_ERROR();
        glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        CHECK_GL_ERROR();
        CHECK_ERR(CGLTexImagePBuffer(ctx, _pBuffer, GL_FRONT));
        
        glBegin(GL_QUADS);
        glTexCoord2f(CGRectGetMinX(bounds), CGRectGetMinY(bounds));
        glVertex2f(-1.0, -1.0);
        glTexCoord2f(CGRectGetMinX(bounds), CGRectGetMaxY(bounds));
        glVertex2f(-1.0, 1.0);
        glTexCoord2f(CGRectGetMaxX(bounds), CGRectGetMaxY(bounds));
        glVertex2f(1.0, 1.0);
        glTexCoord2f(CGRectGetMaxX(bounds), CGRectGetMinY(bounds));
        glVertex2f(1.0, -1.0);
        glEnd();
        CHECK_GL_ERROR();
        
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
    }
    else {
        glColor3f(0.0, 0.0, 0.0);
        
        glBegin(GL_QUADS);
        glVertex2f(-1.0, 1.0);
        glVertex2f(1.0, 1.0);
        glVertex2f(1.0, -1.0);
        glVertex2f(-1.0, -1.0);
        glEnd();
    }

    [super drawInCGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts];
}
#endif
@end
