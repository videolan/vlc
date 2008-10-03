/*****************************************************************************
 * embeddedwindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
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

/* DisableScreenUpdates, SetSystemUIMode, ... */
#import <QuickTime/QuickTime.h>

#import "intf.h"
#import "controls.h"
#import "vout.h"
#import "embeddedwindow.h"
#import "fspanel.h"

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
    o_img_pause = [NSImage imageNamed: @"pause_embedded"];
    [self controlTintChanged];
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector( controlTintChanged )
                                                 name: NSControlTintDidChangeNotification
                                               object: nil];

    /* Useful to save o_view frame in fullscreen mode */
    o_temp_view = [[NSView alloc] init];
    [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];

    o_fullscreen_window = nil;
    o_fullscreen_anim1 = o_fullscreen_anim2 = nil;

    /* Not fullscreen when we wake up */
    [o_btn_fullscreen setState: NO];
    b_fullscreen = NO;

    [self setMovableByWindowBackground:YES];

    [self setDelegate:self];

    /* Make sure setVisible: returns NO */
    [self orderOut:self];
    b_window_is_invisible = YES;
    videoRatio = NSMakeSize( 0., 0. );
}

- (void)controlTintChanged
{
    BOOL b_playing = NO;
    if( [o_btn_play alternateImage] == o_img_play_pressed )
        b_playing = YES;
    
    if( [NSColor currentControlTint] == NSGraphiteControlTint )
    {
        o_img_play_pressed = [NSImage imageNamed: @"play_embedded_graphite"];
        o_img_pause_pressed = [NSImage imageNamed: @"pause_embedded_graphite"];
        [o_btn_backward setAlternateImage: [NSImage imageNamed: @"skip_previous_embedded_graphite"]];
        [o_btn_forward setAlternateImage: [NSImage imageNamed: @"skip_forward_embedded_graphite"]];
        [o_btn_fullscreen setAlternateImage: [NSImage imageNamed: @"fullscreen_graphite"]];
    }
    else
    {
        o_img_play_pressed = [NSImage imageNamed: @"play_embedded_blue"];
        o_img_pause_pressed = [NSImage imageNamed: @"pause_embedded_blue"];
        [o_btn_backward setAlternateImage: [NSImage imageNamed: @"skip_previous_embedded_blue"]];
        [o_btn_forward setAlternateImage: [NSImage imageNamed: @"skip_forward_embedded_blue"]];
        [o_btn_fullscreen setAlternateImage: [NSImage imageNamed: @"fullscreen_blue"]];
    }
    
    if( b_playing )
        [o_btn_play setAlternateImage: o_img_play_pressed];
    else
        [o_btn_play setAlternateImage: o_img_pause_pressed];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [o_img_play release];
    [o_img_play_pressed release];
    [o_img_pause release];
    [o_img_pause_pressed release];
    
    [super dealloc];
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

- (BOOL)windowShouldZoom:(NSWindow *)sender toFrame:(NSRect)newFrame
{
    [self setFrame: newFrame display: YES animate: YES];
    return NO;
}

- (BOOL)windowShouldClose:(id)sender
{
    playlist_t * p_playlist = pl_Hold( VLCIntf );

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
    return YES;
}

- (NSView *)mainView
{
    if (o_fullscreen_window)
        return o_temp_view;
    else
        return o_view;
}

- (void)setVideoRatio:(NSSize)ratio
{
    videoRatio = ratio;
}

- (NSSize)windowWillResize:(NSWindow *)window toSize:(NSSize)proposedFrameSize
{
    if( videoRatio.height == 0. || videoRatio.width == 0. )
        return proposedFrameSize;

    NSRect viewRect = [o_view convertRect:[o_view bounds] toView: nil];
    NSRect contentRect = [self contentRectForFrameRect:[self frame]];
    float marginy = viewRect.origin.y + [self frame].size.height - contentRect.size.height;
    float marginx = contentRect.size.width - viewRect.size.width;
    proposedFrameSize.height = (proposedFrameSize.width - marginx) * videoRatio.height / videoRatio.width + marginy;

    return proposedFrameSize;
}

/*****************************************************************************
 * Fullscreen support
 */

- (BOOL)isFullscreen
{
    return b_fullscreen;
}

- (void)lockFullscreenAnimation
{
    [o_animation_lock lock];
}

- (void)unlockFullscreenAnimation
{
    [o_animation_lock unlock];
}

- (void)enterFullscreen
{
    NSMutableDictionary *dict1, *dict2;
    NSScreen *screen;
    NSRect screen_rect;
    NSRect rect;
    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    BOOL blackout_other_displays = config_GetInt( VLCIntf, "macosx-black" );

    screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_GetInteger( p_vout, "video-device" )]; 
 
    [self lockFullscreenAnimation];

    if (!screen)
    {
        msg_Dbg( p_vout, "chosen screen isn't present, using current screen for fullscreen mode" );
        screen = [self screen];
    }
    if (!screen)
    {
        msg_Dbg( p_vout, "Using deepest screen" );
        screen = [NSScreen deepestScreen];
    }

    vlc_object_release( p_vout );

    screen_rect = [screen frame];

    [o_btn_fullscreen setState: YES];

    [NSCursor setHiddenUntilMouseMoves: YES];
 
    if( blackout_other_displays )        
        [screen blackoutOtherScreens];

    /* Make sure we don't see the window flashes in float-on-top mode */
    originalLevel = [self level];
    [self setLevel:NSNormalWindowLevel];

    /* Only create the o_fullscreen_window if we are not in the middle of the zooming animation */
    if (!o_fullscreen_window)
    {
        /* We can't change the styleMask of an already created NSWindow, so we create an other window, and do eye catching stuff */

        rect = [[o_view superview] convertRect: [o_view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [self frame].origin.x;
        rect.origin.y += [self frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];

        if (![self isVisible] || [self alphaValue] == 0.0 || MACOS_VERSION < 10.4f)
        {
            /* We don't animate if we are not visible or if we are running on
             * Mac OS X <10.4 which doesn't support NSAnimation, instead we
             * simply fade the display */
            CGDisplayFadeReservationToken token;
 
            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade( token, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );
 
            if ([screen isMainScreen])
                SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
 
            [[self contentView] replaceSubview:o_view with:o_temp_view];
            [o_temp_view setFrame:[o_view frame]];
            [o_fullscreen_window setContentView:o_view];

            [o_fullscreen_window makeKeyAndOrderFront:self];
            [o_fullscreen_window orderFront:self animate:YES];

            [o_fullscreen_window setFrame:screen_rect display:YES];

            CGDisplayFade( token, 0.3, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
            CGReleaseDisplayFadeReservation( token);

            /* Will release the lock */
            [self hasBecomeFullscreen];

            return;
        }
 
        /* Make sure we don't see the o_view disappearing of the screen during this operation */
        DisableScreenUpdates();
        [[self contentView] replaceSubview:o_view with:o_temp_view];
        [o_temp_view setFrame:[o_view frame]];
        [o_fullscreen_window setContentView:o_view];
        [o_fullscreen_window makeKeyAndOrderFront:self];
        EnableScreenUpdates();
    }

    if (MACOS_VERSION < 10.4f)
    {
        /* We were already fullscreen nothing to do when NSAnimation
         * is not supported */
        [self unlockFullscreenAnimation];
        return;
    }

    /* We are in fullscreen (and no animation is running) */
    if (b_fullscreen)
    {
        /* Make sure we are hidden */
        [super orderOut: self];
        [self unlockFullscreenAnimation];
        return;
    }

    if (o_fullscreen_anim1)
    {
        [o_fullscreen_anim1 stopAnimation];
        [o_fullscreen_anim1 release];
    }
    if (o_fullscreen_anim2)
    {
        [o_fullscreen_anim2 stopAnimation];
        [o_fullscreen_anim2 release];
    }
 
    if ([screen isMainScreen])
        SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:2];
    dict2 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:self forKey:NSViewAnimationTargetKey];
    [dict1 setObject:NSViewAnimationFadeOutEffect forKey:NSViewAnimationEffectKey];

    [dict2 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict2 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict2 setObject:[NSValue valueWithRect:screen_rect] forKey:NSViewAnimationEndFrameKey];

    /* Strategy with NSAnimation allocation:
        - Keep at most 2 animation at a time
        - leaveFullscreen/enterFullscreen are the only responsible for releasing and alloc-ing
    */
    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict1, nil]];
    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict2, nil]];

    [dict1 release];
    [dict2 release];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.3];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.2];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */
}

