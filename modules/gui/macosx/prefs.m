/*****************************************************************************
 * prefs.m: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: prefs.m,v 1.3 2002/12/25 02:23:36 massiot Exp $
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#import <Cocoa/Cocoa.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#import "intf.h"
#import "prefs.h"

/*****************************************************************************
 * VLCPrefs implementation 
 *****************************************************************************/
@implementation VLCPrefs

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        p_intf = [NSApp getIntf];

        o_pref_panels = [[NSMutableDictionary alloc] init];  
        o_toolbars = [[NSMutableDictionary alloc] init];
        o_scroll_views = [[NSMutableDictionary alloc] init];
        o_panel_views = [[NSMutableDictionary alloc] init];
        o_save_prefs = [[NSMutableDictionary alloc] init];
    }

    return( self );
}

- (void)dealloc
{
    id v1, v2;
    NSEnumerator *o_e1;
    NSEnumerator *o_e2;

#define DIC_REL1(o_dic) \
    { \
    o_e1 = [o_dic objectEnumerator]; \
    while( (v1 = [o_e1 nextObject]) ) \
    { \
        [v1 release]; \
    } \
    [o_dic removeAllObjects]; \
    [o_dic release]; \
    }

#define DIC_REL2(o_dic) \
    { \
        o_e2 = [o_dic objectEnumerator]; \
        while( (v2 = [o_e2 nextObject]) ) \
        { \
            DIC_REL1(v2); \
        } \
        [o_dic removeAllObjects]; \
    }

    DIC_REL1(o_pref_panels);
    DIC_REL2(o_toolbars);
    DIC_REL1(o_scroll_views);
    DIC_REL2(o_panel_views);
    DIC_REL1(o_save_prefs);

#undef DIC_REL1
#undef DIC_REL2

    [super dealloc];
}

- (BOOL)hasPrefs:(NSString *)o_module_name
{
    module_t *p_parser;
    vlc_list_t list;
    char *psz_module_name;
    int i_index;

    psz_module_name = (char *)[o_module_name lossyCString];

    /* look for module */
    list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_parser = (module_t *)list.p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_object_name, psz_module_name ) )
        {
            BOOL b_has_prefs = p_parser->i_config_items != 0;
            vlc_list_release( &list );
            return( b_has_prefs );
        }
    }

    vlc_list_release( &list );

    return( NO );
}

- (void)createPrefPanel:(NSString *)o_module_name
{
    int i_pos;
    int i_module_tag;

    module_t *p_parser = NULL;
    vlc_list_t list;
    module_config_t *p_item;
    char *psz_module_name;
    int i_index;

    NSPanel *o_panel;                   /* panel                        */
    NSRect s_panel_rc;                  /* panel rect                   */
    NSView *o_panel_view;               /* panel view                   */
    NSToolbar *o_toolbar;               /* panel toolbar                */
    NSMutableDictionary *o_tb_items;    /* panel toolbar items          */
    NSScrollView *o_scroll_view;        /* panel scroll view            */ 
    NSRect s_scroll_rc;                 /* panel scroll view rect       */
    NSMutableDictionary *o_views;       /* panel scroll view docviews   */

    NSRect s_rc;                        /* rect                         */
    NSView *o_view;                     /* view                         */
    NSRect s_vrc;                       /* view rect                    */
    NSButton *o_button;                 /* button                       */
    NSRect s_brc;                       /* button rect                  */
    VLCTextField *o_text_field;         /* input field / label          */

    o_panel = [o_pref_panels objectForKey: o_module_name];
    if( o_panel != nil )
    {
        [o_panel center];
        [o_panel makeKeyAndOrderFront: nil];
        return;
    } 

    psz_module_name = (char *)[o_module_name lossyCString];

    /* Look for the selected module */
    list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < list.i_count; i_index++ )
    {
        p_parser = (module_t *)list.p_values[i_index].p_object ;

        if( psz_module_name
            && !strcmp( psz_module_name, p_parser->psz_object_name ) )
        {
            break;
        }
    }

    if( !p_parser || i_index == list.i_count )
    {
        vlc_list_release( &list );
        return;
    }

    /* We found it, now we can start building its configuration interface */

    s_panel_rc = NSMakeRect( 0, 0, 450, 450 );
    o_panel = [[NSPanel alloc] initWithContentRect: s_panel_rc
                               styleMask: NSTitledWindowMask
                               backing: NSBackingStoreBuffered
                               defer: YES];
    o_toolbar = [[NSToolbar alloc] initWithIdentifier: o_module_name];
    [o_panel setTitle: [NSString stringWithFormat: @"%@ (%@)", 
                                 _NS("Preferences"), o_module_name]];
    o_panel_view = [o_panel contentView];

    s_scroll_rc = s_panel_rc; 
    s_scroll_rc.size.height -= 55; s_scroll_rc.origin.y += 55;
    o_scroll_view = [[NSScrollView alloc] initWithFrame: s_scroll_rc];
    [o_scroll_views setObject: o_scroll_view forKey: o_module_name];
    [o_scroll_view setBorderType: NSGrooveBorder]; 
    [o_scroll_view setHasVerticalScroller: YES];
    [o_scroll_view setDrawsBackground: NO];
    [o_scroll_view setRulersVisible: YES];
    [o_panel_view addSubview: o_scroll_view];

    o_tb_items = [[NSMutableDictionary alloc] init];
    o_views = [[NSMutableDictionary alloc] init];

    [o_save_prefs setObject: [[NSMutableArray alloc] init]
                  forKey: o_module_name];

    /* Enumerate config options and add corresponding config boxes */
    p_item = p_parser->p_config;

    i_pos = 0;
    o_view = nil;
    i_module_tag = 3;

