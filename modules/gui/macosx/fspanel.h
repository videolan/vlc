/*****************************************************************************
 * fspanel.h: MacOS X full screen panel
 *****************************************************************************
 * Copyright (C) 2006-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jérôme Decoodt <djc at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

@class VLCWindow;

@interface VLCFSPanel : NSWindow
{
    NSTimer *fadeTimer,*hideAgainTimer;
    NSPoint mouseClic;
    BOOL b_fadeQueued;
    BOOL b_keptVisible;
    BOOL b_alreadyCounting;
    int i_timeToKeepVisibleInSec;

    BOOL b_nonActive;
    BOOL b_displayed;
    BOOL b_voutWasUpdated;
    int i_device;

    VLCWindow *o_vout_window;
}
- (id)initWithContentRect: (NSRect)contentRect
                styleMask: (NSUInteger)aStyle
                  backing: (NSBackingStoreType)bufferingType
                    defer: (BOOL)flag;
- (void)awakeFromNib;
- (BOOL)canBecomeKeyWindow;
- (void)dealloc;

- (void)setPlay;
- (void)setPause;
- (void)setStreamTitle: (NSString *)o_title;
- (void)updatePositionAndTime;
- (void)setSeekable: (BOOL)b_seekable;
- (void)setVolumeLevel: (int)i_volumeLevel;

- (void)setNonActive: (id)noData;
- (void)setActive: (id)noData;

- (void)focus: (NSTimer *)timer;
- (void)unfocus: (NSTimer *)timer;
- (void)mouseExited: (NSEvent *)theEvent;

- (void)fadeIn;
- (void)fadeOut;

- (NSTimer *)fadeTimer;
- (void)setFadeTimer: (NSTimer *)timer;
- (void)autoHide;
- (void)keepVisible: (NSTimer *)timer;

- (void)mouseDown: (NSEvent *)theEvent;
- (void)mouseDragged: (NSEvent *)theEvent;

- (void)setVoutWasUpdated: (VLCWindow *)o_window;
@end

@class VLCProgressView;
@class VLCFSVolumeSlider;

@interface VLCFSPanelView : NSView
{
    NSColor *fillColor;
    NSButton *o_prev, *o_next, *o_bwd, *o_fwd, *o_play, *o_fullscreen;
    NSTextField *o_streamTitle_txt;
    VLCTimeField *o_streamPosition_txt, *o_streamLength_txt;
    NSSlider *o_fs_timeSlider;
    VLCFSVolumeSlider *o_fs_volumeSlider;
    VLCProgressView *o_progress_view;
    NSImage *o_background_img, *o_vol_sld_img, *o_vol_mute_img, *o_vol_max_img, *o_time_sld_img;
    NSTimeInterval last_fwd_event;
    NSTimeInterval last_bwd_event;
}
- (id)initWithFrame: (NSRect)frameRect;
- (void)drawRect: (NSRect)rect;

- (void)setPlay;
- (void)setPause;
- (void)setStreamTitle: (NSString *)o_title;
- (void)updatePositionAndTime;
- (void)setSeekable: (BOOL)b_seekable;
- (void)setVolumeLevel: (int)i_volumeLevel;
- (IBAction)play:(id)sender;
- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)forward:(id)sender;
- (IBAction)backward:(id)sender;
- (IBAction)fsTimeSliderUpdate: (id)sender;
- (IBAction)fsVolumeSliderUpdate: (id)sender;

@end

@interface VLCFSTimeSlider : NSSlider
{
}
- (void)drawKnobInRect: (NSRect)knobRect;
- (void)drawRect: (NSRect)rect;

@end

@interface VLCFSVolumeSlider : VLCVolumeSliderCommon
{
}
- (void)drawKnobInRect: (NSRect)knobRect;
- (void)drawRect: (NSRect)rect;

@end
