//
//  PXSourceListItem.m
//  PXSourceList
//
//  Created by Alex Rozanski on 08/01/2014.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import "PXSourceListItem.h"

@implementation PXSourceListItem {
    NSMutableArray *_children;
}

+ (instancetype)itemWithTitle:(NSString *)title identifier:(NSString *)identifier
{
    return [self itemWithTitle:title identifier:identifier icon:nil];
}

+ (instancetype)itemWithTitle:(NSString *)title identifier:(NSString *)identifier icon:(NSImage *)icon
{
    PXSourceListItem *item = [[self alloc] init];

    item.title = title;
    item.identifier = identifier;
    item.icon = icon;

    return item;
}

+ (instancetype)itemWithRepresentedObject:(id)object icon:(NSImage *)icon
{
    PXSourceListItem *item = [[self alloc] init];

    item.representedObject = object;
    item.icon = icon;

    return item;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;

    _children = [[NSMutableArray alloc] init];

    return self;
}

#pragma mark - Custom Accessors

- (NSArray *)children
{
    return [_children copy];
}

- (void)setChildren:(NSArray *)children
{
    _children = [children mutableCopy];
}

#pragma mark - Child Convenience Methods

- (BOOL)hasChildren
{
    return _children.count > 0;
}

- (void)addChildItem:(PXSourceListItem *)childItem
{
    [_children addObject:childItem];
}

- (void)insertChildItem:(PXSourceListItem *)childItem atIndex:(NSUInteger)index
{
    [_children insertObject:childItem atIndex:index];
}

- (void)removeChildItem:(PXSourceListItem *)childItem
{
    [_children removeObject:childItem];
}

- (void)removeChildItemAtIndex:(NSUInteger)index
{
    [_children removeObjectAtIndex:index];
}

- (void)removeChildItems:(NSArray *)items
{
    [_children removeObjectsInArray:items];
}

- (void)insertChildItems:(NSArray *)items atIndexes:(NSIndexSet *)indexes
{
    [_children insertObjects:items atIndexes:indexes];
}

@end
