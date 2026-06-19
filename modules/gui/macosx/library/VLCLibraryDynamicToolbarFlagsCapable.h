/*****************************************************************************
 * VLCLibraryDynamicToolbarFlagsCapable.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import <Foundation/Foundation.h>

#import "library/VLCLibraryWindowToolbarDelegate.h" // VLCLibraryWindowToolbarDisplayFlags

NS_ASSUME_NONNULL_BEGIN

// Capability for segment view controllers whose desired toolbar item set
// depends on transient UI state (e.g. an overlay being presented), not just
// the static segment configuration. The window reads `toolbarDisplayFlags`
// from any conforming view controller in preference to the segment's
// static value. Conforming controllers are responsible for calling
// `updateToolbarDisplayFlags` on the window when the returned value
// would change.
@protocol VLCLibraryDynamicToolbarFlagsCapable

@property (readonly) VLCLibraryWindowToolbarDisplayFlags toolbarDisplayFlags;

@end

NS_ASSUME_NONNULL_END
