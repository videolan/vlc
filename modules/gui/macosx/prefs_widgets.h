/*****************************************************************************
 * prefs_widgets.h: Preferences controls
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan.org>
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

static NSMenu *o_keys_menu = nil;

@interface VLCConfigControl : NSView

@property (readonly) module_config_t *p_item;

@property (readwrite) NSTextField *label;

@property (readonly) NSString *name;
@property (readonly) int type;
@property (readwrite) int viewType;
@property (readonly) bool advanced;
@property (readonly) int intValue;
@property (readonly) float floatValue;
@property (readonly) char *stringValue;
@property (readonly) int labelSize;


+ (VLCConfigControl *)newControl:(module_config_t *)_p_item
                        withView:(NSView *)o_parent_view;
+ (int)calcVerticalMargin:(int)i_curItem lastItem:(int)i_lastItem;

- (id)initWithFrame:(NSRect)frame
               item:(module_config_t *)p_item;
- (id)initWithItem:(module_config_t *)_p_item
          withView:(NSView *)o_parent_view;

- (void)setYPos:(int)i_yPos;
- (void)alignWithXPosition:(int)i_xPos;

- (void)applyChanges;
- (void)resetValues;

@end

@interface StringConfigControl : VLCConfigControl

@end

@interface StringListConfigControl : VLCConfigControl

@end

@interface FileConfigControl : VLCConfigControl

- (IBAction)openFileDialog:(id)sender;

@end

@interface ModuleConfigControl : VLCConfigControl

@end

@interface IntegerConfigControl : VLCConfigControl

- (IBAction)stepperChanged:(id)sender;

@end

@interface IntegerListConfigControl : VLCConfigControl

@end

@interface RangedIntegerConfigControl : VLCConfigControl

- (IBAction)sliderChanged:(id)sender;

@end

@interface BoolConfigControl : VLCConfigControl

@end

@interface FloatConfigControl : VLCConfigControl

- (IBAction)stepperChanged:(id)sender;

@end

@interface RangedFloatConfigControl : VLCConfigControl

- (IBAction)sliderChanged:(id)sender;

@end

@interface KeyConfigControl : VLCConfigControl

@end

@interface ModuleListConfigControl : VLCConfigControl

@end

@interface SectionControl : VLCConfigControl

@end
