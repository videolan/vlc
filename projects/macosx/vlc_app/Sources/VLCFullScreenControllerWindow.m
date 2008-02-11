/*****************************************************************************
 * VLCFullScreenControllerWindow.m: class that allow media controlling in
 * fullscreen (with the mouse)
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id: VLCBrowsableVideoView.m 24179 2008-01-07 21:00:49Z pdherbemont $
 *
 * Authors:  Jérôme Decoodt <djc at videolan dot org>
 *           Felix Kühne <fkuehne at videolan dot org>
 *           Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import "VLCFullScreenControllerWindow.h"

@interface VLCFullScreenControllerWindow (Private)
- (void)hide;
- (void)show;
- (void)updateTrackingRect;
@end

/*****************************************************************************
 * @implementation VLCFullScreenControllerWindow
 */

@implementation VLCFullScreenControllerWindow

/* We override this initializer so we can set the NSBorderlessWindowMask styleMask, and set a few other important settings */
- (id)initWithContentRect:(NSRect)contentRect 
                styleMask:(unsigned int)aStyle 
                  backing:(NSBackingStoreType)bufferingType 
                    defer:(BOOL)flag
{
    if( self = [super initWithContentRect:contentRect styleMask:NSTexturedBackgroundWindowMask backing:bufferingType defer:flag] )
    {
        [self setOpaque:NO];
        [self setHasShadow: NO];
        [self setBackgroundColor:[NSColor clearColor]];
        
        /* let the window sit on top of everything else and start out completely transparent */
        [self setLevel:NSFloatingWindowLevel];
        [self center];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [hideWindowTimer invalidate];
    [hideWindowTimer release];
    [videoViewTrackingArea release];
    [super dealloc];
}

- (void)awakeFromNib
{
    hideWindowTimer = nil;
    videoViewTrackingArea = nil;

    [self setMovableByWindowBackground:YES];

    /* Make sure we'll detect when to close the window, see animationDidStop:finished: */
    CAAnimation *alphaValueAnimation = [CABasicAnimation animation];
    [alphaValueAnimation setDelegate:self];
    [self setAnimations:[NSDictionary dictionaryWithObject:alphaValueAnimation forKey:@"alphaValue"]];
    hideWindowTimer = nil;

    /* WindowView setup */
    [[mainWindowController.videoView window] setAcceptsMouseMovedEvents:YES];
    [[mainWindowController.videoView window] makeFirstResponder:mainWindowController.videoView];
    [mainWindowController.videoView setPostsBoundsChangedNotifications: YES];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(videoViewDidChangeBounds:) name:NSViewBoundsDidChangeNotification object:(id)mainWindowController.videoView];

    /* Make sure we can know when the mouse is inside us */
    [[self contentView] addTrackingRect:[[self contentView] bounds] owner:self userData:nil assumeInside:NO];

    /* Bindings connection */
    /* Sound */
    [volumeSlider setKnobImage:[NSImage imageNamed:@"fs_volume_slider_knob_highlight.png"]];
    [volumeSlider setBackgroundImage:[NSImage imageNamed:@"fs_volume_slider_bar.png"]];
    [volumeSlider setNeedsDisplay:YES];
    [volumeSlider bind:@"value" toObject:[VLCLibrary sharedLibrary] withKeyPath:@"audio.volume" options: nil];

    /* media position */
    [mediaPositionSlider setKnobImage:[NSImage imageNamed:@"fs_time_slider_knob.png"]];
    [mediaPositionSlider setBackgroundImage:[NSImage imageNamed:@"fs_time_slider.png"]];
    [mediaPositionSlider setNeedsDisplay:YES];

    [mediaPositionSlider bind:@"enabled" toObject:mainWindowController.mediaPlayer withKeyPath:@"media" options: [NSDictionary dictionaryWithObject:@"NonNilAsBoolTransformer" forKey:NSValueTransformerNameBindingOption]];
    [mediaPositionSlider bind:@"enabled2" toObject:mainWindowController.mediaPlayer withKeyPath:@"seekable" options: nil];

    [mediaPositionSlider bind:@"value" toObject:mainWindowController.mediaPlayer withKeyPath:@"position" options:
        [NSDictionary dictionaryWithObjectsAndKeys:@"Float10000FoldTransformer", NSValueTransformerNameBindingOption,
                                                  [NSNumber numberWithBool:NO], NSConditionallySetsEnabledBindingOption, nil ]];


    [fillScreenButton bind:@"value" toObject:mainWindowController.videoView withKeyPath:@"videoLayer.fillScreen" options: nil];
    [fullScreenButton bind:@"value" toObject:mainWindowController.videoView withKeyPath:@"fullScreen" options: nil];

    [mediaReadingProgressText bind:@"value" toObject:mainWindowController.mediaPlayer withKeyPath:@"time.stringValue" options: nil];
    [mediaDescriptionText bind:@"value" toObject:mainWindowController.mediaPlayer withKeyPath:@"description" options: nil];

    /* mainWindowController.mediaPlayer */
    [mediaPlayerPlayPauseStopButton bind:@"enabled" toObject:mainWindowController.mediaPlayer withKeyPath:@"media" options: [NSDictionary dictionaryWithObject:@"NonNilAsBoolTransformer" forKey:NSValueTransformerNameBindingOption]];
    [mediaPlayerPlayPauseStopButton bind:@"state"   toObject:mainWindowController.mediaPlayer withKeyPath:@"playing" options: nil];
    [mediaPlayerPlayPauseStopButton bind:@"alternateImage" toObject:mainWindowController.mediaPlayer withKeyPath:@"stateAsFullScreenButtonAlternateImage" options: nil];
    [mediaPlayerPlayPauseStopButton bind:@"image"   toObject:mainWindowController.mediaPlayer withKeyPath:@"stateAsFullScreenButtonImage" options: nil];
    [mediaPlayerBackwardPrevButton  bind:@"enabled" toObject:mainWindowController.mediaPlayer withKeyPath:@"playing" options: nil];
    [mediaPlayerForwardNextButton   bind:@"enabled" toObject:mainWindowController.mediaPlayer withKeyPath:@"playing" options: nil];
    [mediaPlayerForwardNextButton   setTarget:mainWindowController.mediaPlayer];
    [mediaPlayerForwardNextButton   setAction:@selector(fastForward)];
    [mediaPlayerBackwardPrevButton  setTarget:mainWindowController.mediaPlayer];
    [mediaPlayerBackwardPrevButton  setAction:@selector(rewind)];
    [mediaPlayerPlayPauseStopButton setTarget:mainWindowController.mediaPlayer];
    [mediaPlayerPlayPauseStopButton setAction:@selector(pause)];

    [self bind:@"fullScreen" toObject:mainWindowController.videoView withKeyPath:@"fullScreen" options: nil];
    
    active = NO;
}