#define X_ORIGIN 20
#define Y_ORIGIN (X_ORIGIN - 10) 

#define CHECK_VIEW_HEIGHT \
    { \
        float f_new_pos = s_rc.origin.y + s_rc.size.height + X_ORIGIN; \
        if( f_new_pos > s_vrc.size.height ) \
        { \
            s_vrc.size.height = f_new_pos; \
            [o_view setFrame: s_vrc]; \
        } \
    }

#define CONTROL_LABEL( label ) \
    { \
        s_rc.origin.x += s_rc.size.width + 10; \
        s_rc.size.width = s_vrc.size.width - s_rc.origin.x - X_ORIGIN - 20; \
        o_text_field = [[NSTextField alloc] initWithFrame: s_rc]; \
        [o_text_field setDrawsBackground: NO]; \
        [o_text_field setBordered: NO]; \
        [o_text_field setEditable: NO]; \
        [o_text_field setSelectable: NO]; \
        [o_text_field setStringValue: \
                [NSString stringWithCString: label]]; \
        [o_view addSubview: [o_text_field autorelease]]; \
    }

#define INPUT_FIELD( ctype, cname, label, w, msg, param ) \
    { \
        s_rc.size.height = 23; \
        s_rc.size.width = w; \
        s_rc.origin.y += 10; \
        CHECK_VIEW_HEIGHT; \
        o_text_field = [[VLCTextField alloc] initWithFrame: s_rc]; \
        [o_text_field setAlignment: NSRightTextAlignment]; \
        CONTROL_CONFIG( o_text_field, o_module_name, ctype, cname ); \
        [o_text_field msg: param]; \
        [o_view addSubview: [o_text_field autorelease]]; \
        [[NSNotificationCenter defaultCenter] addObserver: self \
            selector: @selector(configChanged:) \
            name: NSControlTextDidChangeNotification \
            object: o_text_field]; \
        CONTROL_LABEL( label ); \
        s_rc.origin.y += s_rc.size.height; \
        s_rc.origin.x = X_ORIGIN; \
    }

#define INPUT_FIELD_INTEGER( name, label, w, param ) \
    INPUT_FIELD( CONFIG_ITEM_INTEGER, name, label, w, setIntValue, param )
#define INPUT_FIELD_FLOAT( name, label, w, param ) \
    INPUT_FIELD( CONFIG_ITEM_FLOAT, name, label, w, setFloatValue, param )
