/*****************************************************************************
 * AddonListDataSource.m: Addons manager for the Mac
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

#import "AddonListDataSource.h"

#import "StringUtility.h"

@implementation VLCAddon

- (id)initWithAddon:(addon_entry_t *)p_entry
{
    self = [super init];
    if(self) {
        p_addon_entry = addon_entry_Hold(p_entry);
    }

    return self;
}

-(void)dealloc
{
    addon_entry_Release(p_addon_entry);

    [super dealloc];
}

- (uint8_t *)uuid
{
    return p_addon_entry->uuid;
}

- (NSString *)name
{
    return toNSStr(p_addon_entry->psz_name);
}
- (NSString *)author
{
    return toNSStr(p_addon_entry->psz_author);
}

- (NSString *)version
{
    return toNSStr(p_addon_entry->psz_version);
}

- (NSString *)description
{
    return toNSStr(p_addon_entry->psz_description);
}

- (BOOL)isInstalled
{
    return p_addon_entry->e_state == ADDON_INSTALLED;
}

- (addon_type_t)type
{
    return p_addon_entry->e_type;
}

@end
