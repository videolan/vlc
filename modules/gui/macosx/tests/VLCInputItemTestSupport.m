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

@interface VLCMain : NSObject
@end

@implementation VLCMain
@end

intf_thread_t *getIntf(void)
{
    return NULL;
}
