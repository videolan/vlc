/*****************************************************************************
 * VLCKeyboardBlacklightControl.h: MacBook keyboard backlight control for VLC
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 *
 * Authors: Maxime Mouchet <max@maxmouchet.com>
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

@interface VLCKeyboardBacklightControl : NSObject

/*!
 *  Initialize an instance of KeyboardBacklight.
 *
 *  Note: this will return nil if the connection to
 *  the controller could not be opened.
 */
- (id)init;

/*!
 *  Smoothly turn on backlight on MacBook keyboard.
 *
 *  Try to restore the previous brightness level. If the user has put
 *  the backlight on manually (using hardware keys), nothing will be done.
 */
- (void)lightsUp;

/*!
 *  Smoothly turn off backlight on MacBook keyboard.
 */
- (void)lightsDown;

/*!
 *  Smoothly switch on or off backlight on MacBook keyboard.
 *  This will return immediately.
 */
- (void)switchLightsAsync:(BOOL)on;

/*!
 *  Instantly switch on or off backlight on MacBook keyboard.
 */
- (void)switchLightsInstantly:(BOOL)on;

@end
