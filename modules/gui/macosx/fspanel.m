/*****************************************************************************
 * fspanel.m: MacOS X full screen panel
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "intf.h"
#import "controls.h"
#import "vout.h"
#import "fspanel.h"

#define KEEP_VISIBLE_AFTER_ACTION 4 /* time in half-sec until this panel will hide again after an user's action */

/*****************************************************************************
 * VLCFSPanel
 *****************************************************************************/
@implementation VLCFSPanel
/* We override this initializer so we can set the NSBorderlessWindowMask styleMask, and set a few other important settings */
- (id)initWithContentRect:(NSRect)contentRect 
                styleMask:(unsigned int)aStyle 
                  backing:(NSBackingStoreType)bufferingType 
                    defer:(BOOL)flag
{
    id win=[super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:bufferingType defer:flag];
    [win setOpaque:NO];
    [win setHasShadow: NO];
    [win setBackgroundColor:[NSColor clearColor]];
    /* let the window sit on top of everything else and start out completely transparent */
    [win setLevel:NSFloatingWindowLevel];
    [win setAlphaValue:0.0];
    return win;
}

- (void)awakeFromNib
{
    [self setContentView:[[VLCFSPanelView alloc] initWithFrame: [self frame]]];
    BOOL isInside=(NSPointInRect([NSEvent mouseLocation],[self frame]));
    [[self contentView] addTrackingRect:[[self contentView] bounds] owner:self userData:nil assumeInside:isInside];
    if (isInside)
    [self mouseEntered:NULL];
    if (!isInside)
    [self mouseExited:NULL];
}

/* Windows created with NSBorderlessWindowMask normally can't be key, but we want ours to be */
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

-(void)dealloc
{
    if( hideAgainTimer )
        [hideAgainTimer release];
    [self setFadeTimer:nil];
    [super dealloc];
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

- (void)setStreamPos:(float) f_pos setSeconds:(int)i_seconds;
{
    [[self contentView] setStreamPos:f_pos setSeconds:i_seconds];
}

/* This routine is called repeatedly when the mouse enters the window from outside it. */
- (void)focus:(NSTimer *)timer
{
    if( [self alphaValue] < 1.0 )
        [self setAlphaValue:[self alphaValue]+0.1];
    if( [self alphaValue] >= 1.0 )
    {
        [self setAlphaValue: 1.0];
        [self setFadeTimer:nil];
        if( b_fadeQueued )
        {
            b_fadeQueued=NO;
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(unfocus:) userInfo:NULL repeats:YES]];
        }
    }
}

/* This routine is called repeatedly when the mouse exits the window from inside it */
- (void)unfocus:(NSTimer *)timer
{
    if( [self alphaValue] > 0.0 )
        [self setAlphaValue:[self alphaValue]-0.1];
    if( [self alphaValue] <= 0.1 )
    {
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

/* If the mouse enters a window, go make sure we fade in */
- (void)mouseEntered:(NSEvent *)theEvent
{
    [self fadeIn];
}

/* If the mouse exits a window, go make sure we fade out */
- (void)mouseExited:(NSEvent *)theEvent
{
    /* the user left the window, fade out immediatelly and prevent any timer action */
    if( b_alreadyCounting )
    {
        [hideAgainTimer invalidate];
        [hideAgainTimer release];
        b_alreadyCounting = NO;
        hideAgainTimer = NULL;
    }

    /* give up our focus, so the vout may show us again without letting the user clicking it */
    if( [[[[VLCMain sharedInstance] getControls] getVoutView] isFullscreen] )
        [[[[[VLCMain sharedInstance] getControls] getVoutView] window] makeKeyWindow];

    [self fadeOut];
}

- (void)fadeIn
{
    if( [self alphaValue] < 1.0 )
    {
        if (![self fadeTimer])
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(focus:) userInfo:[NSNumber numberWithShort:1] repeats:YES]];
        else if ([[[self fadeTimer] userInfo] shortValue]==0)
            b_fadeQueued=YES;
    }
    [self autoHide];
}

