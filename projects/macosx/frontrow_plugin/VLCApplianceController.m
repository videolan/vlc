/*****************************************************************************
 * VLCApplianceController.m: Front Row plugin
 *****************************************************************************
 * Copyright (C) 2007 - 2008 the VideoLAN Team
 * $Id$
 *
 * Authors: hyei
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

#import "VLCApplianceController.h"

#import <BackRow/BRListControl.h>
#import <BackRow/BRTextMenuItemLayer.h>
#import <BackRow/BRControllerStack.h>

#import "VLCPlayerController.h"

@interface VLCApplianceController ()

@property(retain, nonatomic) NSString * path;

@end

@implementation VLCApplianceController

@synthesize path=_path;

- initWithPath:(NSString*)path
{
    self = [super init];
    
    _contents = [[NSMutableArray alloc] init];
    
    self.path = path;
    
    [[self header] setTitle:[[NSFileManager defaultManager] displayNameAtPath:self.path]];
    [[self list] setDatasource:self];
    
    return self;
}

- (void)dealloc
{
    [_path release];
    [_contents release];
    [super dealloc];
}

- (void)setPath:(NSString*)path
{
    if(path != _path) {
        [_path release];
        _path = [path retain];
        
        [_contents removeAllObjects];
        
        NSArray * contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:_path error:NULL];
        
        for(NSString * name in contents) {
            NSString * filepath = [path stringByAppendingPathComponent:name];
            int ok = 0;
            
            if([name hasPrefix:@"."]) {
                ok = -1;
            }
            
            if(ok == 0) {
                BOOL directory;
                if(![[NSFileManager defaultManager] fileExistsAtPath:filepath isDirectory:&directory]) {
                    ok = -1;
                }
                else if(directory) {
                    ok = 1;
                }
            }
            
            if(ok == 0) {
                NSString * type = [[NSWorkspace sharedWorkspace] typeOfFile:filepath error:NULL];
                if([[NSWorkspace sharedWorkspace] type:type conformsToType:(NSString*)kUTTypeMovie]) {
                    ok = 1;
                }
            }
                
            if(ok == 0) {
                static NSSet * additionalValidExtensions = nil;
                if(additionalValidExtensions == nil) {
                    additionalValidExtensions = [[NSSet alloc] initWithObjects:
                                                 @"mkv",
                                                 nil];
                }
                
                NSString * extension = [[name pathExtension] lowercaseString];
                if([additionalValidExtensions containsObject:extension]) {
                    ok = 1;
                }
            }
            
            if(ok == 1) {
                [_contents addObject:name];
            }
        }
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
#pragma mark Utilities

- (NSString*)pathForRow:(NSInteger)row
{
    NSString * name = [_contents objectAtIndex:row];
    return [self.path stringByAppendingPathComponent:name];
}

- (BOOL)isDirectoryAtPath:(NSString*)path
{
    NSDictionary * attributes = [[NSFileManager defaultManager] fileAttributesAtPath:path traverseLink:YES];
    NSString * type = [attributes objectForKey:NSFileType];
    return [type isEqualToString:NSFileTypeDirectory];
}


#pragma mark -
#pragma mark Data source

- (NSInteger)itemCount
{
    return [_contents count];
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
    return [_contents objectAtIndex:row];
}

- (id)itemForRow:(NSInteger)row
{
    NSString * path = [self pathForRow:row];
    BOOL isDirectory = [self isDirectoryAtPath:path];
    
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

- (NSInteger)rowForTitle:(NSString *)title
{
    return [_contents indexOfObject:title];
}

- (void)itemSelected:(NSInteger)row
{
    NSString * path = [self pathForRow:row];
    BOOL isDirectory = [self isDirectoryAtPath:path];
    
    BRController * controller = nil;
    
    if(isDirectory) {
        controller = [[[VLCApplianceController alloc] initWithPath:path] autorelease];
    }
    else {
#ifdef FAKE
        controller = [[[VLCAppPlayerController alloc] initWithPath:path] autorelease];
#else
        static VLCPlayerController * playerController = nil;
        if(playerController == nil) {
            playerController = [[VLCPlayerController alloc] init];
        }
        
        playerController.media = [VLCMedia mediaWithPath:path];
        controller = playerController;
#endif
    }
    
    if(controller != nil) {
        [[self stack] pushController:controller];
    }
}

@end
