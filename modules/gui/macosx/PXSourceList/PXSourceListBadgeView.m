//
//  PXSourceListBadgeView.m
//  PXSourceList
//
//  Created by Alex Rozanski on 15/11/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import "PXSourceListBadgeView.h"
#import "PXSourceListBadgeCell.h"

@implementation PXSourceListBadgeView

+ (Class)cellClass
{
    return [PXSourceListBadgeCell class];
}

#pragma mark - Custom Accessors

- (void)setBadgeValue:(NSUInteger)badgeValue
{
    [self.cell setBadgeValue:badgeValue];
}

- (NSUInteger)badgeValue
{
    return [self.cell badgeValue];
}

- (NSColor *)textColor
{
    return [self.cell textColor];
}

- (void)setTextColor:(NSColor *)textColor
{
    [self.cell setTextColor:textColor];
}

- (NSColor *)backgroundColor
{
    return [self.cell backgroundColor];
}

- (void)setBackgroundColor:(NSColor *)backgroundColor
{
    [self.cell setBackgroundColor:backgroundColor];
}

@end
