/*****************************************************************************
 * VLCOpenGLVoutView.h: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
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

#import <Cocoa/Cocoa.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <vlc_common.h>
#include <vlc_vout_window.h>

/* Entry point */
int  cocoaglvoutviewInit( vout_window_t * p_vout, const vout_window_cfg_t *cfg );
void cocoaglvoutviewEnd( vout_window_t * p_vout );

/* To commmunicate with the VLC.framework */
@protocol VLCOpenGLVoutEmbedding <NSObject>
- (void)addVoutSubview:(NSView *)view;
- (void)removeVoutSubview:(NSView *)view;

- (void)enterFullscreen;
- (void)leaveFullscreen;

- (BOOL)stretchesVideo;

- (void)setOnTop: (BOOL)ontop; /* Do we really want that in protocol? */
@end

/* VLCOpenGLVoutView */
@interface VLCOpenGLVoutView : NSView
{
    id <VLCOpenGLVoutEmbedding> container;
    vout_window_t * p_wnd;
    NSLock        * objectLock;
}
/* Init a new gl view and register it to both the framework and the
 * vout_thread_t. Must be called from main thread */
+ (void) autoinitOpenGLVoutViewIntVoutWithContainer: (NSData *) args;

- (id) initWithVoutWindow: (vout_window_t *) p_wnd container: (id <VLCOpenGLVoutEmbedding>) container;

- (void) detachFromVoutWindow;
- (id <VLCOpenGLVoutEmbedding>) container;

@end

