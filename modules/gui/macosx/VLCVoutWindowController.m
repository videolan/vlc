/*****************************************************************************
 * VLCVoutWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCVoutWindowController.h"
#import "intf.h"
#import "Windows.h"

@implementation VLCVoutWindowController

- (id)init
{
    self = [super init];
    o_vout_dict = [[NSMutableDictionary alloc] init];
    return self;
}

- (void)dealloc
{
    [o_vout_dict release];
    [super dealloc];
}

- (void)addVout:(VLCVideoWindowCommon *)o_window forDisplay:(vout_window_t *)p_wnd
{
    [o_vout_dict setObject:o_window forKey:[NSValue valueWithPointer:p_wnd]];
}

- (void)removeVoutforDisplay:(NSValue *)o_key
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:o_key];
    if(!o_window) {
        msg_Err(VLCIntf, "Cannot close nonexisting window");
        return;
    }

    if (![NSStringFromClass([o_window class]) isEqualToString:@"VLCMainWindow"]) {
        [o_window orderOut:self];
    }

    [o_vout_dict removeObjectForKey:o_key];
}

- (void)updateWindowsControlsBarWithSelector:(SEL)aSel
{
    [o_vout_dict enumerateKeysAndObjectsUsingBlock:^(id key, VLCVideoWindowCommon *o_window, BOOL *stop) {
        id o_controlsBar = [o_window controlsBar];
        if (o_controlsBar)
            [o_controlsBar performSelector:aSel];
    }];
}

- (void)updateWindowsUsingBlock:(void (^)(VLCVideoWindowCommon *o_window))windowUpdater
{
    [o_vout_dict enumerateKeysAndObjectsUsingBlock:^(id key, VLCVideoWindowCommon *o_window, BOOL *stop) {
        windowUpdater(o_window);
    }];
}

- (void)setNativeVideoSize:(NSSize)size forWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    if(!o_window) {
        msg_Err(VLCIntf, "Cannot set size for nonexisting window");
        return;
    }

    [o_window setNativeVideoSize:size];
}

@end