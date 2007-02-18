/*****************************************************************************
 * embeddedwindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include "intf.h"
#include "vout.h"
#include "embeddedwindow.h"

/*****************************************************************************
 * VLCEmbeddedWindow Implementation
 *****************************************************************************/

@implementation VLCEmbeddedWindow

- (void)awakeFromNib
{
    [self setDelegate: self];

    [o_btn_backward setToolTip: _NS("Rewind")];
    [o_btn_forward setToolTip: _NS("Fast Forward")];
    [o_btn_fullscreen setToolTip: _NS("Fullscreen")];
    [o_btn_play setToolTip: _NS("Play")];
    [o_slider setToolTip: _NS("Position")];

    o_img_play = [NSImage imageNamed: @"play_embedded"];
    o_img_play_pressed = [NSImage imageNamed: @"play_embedded_blue"];
    o_img_pause = [NSImage imageNamed: @"pause_embedded"];
    o_img_pause_pressed = [NSImage imageNamed: @"pause_embedded_blue"];

    o_saved_frame = NSMakeRect( 0.0f, 0.0f, 0.0f, 0.0f );
}

- (void)setTime:(NSString *)o_arg_time position:(float)f_position
{
    [o_time setStringValue: o_arg_time];
    [o_slider setFloatValue: f_position];
}

- (void)playStatusUpdated:(int)i_status
{
    if( i_status == PLAYING_S )
    {
        [o_btn_play setImage: o_img_pause];
        [o_btn_play setAlternateImage: o_img_pause_pressed];
        [o_btn_play setToolTip: _NS("Pause")];
    }
    else
    {
        [o_btn_play setImage: o_img_play];
        [o_btn_play setAlternateImage: o_img_play_pressed];
        [o_btn_play setToolTip: _NS("Play")];
    }
}

- (void)setSeekable:(BOOL)b_seekable
{
    [o_btn_forward setEnabled: b_seekable];
    [o_btn_backward setEnabled: b_seekable];
    [o_slider setEnabled: b_seekable];
}

- (void)setFullscreen:(BOOL)b_fullscreen
{
    [o_btn_fullscreen setState: b_fullscreen];
}

- (void)zoom:(id)sender
{
    if( ![self isZoomed] )
    {
        NSRect zoomRect = [[self screen] frame];
        o_saved_frame = [self frame];
        /* we don't have to take care of the eventual menu bar and dock
          as zoomRect will be cropped automatically by setFrame:display:
          to the right rectangle */
        [self setFrame: zoomRect display: YES animate: YES];
    }
    else
    {
        /* unzoom to the saved_frame if the o_saved_frame coords look sound
           (just in case) */
        if( o_saved_frame.size.width > 0 && o_saved_frame.size.height > 0 )
            [self setFrame: o_saved_frame display: YES animate: YES];
    }
}

- (BOOL)windowShouldClose:(id)sender
{
    playlist_t * p_playlist = pl_Yield( VLCIntf );

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
    return YES;
}

@end
