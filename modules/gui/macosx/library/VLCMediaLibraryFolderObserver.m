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

void fsEventCallback(ConstFSEventStreamRef streamRef,
                     void *clientCallBackInfo,
                     size_t numEvents,
                     void *eventPaths,
                     const FSEventStreamEventFlags *eventFlags,
                     const FSEventStreamEventId *eventIds)
{
    VLCMediaLibraryFolderObserver * const observer =
        (__bridge VLCMediaLibraryFolderObserver *)clientCallBackInfo;
}

@interface VLCMediaLibraryFolderObserver ()

@property (readonly) FSEventStreamRef stream;

@end

@implementation VLCMediaLibraryFolderObserver

- (instancetype)initWithURL:(NSURL *)url
{
    self = [super init];
    if (self) {
        _url = url;
        const CFStringRef urlPathRef = (__bridge CFStringRef)url.path;
        const CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)&urlPathRef, 1, NULL);
        const FSEventStreamCreateFlags createFlags =
            kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagIgnoreSelf;

        _stream = FSEventStreamCreate(kCFAllocatorDefault,
                                      &fsEventCallback,
                                      (__bridge void *)self,
                                      pathsToWatch,
                                      kFSEventStreamEventIdSinceNow,
                                      3.0,
                                      createFlags);

        FSEventStreamSetDispatchQueue(_stream, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));
        FSEventStreamStart(self.stream);
    }
    return self;
}

@end
