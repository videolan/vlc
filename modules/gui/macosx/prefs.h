/*****************************************************************************
 * prefs.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: prefs.h,v 1.9 2003/05/26 14:59:37 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net> 
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

#define PREFS_WRAP 300

@interface VLCTreeItem : NSObject
{
    NSString *o_name;
    int i_object_id;
    VLCTreeItem *o_parent;
    NSMutableArray *o_children;
}

+ (VLCTreeItem *)rootItem;
- (int)numberOfChildren;
- (VLCTreeItem *)childAtIndex:(int)i_index;
- (int)getObjectID;
- (NSString *)getName;
- (BOOL)hasPrefs:(NSString *)o_module_name;

@end

/*****************************************************************************
 * VLCPrefs interface
 *****************************************************************************/
@interface VLCPrefs : NSObject
{
    intf_thread_t *p_intf;
    vlc_bool_t b_advanced;
    VLCTreeItem *o_config_tree;
    NSView *o_empty_view;
    
    IBOutlet id o_prefs_window;
    IBOutlet id o_tree;
    IBOutlet id o_prefs_view;
    IBOutlet id o_save_btn;
    IBOutlet id o_cancel_btn;
    IBOutlet id o_reset_btn;
    IBOutlet id o_advanced_ckb;
}

- (void)initStrings;
- (void)showPrefs;
- (IBAction)savePrefs: (id)sender;
- (IBAction)closePrefs: (id)sender;
- (IBAction)resetAll: (id)sender;
- (IBAction)advancedToggle: (id)sender;
- (void)showViewForID: (int) i_id andName:(NSString *)o_item_name;
- (void)configChanged:(id)o_unknown;

@end

@interface VLCFlippedView : NSView
{

}

@end

#define INTF_CONTROL_CONFIG(x) \
@interface VLC##x : NS##x \
{ \
    NSString *o_module_name; \
    NSString *o_config_name; \
    int i_config_type; \
} \
- (void)setModuleName:(NSString *)_o_module_name; \
- (void)setConfigName:(NSString *)_o_config_name; \
- (void)setConfigType:(int)_i_config_type; \
- (NSString *)moduleName; \
- (NSString *)configName; \
- (int)configType; \
@end

#define IMPL_CONTROL_CONFIG(x) \
@implementation VLC##x \
- (id)init \
{ \
    self = [super init]; \
    if( self != nil ) \
    { \
        o_module_name = nil; \
        o_config_name = nil; \
        i_config_type = 0; \
    } \
    return( self ); \
} \
- (void)dealloc \
{ \
    if( o_module_name != nil ) \
    { \
        [o_module_name release]; \
    } \
    if( o_config_name != nil ) \
    { \
        [o_config_name release]; \
    } \
    [super dealloc]; \
} \
- (void)setModuleName:(NSString *)_o_module_name \
{ \
    if( o_module_name != nil ) \
    { \
        [o_module_name release]; \
    } \
    o_module_name = [_o_module_name retain]; \
} \
- (void)setConfigName:(NSString *)_o_config_name \
{ \
    if( o_config_name != nil ) \
    { \
        [o_config_name release]; \
    } \
    o_config_name = [_o_config_name retain]; \
} \
- (void)setConfigType:(int)_i_config_type \
{ \
    i_config_type = _i_config_type; \
} \
- (NSString *)moduleName \
{ \
    return( o_module_name ); \
} \
- (NSString *)configName \
{ \
    return( o_config_name ); \
} \
- (int)configType \
{ \
    return( i_config_type ); \
} \
@end

INTF_CONTROL_CONFIG(Button);
INTF_CONTROL_CONFIG(PopUpButton);
INTF_CONTROL_CONFIG(ComboBox);
INTF_CONTROL_CONFIG(TextField);
INTF_CONTROL_CONFIG(Slider);

#define CONTROL_CONFIG( obj, mname, ctype, cname ) \
    { \
        [obj setModuleName: mname]; \
        [obj setConfigType: ctype]; \
        [obj setConfigName: [NSString stringWithUTF8String: cname]]; \
    }

