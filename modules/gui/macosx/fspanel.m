/*****************************************************************************
 * fspanel.m: MacOS X full screen panel
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "intf.h"
#import "CoreInteraction.h"
#import "MainWindow.h"
#import "misc.h"
#import "fspanel.h"
#import "CompatibilityFixes.h"

@interface VLCFSPanel ()
- (void)hideMouse;
@end

/*****************************************************************************
 * VLCFSPanel
 *****************************************************************************/
@implementation VLCFSPanel
/* We override this initializer so we can set the NSBorderlessWindowMask styleMask, and set a few other important settings */
- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag
{
    id win = [super initWithContentRect:contentRect styleMask:NSTexturedBackgroundWindowMask backing:bufferingType defer:flag];
    [win setOpaque:NO];
    [win setHasShadow: NO];
    [win setBackgroundColor:[NSColor clearColor]];
    if (!OSX_SNOW_LEOPARD)
        [win setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* let the window sit on top of everything else and start out completely transparent */
    [win setLevel:NSModalPanelWindowLevel];
    i_device = config_GetInt(VLCIntf, "macosx-vdev");
    hideAgainTimer = fadeTimer = nil;
    [self setNonActive:nil];
    return win;
}

- (void)awakeFromNib
{
    [self setContentView:[[VLCFSPanelView alloc] initWithFrame: [self frame]]];
    BOOL isInside = (NSPointInRect([NSEvent mouseLocation],[self frame]));
    [[self contentView] addTrackingRect:[[self contentView] bounds] owner:self userData:nil assumeInside:isInside];
    if (isInside)
        [self mouseEntered:NULL];
    if (!isInside)
        [self mouseExited:NULL];

    if (!OSX_SNOW_LEOPARD)
        [self setAnimationBehavior:NSWindowAnimationBehaviorNone];

    /* get a notification if VLC isn't the active app anymore */
    [[NSNotificationCenter defaultCenter]
    addObserver: self
       selector: @selector(setNonActive:)
           name: NSApplicationDidResignActiveNotification
         object: NSApp];

    /* Get a notification if VLC is the active app again.
     Needed as becomeKeyWindow does not get called when window is activated by clicking */
    [[NSNotificationCenter defaultCenter]
    addObserver: self
       selector: @selector(setActive:)
           name: NSApplicationDidBecomeActiveNotification
         object: NSApp];
}

/* make sure that we don't become key, since we can't handle hotkeys */
- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

-(void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (hideAgainTimer) {
        [hideAgainTimer invalidate];
        [hideAgainTimer release];
    }

    if (o_vout_window)
        [o_vout_window release];

    [self setFadeTimer:nil];
    [super dealloc];
}

-(void)center
{
    /* centre the panel in the lower third of the screen */
    NSPoint theCoordinate;
    NSRect theScreensFrame;
    NSRect theWindowsFrame;
    NSScreen *screen;

    /* user-defined screen */
    screen = [NSScreen screenWithDisplayID: (CGDirectDisplayID)i_device];

    if (!screen)
        /* invalid preferences or none specified, using main screen */
        screen = [NSScreen mainScreen];

    theScreensFrame = [screen frame];
    theWindowsFrame = [self frame];

    theCoordinate.x = (theScreensFrame.size.width - theWindowsFrame.size.width) / 2 + theScreensFrame.origin.x;
    theCoordinate.y = (theScreensFrame.size.height / 3) - theWindowsFrame.size.height + theScreensFrame.origin.y;
    [self setFrameTopLeftPoint: theCoordinate];
}

- (void)setPlay
{
    [[self contentView] setPlay];
}

- (void)setPause
{
    [[self contentView] setPause];
}

- (void)setStreamTitle:(NSString *)o_title
{
    [[self contentView] setStreamTitle: o_title];
}

- (void)updatePositionAndTime
{
    [[self contentView] updatePositionAndTime];
}

- (void)setSeekable:(BOOL) b_seekable
{
    [[self contentView] setSeekable: b_seekable];
}

- (void)setVolumeLevel: (int)i_volumeLevel
{
    [[self contentView] setVolumeLevel: i_volumeLevel];
}

- (void)setNonActive:(id)noData
{
    b_nonActive = YES;

    /* here's fadeOut, just without visibly fading */
    b_displayed = NO;
    [self setAlphaValue:0.0];
    [self setFadeTimer:nil];

    b_fadeQueued = NO;

    [self orderOut: self];
}

- (void)setActive:(id)noData
{
    b_nonActive = NO;

    [[VLCMain sharedInstance] showFullscreenController];
}

/* This routine is called repeatedly to fade in the window */
- (void)focus:(NSTimer *)timer
{
    /* we need to push ourselves to front if the vout window was closed since our last display */
    if (b_voutWasUpdated) {
        [self orderFront: self];
        b_voutWasUpdated = NO;
    }

    if ([self alphaValue] < 1.0) {
        [self setAlphaValue:[self alphaValue]+0.1];
    }
    if ([self alphaValue] >= 1.0) {
        b_displayed = YES;
        [self setAlphaValue: 1.0];
        [self setFadeTimer:nil];
        if (b_fadeQueued) {
            b_fadeQueued=NO;
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(unfocus:) userInfo:NULL repeats:YES]];
        }
    }
}

