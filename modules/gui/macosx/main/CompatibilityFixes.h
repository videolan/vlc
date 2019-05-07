/*****************************************************************************
 * CompatibilityFixes.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2018 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Marvin Scholz <epirat07 -at- gmail -dot- com>
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

NS_ASSUME_NONNULL_BEGIN

#pragma mark -
void swapoutOverride(Class _Nonnull cls, SEL _Nonnull selector);

#ifndef MAC_OS_X_VERSION_10_14

extern NSString *const NSAppearanceNameDarkAqua;

#endif

#ifndef MAC_OS_X_VERSION_10_13

extern NSString *const NSCollectionViewSupplementaryElementKind;

#endif

NS_ASSUME_NONNULL_END
