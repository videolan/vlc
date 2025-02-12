/*****************************************************************************
 * VLCMediaLibraryFolderObserver.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCMediaLibraryFolderObserver.h"

NSString * const VLCMediaLibraryFolderFSEvent = @"VLCMediaLibraryFolderFSEvent";
NSString * const VLCMediaLibraryFolderFSLastEventIdPrefix = @"VLCMediaLibraryFolderFSLastEventId_";

@interface VLCMediaLibraryFolderObserver ()

@property (readonly) FSEventStreamRef stream;
@property (readonly) CFStringRef urlPathRef;
@property (readonly) CFArrayRef pathsToWatch;
@property (readonly) NSString *defaultsKey;

@end

void fsEventCallback(ConstFSEventStreamRef streamRef,
                     void *clientCallBackInfo,
                     size_t numEvents,
                     void *eventPaths,
                     const FSEventStreamEventFlags eventFlags[],
                     const FSEventStreamEventId eventIds[])
{
    if (numEvents == 0) {
        return;
    }
    
    VLCMediaLibraryFolderObserver * const observer =
        (__bridge VLCMediaLibraryFolderObserver *)clientCallBackInfo;
    NSNumber * const eventId = @(eventIds[0]);
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSUserDefaults.standardUserDefaults setObject:eventId forKey:observer.defaultsKey];
        [NSNotificationCenter.defaultCenter postNotificationName:VLCMediaLibraryFolderFSEvent
                                                          object:observer.url];
    });
}

@implementation VLCMediaLibraryFolderObserver

- (instancetype)initWithURL:(NSURL *)url
{
    self = [super init];
    if (self) {
        _url = url;
        _urlPathRef = (__bridge_retained CFStringRef)url.path;
        _pathsToWatch = CFArrayCreate(NULL, (const void **)&_urlPathRef, 1, NULL);
        _defaultsKey =
            [NSString stringWithFormat:@"%@%@", VLCMediaLibraryFolderFSLastEventIdPrefix, url.path];

        FSEventStreamContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
        const FSEventStreamCreateFlags createFlags =
            kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagIgnoreSelf;

        NSNumber * const lastEventIdDefaults =
            (NSNumber *)[NSUserDefaults.standardUserDefaults objectForKey:self.defaultsKey];
        const FSEventStreamEventId lastEventId = lastEventIdDefaults == nil
            ? kFSEventStreamEventIdSinceNow
            : lastEventIdDefaults.unsignedLongLongValue;

        _stream = FSEventStreamCreate(kCFAllocatorDefault,
                                      &fsEventCallback,
                                      &context,
                                      _pathsToWatch,
                                      lastEventId,
                                      3.0,
                                      createFlags);

        FSEventStreamSetDispatchQueue(_stream, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));
        NSAssert(FSEventStreamStart(_stream), @"FSEvent stream should be started for %@", url.path);
    }
    return self;
}

- (void)dealloc
{
    FSEventStreamStop(_stream);
    FSEventStreamRelease(_stream);
    CFRelease(_urlPathRef);
    CFRelease(_pathsToWatch);
}

@end
