/*****************************************************************************
 * prefs.m: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: prefs.m,v 1.25 2003/05/24 02:48:55 hartman Exp $
 *
 * Authors:	Jon Lech Johansen <jon-vl@nanocrew.net>
 *		Derk-Jan Hartman <thedj at users.sf.net>
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

#include "intf.h"
#include "prefs.h"

/*****************************************************************************
 * VLCPrefs implementation
 *****************************************************************************/
@implementation VLCPrefs

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        o_empty_view = [[NSView alloc] init];
    }

    return( self );
}

- (void)dealloc
{
    [o_empty_view release];
    [super dealloc];
}

- (void)awakeFromNib
{
    p_intf = [NSApp getIntf];
    b_advanced = config_GetInt( p_intf, "advanced" );

    [self initStrings];
    [o_advanced_ckb setState: b_advanced];
    [o_prefs_view setBorderType: NSGrooveBorder];
    [o_prefs_view setHasVerticalScroller: YES];
    [o_prefs_view setDrawsBackground: NO];
    [o_prefs_view setRulersVisible: YES];
    [o_prefs_view setDocumentView: o_empty_view];
    [o_tree selectRow:0 byExtendingSelection:NO];
}

- (void)initStrings
{
    [o_prefs_window setTitle: _NS("Preferences")];
    [o_save_btn setTitle: _NS("Save")];
    [o_cancel_btn setTitle: _NS("Cancel")];
    [o_reset_btn setTitle: _NS("Reset All")];
    [o_advanced_ckb setTitle: _NS("Advanced")];
}

- (void)showPrefs
{
    [o_prefs_window center];
    [o_prefs_window makeKeyAndOrderFront:self];
}

- (IBAction)savePrefs: (id)sender
{
    config_SaveConfigFile( p_intf, NULL );
    [o_prefs_window orderOut:self];
}

- (IBAction)closePrefs: (id)sender
{
    [o_prefs_window orderOut:self];
}

- (IBAction)resetAll: (id)sender
{
    config_ResetAll( p_intf );
}

- (IBAction)advancedToggle: (id)sender
{
    b_advanced = !b_advanced;
    [o_advanced_ckb setState: b_advanced];
    [o_tree selectRow: [o_tree selectedRow] byExtendingSelection:NO];
}

- (void)loadConfigTree
{
    
}

- (void)outlineViewSelectionIsChanging:(NSNotification *)o_notification
{
}

- (void)outlineViewSelectionDidChange:(NSNotification *)o_notification
{
    [self showViewForID: [[o_tree itemAtRow:[o_tree selectedRow]] getObjectID] andName: [[o_tree itemAtRow:[o_tree selectedRow]] getName]];
}

- (void)configChanged:(id)o_unknown
{
    id o_vlc_config = [o_unknown isKindOfClass: [NSNotification class]] ?
                      [o_unknown object] : o_unknown;

    int i_type = [o_vlc_config configType];
    NSString *o_name = [o_vlc_config configName];
    char *psz_name = (char *)[o_name lossyCString];

    switch( i_type )
    {

    case CONFIG_ITEM_MODULE:
        {
            char *psz_value;
            NSString *o_value;

            o_value = [o_vlc_config titleOfSelectedItem];
            [o_value isEqualToString: _NS("Auto") ] ? psz_value = "" :
                psz_value = (char *)[o_value lossyCString];
            config_PutPsz( p_intf, psz_name, psz_value );
        }
        break;

    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
        {
            char *psz_value;
            NSString *o_value;

            o_value = [o_vlc_config stringValue];
            psz_value = (char *)[o_value lossyCString];

            config_PutPsz( p_intf, psz_name, psz_value );
        }
        break;

    case CONFIG_ITEM_INTEGER:
    case CONFIG_ITEM_BOOL:
        {
            int i_value = [o_vlc_config intValue];

            config_PutInt( p_intf, psz_name, i_value );
        }
        break;

    case CONFIG_ITEM_FLOAT:
        {
            float f_value = [o_vlc_config floatValue];

            config_PutFloat( p_intf, psz_name, f_value );
        }
        break;

    }
}

