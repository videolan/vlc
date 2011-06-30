/*****************************************************************************
 * embeddedwindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2005-2011 the VideoLAN team
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

#import "intf.h"
#import "controls.h"
#import "vout.h"
#import "embeddedwindow.h"
#import "fspanel.h"
#import "CoreInteraction.h"
#import "playlist.h"
#import <vlc_url.h>

/* SetSystemUIMode, ... */
#import <Carbon/Carbon.h>

/*****************************************************************************
 * VLCEmbeddedWindow Implementation
 *****************************************************************************/

@implementation VLCEmbeddedWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask: (NSUInteger)windowStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation
{
    BOOL b_useTextured = YES;
    if( [[NSWindow class] instancesRespondToSelector:@selector(setContentBorderThickness:forEdge:)] )
    {
        b_useTextured = NO;
        windowStyle ^= NSTexturedBackgroundWindowMask;
    }
    self = [super initWithContentRect:contentRect styleMask:windowStyle backing:bufferingType defer:deferCreation];
    if(! b_useTextured )
    {
        [self setContentBorderThickness:28.0 forEdge:NSMinYEdge];
    }
    return self;
}

- (void)awakeFromNib
{
    [self setDelegate: self];

    /* button strings */
    [o_btn_backward setToolTip: _NS("Rewind")];
    [o_btn_forward setToolTip: _NS("Fast Forward")];
    [o_btn_fullscreen setToolTip: _NS("Fullscreen")];
    [o_btn_play setToolTip: _NS("Play")];
    [o_timeslider setToolTip: _NS("Position")];
    [o_btn_prev setToolTip: _NS("Previous")];
    [o_btn_stop setToolTip: _NS("Stop")];
    [o_btn_next setToolTip: _NS("Next")];
    [o_volumeslider setToolTip: _NS("Volume")];
    [o_btn_playlist setToolTip: _NS("Playlist")];
    [self setTitle: _NS("VLC media player")];

    o_img_play = [NSImage imageNamed: @"play_embedded"];
    o_img_pause = [NSImage imageNamed: @"pause_embedded"];

    /* Set color of sidebar to Leopard's "Sidebar Blue" */
    [o_sidebar_list setBackgroundColor: [NSColor colorWithCalibratedRed:0.820
                                                                  green:0.843
                                                                   blue:0.886
                                                                  alpha:1.0]];

    [self setMinSize:NSMakeSize([o_sidebar_list convertRect:[o_sidebar_list bounds]
                                                     toView: nil].size.width + 551., 114.)];

    /* Useful to save o_view frame in fullscreen mode */
    o_temp_view = [[NSView alloc] init];
    [o_temp_view setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];

    o_fullscreen_window = nil;
    o_makekey_anim = o_fullscreen_anim1 = o_fullscreen_anim2 = nil;

    /* Not fullscreen when we wake up */
    [o_btn_fullscreen setState: NO];
    b_fullscreen = NO;

    [self setMovableByWindowBackground:YES];

    [self setDelegate:self];

    /* Make sure setVisible: returns NO */
    [self orderOut:self];
    b_window_is_invisible = YES;
    videoRatio = NSMakeSize( 0., 0. );

    /* enlarge the time slider and move items around in case we have no window resize control */
    if ([self showsResizeIndicator] == NO) {
        NSRect view_rect;
        view_rect = [o_backgroundimg_right frame];
        
        [o_backgroundimg_right setFrame: NSMakeRect( view_rect.origin.x+15,
                                                    view_rect.origin.y,
                                                    view_rect.size.width,
                                                    view_rect.size.height )];
        
        view_rect = [o_backgroundimg_middle frame];
        [o_backgroundimg_middle setFrame: NSMakeRect( view_rect.origin.x,
                                                     view_rect.origin.y,
                                                     view_rect.size.width+15,
                                                     view_rect.size.height )];
        
        view_rect = [o_timeslider frame];
        [o_timeslider setFrame: NSMakeRect( view_rect.origin.x,
                                           view_rect.origin.y,
                                           view_rect.size.width+15,
                                           view_rect.size.height )];
        
        view_rect = [o_time frame];
        [o_time setFrame: NSMakeRect( view_rect.origin.x+15,
                                     view_rect.origin.y,
                                     view_rect.size.width,
                                     view_rect.size.height )];
    }

    /* we don't want this window to be restored on relaunch */
    if ([self respondsToSelector:@selector(setRestorable:)])
        [self setRestorable:NO];
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
    } else {
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
    [o_timeslider setFloatValue: f_position];
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
    [o_timeslider setEnabled: b_seekable];
}

- (void)setScrollString:(NSString *)o_string
{
    [o_scrollfield setStringValue: o_string];
}