- (BOOL)fullScreen
{
    /* Only to comply to KVC */
    return active;
}

- (void)setFullScreen:(BOOL)fullScreen
{
    if(fullScreen)
    {
        active = YES;
        [self show];
    }
    else
    {
        [self hide];
        active = NO;
    }
}

-(void)center
{
    /* centre the panel in the lower third of the screen */
    NSPoint theCoordinate;
    NSRect theScreensFrame;
    NSRect theWindowsFrame;

    theScreensFrame = [[self screen] frame];

    theWindowsFrame = [self frame];
    
    theCoordinate.x = (theScreensFrame.size.width - theWindowsFrame.size.width) / 2 + theScreensFrame.origin.x;
    theCoordinate.y = (theScreensFrame.size.height / 3) - theWindowsFrame.size.height + theScreensFrame.origin.y;
    [self setFrameTopLeftPoint: theCoordinate];
}

@end

/*****************************************************************************
 * @implementation VLCFullScreenControllerWindow (Private)
 */

@implementation VLCFullScreenControllerWindow (Private)
- (void)show
{
    if(![self isVisible])
        self.alphaValue = 0.0;

    if( !NSPointInRect([NSEvent mouseLocation],[self frame]) )
    {
        [hideWindowTimer invalidate];
        [hideWindowTimer release];
        hideWindowTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector:@selector(hide) userInfo:nil repeats:NO] retain];
    }
    [self orderFront:self];
    [self.animator setAlphaValue:1.0];
}

- (void)hide
{
    [hideWindowTimer invalidate];
    [hideWindowTimer release];
    hideWindowTimer = nil;
    if ([self isVisible])
    {
        [self.animator setAlphaValue:0.0];
        [NSCursor setHiddenUntilMouseMoves:YES];
    }
    [self updateTrackingRect];
}

- (void)updateTrackingRect
{
    VLCBrowsableVideoView * videoView = mainWindowController.videoView;

    if( videoViewTrackingArea )
    {
        [videoView removeTrackingArea:videoViewTrackingArea];
        [videoViewTrackingArea release];
    }
    videoViewTrackingArea = [[NSTrackingArea alloc] initWithRect:[videoView bounds] options:NSTrackingMouseMoved|NSTrackingActiveAlways|NSTrackingAssumeInside|NSTrackingEnabledDuringMouseDrag owner:self userInfo:nil];
    [videoView addTrackingArea:videoViewTrackingArea];

}

@end

/*****************************************************************************
 * @implementation VLCFullScreenControllerWindow (NSAnimationDelegate)
 */

@implementation VLCFullScreenControllerWindow (NSAnimationDelegate)
- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)flag 
{
    if( self.alphaValue == 0.0 )
        [self orderOut:self];
}
@end

/*****************************************************************************
 * @implementation VLCFullScreenControllerWindow (NSTrackingRectCallbacksInVideoView)
 */

@implementation VLCFullScreenControllerWindow (NSTrackingRectCallbacks)
- (void)mouseMoved:(NSEvent *)theEvent
{
    if([theEvent window] != self)
    {
        if( active )
            [self show];
    }
}
- (void)mouseEntered:(NSEvent *)theEvent
{
    if([theEvent window] == self)
    {
        [hideWindowTimer invalidate];
        [hideWindowTimer release];
        hideWindowTimer = nil;
    }
}
- (void)mouseExited:(NSEvent *)theEvent
{
    if([theEvent window] == self)
    {
        [hideWindowTimer invalidate];
        [hideWindowTimer release];
        hideWindowTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector:@selector(hide) userInfo:nil repeats:NO] retain];
    }
    else
    {
        if( active )
            [self hide];
    }
}
- (void)cursorUpdate:(NSEvent *)event
{

}
@end

/*****************************************************************************
 * @implementation VLCFullScreenControllerWindow (VideoViewBoundsChanges)
 */
@implementation VLCFullScreenControllerWindow (VideoViewBoundsChanges)
- (void)videoViewDidChangeBounds:(NSNotification *)theNotification
{
    [self updateTrackingRect];
}

@end