- (void)hasBecomeFullscreen
{
    [o_fullscreen_window makeFirstResponder: [[[VLCMain sharedInstance] getControls] voutView]];

    [o_fullscreen_window makeKeyWindow];
    [o_fullscreen_window setAcceptsMouseMovedEvents: TRUE];

    /* tell the fspanel to move itself to front next time it's triggered */
    [[[[VLCMain sharedInstance] getControls] getFSPanel] setVoutWasUpdated: (int)[[o_fullscreen_window screen] displayID]];

    if([self isVisible])
        [super orderOut: self];

    [[[[VLCMain sharedInstance] getControls] getFSPanel] setActive: nil];

    b_fullscreen = YES;
    [self unlockFullscreenAnimation];
}

- (void)leaveFullscreen
{
    [self leaveFullscreenAndFadeOut: NO];
}

- (void)leaveFullscreenAndFadeOut: (BOOL)fadeout
{
    NSMutableDictionary *dict1, *dict2;
    NSRect frame;

    [self lockFullscreenAnimation];

    b_fullscreen = NO;
    [o_btn_fullscreen setState: NO];

    /* We always try to do so */
    [NSScreen unblackoutScreens];

    /* Don't do anything if o_fullscreen_window is already closed */
    if (!o_fullscreen_window)
    {
        [self unlockFullscreenAnimation];
        return;
    }

    if (fadeout || MACOS_VERSION < 10.4f)
    {
        /* We don't animate if we are not visible or if we are running on
        * Mac OS X <10.4 which doesn't support NSAnimation, instead we
        * simply fade the display */
        CGDisplayFadeReservationToken token;

        CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
        CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );

        [[[[VLCMain sharedInstance] getControls] getFSPanel] setNonActive: nil];
        SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);

        /* Will release the lock */
        [self hasEndedFullscreen];

        /* Our window is hidden, and might be faded. We need to workaround that, so note it
         * here */
        b_window_is_invisible = YES;

        CGDisplayFade( token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0, 0, 0, NO );
        CGReleaseDisplayFadeReservation( token);
        return;
    }

    [self setAlphaValue: 0.0];
    [self orderFront: self];

    [[[[VLCMain sharedInstance] getControls] getFSPanel] setNonActive: nil];
    SetSystemUIMode( kUIModeNormal, kUIOptionAutoShowMenuBar);

    if (o_fullscreen_anim1)
    {
        [o_fullscreen_anim1 stopAnimation];
        [o_fullscreen_anim1 release];
    }
    if (o_fullscreen_anim2)
    {
        [o_fullscreen_anim2 stopAnimation];
        [o_fullscreen_anim2 release];
    }

    frame = [[o_temp_view superview] convertRect: [o_temp_view frame] toView: nil]; /* Convert to Window base coord */
    frame.origin.x += [self frame].origin.x;
    frame.origin.y += [self frame].origin.y;

    dict2 = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict2 setObject:self forKey:NSViewAnimationTargetKey];
    [dict2 setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict2, nil]];
    [dict2 release];

    [o_fullscreen_anim2 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim2 setDuration: 0.3];
    [o_fullscreen_anim2 setFrameRate: 30];

    [o_fullscreen_anim2 setDelegate: self];

    dict1 = [[NSMutableDictionary alloc] initWithCapacity:3];

    [dict1 setObject:o_fullscreen_window forKey:NSViewAnimationTargetKey];
    [dict1 setObject:[NSValue valueWithRect:[o_fullscreen_window frame]] forKey:NSViewAnimationStartFrameKey];
    [dict1 setObject:[NSValue valueWithRect:frame] forKey:NSViewAnimationEndFrameKey];

    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict1, nil]];
    [dict1 release];

    [o_fullscreen_anim1 setAnimationBlockingMode: NSAnimationNonblocking];
    [o_fullscreen_anim1 setDuration: 0.2];
    [o_fullscreen_anim1 setFrameRate: 30];
    [o_fullscreen_anim2 startWhenAnimation: o_fullscreen_anim1 reachesProgress: 1.0];

    /* Make sure o_fullscreen_window is the frontmost window */
    [o_fullscreen_window orderFront: self];

    [o_fullscreen_anim1 startAnimation];
    /* fullscreenAnimation will be unlocked when animation ends */
}

