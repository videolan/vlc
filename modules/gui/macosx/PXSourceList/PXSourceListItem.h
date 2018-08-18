//
//  PXSourceListItem.h
//  PXSourceList
//
//  Created by Alex Rozanski on 08/01/2014.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Foundation/Foundation.h>

/**
 
 `PXSourceListItem` is a generic `NSObject` subclass which can be used to build a hierarchical model for use by
 a `PXSourceList` data source.
 
 @warning While it is not mandatory to use `PXSourceListItem` objects in a `PXSourceList` data source, this
 class is generic enough that it should serve most use cases.
 
 @discussion ### Basic properties
 
 `PXSourceListItem` has been designed to contain properties for the frequently-used information which you need
 from a Source List data source item when implementing the `PXSourceListDataSource` (and possibly
 `PXSourceListDelegate`) methods, namely:
 
   * The title displayed in the Source List for the given item.
   * The icon displayed to the left of the given item in the Source List.
   * The badge value displayed to the right of the given item in the Source List.
   * Child items of the given item.
 
 The existence of these core properties means that it is unlikely that you should have to create your own
 `PXSourceListItem` subclass.
 
 ### Identifying objects
 
 `PXSourceListItem`s are often backed by data model objects that are used in other parts of your application, and
 the API has been designed to be able to easily identify a given model object from any part of your code
 given an arbitrary `PXSourceListItem`. This is useful when you obtain an item using one of `PXSourceList`'s methods
 or are given one as an argument to a `PXSourceListDelegate` or `PXSourceListDataSource` protocol method and you
 need to find its backing data model object to be able to use in application logic.
 
 There are two (often distinct) patterns used to identify a given backing model object in a `PXSourceListItem`
 object:

   * Using the `identifier` property. This is probably the easiest way of identifying items, and these identifiers
     are best defined as string constants which you can reference from multiple places in your code.
   * Using the `representedObject` property. Using `representedObject` can be useful if the underlying model
     object has identifying information about it which you can use when determining which object you're
     working with given a `PXSourceListItem` instance.

 */
@interface PXSourceListItem : NSObject

@property (strong, nonatomic) NSString *title;
@property (strong, nonatomic) NSImage *icon;
@property (weak, nonatomic) id representedObject;
@property (strong, nonatomic) NSString *identifier;
@property (strong, nonatomic) NSNumber *badgeValue;

///---------------------------------------------------------------------------------------
/// @name Convenience initialisers
///---------------------------------------------------------------------------------------
/** Creates and returns an item with the given parameters.

 @param title A title.
 @param identifier An identifier.

 @return An item initialised with the given parameters.
 
 @see itemWithTitle:identifier:icon:
 @see itemWithRepresentedObject:icon:
 
 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
+ (instancetype)itemWithTitle:(NSString *)title identifier:(NSString *)identifier;

/** Creates and returns an item with the given parameters.

 @param title A title.
 @param identifier An identifier.
 @param icon An icon.

 @return An item initialised with the given parameters.
 
 @see itemWithTitle:identifier:
 @see itemWithRepresentedObject:icon:
 
 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
+ (instancetype)itemWithTitle:(NSString *)title identifier:(NSString *)identifier icon:(NSImage *)icon;

/** Creates and returns an item with the given parameters.

 @param object An object.
 @param icon An icon.

 @return An item initialised with the given parameters.
 
 @see itemWithTitle:identifier:
 @see itemWithTitle:identifier:icon:
 
 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
+ (instancetype)itemWithRepresentedObject:(id)object icon:(NSImage *)icon;

///---------------------------------------------------------------------------------------
/// @name Working with child items
///---------------------------------------------------------------------------------------
/**
 @brief Returns the receiver's children.

 @warning This property is backed by an `NSMutableArray` since an item's children *are* mutable. The getter for
 this property returns a copied array for safety, so this getter should not be called excessively.

 @see hasChildren

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
@property (strong, nonatomic) NSArray *children;

/**
 @brief Returns whether the receiver has any child items.
 @discussion This is faster than calling `-children` on the receiver then checking the number of items in the array
 because of how this getter is implemented. See `-children` for more information.

 @see children

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (BOOL)hasChildren;

/**
 @brief Adds an item to the receiver's array of child items.
 @discussion Adds *item* to the end of the receiver's array of child items.
 
 This is a convenience method rather than having to call `-children` on the receiver, create a mutable copy
 and then mutate this array before setting it back on the receiver.
 
 @param childItem An item

 @see insertChildItem:atIndex:

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (void)addChildItem:(PXSourceListItem *)childItem;

/**
 @brief Inserts an item to the receiver's array of child items at a given index.
 @discussion Inserts *item* at *index* in the receiver's array of child items.
 
 This is a convenience method rather than having to call `-children` on the receiver, create a mutable copy
 and then mutate this array before setting it back on the receiver.
 
 @param childItem An item
 @param index An index

 @see addChildItem:
 @see insertChildItems:atIndexes:

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (void)insertChildItem:(PXSourceListItem *)childItem atIndex:(NSUInteger)index;

/**
 @brief Removes an item from the receiver's array of child items.
 @discussion Removes *item* from the receiver's array of child items.
 
 This is a convenience method rather than having to call `-children` on the receiver, create a mutable copy
 and then mutate this array before setting it back on the receiver.
 
 @param childItem An item

 @see removeChildItemAtIndex:
 @see removeChildItems:

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (void)removeChildItem:(PXSourceListItem *)childItem;

/**
 @brief Removes the item at the given index from the receiver's array of child items.
 @discussion Removes the item at the given *index* from the receiver's array of child items.
 
 This is a convenience method rather than having to call `-children` on the receiver, create a mutable copy
 and then mutate this array before setting it back on the receiver.
 
 @param index An integer representing an index in the receiver's array of children

 @see removeChildItem:
 @see removeChildItems:

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (void)removeChildItemAtIndex:(NSUInteger)index;

/**
 @brief Removes the items in the given array from the receiver's array of child items.
 @discussion Removes all of the items in *items* from the receiver's array of children.
 
 This is a convenience method rather than having to call `-children` on the receiver, create a mutable copy
 and then mutate this array before setting it back on the receiver.
 
 @param items An array of `PXSourceListItem` objects

 @see removeChildItem:
 @see removeChildItemAtIndex:

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (void)removeChildItems:(NSArray *)items;

/**
 @brief Inserts the given items into the receiver's array of child items at the given indexes.
 @discussion Inserts all of the items in *items* to the receiver's array of children at the given *indexes*.
 
 This is a convenience method rather than having to call `-children` on the receiver, create a mutable copy
 and then mutate this array before setting it back on the receiver.
 
 @param items An array of `PXSourceListItem` objects
 @param indexes The indexes to insert the child items at

 @see insertChildItem:atIndex:

 @since Requires PXSourceList 2.0.0 and above and the Mac OS X 10.7 SDK or above.
 */
- (void)insertChildItems:(NSArray *)items atIndexes:(NSIndexSet *)indexes;

@end
