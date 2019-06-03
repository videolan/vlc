/*****************************************************************************
 * VLCExtensionsManager.h: Extensions manager for Cocoa
 ****************************************************************************
 * Copyright (C) 2012 VideoLAN and authors
 *
 * Authors: Brendon Justin <brendonjustin@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import "VLCExtensionsDialogProvider.h"

@interface VLCExtensionsManager : NSObject

- (void)buildMenu:(NSMenu *)extMenu;

- (BOOL)loadExtensions;
- (void)unloadExtensions;
- (void)reloadExtensions;

- (void)triggerMenu:(id)sender;
- (void)playingChanged:(int)state;
- (void)metaChanged:(input_item_t *)p_input;

- (BOOL)isLoaded;
- (BOOL)cannotLoad;

@property (readonly) BOOL isUnloading;

@end
