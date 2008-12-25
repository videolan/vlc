/*****************************************************************************
 * VLCMediaListController.m: Front Row plugin
 *****************************************************************************
 * Copyright (C) 2007 - 2008 the VideoLAN Team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCMediaListController.h"
#import "VLCPlayerController.h"

#import <BackRow/BRListControl.h>
#import <BackRow/BRTextMenuItemLayer.h>
#import <BackRow/BRControllerStack.h>
#import <BackRow/BRHeaderControl.h>

@interface VLCMediaListController ()

@property(retain, nonatomic) VLCMediaListAspect * mediaListAspect;

@end

@implementation VLCMediaListController

@synthesize mediaListAspect;

- initWithMediaListAspect:(VLCMediaListAspect *)aMediaListAspect
{
    return [self initWithMediaListAspect:aMediaListAspect andTitle:nil];
}

- initWithMediaListAspect:(VLCMediaListAspect *)aMediaListAspect andTitle:(NSString *)title
{
    if( self = [super init] )
    {
        self.mediaListAspect = aMediaListAspect;
        [self.mediaListAspect addObserver:self forKeyPath:@"media" options:NSKeyValueChangeRemoval|NSKeyValueChangeInsertion|NSKeyValueChangeSetting context:nil];
        [[self list] setDatasource:self];
        isReloading = NO;
        if(title)
        {
            [[self header] setTitle: title];
        }
    }
    return self;
}

- (void)dealloc
{
    [self.mediaListAspect removeObserver:self forKeyPath:@"media"];
    [mediaListAspect release];
    [super dealloc];
}

- (void) observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([keyPath isEqualToString:@"media"]) {
        if(!isReloading)
        {
            isReloading = YES;
            [self performSelector:@selector(reload) withObject:nil afterDelay: [[self list] itemCount] > 10 ? 2. : [[self list] itemCount] ? 0.3 : 0.0];
        }
    }
    else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

- (void)willBePushed
{
    PRINT_ME();
}

- (void)willBePopped
{
    PRINT_ME();    
}

#pragma mark -
#pragma mark Reload hack

- (void) reload
{
    [[self list] reload];
    isReloading = NO;
}

#pragma mark -
#pragma mark Data source

- (NSInteger)itemCount
{
    return [mediaListAspect count];
}

- (CGFloat)heightForRow:(NSInteger)row
{
    return 64.0;
}

- (BOOL)rowSelectable:(NSInteger)row
{
    return YES;
}

- (NSString*)titleForRow:(NSInteger)row
{
    return [[mediaListAspect mediaAtIndex:row] valueForKeyPath:@"metaDictionary.title"];
}

- (id)itemForRow:(NSInteger)row
{
    BOOL isDirectory = ![[mediaListAspect nodeAtIndex:row] isLeaf];
    
    BRTextMenuItemLayer * item = nil;

    if(isDirectory) {
        item = [BRTextMenuItemLayer folderMenuItem];
    }
    else {
        item = [BRTextMenuItemLayer menuItem];
    }

    [item setTitle:[self titleForRow:row]];
    
    return item;
}

- (void)itemSelected:(NSInteger)row
{
    VLCMediaListAspectNode * node = [mediaListAspect nodeAtIndex:row];
    BOOL isDirectory = ![node isLeaf];
    
    BRController * controller = nil;
    
    if(isDirectory) {
        controller = [[[VLCMediaListController alloc] initWithMediaListAspect:[node children] andTitle:[[node media] valueForKeyPath:@"metaDictionary.title"]] autorelease];
    }
    else {
        static VLCPlayerController * playerController = nil;
        if(playerController == nil) {
            playerController = [[VLCPlayerController alloc] init];
        }
        
        playerController.media = [mediaListAspect mediaAtIndex:row];
        controller = playerController;
    }
    
    if(controller != nil) {
        [[self stack] pushController:controller];
    }
}

@end