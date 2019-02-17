//
//  PXSourceListDataSource.h
//  PXViewKit
//
//  Created by Alex Rozanski on 17/10/2009.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Cocoa/Cocoa.h>

@class PXSourceList;

/**
 The `PXSourceListDataSource` protocol defines methods that can be implemented by data sources of `PXSourceList` objects.
 
 Despite many of these methods being optional in their implementation, several methods **must** be implemented by a data source of a `PXSourceList` object. These are:

   - `sourceList:numberOfChildrenOfItem:`
   - `sourceList:child:ofItem:`
   - `sourceList:isItemExpandable:`
   - `sourceList:objectValueForItem:` (although this is optional if the Source List is operating in view-based mode).
 
 ### PXSourceList in View-based mode
 
 As with `NSOutlineView`, `PXSourceList` can operate in cell-based or view-based mode. Of particular note, the
 following `PXSourceListDataSource` methods are *not* used by `PXSourceList` when operating in view-based mode.

   - `-sourceList:itemHasBadge:`
   - `-sourceList:badgeValueForItem:`
   - `-sourceList:badgeTextColorForItem:`
   - `-sourceList:badgeBackgroundColorForItem:`
   - `-sourceList:itemHasIcon:`
   - `-sourceList:iconForItem:`
 
 These properties can be configured in the `PXSourceListDelegate` protocol method, `-sourceList:viewForItem:`.

 @warning Most of the methods defined by this protocol are analagous to those declared by `NSOutlineViewDataSource` (and are marked as such in the member's documentation), but are prefixed by "sourceList:" instead of "outlineView:". Only the most basic information about these methods is included here, and you should refer to the `NSOutlineViewDataSource` protocol documentation for more information.
 */
@protocol PXSourceListDataSource <NSObject>

@required
///---------------------------------------------------------------------------------------
/// @name Working with Items in a Source List
///---------------------------------------------------------------------------------------
/** 
 @brief Returns the number of child items of a given item.

 @param sourceList The Source List that sent the message.
 @param item An item in the data source.

 @return The number of immediate child items of *item*. If *item* is `nil` then you should return the number of top-level items in the Source List item hierarchy.
 
 @since Requires PXSourceList 0.8 and above and the OS X v10.5 SDK or above.
 
 @see sourceList:child:ofItem:
 */
- (NSUInteger)sourceList:(PXSourceList*)sourceList numberOfChildrenOfItem:(id)item;

/**
 @brief Returns the direct child of a given item at the specified index.

 @param aSourceList The Source List that sent the message.
 @param index The index of the child item of *item* to return.
 @param item An item in the data source.

 @return The immediate child of *item* at the specified *index*. If *item* is `nil`, then return the top-level item with index of *index*.

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.

 @see sourceList:numberOfChildrenOfItem:
 */
- (id)sourceList:(PXSourceList*)aSourceList child:(NSUInteger)index ofItem:(id)item;

