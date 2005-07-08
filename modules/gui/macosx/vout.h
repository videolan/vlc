/*****************************************************************************
 * vout.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2001-2005 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * VLCWindow interface
 *****************************************************************************/
@interface VLCWindow : NSWindow
{
    vout_thread_t * p_vout;
    NSView        * o_view;
    NSRect        * s_frame;

    vout_thread_t * p_real_vout;
    Ptr             p_fullscreen_state;
    mtime_t         i_time_mouse_last_moved;
    vlc_bool_t      b_init_ok;
}

- (id) initWithVout: (vout_thread_t *) p_vout view: (NSView *) view
                     frame: (NSRect *) s_frame;
- (id) initReal: (id) sender;
- (void) close;
- (id)   closeReal: (id) sender;
- (void)setOnTop:(BOOL)b_on_top;

- (void)hideMouse:(BOOL)b_hide;
- (void)manage;

- (void)scaleWindowWithFactor: (float)factor;
- (void)toggleFloatOnTop;
- (void)toggleFullscreen;
- (BOOL)isFullscreen;
- (void)snapshot;
- (void)updateTitle;

- (BOOL)windowShouldClose:(id)sender;

@end
