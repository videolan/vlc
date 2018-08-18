//
//  PXSourceListTableCellView.h
//  PXSourceList
//
//  Created by Alex Rozanski on 31/12/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Cocoa/Cocoa.h>

@class PXSourceListBadgeView;

/**
 `PXSourceListTableCellView` is an `NSTableCellView` subclass which can be used when using `PXSourceList`
 in view-based mode.
 
 Similar to `NSTableCellView` and its `textField` and `imageView` outlets, `PXSourceListTableCellView`
 provides a `badgeView` outlet which can be hooked up to a `PXSourceListBadgeView` in Interface Builder
 and then configured in `sourceList:viewForItem:`.
 
 `PXSourceListTableCellView` positions its `badgeView` automatically (as `NSTableCellView` does for the `textField`
 and `imageView` outlets) to be positioned centred (vertically) and rightmost (horizontally) within the table cell's
 bounds. If you want to change this positioning you can do so by creating a `PXSourceListTableCellView` subclass and
 overriding `-layout`, but note that idiomatically, source lists display badges to the right of each row.
 */
@interface PXSourceListTableCellView : NSTableCellView

/**
 @brief The badge view displayed by the cell.
 @discussion When a `PXSourceListTableCellView` instance is created, a `PXSourceListTableCellView` instance
 is *not* automatically created and set to this property (just like with `NSTableCellView` and its
 `textField` and `imageView` properties). This property is purely declared on this class to make creating
 table cell views for a `PXSourceList` in Interface Builder easier without having to declare your own
 `NSTableCellView` subclass.
 
 This property is typically configured in the `PXSourceListDelegate` method `sourceList:viewForItem:`.
 
 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
@property (weak, nonatomic) IBOutlet PXSourceListBadgeView *badgeView;

@end