- (id)getPgbar
{
    if( o_main_pgbar )
        return o_main_pgbar;

    return nil;
}

- (void)setStop:(BOOL)b_input
{
    [o_btn_stop setEnabled: b_input];
}

- (void)setNext:(BOOL)b_input
{
    [o_btn_next setEnabled: b_input];
}

- (void)setPrev:(BOOL)b_input
{
    [o_btn_prev setEnabled: b_input];
}

- (void)setVolumeEnabled:(BOOL)b_input
{
    [o_volumeslider setEnabled: b_input];
}

- (void)setVolumeSlider:(float)f_level
{
    [o_volumeslider setFloatValue: f_level];
}

- (BOOL)windowShouldZoom:(NSWindow *)sender toFrame:(NSRect)newFrame
{
    [self setFrame: newFrame display: YES animate: YES];
    return NO;
}

- (BOOL)windowShouldClose:(id)sender
{
    playlist_t * p_playlist = pl_Get( VLCIntf );
    playlist_Stop( p_playlist );

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

    if( [[VLCCoreInteraction sharedInstance] aspectRatioIsLocked] )
    {
        NSRect viewRect = [o_view convertRect:[o_view bounds] toView: nil];
        NSRect contentRect = [self contentRectForFrameRect:[self frame]];
        float marginy = viewRect.origin.y + [self frame].size.height - contentRect.size.height;
        float marginx = contentRect.size.width - viewRect.size.width;
        proposedFrameSize.height = (proposedFrameSize.width - marginx) * videoRatio.height / videoRatio.width + marginy;
    }

    return proposedFrameSize;
}

- (void)becomeMainWindow
{
    [o_sidebar_list setBackgroundColor: [NSColor colorWithCalibratedRed:0.820
                                                                  green:0.843
                                                                   blue:0.886
                                                                  alpha:1.0]];
	[o_status becomeMainWindow];
    [super becomeMainWindow];
}

- (void)resignMainWindow
{
    [o_sidebar_list setBackgroundColor: [NSColor colorWithCalibratedWhite:0.91 alpha:1.0]];
	[o_status resignMainWindow];
    [super resignMainWindow];
}

- (CGFloat)splitView:(NSSplitView *) splitView constrainSplitPosition:(CGFloat) proposedPosition ofSubviewAt:(NSInteger) index
{
	if([splitView isVertical])
		return proposedPosition;
	else if ( splitView == o_vertical_split )
		return proposedPosition ;
	else {
		float bottom = [splitView frame].size.height - [splitView dividerThickness];
		if(proposedPosition > bottom - 50) {
			[o_btn_playlist setState: NSOffState];
			[o_searchfield setHidden:YES];
			[o_playlist_view setHidden:YES];
			return bottom;
		}
		else {
			[o_btn_playlist setState: NSOnState];
			[o_searchfield setHidden:NO];
			[o_playlist_view setHidden:NO];
			[o_playlist swapPlaylists: o_playlist_table];
			[o_vlc_main togglePlaylist:self];
			return proposedPosition;
		}
	}
}

- (void)splitViewWillResizeSubviews:(NSNotification *) notification
{

}

- (CGFloat)splitView:(NSSplitView *) splitView constrainMinCoordinate:(CGFloat) proposedMin ofSubviewAt:(NSInteger) offset
{
	if([splitView isVertical])
		return 125.;
	else
		return 0.;
}

- (CGFloat)splitView:(NSSplitView *) splitView constrainMaxCoordinate:(CGFloat) proposedMax ofSubviewAt:(NSInteger) offset
{
    if([splitView isVertical])
		return MIN([self frame].size.width - 551, 300);
	else
		return [splitView frame].size.height;
}

- (BOOL)splitView:(NSSplitView *) splitView canCollapseSubview:(NSView *) subview
{
	if([splitView isVertical])
		return NO;
	else
		return NO;
}

- (NSRect)splitView:(NSSplitView *)splitView effectiveRect:(NSRect)proposedEffectiveRect forDrawnRect:(NSRect)drawnRect
   ofDividerAtIndex:(NSInteger)dividerIndex
{
	if([splitView isVertical]) {
		drawnRect.origin.x -= 3;
		drawnRect.size.width += 5;
		return drawnRect;
	}
	else
		return drawnRect;
}

