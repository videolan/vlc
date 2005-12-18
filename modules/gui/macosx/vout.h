/*****************************************************************************
 * vout.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
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
 * VLCEmbeddedList interface
 *****************************************************************************/
@interface VLCEmbeddedList : NSObject
{
    NSMutableArray * o_embedded_array;
}

- (id)getEmbeddedVout;
- (void)releaseEmbeddedVout: (id)o_vout_view;
- (void)addEmbeddedVout: (id)o_vout_view;
- (BOOL)windowContainsEmbedded: (id)o_window;
- (id)getViewForWindow: (id)o_window;

@end

/*****************************************************************************
 * VLCVoutView interface
 *****************************************************************************/
@interface VLCVoutView : NSView
{
    vout_thread_t * p_vout;
    NSView        * o_view;
    NSRect        * s_frame;

    vout_thread_t * p_real_vout;
    id              o_window;
}
- (BOOL)setVout: (vout_thread_t *) p_arg_vout subView: (NSView *) view
                     frame: (NSRect *) s_arg_frame;
- (void)closeVout;
- (void)manage;
- (void)scaleWindowWithFactor: (float)factor;
- (void)setOnTop:(BOOL)b_on_top;
- (void)toggleFloatOnTop;
- (void)toggleFullscreen;
- (BOOL)isFullscreen;
- (void)snapshot;
- (id)getWindow;

+ (id)getVoutView: (vout_thread_t *)p_vout subView: (NSView *) view
                            frame: (NSRect *) s_frame;
+ (vout_thread_t *)getRealVout: (vout_thread_t *)p_vout;

@end

/*****************************************************************************
 * VLCVoutDetachedView interface
 *****************************************************************************/

@interface VLCDetachedVoutView : VLCVoutView
{
    mtime_t i_time_mouse_last_moved;
}

- (void)hideMouse: (BOOL)b_hide;

@end

/*****************************************************************************
 * VLCEmbeddedView interface
 *****************************************************************************/

@interface VLCEmbeddedVoutView : VLCVoutView
{
    BOOL b_used;
}

- (void)setUsed: (BOOL)b_new_used;
- (BOOL)isUsed;

@end

/*****************************************************************************
 * VLCWindow interface
 *****************************************************************************/
@interface VLCWindow : NSWindow
{
    vout_thread_t * p_vout;
    VLCVoutView   * o_view;
    NSRect        * s_frame;

    vout_thread_t * p_real_vout;
    Ptr             p_fullscreen_state;
    vlc_bool_t      b_init_ok;
}

- (id) initWithVout: (vout_thread_t *) p_vout view: (VLCVoutView *) view
                     frame: (NSRect *) s_frame;
- (id)initReal: (id) sender;
- (void)close;
- (void)closeWindow;
- (id)closeReal: (id) sender;
- (id)getVoutView;
- (void)updateTitle;

- (BOOL)windowShouldClose:(id)sender;

@end
