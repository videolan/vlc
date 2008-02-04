//
//  GradientBackgroundView.m
//  iPodConverter
//
//  Created by Pierre d'Herbemont on 1/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "GradientBackgroundView.h"

/**********************************************************
 * Why not drawing something nice?
 */

@implementation GradientBackgroundView
- (void)awakeFromNib
{
    /* Buggy nib files... Force us to be on the back of the view hierarchy */
    NSView * superView;
    [self retain];
    superView = [self superview];
    [self removeFromSuperview];
    [superView addSubview:self positioned: NSWindowBelow relativeTo:nil];
}
- (void)drawRect:(NSRect)rect
{
    
    NSColor * topGradient = [NSColor colorWithCalibratedWhite:.12f alpha:1.0];
    NSColor * bottomGradient   = [NSColor colorWithCalibratedWhite:0.55f alpha:0.9];
	NSGradient * gradient = [[NSGradient alloc] initWithColorsAndLocations:bottomGradient, 0.f, bottomGradient, 0.1f, topGradient, 1.f, nil];
    [gradient drawInRect:self.bounds angle:90.0];
    [super drawRect:rect];
}
@end