- (void)showViewForID: (int)i_id andName: (NSString *)o_item_name
{
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;
    
    int i_pos, i_module_tag, i_index;
    
    NSString *o_module_name;
    NSRect s_rc;                        /* rect                         */
    NSView *o_view;                     /* view                         */
    NSRect s_vrc;                       /* view rect                    */
    VLCTextField *o_text_field;         /* input field / label          */
    
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Get a pointer to the module */
    p_parser = (module_t *)vlc_object_get( p_intf, i_id );
    if( p_parser->i_object_type != VLC_OBJECT_MODULE )
    {
        /* 0OOoo something went really bad */
        return;
    }
    
    /* Enumerate config options and add corresponding config boxes */
    o_module_name = [NSString stringWithCString: p_parser->psz_object_name];
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
        if ( label ) \
        { \
            [o_text_field setStringValue: \
                [NSApp localizedString: label]]; \
        } \
        [o_text_field sizeToFit]; \
        [o_view addSubview: [o_text_field autorelease]]; \
    }

#define INPUT_FIELD( ctype, cname, label, w, msg, param, tip ) \
    { \
        char * psz_duptip = NULL; \
        if ( p_item->psz_longtext != NULL && [NSApp getEncoding] == NSISOLatin1StringEncoding ) \
            psz_duptip = strdup(p_item->psz_longtext); \
        s_rc.size.height = 25; \
        s_rc.size.width = w; \
        s_rc.origin.y += 10; \
        CHECK_VIEW_HEIGHT; \
        o_text_field = [[VLCTextField alloc] initWithFrame: s_rc]; \
        [o_text_field setAlignment: NSRightTextAlignment]; \
        CONTROL_CONFIG( o_text_field, o_module_name, ctype, cname ); \
        [o_text_field msg: param]; \
        if ( psz_duptip != NULL ) \
        { \
            [o_text_field setToolTip: [NSApp localizedString: \
                                       vlc_wraptext(psz_duptip, PREFS_WRAP)]]; \
            free(psz_duptip);\
        } \
        [o_view addSubview: [o_text_field autorelease]]; \
        [[NSNotificationCenter defaultCenter] addObserver: self \
            selector: @selector(configChanged:) \
            name: NSControlTextDidChangeNotification \
            object: o_text_field]; \
        CONTROL_LABEL( label ); \
        s_rc.origin.y += s_rc.size.height; \
        s_rc.origin.x = X_ORIGIN; \
    }

#define INPUT_FIELD_INTEGER( name, label, w, param, tip ) \
    INPUT_FIELD( CONFIG_ITEM_INTEGER, name, label, w, setIntValue, param, tip )
#define INPUT_FIELD_FLOAT( name, label, w, param, tip ) \
    INPUT_FIELD( CONFIG_ITEM_FLOAT, name, label, w, setFloatValue, param, tip )
