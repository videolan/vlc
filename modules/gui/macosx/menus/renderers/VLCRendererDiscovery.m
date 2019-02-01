/*****************************************************************************
 * VLCRendererDiscovery.m: Wrapper class for vlc_renderer_discovery_t
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCRendererDiscovery.h"

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

#import "main/VLCMain.h"

@interface VLCRendererDiscovery ()
{
    intf_thread_t               *p_intf;
    vlc_renderer_discovery_t    *p_rd;
    BOOL                        _isRunning;
}

- (void)handleItemAdded:(vlc_renderer_item_t *)item;
- (void)handleItemRemoved:(const vlc_renderer_item_t *)item;
@end

// C callback event handler functions
static void renderer_event_item_added(vlc_renderer_discovery_t *rd,
                                      vlc_renderer_item_t *item)
{
    VLCRendererDiscovery *target = (__bridge VLCRendererDiscovery*)rd->owner.sys;
    [target handleItemAdded:item];
}

static void renderer_event_item_removed(vlc_renderer_discovery_t *rd,
                                        vlc_renderer_item_t *item)
{
    VLCRendererDiscovery *target = (__bridge VLCRendererDiscovery*)rd->owner.sys;
    [target handleItemRemoved:item];
}

@implementation VLCRendererDiscovery

- (instancetype)initWithName:(const char*)name andLongname:(const char*)longname
{
    self = [super init];

    if (self) {
        if (!name)
            [NSException raise:NSInvalidArgumentException
                        format:@"name must not be nil"];
        _name = [NSString stringWithUTF8String:name];
        _longName = (!longname) ? nil : [NSString stringWithUTF8String:longname];
        _rendererItems = [NSMutableArray array];
    }
    return self;
}

- (void)dealloc
{
    [self stopDiscovery];
}

- (bool)startDiscovery
{
    if (_isRunning) {
        return YES;
    }

    struct vlc_renderer_discovery_owner owner =
    {
        (__bridge void *) self,
        renderer_event_item_added,
        renderer_event_item_removed,
    };

    p_intf = getIntf();

    msg_Dbg(p_intf, "Starting renderer discovery service %s", _name.UTF8String);
    // Create renderer object
    p_rd = vlc_rd_new(VLC_OBJECT(p_intf), _name.UTF8String, &owner);

    if (!p_rd) {
        _isRunning = NO;
        msg_Err(p_intf, "Could not create '%s' renderer discovery service", _name.UTF8String);
        return false;
    }

    _isRunning = YES;
    return true;
}

- (void)stopDiscovery
{
    if (p_rd != NULL) {
        vlc_rd_release(p_rd);
        p_rd = NULL;
        _isRunning = NO;
    }
}

- (void)handleItemAdded:(vlc_renderer_item_t *)base_item
{
    VLCRendererItem *item = [[VLCRendererItem alloc] initWithRendererItem:base_item];
    [_rendererItems addObject:item];
    if (_delegate)
        [_delegate addedRendererItem:item from:self];
}

- (void)handleItemRemoved:(const vlc_renderer_item_t *)base_item
{
    VLCRendererItem *result_item = nil;
    for (VLCRendererItem *item in _rendererItems) {
        if (item.rendererItem == base_item) {
            result_item = item;
            break;
        }
    }
    if (result_item) {
        if (_delegate)
            [_delegate removedRendererItem:result_item from:self];
        [_rendererItems removeObject:result_item];
    } else {
        msg_Err(p_intf, "VLCRendererDiscovery could not find item to remove!");
    }
}

@end
