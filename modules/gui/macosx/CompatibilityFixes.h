/*****************************************************************************
 * CompatibilityFixes.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2018 VLC authors and VideoLAN
 * $Id$
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

#pragma mark -
#pragma OS detection code
#define OSX_EL_CAPITAN_AND_HIGHER (NSAppKitVersionNumber >= 1404)
#define OSX_SIERRA_AND_HIGHER (NSAppKitVersionNumber >= 1485)
#define OSX_SIERRA_DOT_TWO_AND_HIGHER (NSAppKitVersionNumber >= 1504.76) // this is needed to check for MPRemoteCommandCenter
#define OSX_HIGH_SIERRA_AND_HIGHER (NSAppKitVersionNumber >= 1560)
#define OSX_MOJAVE_AND_HIGHER (NSAppKitVersionNumber >= 1639.10)

void swapoutOverride(Class _Nonnull cls, SEL _Nonnull selector);
