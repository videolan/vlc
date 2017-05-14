/*****************************************************************************
 * VLCRendererDiscovery.m: Wrapper class for vlc_renderer_discovery_t
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

#import "VLCMain.h"

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

@interface VLCRendererDiscovery ()
{
    intf_thread_t               *p_intf;
    vlc_renderer_discovery_t    *p_rd;
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
        _name = [NSString stringWithUTF8String:name];
        _longName = (!longname) ? nil : [NSString stringWithUTF8String:longname];
    }
    return self;
}

- (void)dealloc
{
    [self stopDiscovery];
}

- (bool)startDiscovery
{
    p_intf = getIntf();

    msg_Dbg(p_intf, "Starting renderer discovery service %s", _name.UTF8String);
    // Create renderer object
    p_rd = vlc_rd_new(VLC_OBJECT(p_intf), _name.UTF8String);

    if (p_rd) {
    } else {
        msg_Err(p_intf, "Could not create '%s' renderer discovery service", _name.UTF8String);
        return false;
    }

    [self attachEventHandlers];
    int ret = vlc_rd_start(p_rd);
    if (ret != VLC_SUCCESS) {
        msg_Err(p_intf, "Could not start '%s' renderer discovery", _name.UTF8String);
        [self detachEventHandler];
        vlc_rd_release(p_rd);
        p_rd = NULL;
        return false;
    }
    return true;
}

- (void)stopDiscovery
{
    if (p_rd != NULL) {
        [self detachEventHandler];
        vlc_rd_release(p_rd);
        p_rd = NULL;
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
        vlc_renderer_item_t *base_item =  event->u.renderer_discovery_item_added.p_new_item;
        VLCRendererItem *item = [[VLCRendererItem alloc] initWithRendererItem:base_item];
        [_rendererItems addObject:item];
        if (_delegate)
            [_delegate addedRendererItem:item from:self];
        return;
    }
    if (event->type == vlc_RendererDiscoveryItemRemoved) {
        vlc_renderer_item_t *base_item =  event->u.renderer_discovery_item_removed.p_item;

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