#define INPUT_FIELD_STRING( name, label, w, param ) \
    INPUT_FIELD( CONFIG_ITEM_STRING, name, label, w, setStringValue, param )

    if( p_item ) do
    {

        switch( p_item->i_type )
        {

        case CONFIG_HINT_CATEGORY:
        {
            NSString *o_key;
            NSString *o_label;
            NSToolbarItem *o_tbi;

            o_label = [NSString stringWithCString: p_item->psz_text];
            o_tbi = [[NSToolbarItem alloc] initWithItemIdentifier: o_label]; 
            [o_tbi setImage: [NSImage imageNamed: @"NSApplicationIcon"]];
            [o_tbi setLabel: o_label];
            [o_tbi setTarget: self];
            [o_tbi setAction: @selector(selectPrefView:)];

            o_key = [NSString stringWithFormat: @"%02d %@",
                                                i_pos, o_label]; 
            [o_tb_items setObject: o_tbi forKey: o_key];

            s_vrc = s_scroll_rc; s_vrc.size.height -= 4;
            o_view = [[VLCFlippedView alloc] initWithFrame: s_vrc];
            [o_views setObject: o_view forKey: o_label];

            s_rc.origin.x = X_ORIGIN;
            s_rc.origin.y = Y_ORIGIN;

            i_module_tag = 3;

            if( i_pos == 0 )
            {
                [o_scroll_view setDocumentView: o_view]; 
            }

            i_pos++;
        }
        break;

        case CONFIG_ITEM_MODULE:
        {
            NSBox *o_box;
            NSRect s_crc;
            NSView *o_cview;
            NSPopUpButton *o_modules;
            NSButton *o_btn_select;
            NSButton *o_btn_configure;

#define MODULE_BUTTON( button, title, sel ) \
    { \
        s_brc.size.height = 25; \
        s_brc.origin.x += s_brc.size.width + 10; \
        s_brc.size.width = s_crc.size.width - s_brc.origin.x - 10; \
        button = [[NSButton alloc] initWithFrame: s_brc]; \
        [button setButtonType: NSMomentaryPushInButton]; \
        [button setBezelStyle: NSRoundedBezelStyle]; \
        [button setTitle: title]; \
        [button setTag: i_module_tag++]; \
        [button setTarget: self]; \
        [button setAction: @selector(sel)]; \
        [o_cview addSubview: [button autorelease]]; \
    }

            s_rc.size.height = 100;
            s_rc.size.width = s_vrc.size.width - X_ORIGIN * 2 - 20;
            s_rc.origin.y += i_module_tag == 3 ? Y_ORIGIN : 20;

            CHECK_VIEW_HEIGHT;

            o_box = [[NSBox alloc] initWithFrame: s_rc];
            [o_box setTitle: [NSString stringWithCString: p_item->psz_text]];
            [o_view addSubview: [o_box autorelease]];
            s_rc.origin.y += s_rc.size.height + 10;
            o_cview = [[VLCFlippedView alloc] initWithFrame: s_rc];
            [o_box setContentView: [o_cview autorelease]];
            s_crc = [o_cview bounds];

            s_brc = NSMakeRect( 5, 10, 200, 23 ); 
            o_modules = [[NSPopUpButton alloc] initWithFrame: s_brc];
            [o_modules setTag: i_module_tag++];
            [o_modules setTarget: self];
            [o_modules setAction: @selector(moduleSelected:)];
            [o_cview addSubview: [o_modules autorelease]]; 

            MODULE_BUTTON( o_btn_configure, _NS("Configure"), 
                           configureModule: );

            s_brc = NSMakeRect( 8, s_brc.origin.y + s_brc.size.height + 10, 
                                194, 23 ); 
            o_text_field = [[VLCTextField alloc] initWithFrame: s_brc];
            [o_text_field setTag: i_module_tag++];
            [o_text_field setAlignment: NSLeftTextAlignment];
            CONTROL_CONFIG( o_text_field, o_module_name, 
                            CONFIG_ITEM_MODULE, p_item->psz_name );
            [[NSNotificationCenter defaultCenter] addObserver: self
                selector: @selector(configChanged:)
                name: NSControlTextDidChangeNotification
                object: o_text_field];
            [o_cview addSubview: [o_text_field autorelease]];

            s_brc.origin.x += 3;
            MODULE_BUTTON( o_btn_select, _NS("Select"), 
                           selectModule: );

            [o_modules addItemWithTitle: _NS("None")];

            /* build a list of available modules */
            {
                for( i_index = 0; i_index < list.i_count; i_index++ )
                {
                    p_parser = (module_t *)list.p_values[i_index].p_object ;

                    if( !strcmp( p_parser->psz_capability,
                                 p_item->psz_type ) )
                    {
                        NSString *o_object_name = [NSString 
                            stringWithCString: p_parser->psz_object_name];
                        [o_modules addItemWithTitle: o_object_name];
                    }
                }
            }

            if( p_item->psz_value != NULL )
            {
                NSString *o_value =
                    [NSString stringWithCString: p_item->psz_value];

                [o_text_field setStringValue: o_value]; 
                [o_modules selectItemWithTitle: o_value]; 
                [o_btn_configure setEnabled: [self hasPrefs: o_value]]; 
            }
            else
            {
                [o_modules selectItemWithTitle: _NS("None")];
                [o_btn_configure setEnabled: NO];
            }

#undef MODULE_BUTTON
        }
        break;

        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        {

            if( !p_item->ppsz_list )
            {
                char *psz_value = p_item->psz_value ?
                                  p_item->psz_value : "";

                INPUT_FIELD_STRING( p_item->psz_name, p_item->psz_text, 150,
                                    [NSString stringWithCString: psz_value] );
            }
            else
            {
                int i;
                VLCComboBox *o_combo_box;

                s_rc.size.height = 25;
                s_rc.size.width = 150;
                s_rc.origin.y += 10;

                CHECK_VIEW_HEIGHT;

                o_combo_box = [[VLCComboBox alloc] initWithFrame: s_rc];
                CONTROL_CONFIG( o_combo_box, o_module_name, 
                                CONFIG_ITEM_STRING, p_item->psz_name );
                [o_view addSubview: [o_combo_box autorelease]];
                [[NSNotificationCenter defaultCenter] addObserver: self
                    selector: @selector(configChanged:)
                    name: NSControlTextDidChangeNotification
                    object: o_combo_box];
                [[NSNotificationCenter defaultCenter] addObserver: self
                    selector: @selector(configChanged:)
                    name: NSComboBoxSelectionDidChangeNotification 
                    object: o_combo_box];

                for( i=0; p_item->ppsz_list[i]; i++ )
                {
                    [o_combo_box addItemWithObjectValue:
                        [NSString stringWithCString: p_item->ppsz_list[i]]]; 
                }

                CONTROL_LABEL( p_item->psz_text ); 

                s_rc.origin.y += s_rc.size.height;
                s_rc.origin.x = X_ORIGIN;
            }

        }
        break;

        case CONFIG_ITEM_INTEGER:
        {
            INPUT_FIELD_INTEGER( p_item->psz_name, p_item->psz_text, 70, 
                                 p_item->i_value );
        }
        break;

        case CONFIG_ITEM_FLOAT:
        {
            INPUT_FIELD_FLOAT( p_item->psz_name, p_item->psz_text, 70,
                               p_item->f_value );
        }
        break;

        case CONFIG_ITEM_BOOL:
        {
            VLCButton *o_btn_bool;

            s_rc.size.height = 20;
            s_rc.size.width = s_vrc.size.width - X_ORIGIN * 2 - 20;
            s_rc.origin.y += 10;

            CHECK_VIEW_HEIGHT;

            o_btn_bool = [[VLCButton alloc] initWithFrame: s_rc];
            [o_btn_bool setButtonType: NSSwitchButton];
            [o_btn_bool setIntValue: p_item->i_value];
            [o_btn_bool setTitle: 
                [NSString stringWithCString: p_item->psz_text]]; 
            [o_btn_bool setTarget: self];
            [o_btn_bool setAction: @selector(configChanged:)];
            CONTROL_CONFIG( o_btn_bool, o_module_name, 
                            CONFIG_ITEM_BOOL, p_item->psz_name ); 
            [o_view addSubview: [o_btn_bool autorelease]];

            s_rc.origin.y += s_rc.size.height;
        }
        break;

        }

#undef INPUT_FIELD_INTEGER
#undef INPUT_FIELD_FLOAT
#undef INPUT_FIELD_STRING
#undef INPUT_FIELD
#undef CHECK_VIEW_HEIGHT
#undef CONTROL_LABEL
#undef Y_ORIGIN
#undef X_ORIGIN
    }
    while( p_item->i_type != CONFIG_HINT_END && p_item++ );

    vlc_list_release( &list );

    [o_toolbars setObject: o_tb_items forKey: o_module_name];
    [o_toolbar setDelegate: self];
    [o_panel setToolbar: [o_toolbar autorelease]];