/* This routine is called repeatedly to hide the window */
- (void)unfocus:(NSTimer *)timer
{
    if (b_keptVisible) {
        b_keptVisible = NO;
        b_fadeQueued = NO;
        [self setFadeTimer: NULL];
        [self fadeIn];
        return;
    }
    if ([self alphaValue] > 0.0) {
        [self setAlphaValue:[self alphaValue]-0.05];
    }
    if ([self alphaValue] <= 0.05) {
        b_displayed = NO;
        [self setAlphaValue:0.0];
        [self setFadeTimer:nil];
        if (b_fadeQueued) {
            b_fadeQueued=NO;
            [self setFadeTimer:
                [NSTimer scheduledTimerWithTimeInterval:0.1
                                                 target:self
                                               selector:@selector(focus:)
                                               userInfo:NULL
                                                repeats:YES]];
        }
    }
}

- (void)mouseExited:(NSEvent *)theEvent
{
    /* give up our focus, so the vout may show us again without letting the user clicking it */
    if (o_vout_window && var_GetBool(pl_Get(VLCIntf), "fullscreen"))
        [o_vout_window makeKeyWindow];
}

- (void)hideMouse
{
    [NSCursor setHiddenUntilMouseMoves: YES];
}

- (void)fadeIn
{
    /* in case that the user don't want us to appear, make sure we hide the mouse */

    if (!config_GetInt(VLCIntf, "macosx-fspanel")) {
        float time = (float)var_CreateGetInteger(VLCIntf, "mouse-hide-timeout") / 1000.;
        [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:time target:self selector:@selector(hideMouse) userInfo:nil repeats:NO]];
        return;
    }

    if (b_nonActive)
        return;

    [self orderFront: nil];

    if ([self alphaValue] < 1.0 || b_displayed != YES) {
        if (![self fadeTimer])
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(focus:) userInfo:[NSNumber numberWithInt:1] repeats:YES]];
        else if ([[[self fadeTimer] userInfo] shortValue]==0)
            b_fadeQueued=YES;
    }
    [self autoHide];
}

- (void)fadeOut
{
    if (NSPointInRect([NSEvent mouseLocation],[self frame]))
        return;

    if (([self alphaValue] > 0.0)) {
        if (![self fadeTimer])
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(unfocus:) userInfo:[NSNumber numberWithInt:0] repeats:YES]];
        else if ([[[self fadeTimer] userInfo] shortValue]==1)
            b_fadeQueued=YES;
    }
}

/* triggers a timer to autoHide us again after some seconds of no activity */
- (void)autoHide
{
    /* this will tell the timer to start over again or to start at all */
    b_keptVisible = YES;

    /* get us a valid timer */
    if (!b_alreadyCounting) {
        i_timeToKeepVisibleInSec = var_CreateGetInteger(VLCIntf, "mouse-hide-timeout") / 500;
        if (hideAgainTimer) {
            [hideAgainTimer invalidate];
            [hideAgainTimer autorelease];
        }
        /* released in -autoHide and -dealloc */
        hideAgainTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.5
                                                          target: self
                                                        selector: @selector(keepVisible:)
                                                        userInfo: nil
                                                         repeats: YES] retain];
        b_alreadyCounting = YES;
    }
}

