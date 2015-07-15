//
//  SideBarItem.m
//  PXSourceList
//
//  Created by Alex Rozanski on 08/01/2010.
//  Copyright 2010 Alex Rozanski http://perspx.com
//
//  GC-enabled code revised by Stefan Vogt http://byteproject.net
//
//  Adapted to VLC media player by Felix Paul Kühne


#import "SideBarItem.h"


@implementation SideBarItem

#pragma mark -
#pragma mark Init/Dealloc/Finalize

- (id)init
{
    if (self=[super init]) {
        _badgeValue = -1; //We don't want a badge value by default
        _sdtype = -1; //no sd type set
    }

    return self;
}


+ (id)itemWithTitle:(NSString*)aTitle identifier:(NSString*)anIdentifier
{
    SideBarItem *item = [SideBarItem itemWithTitle:aTitle identifier:anIdentifier icon:nil];

    return item;
}


+ (id)itemWithTitle:(NSString*)aTitle identifier:(NSString*)anIdentifier icon:(NSImage*)anIcon
{

    SideBarItem *item = [[SideBarItem alloc] init];

    [item setTitle:aTitle];
    [item setIdentifier:anIdentifier];
    [item setIcon:anIcon];

    return item;
}

#pragma mark -
#pragma mark Custom Accessors

- (BOOL)hasBadge
{
    return _badgeValue!=-1;
}

- (BOOL)hasChildren
{
    return [_children count]>0;
}

- (BOOL)hasIcon
{
    return _icon!=nil;
}

@end