- (IBAction)togglePlaylist:(id)sender
{
	NSView *playback_area = [[o_vertical_split subviews] objectAtIndex:0];
	NSView *playlist_area = [[o_vertical_split subviews] objectAtIndex:1];
	NSRect newVid = [playback_area frame];
	NSRect newList = [playlist_area frame];
	if(newList.size.height < 50 && sender != self && sender != o_vlc_main) {
		newList.size.height = newVid.size.height/2;
		newVid.size.height = newVid.size.height/2;
		newVid.origin.y = newVid.origin.y + newList.size.height;
		[o_btn_playlist setState: NSOnState];
		[o_searchfield setHidden:NO];
		[o_playlist_view setHidden:NO];
		[o_playlist swapPlaylists: o_playlist_table];
		[o_vlc_main togglePlaylist:self];
	}
	else {
		newVid.size.height = newVid.size.height + newList.size.height;
		newList.size.height = 0;
		newVid.origin.y = 0;
		[o_btn_playlist setState: NSOffState];
		[o_searchfield setHidden:YES];
		[o_playlist_view setHidden:YES];
	}
	[playback_area setFrame: newVid];
	[playlist_area setFrame: newList];
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
    vout_thread_t *p_vout = getVout();
    BOOL blackout_other_displays = config_GetInt( VLCIntf, "macosx-black" );

    if( p_vout )
        screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_GetInteger( p_vout, "video-device" )];

    [self lockFullscreenAnimation];

    if (!screen)
    {
        msg_Dbg( VLCIntf, "chosen screen isn't present, using current screen for fullscreen mode" );
        screen = [self screen];
    }
    if (!screen)
    {
        msg_Dbg( VLCIntf, "Using deepest screen" );
        screen = [NSScreen deepestScreen];
    }

    if( p_vout )
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
        /* We can't change the styleMask of an already created NSWindow, so we create another window, and do eye catching stuff */

        rect = [[o_view superview] convertRect: [o_view frame] toView: nil]; /* Convert to Window base coord */
        rect.origin.x += [self frame].origin.x;
        rect.origin.y += [self frame].origin.y;
        o_fullscreen_window = [[VLCWindow alloc] initWithContentRect:rect styleMask: NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
        [o_fullscreen_window setBackgroundColor: [NSColor blackColor]];
        [o_fullscreen_window setCanBecomeKeyWindow: YES];

        if (![self isVisible] || [self alphaValue] == 0.0)
        {
            /* We don't animate if we are not visible, instead we
             * simply fade the display */
            CGDisplayFadeReservationToken token;

            CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
            CGDisplayFade( token, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );

            if ([screen isMainScreen])
                SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);

            [[o_view superview] replaceSubview:o_view with:o_temp_view];
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
        NSDisableScreenUpdates();
        [[o_view superview] replaceSubview:o_view with:o_temp_view];
        [o_temp_view setFrame:[o_view frame]];
        [o_fullscreen_window setContentView:o_view];
        [o_fullscreen_window makeKeyAndOrderFront:self];
        NSEnableScreenUpdates();
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
    o_fullscreen_anim1 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict1]];
    o_fullscreen_anim2 = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict2]];

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
    [o_fullscreen_window makeFirstResponder: [[[VLCMain sharedInstance] controls] voutView]];

    [o_fullscreen_window makeKeyWindow];
    [o_fullscreen_window setAcceptsMouseMovedEvents: TRUE];

    /* tell the fspanel to move itself to front next time it's triggered */
    [[[[VLCMain sharedInstance] controls] fspanel] setVoutWasUpdated: (int)[[o_fullscreen_window screen] displayID]];

    if([self isVisible])
        [super orderOut: self];

    [[[[VLCMain sharedInstance] controls] fspanel] setActive: nil];

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

    if (fadeout)
    {
        /* We don't animate if we are not visible, instead we
        * simply fade the display */
        CGDisplayFadeReservationToken token;

        CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &token);
        CGDisplayFade( token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, YES );

        [[[[VLCMain sharedInstance] controls] fspanel] setNonActive: nil];
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

    [[[[VLCMain sharedInstance] controls] fspanel] setNonActive: nil];
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
    NSDisableScreenUpdates();
    [o_view retain];
    [o_view removeFromSuperviewWithoutNeedingDisplay];
    [[o_temp_view superview] replaceSubview:o_temp_view with:o_view];
    [o_view release];
    [o_view setFrame:[o_temp_view frame]];
    [self makeFirstResponder: o_view];
    if ([self isVisible])
        [super makeKeyAndOrderFront:self]; /* our version contains a workaround */
    [o_fullscreen_window orderOut: self];
    NSEnableScreenUpdates();

    [o_fullscreen_window release];
    o_fullscreen_window = nil;
    [self setLevel:originalLevel];

    [self unlockFullscreenAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation
{
    NSArray *viewAnimations;
    if( o_makekey_anim == animation )
    {
        [o_makekey_anim release];
        return;
    }
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

    NSMutableDictionary * dict = [[NSMutableDictionary alloc] initWithCapacity:2];
    [dict setObject:self forKey:NSViewAnimationTargetKey];
    [dict setObject:NSViewAnimationFadeInEffect forKey:NSViewAnimationEffectKey];

    o_makekey_anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];
    [dict release];

    [o_makekey_anim setAnimationBlockingMode: NSAnimationNonblocking];
    [o_makekey_anim setDuration: 0.1];
    [o_makekey_anim setFrameRate: 30];
    [o_makekey_anim setDelegate: self];

    [o_makekey_anim startAnimation];
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

        NSViewAnimation * anim = [[NSViewAnimation alloc] initWithViewAnimations:[NSArray arrayWithObject:dict]];
        [dict release];

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

/*****************************************************************************
 * embeddedbackground
 *****************************************************************************/


@implementation embeddedbackground

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSTIFFPboardType,
                                   NSFilenamesPboardType, nil]];
    [self addSubview: o_timeslider];
    [self addSubview: o_scrollfield];
    [self addSubview: o_time];
    [self addSubview: o_main_pgbar];
    [self addSubview: o_btn_backward];
    [self addSubview: o_btn_forward];
    [self addSubview: o_btn_fullscreen];
    [self addSubview: o_btn_equalizer];
    [self addSubview: o_btn_playlist];
    [self addSubview: o_btn_play];
    [self addSubview: o_btn_prev];
    [self addSubview: o_btn_stop];
    [self addSubview: o_btn_next];
    [self addSubview: o_btn_volume_down];
    [self addSubview: o_volumeslider];
    [self addSubview: o_btn_volume_up];
    [self addSubview: o_searchfield];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask])
        == NSDragOperationGeneric)
    {
        return NSDragOperationGeneric;
    }
    else
    {
        return NSDragOperationNone;
    }
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObjects: NSFilenamesPboardType, nil];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];
    BOOL b_autoplay = config_GetInt( VLCIntf, "macosx-autoplay" );

    if( o_carried_data )
    {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType])
        {
            int i;
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType]
                                 sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];

            for( i = 0; i < (int)[o_values count]; i++)
            {
                NSDictionary *o_dic;
                char *psz_uri = make_URI([[o_values objectAtIndex:i] UTF8String], NULL);
                if( !psz_uri )
                    continue;

                o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
                free( psz_uri );

                o_array = [o_array arrayByAddingObject: o_dic];
            }
            if( b_autoplay )
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            else
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:YES];
            return YES;
        }
    }
    [self setNeedsDisplay:YES];
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)rect
{
    NSImage *leftImage = [NSImage imageNamed:@"display_left"];
    NSImage *middleImage = [NSImage imageNamed:@"display_middle"];
    NSImage *rightImage = [NSImage imageNamed:@"display_right"];
    [middleImage setSize:NSMakeSize(NSWidth( [self bounds] ) - 134 - [leftImage size].width - [rightImage size].width, [middleImage size].height)];
    [middleImage setScalesWhenResized:YES];
    [leftImage compositeToPoint:NSMakePoint( 122., 40. ) operation:NSCompositeSourceOver];
    [middleImage compositeToPoint:NSMakePoint( 122. + [leftImage size].width, 40. ) operation:NSCompositeSourceOver];
    [rightImage compositeToPoint:NSMakePoint( NSWidth( [self bounds] ) - 12 - [rightImage size].width, 40. ) operation:NSCompositeSourceOver];
}