/**
 @brief Returns a Boolean value indicating whether a given item in the Source List is expandable.
 @discussion An expandable item is one which contains child items, and can be expanded to display these. Additionally, if a group item is always displayed as expanded (denoted by `-sourceList:isGroupAlwaysExpanded:` from the `PXSourceListDelegate` protocol) then you must return `YES` from this method for the given group item.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return `YES` if *item* can be expanded, or `NO` otherwise.

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)sourceList:(PXSourceList*)aSourceList isItemExpandable:(id)item;

@optional
/**
 @brief Returns the data object associated with a given item.
 @discussion When using the Source List in cell-based mode, returning the text to be displayed for cells representing Group items, the Source List will *not* transform the titles to uppercase so that they display like in iTunes or iCal, such as "LIBRARY". This is to account for edge cases such as words like "iTunes" which should be capitalized as "iTUNES" and so to do this you must pass uppercase titles yourself. It is strongly recommended that text displayed for group items is uppercased in this way, to fit the conventional style of Source List Group headers.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return The data object associated with `item`.

 @warning This is a required method when using the Source List in cell-based mode.

 @see sourceList:setObjectValue:forItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (id)sourceList:(PXSourceList*)aSourceList objectValueForItem:(id)item;

/**
 @brief Sets the associated object value of a specified item.
 @discussion This method must be implemented if the Source List is operating in cell-based mode and any items in the Source List are editable.

 @param aSourceList The Source List that sent the message.
 @param object The new object value for the given item.
 @param item An item in the data source.

 @see sourceList:objectValueForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (void)sourceList:(PXSourceList*)aSourceList setObjectValue:(id)object forItem:(id)item;

///---------------------------------------------------------------------------------------
/// @name Working with Badges
///---------------------------------------------------------------------------------------

/**
 @brief Returns a Boolean specifying whether a given item shows a badge or not.
 @discussion This method can be implemented by the data source to specify whether a given item displays a badge or not. A badge is a rounded rectangle containing a number (the badge value), displayed to the right of a row's cell.

 This method must be implemented for the other badge-related data source methods – sourceList:badgeValueForItem:, sourceList:badgeTextColorForItem: and sourceList:badgeBackgroundColorForItem: – to be called.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return `YES` if *item* should display a badge, or `NO` otherwise.
 
 @warning This method is only used by the Source List when operating in cell-based mode. When the Source List is operating in view-based mode, the view for each cell is responsible for managing a badge, if applicable.

 @see sourceList:badgeValueForItem:
 @see sourceList:badgeTextColorForItem:
 @see sourceList:badgeBackgroundColorForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)sourceList:(PXSourceList*)aSourceList itemHasBadge:(id)item;

/**
 @brief Returns an integer specifying the badge value for a particular item.
 @discussion This method can be implemented by the data source to specify a badge value for any particular item. If you want an item to display a badge, you must also implement sourceList:itemHasBadge: and return `YES` for that item. Returning `NO` for items in sourceList:itemHasBadge: means that this method will not be called for that item.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return The badge value for *item*.
 
 @warning This method is only used by the Source List when operating in cell-based mode. When the Source List is operating in view-based mode, the view for each cell is responsible for managing a badge, if applicable.

 @see sourceList:itemHasBadge:
 @see sourceList:badgeTextColorForItem:
 @see sourceList:badgeBackgroundColorForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSInteger)sourceList:(PXSourceList*)aSourceList badgeValueForItem:(id)item;

/**
 @brief Returns a color that is used for the badge text color of an item in the Source List.
 @discussion This method can be implemented by the data source to specify a custom badge color for a particular item.

 This method is only called for *item* if you return `YES` for *item* in sourceList:itemHasBadge:.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.
 
 @return An `NSColor` object to use for the text color of *item*'s badge or `nil` to use the default badge text color.
 
 @warning This method is only used by the Source List when operating in cell-based mode. When the Source List is operating in view-based mode, the view for each cell is responsible for managing a badge, if applicable.

 @see sourceList:itemHasBadge:
 @see sourceList:badgeValueForItem:
 @see sourceList:badgeBackgroundColorForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSColor*)sourceList:(PXSourceList*)aSourceList badgeTextColorForItem:(id)item;

/**
 @brief Returns a color that is used for the badge background color of an item in the Source List.
 @discussion This method can be implemented by the data source to specify a custom badge background color for a particular item.

 This method is only called for *item* if you return `YES` for *item* in sourceList:itemHasBadge:.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return An `NSColor` object to use for the background color of *item*'s badge or `nil` to use the default badge background color.
 
 @warning This method is only used by the Source List when operating in cell-based mode. When the Source List is operating in view-based mode, the view for each cell is responsible for managing a badge, if applicable.

 @see sourceList:itemHasBadge:
 @see sourceList:badgeValueForItem:
 @see sourceList:badgeTextColorForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSColor*)sourceList:(PXSourceList*)aSourceList badgeBackgroundColorForItem:(id)item;

///---------------------------------------------------------------------------------------
/// @name Working with Icons
///---------------------------------------------------------------------------------------
/**
 @brief Returns a Boolean value that indicates whether a given item shows an icon or not.
 @discussion This method can be implemented by the data source to specify whether items contain icons or not. Icons are images which are shown to the left of the row's cell, and provide a visual which accompanies the cell.

 This method must be implemented if you want to return an icon with `sourceList:iconForItem:`.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return `YES` if *item* displays an icon, or `NO` otherwise.

 @warning This method is only used and invoked by the Source List when operating in cell-based mode. When the Source List is operating in view-based mode, the view for each cell is responsible for managing its icon, if applicable.

 @see sourceList:iconForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)sourceList:(PXSourceList*)aSourceList itemHasIcon:(id)item;

/**
 @brief Returns the icon for a given item in the Source List.
 @discussion This method must be implemented by the data source if you return `YES` in `sourceList:itemHasIcon:` for any item in the Source List.

 The maximum size of each icon is specified with the Source List's `iconSize` property. If the returned image is larger than the icon size property on the Source List, then it is proportionally resized down to fit this size.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return An `NSImage` that is to be used for the icon for *item*.

 @warning This method is only used and invoked by the Source List when operating in cell-based mode. When the Source List is operating in view-based mode, the view for each cell is responsible for managing its icon, if applicable.

 @see sourceList:itemHasIcon:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSImage*)sourceList:(PXSourceList*)aSourceList iconForItem:(id)item;

//The rest of these methods are basically "wrappers" for the NSOutlineViewDataSource methods
///---------------------------------------------------------------------------------------
/// @name Supporting Object Persistence
///---------------------------------------------------------------------------------------
/**
 @brief Invoked by *aSourceList* to return the item for the archived *object*.
 @discussion This method is analagous to `-outlineView:itemForPersistentObject:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List that sent the message
 @param object The archived representation of the item in the Source List's data source

 @return The unarchived item corresponding to *object*.

 @see sourceList:persistentObjectForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (id)sourceList:(PXSourceList*)aSourceList itemForPersistentObject:(id)object;

/**
 @brief Invoked by *aSourceList* to return an archived object for *item*.
 @discussion This method is analagous to `-outlineView:persistentObjectForItem:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return The unarchived item corresponding to *object*.

 @see sourceList:persistentObjectForItem:

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (id)sourceList:(PXSourceList*)aSourceList persistentObjectForItem:(id)item;

///---------------------------------------------------------------------------------------
/// @name Supporting Drag and Drop
///---------------------------------------------------------------------------------------
/**
 @brief Returns a Boolean value indicating whether a drag operation is allowed.
 @discussion This method is analagous to `-outlineView:writeItems:toPasteboard:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List that sent the message.
 @param items An array of items that are participating in the drag.
 @param pboard The pasteboard to which to write the drag data.

 @return `YES` if the drag should be allowed and the items were successfully written to the pasteboard, or `NO` otherwise.

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (BOOL)sourceList:(PXSourceList*)aSourceList writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pboard;

/**
 @brief Used by a Source List to determine a valid drop target.
 @discussion This method is analagous to `-outlineView:validateDrop:proposedItem:proposedChildIndex:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param sourceList The Source List that sent the message.
 @param info An object which contains more information about the dragging operation.
 @param item The proposed parent item.
 @param index The proposed child index of the parent.

 @return An `NSDragOperation` value that indicates which dragging operation the Source List should perform.

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSDragOperation)sourceList:(PXSourceList*)sourceList validateDrop:(id < NSDraggingInfo >)info proposedItem:(id)item proposedChildIndex:(NSInteger)index;

/**
 @brief Returns a Boolean value specifying whether a drag operation was successful.
 @discussion This method is analagous to `-outlineView:acceptDrop:item:childIndex:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List that sent the message.
 @param info An object that contains more information about the dragging operation.
 @param item The parent of the item which the cursor was over when the mouse button was released.
 @param index The index of the child of `item` which the cursor was over when the mouse button was released.

 @return `YES` if the drop was successful, or `NO` otherwise.

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK.
 */
