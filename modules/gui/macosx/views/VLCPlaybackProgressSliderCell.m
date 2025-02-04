/*****************************************************************************
 * VLCPlaybackProgressSliderCell.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCPlaybackProgressSliderCell.h"

#import <CoreVideo/CoreVideo.h>

#import "extensions/NSGradient+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"

#import "library/VLCLibraryUIUnits.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"

#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"

@interface VLCPlaybackProgressSliderCell ()
{
    NSInteger _animationWidth;
    NSInteger _animationPosition;
    double _lastTime;
    double _deltaToLastFrame;
    CVDisplayLinkRef _displayLink;

    NSColor *_emptySliderBackgroundColor;

    enum vlc_player_abloop _abLoopState;
    CGFloat _aToBLoopAMarkPosition; // Position of the A loop mark as a fraction of slider width
    CGFloat _aToBLoopBMarkPosition; // Position of the B loop mark as a fraction of slider width
}

- (void)displayLink:(CVDisplayLinkRef)displayLink tickWithTime:(const CVTimeStamp *)inNow;

@end

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
                                    const CVTimeStamp *inNow,
                                    const CVTimeStamp *inOutputTime,
                                    CVOptionFlags flagsIn,
                                    CVOptionFlags *flagsOut,
                                    void *displayLinkContext)
{
    const CVTimeStamp inNowCopy = *inNow;
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlaybackProgressSliderCell * const sliderCell = (__bridge VLCPlaybackProgressSliderCell*)displayLinkContext;
        [sliderCell displayLink:displayLink tickWithTime:&inNowCopy];
    });
    return kCVReturnSuccess;
}

@implementation VLCPlaybackProgressSliderCell

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _animationWidth = self.controlView.bounds.size.width;

        [self setSliderStyleLight];
        [self updateAtoBLoopState];
        [self initDisplayLink];

        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(abLoopStateChanged:)
                                   name:VLCPlayerABLoopStateChanged
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    CVDisplayLinkRelease(_displayLink);
}

- (void)initDisplayLink
{
    const CVReturn ret = CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
    NSAssert(ret == kCVReturnSuccess && _displayLink != NULL, @"Could not init displaylink");
    CVDisplayLinkSetOutputCallback(_displayLink, DisplayLinkCallback, (__bridge void*) self);
}

- (void)setSliderStyleLight
{
    _emptySliderBackgroundColor = NSColor.VLCSliderLightBackgroundColor;
}

- (void)setSliderStyleDark
{
    _emptySliderBackgroundColor = NSColor.VLCSliderDarkBackgroundColor;
}

- (void)abLoopStateChanged:(NSNotification *)notification
{
    [self updateAtoBLoopState];
}

- (void)updateAtoBLoopState
{
    VLCPlayerController * const playerController =
        VLCMain.sharedInstance.playQueueController.playerController;

    _abLoopState = playerController.abLoopState;
    _aToBLoopAMarkPosition = playerController.aLoopPosition;
    _aToBLoopBMarkPosition = playerController.bLoopPosition;
}

- (void)displayLink:(CVDisplayLinkRef)displayLink tickWithTime:(const CVTimeStamp *)inNow
{
    if (_lastTime == 0) {
        _deltaToLastFrame = 0;
    } else {
        _deltaToLastFrame = (double)(inNow->videoTime - _lastTime) / inNow->videoTimeScale;
    }
    _lastTime = inNow->videoTime;

    self.controlView.needsDisplay = YES;
}

#pragma mark -
#pragma mark Normal slider drawing

- (void)drawKnob:(NSRect)knobRect
{
    if (self.knobHidden) {
        return;
    }

    [super drawKnob:knobRect];
}

- (void)drawBarInside:(NSRect)rect flipped:(BOOL)flipped
{
    static const CGFloat trackBorderRadius = 1;

    // Empty Track Drawing
    NSBezierPath * const emptyTrackPath =
        [NSBezierPath bezierPathWithRoundedRect:rect
                                        xRadius:trackBorderRadius
                                        yRadius:trackBorderRadius];
    [_emptySliderBackgroundColor setFill];
    [emptyTrackPath fill];

    if (self.knobHidden) {
        return;
    }

    // Calculate filled track
    NSRect filledTrackRect = rect;
    const NSRect knobRect = [self knobRectFlipped:NO];
    filledTrackRect.size.width = knobRect.origin.x + (knobRect.size.width / 2);

    // Filled Track Drawing
    NSBezierPath * const filledTrackPath = 
        [NSBezierPath bezierPathWithRoundedRect:filledTrackRect
                                        xRadius:trackBorderRadius
                                        yRadius:trackBorderRadius];

    [NSColor.VLCAccentColor setFill];
    [filledTrackPath fill];
}

#pragma mark -
#pragma mark Indefinite slider drawing

- (void)drawAnimationInRect:(NSRect)rect
{
    [NSGraphicsContext saveGraphicsState];
    rect = NSInsetRect(rect, 1.0, 1.0);
    NSBezierPath * const fullPath =
        [NSBezierPath bezierPathWithRoundedRect:rect xRadius:2.0 yRadius:2.0];
    [fullPath setClip];

    // Use previously calculated position
    rect.origin.x = _animationPosition;

    // Calculate new position for next Frame
    if (_animationPosition < (rect.size.width + _animationWidth)) {
        _animationPosition += (rect.size.width + _animationWidth) * _deltaToLastFrame;
    } else {
        _animationPosition = -(_animationWidth);
    }

    rect.size.width = _animationWidth;

    [NSGraphicsContext restoreGraphicsState];
    _deltaToLastFrame = 0;
}

- (void)drawCustomTickMarkAtPosition:(const CGFloat)position 
                         inCellFrame:(const NSRect)cellFrame
                           withColor:(NSColor * const)color
{
    const CGFloat tickThickness = VLCLibraryUIUnits.sliderTickThickness;
    const CGSize cellSize = cellFrame.size;
    NSRect tickFrame;

    if (self.isVertical) {
        const CGFloat tickY = cellSize.height * position;
        tickFrame = NSMakeRect(cellFrame.origin.x, tickY, cellSize.width, tickThickness);
    } else {
        const CGFloat tickX = cellSize.width * position;
        tickFrame = NSMakeRect(tickX, cellFrame.origin.y, tickThickness, cellSize.height);
    }

    const NSAlignmentOptions alignOpts =
        NSAlignMinXOutward | NSAlignMinYOutward | NSAlignWidthOutward | NSAlignMaxYOutward;
    const NSRect finalTickRect =
        [self.controlView backingAlignedRect:tickFrame options:alignOpts];

    [color setFill];
    NSRectFill(finalTickRect);
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    if (self.indefinite) {
        return [self drawAnimationInRect:cellFrame];
    } else {
        [super drawWithFrame:cellFrame inView:controlView];
    }

    if (_abLoopState == VLC_PLAYER_ABLOOP_NONE) {
        return;
    }

    if (_aToBLoopAMarkPosition >= 0) {
        [self drawCustomTickMarkAtPosition:_aToBLoopAMarkPosition
                               inCellFrame:cellFrame
                                 withColor:_emptySliderBackgroundColor];
    }
    if (_aToBLoopBMarkPosition >= 0) {
        [self drawCustomTickMarkAtPosition:_aToBLoopBMarkPosition
                               inCellFrame:cellFrame
                                 withColor:_emptySliderBackgroundColor];
    }

    // Redraw knob
    [super drawKnob];
}

#pragma mark -
#pragma mark Animation handling

- (void)beginAnimating
{
    const CVReturn err = CVDisplayLinkStart(_displayLink);
    NSAssert(err == kCVReturnSuccess, @"Display link animation start should not return error!");
    _animationPosition = -(_animationWidth);
    self.enabled = NO;
}

- (void)endAnimating
{
    CVDisplayLinkStop(_displayLink);
    self.enabled = YES;

}

- (void)setIndefinite:(BOOL)indefinite
{
    if (self.indefinite == indefinite) {
        return;
    }

    _indefinite = indefinite;

    if (indefinite) {
        [self beginAnimating];
    } else {
        [self endAnimating];
    }
}

- (void)setKnobHidden:(BOOL)knobHidden
{
    _knobHidden = knobHidden;
    self.controlView.needsDisplay = YES;
}


@end