- (void)mouseDown:(NSEvent *)event
{
    dragStart = [self convertPoint:[event locationInWindow] fromView:nil];
}

- (void)mouseDragged:(NSEvent *)event
{
    NSPoint dragLocation = [self convertPoint:[event locationInWindow] fromView:nil];
    NSPoint winOrigin = [o_window frame].origin;

    NSPoint newOrigin = NSMakePoint(winOrigin.x + (dragLocation.x - dragStart.x),
                                    winOrigin.y + (dragLocation.y - dragStart.y));
    [o_window setFrameOrigin: newOrigin];
}

@end

/*****************************************************************************
 * statusbar
 *****************************************************************************/


@implementation statusbar
- (void)awakeFromNib
{
    [self addSubview: o_text];
	mainwindow = YES;
}

- (void)resignMainWindow
{
	mainwindow = NO;
	[self needsDisplay];
}

- (void)becomeMainWindow
{
	mainwindow = YES;
	[self needsDisplay];
}

- (void)drawRect:(NSRect)rect
{
	if(mainwindow)
		[[NSColor colorWithCalibratedRed:0.820
								   green:0.843
									blue:0.886
								   alpha:1.0] set];
	else
		[[NSColor colorWithCalibratedWhite:0.91 alpha:1.0] set];
	NSRectFill(rect);
	/*NSRect divider = rect;
	divider.origin.y += divider.size.height - 1;
	divider.size.height = 1;
	[[NSColor colorWithCalibratedWhite:0.65 alpha:1.] set];
	NSRectFill(divider);*/
}
@end