- (void)keepVisible:(NSTimer *)timer
{
    /* if the user triggered an action, start over again */
    if (b_keptVisible)
        b_keptVisible = NO;

    /* count down until we hide ourselfes again and do so if necessary */
    if (--i_timeToKeepVisibleInSec < 1) {
        [self hideMouse];
        [self fadeOut];
        [hideAgainTimer invalidate]; /* released in -autoHide and -dealloc */
        b_alreadyCounting = NO;
    }
}

/* A getter and setter for our main timer that handles window fading */
- (NSTimer *)fadeTimer
{
    return fadeTimer;
}

- (void)setFadeTimer:(NSTimer *)timer
{
    [timer retain];
    [fadeTimer invalidate];
    [fadeTimer autorelease];
    fadeTimer=timer;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    mouseClic = [theEvent locationInWindow];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    NSPoint point = [NSEvent mouseLocation];
    point.x -= mouseClic.x;
    point.y -= mouseClic.y;
    [self setFrameOrigin:point];
}

- (void)setVoutWasUpdated: (VLCWindow *)o_window
{
    b_voutWasUpdated = YES;
    if (o_vout_window)
        [o_vout_window release];
    o_vout_window = [o_window retain];
    int i_newdevice = (int)[[o_vout_window screen] displayID];
    if ((i_newdevice != i_device && i_device != 0) || i_newdevice != [[self screen] displayID]) {
        i_device = i_newdevice;
        [self center];
    } else
        i_device = i_newdevice;
}
@end

/*****************************************************************************
 * FSPanelView
 *****************************************************************************/
@implementation VLCFSPanelView

#define addButton(o_button, imageOff, imageOn, _x, _y, action, AXDesc, ToolTip)               \
    s_rc.origin.x = _x;                                                                         \
    s_rc.origin.y = _y;                                                                         \
    o_button = [[NSButton alloc] initWithFrame: s_rc];                                 \
    [o_button setButtonType: NSMomentaryChangeButton];                                          \
    [o_button setBezelStyle: NSRegularSquareBezelStyle];                                        \
    [o_button setBordered: NO];                                                                 \
    [o_button setFont:[NSFont systemFontOfSize:0]];                                             \
    [o_button setImage:[NSImage imageNamed:imageOff]];                                 \
    [o_button setAlternateImage:[NSImage imageNamed:imageOn]];                         \
    [o_button sizeToFit];                                                                       \
    [o_button setTarget: self];                                                                 \
    [o_button setAction: @selector(action:)];                                                   \
    [[o_button cell] accessibilitySetOverrideValue:AXDesc forAttribute:NSAccessibilityDescriptionAttribute]; \
    [[o_button cell] accessibilitySetOverrideValue:ToolTip forAttribute:NSAccessibilityTitleAttribute]; \
    [o_button setToolTip: ToolTip]; \
    [self addSubview:o_button];

#define addTextfield(class, o_text, align, font, color)                                    \
    o_text = [[class alloc] initWithFrame: s_rc];                            \
    [o_text setDrawsBackground: NO];                                                        \
    [o_text setBordered: NO];                                                               \
    [o_text setEditable: NO];                                                               \
    [o_text setSelectable: NO];                                                             \
    [o_text setStringValue: _NS("(no item is being played)")];                                                    \
    [o_text setAlignment: align];                                                           \
    [o_text setTextColor: [NSColor color]];                                                 \
    [o_text setFont:[NSFont font:[NSFont smallSystemFontSize]]];                     \
    [self addSubview:o_text];