#define INPUT_FIELD_STRING( name, label, w, param, tip ) \
    INPUT_FIELD( CONFIG_ITEM_STRING, name, label, w, setStringValue, param, tip )

    /* Init View */
    s_vrc = [[o_prefs_view contentView] bounds]; s_vrc.size.height -= 4;
    o_view = [[VLCFlippedView alloc] initWithFrame: s_vrc];
    s_rc.origin.x = X_ORIGIN;
    s_rc.origin.y = Y_ORIGIN;
    BOOL b_right_cat = FALSE;

    if( p_item ) do
    {
        if( p_item->i_type == CONFIG_HINT_CATEGORY )
        {
            if( !strcmp( p_parser->psz_object_name, "main" ) &&
                [o_item_name isEqualToString: [NSApp localizedString: p_item->psz_text]] )
            {
                b_right_cat = TRUE;
            } else if( strcmp( p_parser->psz_object_name, "main" ) )
            {
                 b_right_cat = TRUE;
            } else b_right_cat = FALSE; 
        } else if( p_item->i_type == CONFIG_HINT_END && !strcmp( p_parser->psz_object_name, "main" ) )
        {
            b_right_cat = FALSE;
        }
        
        if( (p_item->b_advanced && !b_advanced ) || !b_right_cat )
        {
            continue;
        }
        switch( p_item->i_type )
        {
            case CONFIG_ITEM_MODULE:
            {
                VLCPopUpButton *o_modules;
                module_t *p_a_module;
                char * psz_duptip = NULL;
                if ( p_item->psz_longtext != NULL && [NSApp getEncoding] == NSISOLatin1StringEncoding )
                    psz_duptip = strdup(p_item->psz_longtext);
        
                s_rc.size.height = 30;
                s_rc.size.width = 200;
                s_rc.origin.y += 10;
                
                CHECK_VIEW_HEIGHT;
    
                o_modules = [[VLCPopUpButton alloc] initWithFrame: s_rc];
                CONTROL_CONFIG( o_modules, o_module_name,
                                    CONFIG_ITEM_MODULE, p_item->psz_name );
                [o_modules setTarget: self];
                [o_modules setAction: @selector(configChanged:)];
                [o_modules sendActionOn:NSLeftMouseUpMask];
                
                if ( psz_duptip != NULL )
                {
                    [o_modules setToolTip: [NSApp localizedString:
                                            vlc_wraptext(psz_duptip, PREFS_WRAP)]];
                    free( psz_duptip );
                }
                [o_view addSubview: [o_modules autorelease]];

                [o_modules addItemWithTitle: _NS("Auto")];

                /* build a list of available modules */
                {
                    for( i_index = 0; i_index < p_list->i_count; i_index++ )
                    {
                        p_a_module = (module_t *)p_list->p_values[i_index].p_object ;
    
                        if( !strcmp( p_a_module->psz_capability,
                                    p_item->psz_type ) )
                        {
                            NSString *o_object_name = [NSApp
                                localizedString: p_a_module->psz_object_name];
                            [o_modules addItemWithTitle: o_object_name];
                        }
                    }
                }
    
                if( p_item->psz_value != NULL )
                {
                    NSString *o_value =
                        [NSApp localizedString: p_item->psz_value];
    
                    [o_modules selectItemWithTitle: o_value];
                }
                else
                {
                    [o_modules selectItemWithTitle: _NS("Auto")];
                }

                CONTROL_LABEL( p_item->psz_text );
                s_rc.origin.y += s_rc.size.height;
                s_rc.origin.x = X_ORIGIN;
            }
            break;

            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_DIRECTORY:
            {
    
                if( !p_item->ppsz_list )
                {
                    char *psz_value = p_item->psz_value ?
                                    p_item->psz_value : "";
    
                    INPUT_FIELD_STRING( p_item->psz_name, p_item->psz_text, 200,
                                        [NSApp localizedString: psz_value],
                                        p_item->psz_longtext );
                }
                else
                {
                    int i;
                    VLCComboBox *o_combo_box;
                    char * psz_duptip = NULL;
                    if ( p_item->psz_longtext != NULL && [NSApp getEncoding] == NSISOLatin1StringEncoding )
                        psz_duptip = strdup(p_item->psz_longtext);
    
                    s_rc.size.height = 27;
                    s_rc.size.width = 200;
                    s_rc.origin.y += 10;
    
                    CHECK_VIEW_HEIGHT;
    
                    o_combo_box = [[VLCComboBox alloc] initWithFrame: s_rc];
                    CONTROL_CONFIG( o_combo_box, o_module_name,
                                    CONFIG_ITEM_STRING, p_item->psz_name );
                    [o_combo_box setTarget: self];
                    [o_combo_box setAction: @selector(configChanged:)];
                    [o_combo_box sendActionOn:NSLeftMouseUpMask];

                    if ( psz_duptip != NULL )
                    {
                        [o_combo_box setToolTip: [NSApp localizedString:
                                            vlc_wraptext(psz_duptip, PREFS_WRAP)]];
                        free( psz_duptip );
                    }
                    [o_view addSubview: [o_combo_box autorelease]];
                    
                    for( i=0; p_item->ppsz_list[i]; i++ )
                    {
                        [o_combo_box addItemWithObjectValue:
                            [NSApp localizedString: p_item->ppsz_list[i]]];
                    }
                    [o_combo_box setStringValue: [NSApp localizedString: 
                        p_item->psz_value ? p_item->psz_value : ""]];
    
                    CONTROL_LABEL( p_item->psz_text );
    
                    s_rc.origin.y += s_rc.size.height;
                    s_rc.origin.x = X_ORIGIN;
                }
    
            }
            break;
    
            case CONFIG_ITEM_INTEGER:
            {
                INPUT_FIELD_INTEGER( p_item->psz_name, p_item->psz_text, 70,
                                    p_item->i_value, p_item->psz_longtext );
            }
            break;
    
            case CONFIG_ITEM_FLOAT:
            {
                INPUT_FIELD_FLOAT( p_item->psz_name, p_item->psz_text, 70,
                                p_item->f_value, p_item->psz_longtext );
            }
            break;
    
            case CONFIG_ITEM_BOOL:
            {
                VLCButton *o_btn_bool;
                char * psz_duptip = NULL;
                if ( p_item->psz_longtext != NULL && [NSApp getEncoding] == NSISOLatin1StringEncoding )
                    psz_duptip = strdup(p_item->psz_longtext);
    
                s_rc.size.height = 27;
                s_rc.size.width = s_vrc.size.width - X_ORIGIN * 2 - 20;
                s_rc.origin.y += 10;
    
                CHECK_VIEW_HEIGHT;
    
                o_btn_bool = [[VLCButton alloc] initWithFrame: s_rc];
                [o_btn_bool setButtonType: NSSwitchButton];
                [o_btn_bool setIntValue: p_item->i_value];
                [o_btn_bool setTitle:
                    [NSApp localizedString: p_item->psz_text]];
                if ( psz_duptip != NULL )
                {
                    [o_btn_bool setToolTip: [NSApp localizedString:
                                            vlc_wraptext(psz_duptip, PREFS_WRAP)]];
                    free( psz_duptip );
                }
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
        vlc_list_release( p_list );
    
    [o_prefs_view setDocumentView: o_view];
    [o_prefs_view setNeedsDisplay: TRUE];
}


@end

@implementation VLCPrefs (NSTableDataSource)

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item {
    return (item == nil) ? [[VLCTreeItem rootItem] numberOfChildren] : [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return (item == nil) ? YES : ([item numberOfChildren] != -1);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item {
    return (item == nil) ? [[VLCTreeItem rootItem] childAtIndex:index] : [item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item {
    return (item == nil) ? @"" : (id)[item getName];
}

@end

@implementation VLCTreeItem

static VLCTreeItem *o_root_item = nil;

#define IsALeafNode ((id)-1)

- (id)initWithName: (NSString *)o_item_name ID: (int)i_id parent:(VLCTreeItem *)o_parent_item
{
    self = [super init];

    if( self != nil )
    {
        o_name = [o_item_name copy];
        i_object_id = i_id;
        o_parent = o_parent_item;
    }
    return( self );
}

+ (VLCTreeItem *)rootItem {
   if (o_root_item == nil) o_root_item = [[VLCTreeItem alloc] initWithName:@"main" ID: 0 parent:nil];
   return o_root_item;       
}

- (void)dealloc
{
    if (o_children != IsALeafNode) [o_children release];
    [o_name release];
    [super dealloc];
}

/* Creates and returns the array of children
 * Loads children incrementally */
- (NSArray *)children {
    if (o_children == NULL) {
        intf_thread_t *p_intf = [NSApp getIntf];
        vlc_list_t      *p_list;
        module_t        *p_module;
        module_config_t *p_item;
        int i_index,j;

        /* List the plugins */
        p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
        if( !p_list ) return nil;

        if( [[self getName] isEqualToString: @"main"] )
        {
            /*
            * Build a tree of the main options
            */
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    break;
            }
            if( i_index < p_list->i_count )
            {
                /* We found the main module */
        
                /* Enumerate config categories and store a reference so we can
                 * generate their config panel them when it is asked by the user. */
                p_item = p_module->p_config;
                o_children = [[NSMutableArray alloc] initWithCapacity:10];

                if( p_item ) do
                {
                    NSString *o_child_name;
                    
                    switch( p_item->i_type )
                    {
                    case CONFIG_HINT_CATEGORY:
                        o_child_name = [NSApp localizedString: p_item->psz_text];
                        [o_children addObject:[[VLCTreeItem alloc] initWithName: o_child_name
                            ID: p_module->i_object_id parent:self]];
                        break;
                    }
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                
                /* Add the plugins item */
                [o_children addObject:[[VLCTreeItem alloc] initWithName: _NS("Modules")
                    ID: 0 parent:self]];
            }
            else
            {
                o_children = IsALeafNode;
            }
        }
        else if( [[self getName] isEqualToString: _NS("Modules")] )
        {
            /* Add the capabilities */
            o_children = [[NSMutableArray alloc] initWithCapacity:10];
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;
        
                /* Exclude the main module */
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    continue;
        
                /* Exclude empty plugins */
                p_item = p_module->p_config;
                if( !p_item ) continue;
                do
                {
                    if( p_item->i_type & CONFIG_ITEM )
                        break;
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                if( p_item->i_type == CONFIG_HINT_END ) continue;
        
                /* Create the capability tree if it doesn't already exist */
                NSString *o_capability;
                o_capability = [NSApp localizedString: p_module->psz_capability];
                if( !p_module->psz_capability || !*p_module->psz_capability )
                {
                    /* Empty capability ? Let's look at the submodules */
                    module_t * p_submodule;
                    for( j = 0; j < p_module->i_children; j++ )
                    {
                        p_submodule = (module_t*)p_module->pp_children[ j ];
                        if( p_submodule->psz_capability && *p_submodule->psz_capability )
                        {
                            o_capability = [NSApp localizedString: p_submodule->psz_capability];
                            BOOL b_found = FALSE;
                            for( j = 0; j < [o_children count]; j++ )
                            {
                                if( [[[o_children objectAtIndex:j] getName] isEqualToString: o_capability] )
                                {
                                    b_found = TRUE;
                                    break;
                                }
                            }
                            if( !b_found )
                            {
                                [o_children addObject:[[VLCTreeItem alloc] initWithName: o_capability
                                ID: 0 parent:self]];
                            }
                        }
                    }
                }

                BOOL b_found = FALSE;
                for( j = 0; j < [o_children count]; j++ )
                {
                    if( [[[o_children objectAtIndex:j] getName] isEqualToString: o_capability] )
                    {
                        b_found = TRUE;
                        break;
                    }
                }
                if( !b_found )
                {
                    [o_children addObject:[[VLCTreeItem alloc] initWithName: o_capability
                    ID: 0 parent:self]];
                }
            }
        }
        else if( [[o_parent getName] isEqualToString: _NS("Modules")] )
        {
            /* Now add the modules */
            o_children = [[NSMutableArray alloc] initWithCapacity:10];
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;
        
                /* Exclude the main module */
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    continue;
        
                /* Exclude empty plugins */
                p_item = p_module->p_config;
                if( !p_item ) continue;
                do
                {
                    if( p_item->i_type & CONFIG_ITEM )
                        break;
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                if( p_item->i_type == CONFIG_HINT_END ) continue;
        
                /* Check the capability */
                NSString *o_capability;
                o_capability = [NSApp localizedString: p_module->psz_capability];
                if( !p_module->psz_capability || !*p_module->psz_capability )
                {
                    /* Empty capability ? Let's look at the submodules */
                    module_t * p_submodule;
                    for( j = 0; j < p_module->i_children; j++ )
                    {
                        p_submodule = (module_t*)p_module->pp_children[ j ];
                        if( p_submodule->psz_capability && *p_submodule->psz_capability )
                        {
                            o_capability = [NSApp localizedString: p_submodule->psz_capability];
                            if( [o_capability isEqualToString: [self getName]] )
                            {
                            [o_children addObject:[[VLCTreeItem alloc] initWithName:
                                [NSApp localizedString: p_module->psz_object_name ]
                                ID: p_module->i_object_id parent:self]];
                            }
                        }
                    }
                }
                else if( [o_capability isEqualToString: [self getName]] )
                {
                    [o_children addObject:[[VLCTreeItem alloc] initWithName:
                        [NSApp localizedString: p_module->psz_object_name ]
                        ID: p_module->i_object_id parent:self]];
                }
            }
        }
        else
        {
            /* all the other stuff are leafs */
            o_children = IsALeafNode;
        }
    }
    return o_children;
}

- (int)getObjectID
{
    return i_object_id;
}

- (NSString *)getName
{
    return o_name;
}

- (VLCTreeItem *)childAtIndex:(int)i_index {
    return [[self children] objectAtIndex:i_index];
}

- (int)numberOfChildren {
    id i_tmp = [self children];
    return (i_tmp == IsALeafNode) ? (-1) : [i_tmp count];
}

- (BOOL)hasPrefs:(NSString *)o_module_name
{
    intf_thread_t *p_intf = [NSApp getIntf];
    module_t *p_parser;
    vlc_list_t *p_list;
    char *psz_module_name;
    int i_index;

    psz_module_name = (char *)[o_module_name lossyCString];

    /* look for module */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_object_name, psz_module_name ) )
        {
            BOOL b_has_prefs = p_parser->i_config_items != 0;
            vlc_list_release( p_list );
            return( b_has_prefs );
        }
    }

    vlc_list_release( p_list );

    return( NO );
}

@end


@implementation VLCFlippedView

- (BOOL)isFlipped
{
    return( YES );
}

@end

IMPL_CONTROL_CONFIG(Button);
IMPL_CONTROL_CONFIG(PopUpButton);
IMPL_CONTROL_CONFIG(ComboBox);
IMPL_CONTROL_CONFIG(TextField);
