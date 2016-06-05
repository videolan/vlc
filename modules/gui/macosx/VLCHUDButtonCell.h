/*****************************************************************************
 * VLCHUDButtonCell.h: Custom button cell UI for dark HUD Panels
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 -at- gmail -dot- com>
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

/* Custom subclass that provides a dark NSButtonCell for HUD Panels.
 *
 * It does NOT support checkboxes or radio buttons,
 * use VLCHUDCheckboxCell and VLCHUDRadiobuttonCell instead.
 */
@interface VLCHUDButtonCell : NSButtonCell

@property NSGradient *normalGradient;
@property NSGradient *disabledGradient;
@property NSGradient *highlightGradient;
@property NSGradient *pushedGradient;

@property NSColor *enabledTextColor;
@property NSColor *disabledTextColor;

@end