- (void)fadeOut
{
    if( ( [self alphaValue] > 0.0 ) && !NSPointInRect( [NSEvent mouseLocation], [self frame] ) )
    {
        if (![self fadeTimer])
            [self setFadeTimer:[NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(unfocus:) userInfo:[NSNumber numberWithShort:0] repeats:YES]];
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
        hideAgainTimer = [NSTimer scheduledTimerWithTimeInterval: 0.5
                                                          target: self 
                                                        selector: @selector(keepVisible:)
                                                        userInfo: nil 
                                                         repeats: YES];
        [hideAgainTimer fire];
        [hideAgainTimer retain];
        b_alreadyCounting = YES;
    }
}

- (void)keepVisible:(NSTimer *)timer
{
    /* if the user triggered an action, start over again */
    if( b_keptVisible )
    {
        i_timeToKeepVisibleInSec = KEEP_VISIBLE_AFTER_ACTION;
        b_keptVisible = NO;
    }
    
    /* count down until we hide ourselfes again and do so if necessary */
    i_timeToKeepVisibleInSec -= 1;
    if( i_timeToKeepVisibleInSec < 1 )
    {
        [self fadeOut];
        [timer invalidate];
        [timer release];
        b_alreadyCounting = NO;
        timer = NULL;
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
    [fadeTimer release];
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
    image = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:imageOff]];    \
    [o_button setImage:image];                                                                  \
    image = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:imageOn]];     \
    [o_button setAlternateImage:image];                                                         \
    [o_button sizeToFit];                                                                       \
    [o_button setTarget: self];                                                                 \
    [o_button setAction: @selector(action:)];                                                   \
    [self addSubview:o_button];

#define addTextfield( o_text, align, font, color, size )                                    \
    o_text = [[NSTextField alloc] initWithFrame: s_rc];                            \
    [o_text setDrawsBackground: NO];                                                        \
    [o_text setBordered: NO];                                                               \
    [o_text setEditable: NO];                                                               \
    [o_text setSelectable: NO];                                                             \
    [o_text setStringValue: _NS("(no item is being played)")];                                                    \
    [o_text setAlignment: align];                                                           \
    [o_text setTextColor: [NSColor color]];                                                 \
    [o_text setFont:[NSFont font:[NSFont smallSystemFontSize] - size]];                     \
    [self addSubview:o_text];

- (id)initWithFrame:(NSRect)frameRect
{
    NSBundle * bundle = [NSBundle mainBundle];
    id view = [super initWithFrame:frameRect];
    fillColor = [[NSColor clearColor] retain];
    NSRect s_rc = [self frame];
    NSImage * image;
    addButton( o_prev, @"fs_skip_previous.png" , @"fs_skip_previous_highlight.png", 174, 15, prev );
    addButton( o_slow, @"fs_rewind.png"        , @"fs_rewind_highlight.png"       , 211, 14, slower );
    addButton( o_play, @"fs_play.png"          , @"fs_play_highlight.png"         , 269, 10, play );
    addButton( o_fast, @"fs_forward.png"       , @"fs_forward_highlight.png"      , 313, 14, faster );
    addButton( o_next, @"fs_skip_next.png"     , @"fs_skip_next_highlight.png"    , 365, 15, next );
    addButton( o_fullscreen, @"fs_exit_fullscreen.png", @"fs_exit_fullscreen_hightlight.png", 507, 13, windowAction );
/*
    addButton( o_button, @"FSVolumeThumbOff.tif"   , @"FSVolumeThumbOn.tif"  ,  38, 51, something );
 */

    s_rc = [self frame];
    s_rc.origin.x = 15;
    s_rc.origin.y = 53;
    s_rc.size.width = 518;
    s_rc.size.height = 9;
    o_time_slider = [[[VLCFSTimeSlider alloc] initWithFrame: s_rc] retain];
    [o_time_slider setMinValue:0];
    [o_time_slider setMaxValue:1];
    [o_time_slider setFloatValue: 0];
    [o_time_slider setAction: @selector(timesliderUpdate:)];
    [self addSubview: o_time_slider];
    
    s_rc = [self frame];
    s_rc.origin.x = 98;
    s_rc.origin.y = 64;
    s_rc.size.width = 352;
    s_rc.size.height = 14;
    addTextfield( o_textfield, NSCenterTextAlignment, systemFontOfSize, whiteColor, 0 );
    s_rc.origin.x = 486;
    s_rc.origin.y = 64;
    s_rc.size.width = 50;
    addTextfield( o_textPos, NSRightTextAlignment, systemFontOfSize, whiteColor, 0 );

    return view;
}

