//
//  PXSourceList.h
//  PXSourceList
//
//  Created by Alex Rozanski on 05/09/2009.
//  Copyright 2009-10 Alex Rozanski http://perspx.com
//

#import <Cocoa/Cocoa.h>

#import "PXSourceListDelegate.h"
#import "PXSourceListDataSource.h"

@interface PXSourceList: NSOutlineView <NSOutlineViewDelegate, NSOutlineViewDataSource>
{
    id <PXSourceListDelegate> _secondaryDelegate; //Used to store the publicly visible delegate
    id <PXSourceListDataSource> _secondaryDataSource; //Used to store the publicly visible data source
}

@property (nonatomic) NSSize iconSize;

@property (assign) id<PXSourceListDataSource, NSOutlineViewDataSource> _Nullable dataSource;
@property (assign) id<PXSourceListDelegate, NSOutlineViewDelegate> _Nullable delegate;

- (NSUInteger)numberOfGroups; //Returns the number of groups in the Source List
- (BOOL)isGroupItem:(nonnull id)item; //Returns whether `item` is a group
- (BOOL)isGroupAlwaysExpanded:(nonnull id)group; //Returns whether `group` is displayed as always expanded

- (BOOL)itemHasBadge:(nonnull id)item; //Returns whether `item` has a badge
- (NSInteger)badgeValueForItem:(nonnull id)item; //Returns the badge value for `item`

@end