- (BOOL)sourceList:(PXSourceList*)aSourceList acceptDrop:(id < NSDraggingInfo >)info item:(id)item childIndex:(NSInteger)index;

/**
 @brief Returns an array of filenames (not file paths) for the created files that the receiver promises to create.
 @discussion This method is analagous to `-outlineView:namesOfPromisedFilesDroppedAtDestination:forDraggedItems:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List that sent the message.
 @param dropDestination The drop location where the files are created.
 @param items The items that are being dragged.

 @return An array of filenames (not file paths) for the created files that the receiver promises to create.

 @since Requires PXSourceList 0.8 or above and the OS X v10.5 SDK or above.
 */
- (NSArray *)sourceList:(PXSourceList*)aSourceList namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination forDraggedItems:(NSArray *)items;

///---------------------------------------------------------------------------------------
/// @name Drag and drop methods for 10.7+
///---------------------------------------------------------------------------------------

/**
 @brief Invoked to allow the Source List to support multiple item dragging.
 @discussion This method is analagous to `-outlineView:pasteboardWriterForItem:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List that sent the message.
 @param item An item in the data source.

 @return Returns an instance of `NSPasteboardItem` or a custom object that implements the `NSPasteboardWriting` protocol. Returning `nil` excludes the item from being dragged.

 @since Requires PXSourceList 0.8 or above and the OS X v10.7 SDK or above.
 */
- (id <NSPasteboardWriting>)sourceList:(PXSourceList *)aSourceList pasteboardWriterForItem:(id)item;

/**
 @brief Implement this method know when the given dragging session is about to begin and potentially modify the dragging session.
 @discussion This method is analagous to `-outlineView:draggingSession:willBeginAtPoint:forItems:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List in which the drag is about to begin.
 @param session The dragging session that is about to begin.
 @param screenPoint The point onscreen at which the drag is to begin.
 @param draggedItems An array of items to be dragged, excluding items for which `sourceList:pasteboardWriterForItem:` returns `nil`.

 @since Requires PXSourceList 0.8 or above and the OS X v10.7 SDK or above.
 */
- (void)sourceList:(PXSourceList *)aSourceList draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)screenPoint forItems:(NSArray *)draggedItems;

/**
 @brief Implement this method to know when the given dragging session has ended.
 @discussion This method is analagous to `-outlineView:draggingSession:endedAtPoint:operation:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List in which the drag ended.
 @param session The dragging session that ended.
 @param screenPoint The point onscreen at which the drag ended.
 @param operation A mask specifying the types of drag operations permitted by the dragging source.

 @since Requires PXSourceList 0.8 or above and the OS X v10.7 SDK or above.
 */
- (void)sourceList:(PXSourceList *)aSourceList draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation;

/**
 @brief Implement this method to enable the Source List to update dragging items as they are dragged over the view.
 @discussion This method is analagous to `-outlineView:updateDraggingItemsForDrag:` declared on `NSOutlineViewDataSource`. See the documentation for this method for more information.

 @param aSourceList The Source List in which the drag occurs.
 @param draggingInfo The dragging info object.

 @since Requires PXSourceList 0.8 or above and the OS X v10.7 SDK or above.
 */
- (void)sourceList:(PXSourceList *)aSourceList updateDraggingItemsForDrag:(id <NSDraggingInfo>)draggingInfo;

@end