#define DEF_PANEL_BUTTON( tag, title, sel ) \
    { \
        o_button = [[NSButton alloc] initWithFrame: s_rc]; \
        [o_button setButtonType: NSMomentaryPushInButton]; \
        [o_button setBezelStyle: NSRoundedBezelStyle]; \
        [o_button setAction: @selector(sel)]; \
        [o_button setTarget: self]; \
        [o_button setTitle: title]; \
        [o_button setTag: tag]; \
        [o_panel_view addSubview: [o_button autorelease]]; \
    }

    s_rc.origin.y = s_panel_rc.origin.y + 14;
    s_rc.size.height = 25; s_rc.size.width = 100;
    s_rc.origin.x = s_panel_rc.size.width - s_rc.size.width - 14;
    DEF_PANEL_BUTTON( 0, _NS("OK"), clickedCancelOK: ); 
    [o_panel setDefaultButtonCell: [o_button cell]];

    s_rc.origin.x -= s_rc.size.width;
    DEF_PANEL_BUTTON( 1, _NS("Cancel"), clickedCancelOK: );

    s_rc.origin.x -= s_rc.size.width;
    DEF_PANEL_BUTTON( 2, _NS("Apply"), clickedApply: );
    [o_button setEnabled: NO];

#undef DEF_PANEL_BUTTON

    [o_pref_panels setObject: o_panel forKey: o_module_name];
    [o_panel_views setObject: o_views forKey: o_module_name];

    [o_panel center];
    [o_panel makeKeyAndOrderFront: nil];
}