- (void)hasEndedFullscreen
{
    /* This function is private and should be only triggered at the end of the fullscreen change animation */
    /* Make sure we don't see the o_view disappearing of the screen during this operation */
    DisableScreenUpdates();
    [o_view retain];
    [o_view removeFromSuperviewWithoutNeedingDisplay];
    [[self contentView] replaceSubview:o_temp_view with:o_view];
    [o_view release];
    [o_view setFrame:[o_temp_view frame]];
    [self makeFirstResponder: o_view];
    if ([self isVisible])
        [super makeKeyAndOrderFront:self]; /* our version contains a workaround */
    [o_fullscreen_window orderOut: self];
    EnableScreenUpdates();

    [o_fullscreen_window release];
    o_fullscreen_window = nil;
    [self setLevel:originalLevel];

    [self unlockFullscreenAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    NSArray *viewAnimations;

    if ([animation currentValue] < 1.0)
        return;

    /* Fullscreen ended or started (we are a delegate only for leaveFullscreen's/enterFullscren's anim2) */
    viewAnimations = [o_fullscreen_anim2 viewAnimations];
    if ([viewAnimations count] >=1 &&
        [[[viewAnimations objectAtIndex: 0] objectForKey: NSViewAnimationEffectKey] isEqualToString:NSViewAnimationFadeInEffect])
    {
        /* Fullscreen ended */
        [self hasEndedFullscreen];
    }
    else
    {
        /* Fullscreen started */
        [self hasBecomeFullscreen];
    }
}

- (void)orderOut: (id)sender
{
    [super orderOut: sender];

    /* Make sure we leave fullscreen */
    [self leaveFullscreenAndFadeOut: YES];
}

- (void)makeKeyAndOrderFront: (id)sender
{
    /* Hack
     * when we exit fullscreen and fade out, we may endup in
     * having a window that is faded. We can't have it fade in unless we
     * animate again. */

    if(!b_window_is_invisible)
    {
        /* Make sure we don't do it too much */
        [super makeKeyAndOrderFront: sender];
        return;
    }

    [super setAlphaValue:0.0f];
    [super makeKeyAndOrderFront: sender];

    NSMutableDictionary * dict = [[[NSMutableDictionary alloc] initWithCapacity:2] autorelease];
    [dict setObject:self forKey:NSViewAnimationTargetKey];
    [dict setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    NSViewAnimation * anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];

    [anim setAnimationBlockingMode: NSAnimationNonblocking];
    [anim setDuration: 0.1];
    [anim setFrameRate: 30];

    [anim startAnimation];
    b_window_is_invisible = NO;

    /* fullscreenAnimation will be unlocked when animation ends */
}



/* Make sure setFrame gets executed on main thread especially if we are animating.
 * (Thus we won't block the video output thread) */
- (void)setFrame:(NSRect)frame display:(BOOL)display animate:(BOOL)animate
{
    struct { NSRect frame; BOOL display; BOOL animate;} args;
    NSData *packedargs;

    args.frame = frame;
    args.display = display;
    args.animate = animate;

    packedargs = [NSData dataWithBytes:&args length:sizeof(args)];

    [self performSelectorOnMainThread:@selector(setFrameOnMainThread:)
                    withObject: packedargs waitUntilDone: YES];
}

- (void)setFrameOnMainThread:(NSData*)packedargs
{
    struct args { NSRect frame; BOOL display; BOOL animate; } * args = (struct args*)[packedargs bytes];

    if( args->animate )
    {
        /* Make sure we don't block too long and set up a non blocking animation */
        NSDictionary * dict = [NSDictionary dictionaryWithObjectsAndKeys:
            self, NSViewAnimationTargetKey,
            [NSValue valueWithRect:[self frame]], NSViewAnimationStartFrameKey,
            [NSValue valueWithRect:args->frame], NSViewAnimationEndFrameKey, nil];

        NSViewAnimation * anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];

        [anim setAnimationBlockingMode: NSAnimationNonblocking];
        [anim setDuration: 0.4];
        [anim setFrameRate: 30];
        [anim startAnimation];
    }
    else {
        [super setFrame:args->frame display:args->display animate:args->animate];
    }

}
@end