- (void)setPlay
{
    NSBundle *bundle = [NSBundle mainBundle];
    NSImage *image;
    [[o_play image] release];
    [[o_play alternateImage] release];
    image = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"fs_play.png"]];
    [o_play setImage:image];
    image = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"fs_play_highlight.png"]];
    [o_play setAlternateImage:image];
}

- (void)setPause
{
    NSBundle *bundle = [NSBundle mainBundle];
    NSImage *image;
    [[o_play image] release];
    [[o_play alternateImage] release];
    image = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"fs_pause.png"]];
    [o_play setImage:image];
    image = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"fs_pause_highlight.png"]];
    [o_play setAlternateImage:image];
}

- (void)setStreamTitle:(NSString *)o_title
{
    [o_textfield setStringValue: o_title];
}

- (void)setStreamPos:(float) f_pos setSeconds:(int)i_seconds
{
    NSString *o_pos = [NSString stringWithFormat: @"%d:%02d:%02d",
                        (int) (i_seconds / (60 * 60)),
                        (int) (i_seconds / 60 % 60),
                        (int) (i_seconds % 60)];

    [o_textPos setStringValue: o_pos];
    [o_time_slider setFloatValue: f_pos];
}

- (IBAction)play:(id)sender
{
    [[[VLCMain sharedInstance] getControls] play: sender];
}

- (IBAction)faster:(id)sender
{
    [[[VLCMain sharedInstance] getControls] faster: sender];
}

- (IBAction)slower:(id)sender
{
    [[[VLCMain sharedInstance] getControls] slower: sender];
}

- (IBAction)prev:(id)sender
{
    [[[VLCMain sharedInstance] getControls] prev: sender];
}

- (IBAction)next:(id)sender
{
    [[[VLCMain sharedInstance] getControls] next: sender];
}

- (IBAction)windowAction:(id)sender
{
    [[[VLCMain sharedInstance] getControls] windowAction: sender];
}

- (IBAction)timesliderUpdate:(id)sender
{
    [[VLCMain sharedInstance] timesliderUpdate: sender];
}

#define addImage(image, _x, _y, mode, _width)                                               \
    img = [[[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:image]] autorelease];     \
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
    NSBundle *bundle = [NSBundle mainBundle];
	NSRect frame = [self frame];
    NSRect image_rect;
    NSImage *img;
    addImage( @"fs_background.png", 0, 0, NSCompositeCopy, 0 );
    addImage( @"fs_volume_slider_bar.png" , 26, 22, NSCompositeSourceOver, 0 );
    addImage( @"fs_volume_mute.png", 16, 18, NSCompositeSourceOver, 0 );
    addImage( @"fs_volume_max.png", 124, 17, NSCompositeSourceOver, 0 );
    addImage(@"fs_time_slider.png" ,  15, 53, NSCompositeSourceOver, 0);
}

@end

/*****************************************************************************
 * VLCFSTimeSlider
 *****************************************************************************/
@implementation VLCFSTimeSlider
void drawKnobInRect(NSRect knobRect)
{
    NSBundle *bundle = [NSBundle mainBundle];
    NSRect image_rect;
    NSImage *img = [[NSImage alloc] initWithContentsOfFile:[bundle pathForImageResource:@"fs_time_slider_knob.png"]];
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
    drawFrameInRect(rect);
    drawKnobInRect(knobRect);
}

@end