- (void)destroyPrefPanel:(id)o_unknown
{
    id v1;
    NSPanel *o_panel;
    NSEnumerator *o_e1;
    NSMutableArray *o_prefs;
    NSMutableDictionary *o_dic;
    NSScrollView *o_scroll_view;
    NSString *o_module_name;

    o_module_name = (NSString *)([o_unknown isKindOfClass: [NSTimer class]] ?
                                 [o_unknown userInfo] : o_unknown);

#define DIC_REL(dic) \
    { \
    o_dic = [dic objectForKey: o_module_name]; \
    [dic removeObjectForKey: o_module_name]; \
    o_e1 = [o_dic objectEnumerator]; \
    while( (v1 = [o_e1 nextObject]) ) \
    { \
        [v1 release]; \
    } \
    [o_dic removeAllObjects]; \
    [o_dic release]; \
    }

    o_panel = [o_pref_panels objectForKey: o_module_name];
    [o_pref_panels removeObjectForKey: o_module_name];
    [o_panel release];

    DIC_REL(o_toolbars);

    o_scroll_view = [o_scroll_views objectForKey: o_module_name];
    [o_scroll_views removeObjectForKey: o_module_name];
    [o_scroll_view release];

    DIC_REL(o_panel_views);

    o_prefs = [o_save_prefs objectForKey: o_module_name];
    [o_save_prefs removeObjectForKey: o_module_name];
    [o_prefs removeAllObjects];
    [o_prefs release];

#undef DIC_REL

}

- (void)selectPrefView:(id)sender
{
    NSView *o_view;
    NSString *o_module_name;
    NSScrollView *o_scroll_view;
    NSMutableDictionary *o_views;

    o_module_name = [[sender toolbar] identifier];
    o_views = [o_panel_views objectForKey: o_module_name];
    o_view = [o_views objectForKey: [sender label]];

    o_scroll_view = [o_scroll_views objectForKey: o_module_name];
    [o_scroll_view setDocumentView: o_view]; 
}

- (void)moduleSelected:(id)sender
{
    NSButton *o_btn_config;
    NSString *o_module_name;
    BOOL b_has_prefs = NO;

    o_module_name = [sender titleOfSelectedItem];
    o_btn_config = [[sender superview] viewWithTag: [sender tag] + 1];

    if( ![o_module_name isEqualToString: _NS("None")] )
    {
        b_has_prefs = [self hasPrefs: o_module_name];
    }

    [o_btn_config setEnabled: b_has_prefs];
}

- (void)configureModule:(id)sender
{
    NSString *o_module_name;
    NSPopUpButton *o_modules;

    o_modules = [[sender superview] viewWithTag: [sender tag] - 1]; 
    o_module_name = [o_modules titleOfSelectedItem];

    [self createPrefPanel: o_module_name];
}

- (void)selectModule:(id)sender
{
    NSString *o_module_name;
    NSPopUpButton *o_modules;
    NSTextField *o_module;

    o_module = [[sender superview] viewWithTag: [sender tag] - 1];
    o_modules = [[sender superview] viewWithTag: [sender tag] - 3];
    o_module_name = [o_modules titleOfSelectedItem];

    if( [o_module_name isEqualToString: _NS("None")] )
    {
        o_module_name = [NSString string];
    }

    [o_module setStringValue: o_module_name];
    [self configChanged: o_module];
}

