/*****************************************************************************
 * VLCLocalMediaSourceNodeObservation.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLocalMediaSourceNodeObservation.h"

#include <fcntl.h>
#include <unistd.h>

@interface VLCLocalMediaSourceNodeObservation ()
{
    dispatch_source_t _dispatchSource;
}
@end

@implementation VLCLocalMediaSourceNodeObservation

- (nullable instancetype)initWithURL:(NSURL *)url
                         eventHandler:(void (^)(dispatch_source_vnode_flags_t eventFlags))eventHandler
{
    self = [super init];
    if (self) {
        const int descriptor = open(url.path.UTF8String, O_EVTONLY);
        if (descriptor == -1) {
            return nil;
        }

        _dispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE,
                                                 descriptor,
                                                 DISPATCH_VNODE_WRITE |
                                                 DISPATCH_VNODE_DELETE |
                                                 DISPATCH_VNODE_RENAME,
                                                 dispatch_get_main_queue());
        if (_dispatchSource == nil) {
            close(descriptor);
            return nil;
        }

        const __weak typeof(self) weakSelf = self;
        dispatch_source_set_event_handler(_dispatchSource, ^{
            const typeof(self) strongSelf = weakSelf;
            if (strongSelf == nil || dispatch_source_testcancel(strongSelf->_dispatchSource)) {
                return;
            }
            eventHandler(dispatch_source_get_data(strongSelf->_dispatchSource));
        });
        dispatch_source_set_cancel_handler(_dispatchSource, ^{
            close(descriptor);
        });
        dispatch_resume(_dispatchSource);
    }
    return self;
}

- (void)dealloc
{
    [self cancel];
}

- (void)cancel
{
    if (_dispatchSource != nil) {
        dispatch_source_cancel(_dispatchSource);
    }
}

@end