- (id)initWithFrame:(NSRect)frameRect
{
    id view = [super initWithFrame:frameRect];
    fillColor = [[NSColor clearColor] retain];
    NSRect s_rc = [self frame];
    addButton(o_prev, @"fs_skip_previous_highlight" , @"fs_skip_previous", 174, 15, prev, _NS("Click to go to the previous playlist item."), _NS("Previous"));
    addButton(o_bwd, @"fs_rewind_highlight"        , @"fs_rewind"       , 211, 14, backward, _NS("Click and hold to skip backward through the current media."), _NS("Backward"));
    addButton(o_play, @"fs_play_highlight"          , @"fs_play"         , 265, 10, play, _NS("Click to play or pause the current media."), _NS("Play/Pause"));
    addButton(o_fwd, @"fs_forward_highlight"       , @"fs_forward"      , 313, 14, forward, _NS("Click and hold to skip forward through the current media."), _NS("Forward"));
    addButton(o_next, @"fs_skip_next_highlight"     , @"fs_skip_next"    , 365, 15, next, _NS("Click to go to the next playlist item."), _NS("Next"));
    addButton(o_fullscreen, @"fs_exit_fullscreen_highlight", @"fs_exit_fullscreen", 507, 13, toggleFullscreen, _NS("Click to exit fullscreen playback."), _NS("Toggle Fullscreen mode"));
/*
    addButton(o_button, @"image (off state)", @"image (on state)", 38, 51, something, accessibility help string, usual tool tip);
 */
    [o_fwd setContinuous:YES];
    [o_bwd setContinuous:YES];

    /* time slider */
    // (surrounding progress view for swipe behaviour)
    s_rc.origin.x = 15;
    s_rc.origin.y = 45;
    s_rc.size.width = 518;
    s_rc.size.height = 13;
    o_progress_view = [[VLCProgressView alloc] initWithFrame: s_rc];
    s_rc.origin.x = 0;
    s_rc.origin.y = 0;
    o_fs_timeSlider = [[VLCFSTimeSlider alloc] initWithFrame: s_rc];
    [o_fs_timeSlider setMinValue:0];
    [o_fs_timeSlider setMaxValue:10000];
    [o_fs_timeSlider setFloatValue: 0];
    [o_fs_timeSlider setContinuous: YES];
    [o_fs_timeSlider setTarget: self];
    [o_fs_timeSlider setAction: @selector(fsTimeSliderUpdate:)];
    [[o_fs_volumeSlider cell] accessibilitySetOverrideValue:_NS("Position") forAttribute:NSAccessibilityTitleAttribute];
    [[o_fs_timeSlider cell] accessibilitySetOverrideValue:_NS("Click and move the mouse while keeping the button pressed to use this slider to change current playback position.") forAttribute:NSAccessibilityDescriptionAttribute];
    [self addSubview: o_progress_view];
    [o_progress_view addSubview: o_fs_timeSlider];

    /* volume slider */
    s_rc = [self frame];
    s_rc.origin.x = 26;
    s_rc.origin.y = 20;
    s_rc.size.width = 95;
    s_rc.size.height = 12;
    o_fs_volumeSlider = [[VLCFSVolumeSlider alloc] initWithFrame: s_rc];
    [o_fs_volumeSlider setMinValue:0];
    [o_fs_volumeSlider setMaxValue: [[VLCCoreInteraction sharedInstance] maxVolume]];
    [o_fs_volumeSlider setIntValue:AOUT_VOLUME_DEFAULT];
    [o_fs_volumeSlider setContinuous: YES];
    [o_fs_volumeSlider setTarget: self];
    [o_fs_volumeSlider setAction: @selector(fsVolumeSliderUpdate:)];
    [o_fs_volumeSlider setUsesBrightArtwork:NO];
    [[o_fs_volumeSlider cell] accessibilitySetOverrideValue:_NS("Volume") forAttribute:NSAccessibilityTitleAttribute];
    [[o_fs_volumeSlider cell] accessibilitySetOverrideValue:_NS("Click and move the mouse while keeping the button pressed to use this slider to change the volume.") forAttribute:NSAccessibilityDescriptionAttribute];
    [self addSubview: o_fs_volumeSlider];

    /* time counter and stream title output fields */
    s_rc = [self frame];
    // 10 px gap between time fields
    s_rc.origin.x = 90;
    s_rc.origin.y = 64;
    s_rc.size.width = 361;
    s_rc.size.height = 14;
    addTextfield(NSTextField, o_streamTitle_txt, NSCenterTextAlignment, systemFontOfSize, whiteColor);
    s_rc.origin.x = 15;
    s_rc.origin.y = 64;
    s_rc.size.width = 65;
    addTextfield(VLCTimeField, o_streamPosition_txt, NSLeftTextAlignment, systemFontOfSize, whiteColor);
    s_rc.origin.x = 471;
    s_rc.origin.y = 64;
    s_rc.size.width = 65;
    addTextfield(VLCTimeField, o_streamLength_txt, NSRightTextAlignment, systemFontOfSize, whiteColor);
    [o_streamLength_txt setRemainingIdentifier: @"DisplayFullscreenTimeAsTimeRemaining"];

    o_background_img = [[NSImage imageNamed:@"fs_background"] retain];
    o_vol_sld_img = [[NSImage imageNamed:@"fs_volume_slider_bar"] retain];
    o_vol_mute_img = [[NSImage imageNamed:@"fs_volume_mute_highlight"] retain];
    o_vol_max_img = [[NSImage imageNamed:@"fs_volume_max_highlight"] retain];
    o_time_sld_img = [[NSImage imageNamed:@"fs_time_slider"] retain];

    return view;
}

