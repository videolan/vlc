//
//  PXSourceListTableCellView.m
//  PXSourceList
//
//  Created by Alex Rozanski on 31/12/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import "PXSourceListTableCellView.h"
#import "PXSourceListBadgeView.h"

@implementation PXSourceListTableCellView

- (void)layout
{
    [super layout];

    if (!self.badgeView)
        return;

    [self.badgeView sizeToFit];

    NSRect bounds = self.bounds;
    NSSize badgeSize = self.badgeView.frame.size;
    self.badgeView.frame = NSMakeRect(NSMaxX(bounds) - badgeSize.width,
                                      NSMidY(bounds) - round(badgeSize.height / 2.0f),
                                      badgeSize.width, badgeSize.height);
}

@end