- (void)configChanged:(id)o_unknown
{
    id o_vlc_config = [o_unknown isKindOfClass: [NSNotification class]] ?
                      [o_unknown object] : o_unknown;

    NSString *o_module_name = [o_vlc_config moduleName]; 
    NSPanel *o_pref_panel = [o_pref_panels objectForKey: o_module_name]; 
    NSMutableArray *o_prefs = [o_save_prefs objectForKey: o_module_name];

    if( [o_prefs indexOfObjectIdenticalTo: o_vlc_config] == NSNotFound )
    {
        NSView *o_pref_view = [o_pref_panel contentView];
        NSButton *o_btn_apply = [o_pref_view viewWithTag: 2]; 

        [o_prefs addObject: o_vlc_config];
        [o_btn_apply setEnabled: YES];
    }
}

- (void)clickedApply:(id)sender
{
    id o_vlc_control;
    NSEnumerator *o_enum;

    NSView *o_config_view = [sender superview];
    NSWindow *o_config_panel = [o_config_view window];    
    NSButton *o_btn_apply = [o_config_view viewWithTag: 2]; 
    NSString *o_module_name = [[o_config_panel toolbar] identifier];
    NSMutableArray *o_prefs = [o_save_prefs objectForKey: o_module_name];

    o_enum = [o_prefs objectEnumerator];
    while( ( o_vlc_control = [o_enum nextObject] ) )
    {
        int i_type = [o_vlc_control configType];
        NSString *o_name = [o_vlc_control configName];
        char *psz_name = (char *)[o_name lossyCString];

        switch( i_type )
        {

        case CONFIG_ITEM_MODULE:
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
            {
                char *psz_value;
                NSString *o_value;

                o_value = [o_vlc_control stringValue];
                psz_value = (char *)[o_value lossyCString];

                config_PutPsz( p_intf, psz_name, 
                               *psz_value ? psz_value : NULL );
            }
            break;

        case CONFIG_ITEM_INTEGER:
        case CONFIG_ITEM_BOOL:
            {
                int i_value = [o_vlc_control intValue];

                config_PutInt( p_intf, psz_name, i_value );
            }
            break;

        case CONFIG_ITEM_FLOAT:
            {
                float f_value = [o_vlc_control floatValue];

                config_PutFloat( p_intf, psz_name, f_value );
            }
            break;

        }
    }

    [o_btn_apply setEnabled: NO];
    [o_prefs removeAllObjects];

    config_SaveConfigFile( p_intf, NULL );
}

- (void)clickedCancelOK:(id)sender
{
    NSWindow *o_pref_panel = [[sender superview] window];
    NSString *o_module_name = [[o_pref_panel toolbar] identifier];

    if( [[sender title] isEqualToString: _NS("OK")] )
    {
        [self clickedApply: sender];
    }

    [o_pref_panel close];

    if( [self respondsToSelector: @selector(performSelectorOnMainThread:
                                            withObject:waitUntilDone:)] ) 
    {
        [self performSelectorOnMainThread: @selector(destroyPrefPanel:)
                                           withObject: o_module_name
                                           waitUntilDone: NO];
    }
    else
    {
        [NSTimer scheduledTimerWithTimeInterval: 0.1
                 target: self selector: @selector(destroyPrefPanel:)
                 userInfo: o_module_name repeats: NO];
    }
}

@end

@implementation VLCPrefs (NSToolbarDelegate)

- (NSToolbarItem *)toolbar:(NSToolbar *)o_toolbar 
                   itemForItemIdentifier:(NSString *)o_item_id 
                   willBeInsertedIntoToolbar:(BOOL)b_flag
{
    NSMutableDictionary *o_toolbar_items;
    NSString *o_module_name = [o_toolbar identifier];

    o_toolbar_items = [o_toolbars objectForKey: o_module_name];
    if( o_toolbar_items == nil )
    {
        return( nil );
    }

    return( [o_toolbar_items objectForKey: o_item_id] );
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)o_toolbar
{
    return( [self toolbarDefaultItemIdentifiers: o_toolbar] );
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)o_toolbar
{
    NSArray *o_ids;
    NSMutableDictionary *o_toolbar_items;
    NSString *o_module_name = [o_toolbar identifier];

    o_toolbar_items = [o_toolbars objectForKey: o_module_name];
    if( o_toolbar_items == nil )
    {
        return( nil );
    }  

    o_ids = [[o_toolbar_items allKeys] 
        sortedArrayUsingSelector: @selector(compare:)];

    return( o_ids );
}

@end

@implementation VLCFlippedView

- (BOOL)isFlipped
{
    return( YES );
}

@end

IMPL_CONTROL_CONFIG(Button);
IMPL_CONTROL_CONFIG(ComboBox);
IMPL_CONTROL_CONFIG(TextField);
