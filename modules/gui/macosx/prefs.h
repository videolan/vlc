/*****************************************************************************
 * prefs.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: prefs.h,v 1.1 2002/11/05 03:57:16 jlj Exp $
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

/*****************************************************************************
 * VLCPrefs interface
 *****************************************************************************/
@interface VLCPrefs : NSObject
{
    intf_thread_t *p_intf;

    NSMutableDictionary *o_pref_panels;
    NSMutableDictionary *o_toolbars;
    NSMutableDictionary *o_scroll_views;
    NSMutableDictionary *o_panel_views;
    NSMutableDictionary *o_save_prefs;
}

- (BOOL)hasPrefs:(NSString *)o_module_name;
- (void)createPrefPanel:(NSString *)o_module_name;
- (void)destroyPrefPanel:(id)o_unknown;
- (void)selectPrefView:(id)sender;

- (void)moduleSelected:(id)sender;
- (void)configureModule:(id)sender;
- (void)selectModule:(id)sender;

- (void)configChanged:(id)o_unknown;

- (void)clickedApply:(id)sender;
- (void)clickedCancelOK:(id)sender;

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
INTF_CONTROL_CONFIG(ComboBox);
INTF_CONTROL_CONFIG(TextField);

#define CONTROL_CONFIG( obj, mname, ctype, cname ) \
    { \
        [obj setModuleName: mname]; \
        [obj setConfigType: ctype]; \
        [obj setConfigName: [NSString stringWithCString: cname]]; \
    }