- (void)dealloc
{
    [o_background_img release];
    [o_vol_sld_img release];
    [o_vol_mute_img release];
    [o_vol_max_img release];
    [o_time_sld_img release];
    [o_fs_timeSlider release];
    [o_fs_volumeSlider release];
    [o_prev release];
    [o_next release];
    [o_bwd release];
    [o_play release];
    [o_fwd release];
    [o_fullscreen release];
    [o_streamTitle_txt release];
    [o_streamPosition_txt release];
    [super dealloc];
}

- (void)setPlay
{
    [o_play setImage:[NSImage imageNamed:@"fs_play_highlight"]];
    [o_play setAlternateImage: [NSImage imageNamed:@"fs_play"]];
}

- (void)setPause
{
    [o_play setImage: [NSImage imageNamed:@"fs_pause_highlight"]];
    [o_play setAlternateImage: [NSImage imageNamed:@"fs_pause"]];
}

- (void)setStreamTitle:(NSString *)o_title
{
    [o_streamTitle_txt setStringValue: o_title];
}

- (void)updatePositionAndTime
{
    input_thread_t * p_input;
    p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        
        vlc_value_t pos;
        float f_updated;

        var_Get(p_input, "position", &pos);
        f_updated = 10000. * pos.f_float;
        [o_fs_timeSlider setFloatValue: f_updated];

        vlc_value_t time;
        char psz_time[MSTRTIME_MAX_SIZE];

        var_Get(p_input, "time", &time);
        mtime_t dur = input_item_GetDuration(input_GetItem(p_input));

        // update total duration (right field)
        if(dur <= 0) {
            [o_streamLength_txt setHidden: YES];
        } else {
            [o_streamLength_txt setHidden: NO];

            NSString *o_total_time;
            if ([o_streamLength_txt timeRemaining]) {
                mtime_t remaining = 0;
                if (dur > time.i_time)
                    remaining = dur - time.i_time;
                o_total_time = [NSString stringWithFormat: @"-%s", secstotimestr(psz_time, (remaining / 1000000))];
            } else
                o_total_time = [NSString stringWithUTF8String:secstotimestr(psz_time, (dur / 1000000))];

            [o_streamLength_txt setStringValue: o_total_time];
        }

        // update current position (left field)
        NSString *o_playback_pos = [NSString stringWithUTF8String:secstotimestr(psz_time, (time.i_time / 1000000))];
               
        [o_streamPosition_txt setStringValue: o_playback_pos];
        vlc_object_release(p_input);
    } else {
        [o_fs_timeSlider setFloatValue: 0.0];
        [o_streamPosition_txt setStringValue: @"00:00"];
        [o_streamLength_txt setHidden: YES];
    }

}

- (void)setSeekable:(BOOL)b_seekable
{
    [o_bwd setEnabled: b_seekable];
    [o_fwd setEnabled: b_seekable];
    [o_fs_timeSlider setEnabled: b_seekable];
}

- (void)setVolumeLevel: (int)i_volumeLevel
{
    [o_fs_volumeSlider setIntValue: i_volumeLevel];
}

- (IBAction)play:(id)sender
{
    [[VLCCoreInteraction sharedInstance] playOrPause];
}

