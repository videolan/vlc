/*****************************************************************************
 * VLCRendererDiscovery.m: Wrapper class for vlc_renderer_discovery
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
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

#import "intf.h"

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

@interface VLCRendererDiscovery ()
{
    intf_thread_t          *p_intf;
    vlc_renderer_discovery *p_rd;
}

- (void)handleEvent:(const vlc_event_t *)event;
@end

// C callback event handler function for vlc_event_manager
static void renderer_event_received(const vlc_event_t *p_event, void *user_data)
{
    VLCRendererDiscovery *target = (__bridge VLCRendererDiscovery*)user_data;
    [target handleEvent:p_event];
}

@implementation VLCRendererDiscovery

- (instancetype)initWithName:(const char*)name andLongname:(const char*)longname
{
    self = [super init];

    if (self) {
        if (!name)
            [NSException raise:NSInvalidArgumentException
                        format:@"name must not be nil"];

        // Create renderer object
        p_intf = getIntf();
        p_rd = vlc_rd_new(VLC_OBJECT(p_intf), name);

        if (p_rd) {
            _name = [NSString stringWithUTF8String:name];
            _longName = (!longname) ? nil : [NSString stringWithUTF8String:longname];
            _discoveryStarted = false;
        } else {
            msg_Err(p_intf, "Could not create '%s' renderer discovery service", name);
            self = nil;
        }
    }
    return self;
}

- (void)dealloc
{
    if (_discoveryStarted)
        [self stopDiscovery];
    if (p_rd != NULL)
        vlc_rd_release(p_rd);
}

- (bool)startDiscovery
{
    msg_Dbg(p_intf, "Starting renderer discovery service %s", _name.UTF8String);
    [self attachEventHandlers];
    int ret = vlc_rd_start(p_rd);
    if (ret == VLC_SUCCESS) {
        _discoveryStarted = true;
        return true;
    } else {
        msg_Err(p_intf, "Could not start '%s' renderer discovery", _name.UTF8String);
        [self detachEventHandler];
        return false;
    }
}

- (void)stopDiscovery
{
    if (_discoveryStarted) {
        [self detachEventHandler];
        vlc_rd_stop(p_rd);
        _discoveryStarted = false;
    }
}

- (void)attachEventHandlers
{
    vlc_event_manager_t *em = vlc_rd_event_manager(p_rd);
    vlc_event_attach(em, vlc_RendererDiscoveryItemAdded, renderer_event_received, (__bridge void *)self);
    vlc_event_attach(em, vlc_RendererDiscoveryItemRemoved, renderer_event_received, (__bridge void *)self);
}

- (void)detachEventHandler
{
    vlc_event_manager_t *em = vlc_rd_event_manager(p_rd);
    vlc_event_detach(em, vlc_RendererDiscoveryItemAdded, renderer_event_received, (__bridge void *)self);
    vlc_event_detach(em, vlc_RendererDiscoveryItemRemoved, renderer_event_received, (__bridge void *)self);
}

- (void)handleEvent:(const vlc_event_t *)event
{
    if (event->type == vlc_RendererDiscoveryItemAdded) {
        vlc_renderer_item *base_item =  event->u.renderer_discovery_item_added.p_new_item;
        VLCRendererItem *item = [[VLCRendererItem alloc] initWithRendererItem:base_item];
        [_rendererItems addObject:item];
        if (_delegate)
            [_delegate addedRendererItem:item from:self];
        return;
    }
    if (event->type == vlc_RendererDiscoveryItemRemoved) {
        vlc_renderer_item *base_item =  event->u.renderer_discovery_item_removed.p_item;

        VLCRendererItem *result_item = nil;
        for (VLCRendererItem *item in _rendererItems) {
            if (item.rendererItem == base_item) {
                result_item = item;
                return;
            }
        }
        if (result_item) {
            [_rendererItems removeObject:result_item];
            if (_delegate)
                [_delegate removedRendererItem:result_item from:self];
        } else {
            msg_Err(p_intf, "VLCRendererDiscovery could not find item to remove!");
        }
        return;
    }
    msg_Err(p_intf, "VLCRendererDiscovery received event of unhandled type");
}

@end
