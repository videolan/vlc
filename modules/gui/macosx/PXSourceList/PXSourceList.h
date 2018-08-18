//
//  PXSourceList.h
//  PXSourceList
//
//  Created by Alex Rozanski on 05/09/2009.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Cocoa/Cocoa.h>

#import "PXSourceListDelegate.h"
#import "PXSourceListDataSource.h"
#import "PXSourceListItem.h"
#import "PXSourceListBadgeView.h"
#import "PXSourceListTableCellView.h"

/**

 `PXSourceList` is an `NSOutlineView` subclass that makes using a source list (the sidebar
 seen in applications such as iTunes and Mail.app) easier by providing common styling and idiomatic
 behaviour of source lists for you through a clean and simple API. Notable features of PXSourceList include:

   - All root-level, "group" items are displayed using `NSOutlineView`'s group styling by default and requires no additional setup.
   - Source List items often display an *icon* and *badge* (for information such as unread counts). This is built into
 the API to make configuring these quick and easy, and a badge `NSControl` subclass is included which can be added to
 `NSTableCellView` objects when using `PXSourceList` in *view-based* mode to display badges (see the next section for details).
   - `PXSourceList` implements support for showing groups as "always expanded" -- where their child items are always shown
 and no 'Show'/'Hide' button is displayed when hovering over the group. This is often useful for listing the main
 contexts your application can be in at any given time, which the user can select to change views. As it is paramount to
 your application's navigation, these groups are often displayed with their child items always shown.
   - `PXSourceList` objects operate with only one column and do not display a header, something
 which is reflected in the API and makes the control easier to use.

 Like `NSOutlineView` and `NSTableView`, a `PXSourceList` object does not store its own data, but retrieves
 values from a weakly-referenced data source (see the `PXSourceListDataSource` protocol). A `PXSourceList`
 object can also have a delegate, to which it sends messages when certain events occur (see the
 `PXSourceListDelegate` protocol for more information).
 
 ### Cell-based vs. view-based mode
 
 Like `NSTableView` and `NSOutlineView`, PXSourceList can operate in both cell-based and view-based mode in
 relation to how you provide content to be displayed.
 
 When using PXSourceList in cell-based mode, it can manage drawing of icons and badges for you through custom
 drawing and `PXSourceListDataSource` methods. However, when using PXSourceList in view-based mode, it can't
 do this directly, because cell views are configured independently in Interface Builder (or programmatically)
 and configured in the `PXSourceListDataSource` method, `-sourceList:viewForItem:`.
 
 Instead, in view-based mode, you should set up the icon for each item in `-sourceList:viewForItem:` using the
 `imageView` property of `NSTableCellView`, and the `badgeView` property if using `PXSourceListTableCellView`
 objects to display your content. Additionally, there are several classes provided alongside `PXSourceList`
 which make this set up a lot easier:

 - `PXSourceListTableCellView`: an `NSTableCellView` subclass which exposes a `badgeView` outlet that can be
   hooked up to a `PXSourceListBadgeView` instance (see below) in Interface Builder. Along with `NSTableCellView`
   and its `textField` and `imageView` properties, `PXSourceListTableCellView` is an `NSTableCellView` subclass which
   allows you to easily display an icon, title and a badge for each item in the Source List without subclassing.
 - `PXSourceListBadgeView`: a view class for displaying badges, which can be used in your table cell views and
   configured to display a particular badge number. Additionally, instances can be configured to use custom text and
   background colours, although it will use the regular Source List styling of light text on a grey-blue background
   by default.
 
 ### Creating a data source model with `PXSourceListItem`
 
 Like `NSOutlineView`, PXSourceList queries its data source to build up a tree-like structure of content using
 `-sourceList:numberOfChildrenOfItem:` and `-sourceList:child:ofItem:`. Often it is practical to store the structure
 of your Source List content in a tree structure which can then be easily returned the the Source List using
 these two data source methods.
 
 To help with this, the generic `PXSourceListItem` class has been included with PXSourceList which can be
 used to build this tree structure. It declares properties such as `title` and `icon` which are useful in
 storing display information which can then be used in `-sourceList:viewForItem:` or `-sourceList:objectValueForItem:`,
 as well as a `children` property with convenience methods for mutating its list of children. Take a look at the
 `PXSourceListItem` documentation for more information, as well as the cell-based and view-based example
 projects included for examples of how to use this class in your own projects.

 */
