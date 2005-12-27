/*****************************************************************************
 * embeddedwindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org> 
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
 * VLCEmbeddedWindow interface
 *****************************************************************************/


@interface VLCEmbeddedWindow : NSObject
{
    IBOutlet id o_btn_backward;
    IBOutlet id o_btn_forward;
    IBOutlet id o_btn_fullscreen;
    IBOutlet id o_btn_play;
    IBOutlet id o_slider;
    IBOutlet id o_time;
    IBOutlet id o_window;

    NSImage * o_img_play;
    NSImage * o_img_play_pressed;
    NSImage * o_img_pause;
    NSImage * o_img_pause_pressed;
}

- (void)setTime:(NSString *)o_arg_ime position:(float)f_position;
- (void)playStatusUpdated:(int)i_status;
- (void)setSeekable:(BOOL)b_seekable;
- (void)setFullscreen:(BOOL)b_fullscreen;

@end

