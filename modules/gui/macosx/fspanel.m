/*****************************************************************************
 * fspanel.m: MacOS X full screen panel
 *****************************************************************************
 * Copyright (C) 2006-2011 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jérôme Decoodt <djc at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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
#import <vlc_aout_intf.h>

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
    if (OSX_LION)
        [win setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    /* let the window sit on top of everything else and start out completely transparent */
    [win setLevel:NSModalPanelWindowLevel];
    i_device = 0;
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

    [self center];
    
    /* get a notification if VLC isn't the active app anymore */
    [[NSNotificationCenter defaultCenter]
    addObserver: self
       selector: @selector(setNonActive:)
           name: NSApplicationDidResignActiveNotification
         object: NSApp];
    
    /* get a notification if VLC is the active app again */
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

    if( hideAgainTimer )
    {
        [hideAgainTimer invalidate];
        [hideAgainTimer release];
    }
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
    {
        /* invalid preferences or none specified, using main screen */
        screen = [NSScreen mainScreen];
    }

    theScreensFrame = [screen frame];
    theWindowsFrame = [self frame];
    
    if( theScreensFrame.size.width >= 1920 ) //  17" MBP, 24"/27" iMacs, external displays
        b_usingBigScreen = YES;

    if( (b_usingBigScreen && theWindowsFrame.size.width < 820) || (!b_usingBigScreen && theWindowsFrame.size.width > 550) )
        [self adaptWindowSizeToScreen];

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

- (void)setStreamPos:(float) f_pos andTime:(NSString *)o_time
{
    [[self contentView] setStreamPos:f_pos andTime: o_time];
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
    [self orderOut: self];
    
    /* here's fadeOut, just without visibly fading */
    b_displayed = NO;
    [self setAlphaValue:0.0];
    [self setFadeTimer:nil];
    b_fadeQueued = NO;
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
    if( b_voutWasUpdated )
    {
        [self orderFront: self];
        b_voutWasUpdated = NO;
    }

    if( [self alphaValue] < 1.0 )
        [self setAlphaValue:[self alphaValue]+0.1];
    if( [self alphaValue] >= 1.0 )
    {
        b_displayed = YES;
        [self setAlphaValue: 1.0];
        [self setFadeTimer:nil];
        if( b_fadeQueued )
        {
            b_fadeQueued=NO;
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(unfocus:) userInfo:NULL repeats:YES]];
        }
    }
}

/* This routine is called repeatedly to hide the window */
- (void)unfocus:(NSTimer *)timer
{
    if( b_keptVisible )
    {
        b_keptVisible = NO;
        b_fadeQueued = NO;
        [self setFadeTimer: NULL];
        [self fadeIn];
        return;
    }
    if( [self alphaValue] > 0.0 )
        [self setAlphaValue:[self alphaValue]-0.05];
    if( [self alphaValue] <= 0.05 )
    {
        b_displayed = NO;
        [self setAlphaValue:0.0];
        [self setFadeTimer:nil];
        if( b_fadeQueued )
        {
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
    vout_thread_t *p_vout = getVout();
    if (p_vout)
    {
        if (var_GetBool( p_vout, "fullscreen" ))
            [[[[VLCMainWindow sharedInstance] videoView] window] makeKeyWindow];
        vlc_object_release( p_vout );
    }
}

- (void)hideMouse
{
    [NSCursor setHiddenUntilMouseMoves: YES];
}

- (void)fadeIn
{
    /* in case that the user don't want us to appear, make sure we hide the mouse */

    if( !config_GetInt( VLCIntf, "macosx-fspanel" ) )
    {
        float time = (float)var_CreateGetInteger( VLCIntf, "mouse-hide-timeout" ) / 1000.;
        [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:time target:self selector:@selector(hideMouse) userInfo:nil repeats:NO]];
        return;
    }

    if( b_nonActive )
        return;

    [self orderFront: nil];
    
    if( [self alphaValue] < 1.0 || b_displayed != YES )
    {
        if (![self fadeTimer])
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(focus:) userInfo:[NSNumber numberWithShort:1] repeats:YES]];
        else if ([[[self fadeTimer] userInfo] shortValue]==0)
            b_fadeQueued=YES;
    }
    [self autoHide];
}

