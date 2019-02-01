//
//  PXSourceListBadgeCell.h
//  PXSourceList
//
//  Created by Alex Rozanski on 15/11/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Cocoa/Cocoa.h>

/* This is the cell which backs drawing done by PXSourceListBadgeView, and is used internally for
   drawing badges when PXSourceList is used in cell-based mode.
 
   You shouldn't need to interact with this class directly.
 */
@interface PXSourceListBadgeCell : NSCell

@property (strong, nonatomic) NSColor *textColor;
@property (strong, nonatomic) NSColor *backgroundColor;
@property (assign, nonatomic) NSUInteger badgeValue;

@end