@interface PXSourceList: NSOutlineView <NSOutlineViewDelegate, NSOutlineViewDataSource>

///---------------------------------------------------------------------------------------
/// @name Delegate and Data Source
///---------------------------------------------------------------------------------------

/** Used to set the Source List's data source.
 
 @warning Unfortunately, due to the way that `PXSourceList` is implemented, sending `-dataSource` to the Source List
 will return a proxy object which is used internally. As such you should only use this setter and not invoke `-dataSource`
 to retrieve the data source object.
 
 @param dataSource An object to use for the data source.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (void)setDataSource:(id<PXSourceListDataSource>)dataSource;

/** Used to set the Source List's delegate.
 
 @warning Unfortunately, due to the way that `PXSourceList` is implemented, sending `-delegate` to the Source List
 will return a proxy object which is used internally. As such you should only use this setter and not invoke `-delegate`
 to retrieve the data source object.
 
 @param delegate An object to use for the delegate.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (void)setDelegate:(id<PXSourceListDelegate>)delegate;

///---------------------------------------------------------------------------------------
/// @name Setting Display Attributes
///---------------------------------------------------------------------------------------

/** Returns the size of icons in points to display in items in the Source List.
 
 @discussion The default value is 16 x 16.

 @warning This property only applies when using `PXSourceList` in cell-based mode. If set on a Source List
 operating in view-based mode, this value is not used.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */

@property (nonatomic, assign) NSSize iconSize;

///---------------------------------------------------------------------------------------
/// @name Working with Groups
///---------------------------------------------------------------------------------------

@property (readonly) NSUInteger numberOfGroups;

/** Returns a Boolean value that indicates whether a given item in the Source List is a group item.

 @param item The item to query about.
 
 @return `YES` if *item* exists in the Source List and is a group item, otherwise `NO`.
 
 @discussion "Group" items are defined as root items in the Source List tree hierarchy.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)isGroupItem:(id)item;

/** Returns a Boolean value that indicates whether a given group item in the Source List is always expanded.
 
 @param group The given group item.
 
 @return `YES` if *group* is a group item in the Source List which is displayed as always expanded, or `NO` otherwise.
 
 @discussion "Group" items are defined as root items in the Source List tree hierarchy. A group item that is displayed
 as always expanded doesn't show a 'Show'/'Hide' button on hover as with regular group items. It is automatically expanded
 when the Source List's data is reloaded and cannot be collapsed.
 
 This method calls the `-sourceList:isGroupAlwaysExpanded:` method on the Source List's delegate to determine
 whether the particular group item is displayed as always expanded or not.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)isGroupAlwaysExpanded:(id)group;

///---------------------------------------------------------------------------------------
/// @name Working with Badges
///---------------------------------------------------------------------------------------

/** Returns a Boolean value that indicates whether a given item in the Source List displays a badge.

 @param item The given item.

 @return `YES` if the Source List is operating in cell-based mode and *item* displays a badge, or `NO` otherwise.

 @discussion This method calls the `-sourceList:itemHasBadge:` method on the Source List's delegate to determine
 whether the item displays a badge or not.
 
 @warning This method only applies when using a Source List in cell-based mode. If sent to a Source List in view-based mode, this
 method returns `NO`.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)itemHasBadge:(id)item;

/** Returns the integer value of the badge for a given item.

 @param item The given item.

 @return The integer value of the badge for *item* if the Source List is operating in cell-based mode and *item* displays a badge, or `NSNotFound` otherwise.

 @discussion This method calls the `-sourceList:badgeValueForItem:` method on the Source List's data source to determine
 the item's badge value.

 @warning This method only applies when using a Source List in cell-based mode. If sent to a Source List in view-based mode, this
 method returns `NSNotFound`.
 
 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSInteger)badgeValueForItem:(id)item;

/* === Unavailable methods ===
 
   As a side-effect of PXSourceList's internal implementation, these methods shouldn't be used to query the delegate or data
   source. I am *always* looking for a way to remove this limitation. Please file an issue at https://github.com/Perspx/PXSourceList if you
   have any ideas!
 */
- (id <NSOutlineViewDelegate>)delegate __attribute__((unavailable("-delegate shouldn't be called on PXSourceList. See the documentation for more information.")));
- (id <NSOutlineViewDataSource>)dataSource __attribute__((unavailable("-dataSource shouldn't be called on PXSourceList. See the documentation for more information.")));

@end

