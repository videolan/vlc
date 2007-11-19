/*****************************************************************************
 * VLCController.h: VLC.app main controller
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import <VLC/VLC.h>

#import "VLCController.h"

@implementation VLCController
- (void)awakeFromNib
{
    VLCMediaList * mediaList = [[VLCMediaList alloc] init];
    [mediaList addMedia:[VLCMedia mediaWithURL:@"/dev/null"]];
    NSArrayController * arrayController = [[NSArrayController alloc] init];
    [arrayController bind:@"contentArray" toObject:mediaList withKeyPath:@"media" options:nil];
    NSMutableDictionary *bindingOptions = [NSMutableDictionary dictionary];

    [bindingOptions setObject:@"No Title" forKey:NSDisplayNameBindingOption];
		
	/* binding for "title" column */
    NSTableColumn * tableColumn = [detailList tableColumnWithIdentifier:@"title"];
	
	[tableColumn bind:@"value" toObject: arrayController
		  withKeyPath:@"arrangedObjects.url" options:bindingOptions];
    
}


@end