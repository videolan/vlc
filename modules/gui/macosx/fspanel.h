/*****************************************************************************
 * fspanel.h: MacOS X full screen panel
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: JŽr™me Decoodt <djc at videolan dot org>
 *          Felix KŸhne <fkuehne at videolan dot org>
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

@interface VLCFSPanel : NSWindow
{
    NSTimer *fadeTimer,*hideAgainTimer;
    NSPoint mouseClic;
    BOOL b_fadeQueued;
    BOOL b_keptVisible;
    BOOL b_alreadyCounting;
    int i_timeToKeepVisibleInSec;
}
- (id)initWithContentRect: (NSRect)contentRect 
                styleMask: (unsigned int)aStyle 
                  backing: (NSBackingStoreType)bufferingType
                    defer: (BOOL)flag;
- (void)awakeFromNib;
- (BOOL)canBecomeKeyWindow;
- (void)dealloc;

- (void)setPlay;
- (void)setPause;
- (void)setStreamTitle:(NSString *)o_title;
- (void)setStreamPos:(float) f_pos setSeconds:(int)i_seconds;

- (void)focus:(NSTimer *)timer;
- (void)unfocus:(NSTimer *)timer;
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;

- (void)fadeIn;
- (void)fadeOut;

- (NSTimer *)fadeTimer;
- (void)setFadeTimer:(NSTimer *)timer;
- (void)autoHide;
- (void)keepVisible:(NSTimer *)timer;

- (void)mouseDown:(NSEvent *)theEvent;
- (void)mouseDragged:(NSEvent *)theEvent;

@end

@interface VLCFSPanelView : NSView
{
    NSColor *fillColor;
    NSButton *o_prev, *o_next, *o_slow, *o_fast, *o_play, *o_fullscreen;
    NSTextField *o_textfield, *o_textPos;
    NSSlider *o_time_slider;
}
- (id)initWithFrame:(NSRect)frameRect;
- (void)drawRect:(NSRect)rect;

- (void) setPlay;
- (void) setPause;
- (void) setStreamTitle: (NSString *)o_title;
- (void) setStreamPos:(float) f_pos setSeconds:(int)i_seconds;
- (IBAction)play:(id)sender;
- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)faster:(id)sender;
- (IBAction)slower:(id)sender;

@end

@interface VLCFSTimeSlider : NSSlider
{}
void drawKnobInRect(NSRect knobRect);
void drawFrameInRect(NSRect frameRect);
- (void)drawRect:(NSRect)rect;

@end
