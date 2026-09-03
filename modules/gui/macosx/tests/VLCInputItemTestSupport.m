/****************************************************************************
 * VLCInputItemTestSupport.m: VLC input item test-only application stubs
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *****************************************************************************/

#import <Cocoa/Cocoa.h>
#import <vlc_interface.h>

#import "VLCInputItemTestSupport.h"

const char vlc_module_name[] = "VLCMacOSXTests";

static NSImage *sQuickLookImage;
static NSImage *sWorkspaceImage;
static BOOL sDidReveal;
static BOOL sDidReload;
static intf_thread_t *sInterfaceThread;

@interface VLCLibraryController : NSObject
@end

@interface VLCMain : NSObject
+ (instancetype)sharedInstance;
@end

@interface VLCLibraryNameCache : NSObject
+ (instancetype)sharedInstance;
- (NSString *)artistNameForID:(int64_t)artistID;
@end

@implementation VLCLibraryController

- (void)reloadMediaLibraryFoldersForInputItems:(NSArray * __unused)inputItems
{
    sDidReload = YES;
}

@end

@implementation VLCMain

+ (instancetype)sharedInstance
{
    static VLCMain *main;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        main = [[VLCMain alloc] init];
    });
    return main;
}

- (VLCLibraryController *)libraryController
{
    static VLCLibraryController *libraryController;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        libraryController = [[VLCLibraryController alloc] init];
    });
    return libraryController;
}

@end

@implementation VLCLibraryNameCache

+ (instancetype)sharedInstance
{
    static VLCLibraryNameCache *cache;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [[VLCLibraryNameCache alloc] init];
    });
    return cache;
}

- (NSString *)artistNameForID:(int64_t __unused)artistID
{
    return nil;
}

@end

intf_thread_t *getIntf(void)
{
    return sInterfaceThread;
}

void VLCInputItemTestSetQuickLookImage(NSImage *image)
{
    sQuickLookImage = image;
}

void VLCInputItemTestSetWorkspaceImage(NSImage *image)
{
    sWorkspaceImage = image;
}

void VLCInputItemTestSetInterface(intf_thread_t *interfaceThread)
{
    sInterfaceThread = interfaceThread;
}

void VLCInputItemTestResetAppKitState(void)
{
    sQuickLookImage = nil;
    sWorkspaceImage = nil;
    sDidReveal = NO;
    sDidReload = NO;
    sInterfaceThread = NULL;
}

BOOL VLCInputItemTestDidReveal(void)
{
    return sDidReveal;
}

BOOL VLCInputItemTestDidReload(void)
{
    return sDidReload;
}

@interface NSImage (VLCInputItemTestSupport)
+ (void)quickLookPreviewForLocalPath:(NSString *)path
                             withSize:(NSSize)size
                    completionHandler:(void (^)(NSImage *image))completionHandler;
@end

@interface NSWorkspace (VLCInputItemTestSupport)
- (NSImage *)iconForFile:(NSString *)fullPath;
- (void)activateFileViewerSelectingURLs:(NSArray<NSURL *> *)fileURLs;
@end

@implementation NSWorkspace (VLCInputItemTestSupport)

- (NSImage *)iconForFile:(NSString * __unused)fullPath
{
    return sWorkspaceImage;
}

- (void)activateFileViewerSelectingURLs:(NSArray<NSURL *> * __unused)fileURLs
{
    sDidReveal = YES;
}

@end

@implementation NSImage (VLCInputItemTestSupport)

+ (void)quickLookPreviewForLocalPath:(NSString * __unused)path
                             withSize:(NSSize __unused)size
                    completionHandler:(void (^)(NSImage *image))completionHandler
{
    completionHandler(sQuickLookImage);
}

@end