- (IBAction)forward:(id)sender
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_fwd_event) > 0.16) {
        // we just skipped 4 "continous" events, otherwise we are too fast
        [[VLCCoreInteraction sharedInstance] forwardExtraShort];
        last_fwd_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)backward:(id)sender
{
    if (([NSDate timeIntervalSinceReferenceDate] - last_bwd_event) > 0.16) {
        // we just skipped 4 "continous" events, otherwise we are too fast
        [[VLCCoreInteraction sharedInstance] backwardExtraShort];
        last_bwd_event = [NSDate timeIntervalSinceReferenceDate];
    }
}

- (IBAction)prev:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}

- (IBAction)next:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}

- (IBAction)toggleFullscreen:(id)sender
{
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (IBAction)fsTimeSliderUpdate:(id)sender
{
    input_thread_t * p_input;
    p_input = pl_CurrentInput(VLCIntf);
    if (p_input != NULL) {
        vlc_value_t pos;

        pos.f_float = [o_fs_timeSlider floatValue] / 10000.;
        var_Set(p_input, "position", pos);
        vlc_object_release(p_input);
    }
    [[VLCMain sharedInstance] updatePlaybackPosition];
}

- (IBAction)fsVolumeSliderUpdate:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVolume: [sender intValue]];
}

#define addImage(image, _x, _y, mode)                                                       \
    image_size = [image size];                                                              \
    image_rect.size = image_size;                                                           \
    image_rect.origin.x = 0;                                                                \
    image_rect.origin.y = 0;                                                                \
    frame.origin.x = _x;                                                                    \
    frame.origin.y = _y;                                                                    \
    frame.size = image_size;                                                                \
    [image drawInRect:frame fromRect:image_rect operation:mode fraction:1];

- (void)drawRect:(NSRect)rect
{
    NSRect frame = [self frame];
    NSRect image_rect;
    NSSize image_size;
    NSImage *img;
    addImage(o_background_img, 0, 0, NSCompositeCopy);
    addImage(o_vol_sld_img, 26, 23, NSCompositeSourceOver);
    addImage(o_vol_mute_img, 16, 18, NSCompositeSourceOver);
    addImage(o_vol_max_img, 124, 18, NSCompositeSourceOver);
    addImage(o_time_sld_img, 15, 45, NSCompositeSourceOver);
}

@end

/*****************************************************************************
 * VLCFSTimeSlider
 *****************************************************************************/
@implementation VLCFSTimeSlider
- (void)drawKnobInRect:(NSRect)knobRect
{
    NSRect image_rect;
    NSImage *img = [NSImage imageNamed:@"fs_time_slider_knob_highlight"];
    image_rect.size = [img size];
    image_rect.origin.x = 0;
    image_rect.origin.y = 0;
    knobRect.origin.x += (knobRect.size.width - image_rect.size.width) / 2;
    knobRect.size.width = image_rect.size.width;
    knobRect.size.height = image_rect.size.height;
    [img drawInRect:knobRect fromRect:image_rect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.y+=4;
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    [self drawKnobInRect: knobRect];
}

@end

/*****************************************************************************
* VLCFSVolumeSlider
*****************************************************************************/
@implementation VLCFSVolumeSlider

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if(self) {
        [self setCell:[[[VolumeSliderCell alloc] init] autorelease]];
    }

    return self;
}

- (void)drawKnobInRect:(NSRect) knobRect
{
    NSRect image_rect;
    NSImage *img = [NSImage imageNamed:@"fs_volume_slider_knob_highlight"];
    image_rect.size = [img size];
    image_rect.origin.x = 0;
    image_rect.origin.y = 0;
    knobRect.origin.x += (knobRect.size.width - image_rect.size.width) / 2;
    knobRect.size.width = image_rect.size.width;
    knobRect.size.height = image_rect.size.height;
    [img drawInRect:knobRect fromRect:image_rect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    [self drawFullVolumeMarker];

    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.y+=7.5;
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    [self drawKnobInRect: knobRect];
}

- (void)drawFullVolBezierPath:(NSBezierPath*)bezierPath
{
    CGFloat fullVolPos = [self fullVolumePos];
    [bezierPath moveToPoint:NSMakePoint(fullVolPos, [self frame].size.height)];
    [bezierPath lineToPoint:NSMakePoint(fullVolPos, 1.)];
}

@end