- (void)fadeOut
{
    if( NSPointInRect([NSEvent mouseLocation],[self frame]))
        return;

    if( ( [self alphaValue] > 0.0 ) )
    {
        if (![self fadeTimer])
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(unfocus:) userInfo:[NSNumber numberWithShort:0] repeats:YES]];
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
    if(! b_alreadyCounting )
    {
        i_timeToKeepVisibleInSec = var_CreateGetInteger( VLCIntf, "mouse-hide-timeout" ) / 500;
        if( hideAgainTimer )
        {
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
    if( b_keptVisible )
        b_keptVisible = NO;

    /* count down until we hide ourselfes again and do so if necessary */
    if( --i_timeToKeepVisibleInSec < 1 )
    {
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

- (BOOL)isDisplayed
{
    return b_displayed;
}

- (void)setVoutWasUpdated: (int)i_newdevice;
{
    b_voutWasUpdated = YES;
    if( i_newdevice != i_device )
    {
        i_device = i_newdevice;
        [self center];
    }
}

- (void)adaptWindowSizeToScreen
{
    NSRect theWindowsFrame = [self frame];
    if( b_usingBigScreen )
    {
        theWindowsFrame.size.width = 824;
        theWindowsFrame.size.height = 131;
    }
    else
    {
        theWindowsFrame.size.width = 549;
        theWindowsFrame.size.height = 87;
    }

    [[self contentView] adaptViewSizeToScreen: b_usingBigScreen];

    [self setFrame:theWindowsFrame display:YES animate:YES];
}
@end

/*****************************************************************************
 * FSPanelView
 *****************************************************************************/
@implementation VLCFSPanelView

#define addButton( o_button, imageOff, imageOn, _x, _y, action )                                \
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
    [self addSubview:o_button];

#define addTextfield( class, o_text, align, font, color )                                    \
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

#define restyleButton( o_button, imageOff, imageOn, _x, _y ) \
    [o_button setFrameOrigin: NSMakePoint( _x, _y )]; \
    [o_button setImage: [NSImage imageNamed: imageOff]]; \
    [o_button setAlternateImage: [NSImage imageNamed: imageOn]]; \
    [o_button sizeToFit]; \
    [o_button setNeedsDisplay: YES]

#define restyleTextfieldOrSlider( o_field, _x, _y, _w, _h ) \
    [o_field setFrameOrigin: NSMakePoint( _x, _y )]; \
    [o_field setFrameSize: NSMakeSize( _w, _h )]; \
    [o_field setNeedsDisplay: YES]


- (id)initWithFrame:(NSRect)frameRect
{
    id view = [super initWithFrame:frameRect];
    fillColor = [[NSColor clearColor] retain];
    NSRect s_rc = [self frame];
    addButton( o_prev, @"fs_skip_previous" , @"fs_skip_previous_highlight", 174, 15, prev );
    addButton( o_bwd, @"fs_rewind"        , @"fs_rewind_highlight"       , 211, 14, backward );
    addButton( o_play, @"fs_play"          , @"fs_play_highlight"         , 267, 10, play );
    addButton( o_fwd, @"fs_forward"       , @"fs_forward_highlight"      , 313, 14, forward );
    addButton( o_next, @"fs_skip_next"     , @"fs_skip_next_highlight"    , 365, 15, next );
    addButton( o_fullscreen, @"fs_exit_fullscreen", @"fs_exit_fullscreen_hightlight", 507, 13, toggleFullscreen );
/*
    addButton( o_button, @"image (off state)", @"image (on state)", 38, 51, something );
 */

    /* time slider */
    s_rc = [self frame];
    s_rc.origin.x = 15;
    s_rc.origin.y = 55;
    s_rc.size.width = 518;
    s_rc.size.height = 9;
    o_fs_timeSlider = [[VLCFSTimeSlider alloc] initWithFrame: s_rc];
    [o_fs_timeSlider setMinValue:0];
    [o_fs_timeSlider setMaxValue:10000];
    [o_fs_timeSlider setFloatValue: 0];
    [o_fs_timeSlider setContinuous: YES];
    [o_fs_timeSlider setTarget: self];
    [o_fs_timeSlider setAction: @selector(fsTimeSliderUpdate:)];
    [self addSubview: o_fs_timeSlider];

    /* volume slider */
    s_rc = [self frame];
    s_rc.origin.x = 26;
    s_rc.origin.y = 20;
    s_rc.size.width = 95;
    s_rc.size.height = 10;
    o_fs_volumeSlider = [[VLCFSVolumeSlider alloc] initWithFrame: s_rc];
    [o_fs_volumeSlider setMinValue:0];
    [o_fs_volumeSlider setMaxValue:AOUT_VOLUME_MAX];
    [o_fs_volumeSlider setIntValue:AOUT_VOLUME_DEFAULT];
    [o_fs_volumeSlider setContinuous: YES];
    [o_fs_volumeSlider setTarget: self];
    [o_fs_volumeSlider setAction: @selector(fsVolumeSliderUpdate:)];
    [self addSubview: o_fs_volumeSlider];
    
    /* time counter and stream title output fields */
    s_rc = [self frame];
    s_rc.origin.x = 98;
    s_rc.origin.y = 64;
    s_rc.size.width = 352;
    s_rc.size.height = 14;
    addTextfield( NSTextField, o_streamTitle_txt, NSCenterTextAlignment, systemFontOfSize, whiteColor );
    s_rc.origin.x = 471;
    s_rc.origin.y = 64;
    s_rc.size.width = 65;
    addTextfield( VLCTimeField, o_streamPosition_txt, NSRightTextAlignment, systemFontOfSize, whiteColor );

    return view;
}

- (void)dealloc
{
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
    if( b_usingBigScreen )
    {
        [o_play setImage:[NSImage imageNamed:@"fs_play@x1.5"]];
        [o_play setAlternateImage: [NSImage imageNamed:@"fs_play_highlight@x1.5"]];
    }
    else
    {
        [o_play setImage:[NSImage imageNamed:@"fs_play"]];
        [o_play setAlternateImage: [NSImage imageNamed:@"fs_play_highlight"]];
    }
}

- (void)setPause
{
    if( b_usingBigScreen )
    {
        [o_play setImage: [NSImage imageNamed:@"fs_pause@x1.5"]];
        [o_play setAlternateImage: [NSImage imageNamed:@"fs_pause_highlight@x1.5"]];
    }
    else
    {
        [o_play setImage: [NSImage imageNamed:@"fs_pause"]];
        [o_play setAlternateImage: [NSImage imageNamed:@"fs_pause_highlight"]];
    }
}

- (void)setStreamTitle:(NSString *)o_title
{
    [o_streamTitle_txt setStringValue: o_title];
}

- (void)setStreamPos:(float) f_pos andTime:(NSString *)o_time
{
    [o_streamPosition_txt setStringValue: o_time];
    [o_fs_timeSlider setFloatValue: f_pos];
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
    [[VLCCoreInteraction sharedInstance] play];
}

- (IBAction)forward:(id)sender
{
    [[VLCCoreInteraction sharedInstance] forward];
}

- (IBAction)backward:(id)sender
{
    [[VLCCoreInteraction sharedInstance] backward];
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
    p_input = pl_CurrentInput( VLCIntf );
    if( p_input != NULL )
    {
        vlc_value_t pos;

        pos.f_float = [o_fs_timeSlider floatValue] / 10000.;
        var_Set( p_input, "position", pos );
        vlc_object_release( p_input );
    }
    [[VLCMain sharedInstance] updatePlaybackPosition];
}

- (IBAction)fsVolumeSliderUpdate:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setVolume: [sender intValue]];
}

#define addImage(image, _x, _y, mode, _width)                                               \
    img = [NSImage imageNamed:image];                                              \
    image_rect.size = [img size];                                                           \
    image_rect.origin.x = 0;                                                                \
    image_rect.origin.y = 0;                                                                \
    frame.origin.x = _x;                                                                    \
    frame.origin.y = _y;                                                                    \
    frame.size = [img size];                                                                \
    if( _width ) frame.size.width = _width;                                                 \
    [img drawInRect:frame fromRect:image_rect operation:mode fraction:1];

- (void)drawRect:(NSRect)rect
{
    NSRect frame = [self frame];
    NSRect image_rect;
    NSImage *img;
    if (b_usingBigScreen)
    {
        addImage( @"fs_background@x1.5", 0, 0, NSCompositeCopy, 0 );
        addImage( @"fs_volume_slider_bar@x1.5", 39, 35.5, NSCompositeSourceOver, 0 );
        addImage( @"fs_volume_mute@x1.5", 24, 27, NSCompositeSourceOver, 0 );
        addImage( @"fs_volume_max@x1.5", 186, 27, NSCompositeSourceOver, 0 );
        addImage( @"fs_time_slider@x1.5", 22.5, 79.5, NSCompositeSourceOver, 0);
    }
    else
    {
        addImage( @"fs_background", 0, 0, NSCompositeCopy, 0 );
        addImage( @"fs_volume_slider_bar", 26, 23, NSCompositeSourceOver, 0 );
        addImage( @"fs_volume_mute", 16, 18, NSCompositeSourceOver, 0 );
        addImage( @"fs_volume_max", 124, 18, NSCompositeSourceOver, 0 );
        addImage( @"fs_time_slider", 15, 53, NSCompositeSourceOver, 0);
    }
}

- (void)adaptViewSizeToScreen:(BOOL)b_value
{
    b_usingBigScreen = b_value;

    if (b_usingBigScreen)
    {
        restyleButton( o_prev, @"fs_skip_previous@x1.5", @"fs_skip_previous_highlight@x1.5", 261, 22.5 );
        restyleButton( o_bwd, @"fs_rewind@x1.5", @"fs_rewind_highlight@x1.5", 316.5, 21 );
        restyleButton( o_play, @"fs_play@x1.5", @"fs_play_highlight@x1.5", 400.5, 15 );
        restyleButton( o_fwd, @"fs_forward@x1.5", @"fs_forward_highlight@x1.5", 469.5, 21 );
        restyleButton( o_next, @"fs_skip_next@x1.5", @"fs_skip_next_highlight@x1.5", 547.5, 22.5 );
        restyleButton( o_fullscreen, @"fs_exit_fullscreen@x1.5", @"fs_exit_fullscreen_hightlight@x1.5", 765.5, 19.5 );
        restyleTextfieldOrSlider( o_streamTitle_txt, 148, 96, 528, 21 );
        [o_streamTitle_txt setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
        restyleTextfieldOrSlider( o_streamPosition_txt, 718, 96, 82.5, 21 );
        [o_streamPosition_txt setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
        restyleTextfieldOrSlider( o_fs_timeSlider, 22.5, 82.5, 777, 13.5 );
        restyleTextfieldOrSlider( o_fs_volumeSlider, 39, 32, 142.5, 15);
    }
    else
    {
        restyleButton( o_prev, @"fs_skip_previous", @"fs_skip_previous_highlight", 174, 15 );
        restyleButton( o_bwd, @"fs_rewind", @"fs_rewind_highlight", 211, 14 );
        restyleButton( o_play, @"fs_play", @"fs_play_highlight", 267, 10 );
        restyleButton( o_fwd, @"fs_forward", @"fs_forward_highlight", 313, 14 );
        restyleButton( o_next, @"fs_skip_next", @"fs_skip_next_highlight", 365, 15 );
        restyleButton( o_fullscreen, @"fs_exit_fullscreen", @"fs_exit_fullscreen_hightlight", 507, 13 );
        restyleTextfieldOrSlider( o_streamTitle_txt, 98, 64, 352, 14 );
        [o_streamTitle_txt setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
        restyleTextfieldOrSlider( o_streamPosition_txt, 471, 64, 65, 14);
        [o_streamPosition_txt setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
        restyleTextfieldOrSlider( o_fs_timeSlider, 15, 55, 518, 9 );
        restyleTextfieldOrSlider( o_fs_volumeSlider, 26, 20, 95, 10);
    }
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
    knobRect.origin.y+=7.5;
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    [self drawKnobInRect: knobRect];
}

@end

/*****************************************************************************
* VLCFSVolumeSlider
*****************************************************************************/
@implementation VLCFSVolumeSlider
- (void)drawKnobInRect:(NSRect) knobRect
{
    NSRect image_rect;
    NSImage *img = [NSImage imageNamed:@"fs_volume_slider_knob"];
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
    knobRect.origin.y+=6;
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    [self drawKnobInRect: knobRect];
}

@end

