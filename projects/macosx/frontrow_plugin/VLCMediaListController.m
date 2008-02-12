//
//  VLCMediaListController.m
//  FRVLC
//
//  Created by Pierre d'Herbemont on 2/11/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "VLCMediaListController.h"
#import "VLCPlayerController.h"

#import <BackRow/BRListControl.h>
#import <BackRow/BRTextMenuItemLayer.h>
#import <BackRow/BRControllerStack.h>

@interface VLCMediaListController ()

@property(retain, nonatomic) VLCMediaListAspect * mediaListAspect;

@end

@implementation VLCMediaListController

@synthesize mediaListAspect;

- initWithMediaListAspect:(VLCMediaListAspect *)aMediaListAspect
{
    self = [super init];
        
    self.mediaListAspect = aMediaListAspect;
    [self.mediaListAspect addObserver:self forKeyPath:@"media" options:NSKeyValueChangeRemoval|NSKeyValueChangeInsertion|NSKeyValueChangeSetting context:nil];
    [[self list] setDatasource:self];
    isReloading = NO;

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
            [self performSelector:@selector(reload) withObject:nil afterDelay:2.];
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
    BOOL isDirectory = ![[mediaListAspect nodeAtIndex:row] isLeaf];
    
    BRController * controller = nil;
    
    if(isDirectory) {
        controller = [[[VLCMediaListController alloc] initWithMediaListAspect:[[mediaListAspect nodeAtIndex:row] children]] autorelease];
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