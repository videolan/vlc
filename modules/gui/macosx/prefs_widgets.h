/*****************************************************************************
 * prefs_widgets.h: Preferences controls
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org> 
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


@interface VLCConfigControl : NSView
{
    vlc_object_t    *p_this;
    module_config_t *p_item;
    char            *psz_name;
    NSTextField     *o_label;
    int             i_type;
    vlc_bool_t      b_advanced;
    NSView          *contentView;
}

+ (VLCConfigControl *)newControl: (module_config_t *)p_item withView: (NSView *)o_parent_view withObject: (vlc_object_t *)p_this offset:(NSPoint) offset;
- (id)initWithFrame: (NSRect)frame item: (module_config_t *)p_item withObject: (vlc_object_t *)_p_this;
- (NSString *)getName;
- (int)getType;
- (BOOL)isAdvanced;

- (int)intValue;
- (float)floatValue;
- (char *)stringValue;

@end

@interface KeyConfigControl : VLCConfigControl
{
    NSMatrix        *o_matrix;
    NSComboBox      *o_combo;
}

@end
#if 0

@interface ModuleConfigControl : VLCConfigControl
{
    NSPopUpButton   *o_popup;
}

@end
#endif
@interface StringConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
}

@end

@interface StringListConfigControl : VLCConfigControl
{
    NSComboBox      *o_combo;
}

@end
@interface FileConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
    NSButton        *o_button;
    BOOL            b_directory;
}

- (IBAction)openFileDialog: (id)sender;
- (void)pathChosenInPanel:(NSOpenPanel *)o_sheet withReturn:(int)i_return_code contextInfo:(void  *)o_context_info;

@end

@interface IntegerConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
    NSStepper       *o_stepper;
}

- (IBAction)stepperChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end

@interface IntegerListConfigControl : VLCConfigControl
{
    NSComboBox      *o_combo;
}

@end

@interface RangedIntegerConfigControl : VLCConfigControl
{
    NSSlider        *o_slider;
    NSTextField     *o_textfield;
    NSTextField     *o_textfield_min;
    NSTextField     *o_textfield_max;
}

- (IBAction)sliderChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end
#if 0

@interface FloatConfigControl : VLCConfigControl
{
    NSTextField     *o_textfield;
}

@end

@interface RangedFloatConfigControl : VLCConfigControl
{
    NSSlider        *o_slider;
    NSTextField     *o_textfield;
    NSTextField     *o_textfield_min;
    NSTextField     *o_textfield_max;
}

- (IBAction)sliderChanged:(id)sender;
- (void)textfieldChanged:(NSNotification *)o_notification;

@end


@interface BoolConfigControl : VLCConfigControl
{
    NSButton        *o_checkbox;
}

@end
#endif