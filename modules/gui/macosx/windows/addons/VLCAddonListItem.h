/*****************************************************************************
 * VLCAddonListItem.h: Addons manager for the Mac
 ****************************************************************************
 * Copyright (C) 2014 VideoLAN and authors
 * Authors:       Felix Paul Kühne <fkuehne # videolan.org>
 *                David Fuhrmann <dfuhrmann # videolan.org>
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

#import <Cocoa/Cocoa.h>

#import <vlc_common.h>
#import <vlc_addons.h>

@interface VLCAddonListItem : NSObject

- (id)initWithAddon:(addon_entry_t *)p_entry;

- (NSData *)uuid;

- (NSString *)name;
- (NSString *)author;
- (NSString *)version;
- (NSString *)description;

- (BOOL)isInstalled;
- (addon_type_t)type;

@end
