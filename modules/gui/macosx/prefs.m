/*****************************************************************************
 * prefs.m: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

/* VLCPrefs manages the main preferences dialog 
   the class is related to wxwindows intf, PrefsPanel */
/* VLCTreeItem should contain:
   - the children of the treeitem
   - the associated prefs widgets
   - the documentview with all the prefs widgets in it
   - a saveChanges action
   - a revertChanges action
   - an advanced action (to hide/show advanced options)
   - a redraw view action
   - the children action should generate a list of the treeitems children (to be used by VLCPrefs datasource)

   The class is sort of a mix of wxwindows intfs, PrefsTreeCtrl and ConfigTreeData
*/
/* VLCConfigControl are subclassed NSView's containing and managing individual config items
   the classes are VERY closely related to wxwindows ConfigControls */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc_config_cat.h>

#include "intf.h"
#include "prefs.h"
#include "prefs_widgets.h"
#include "vlc_keys.h"

/*****************************************************************************
 * VLCPrefs implementation
 *****************************************************************************/
@implementation VLCPrefs

static VLCPrefs *_o_sharedMainInstance = nil;

+ (VLCPrefs *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedMainInstance ) {
        [self dealloc];
    }
    else
    {
        _o_sharedMainInstance = [super init];
        p_intf = VLCIntf;
        o_empty_view = [[NSView alloc] init];
    }

    return _o_sharedMainInstance;
}

- (void)dealloc
{
    [o_empty_view release];
    [super dealloc];
}

- (void)awakeFromNib
{
    p_intf = VLCIntf;
    b_advanced = config_GetInt( p_intf, "advanced" );

    [self initStrings];
    [o_advanced_ckb setState: b_advanced];
    [o_prefs_view setBorderType: NSGrooveBorder];
    [o_prefs_view setHasVerticalScroller: YES];
    [o_prefs_view setDrawsBackground: NO];
    [o_prefs_view setDocumentView: o_empty_view];
    [o_tree selectRow:0 byExtendingSelection:NO];
}

- (void)setTitle: (NSString *) o_title_name
{
    [o_title setStringValue: o_title_name];
}

- (void)showPrefs
{
    /* load our nib (if not already loaded) */
    [NSBundle loadNibNamed:@"Preferences" owner:self];

    [o_prefs_window center];
    [o_prefs_window makeKeyAndOrderFront:self];
}

- (void)initStrings
{
    [o_prefs_window setTitle: _NS("Preferences")];
    [o_save_btn setTitle: _NS("Save")];
    [o_cancel_btn setTitle: _NS("Cancel")];
    [o_reset_btn setTitle: _NS("Reset All")];
    [o_advanced_ckb setTitle: _NS("Advanced")];
}

- (IBAction)savePrefs: (id)sender
{
    /* TODO: call savePrefs on Root item */
    [[VLCTreeItem rootItem] applyChanges];
    config_SaveConfigFile( p_intf, NULL );
    [o_prefs_window orderOut:self];
}

- (IBAction)closePrefs: (id)sender
{
    [o_prefs_window orderOut:self];
}

- (IBAction)resetAll: (id)sender
{
    NSBeginInformationalAlertSheet(_NS("Reset Preferences"), _NS("Cancel"),
        _NS("Continue"), nil, o_prefs_window, self,
        @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil,
        _NS("Beware this will reset your VLC media player preferences.\n"
            "Are you sure you want to continue?") );
}

- (void)sheetDidEnd:(NSWindow *)o_sheet returnCode:(int)i_return
    contextInfo:(void *)o_context
{
    if( i_return == NSAlertAlternateReturn )
    {
        [o_prefs_view setDocumentView: o_empty_view];
        config_ResetAll( p_intf );
        [[VLCTreeItem rootItem] resetView];
        [[o_tree itemAtRow:[o_tree selectedRow]]
            showView:o_prefs_view advancedView:
            ( [o_advanced_ckb state] == NSOnState ) ? VLC_TRUE : VLC_FALSE];
    }
}

- (IBAction)advancedToggle: (id)sender
{
    b_advanced = !b_advanced;
    [o_advanced_ckb setState: b_advanced];
    /* refresh the view of the current treeitem */
    [[o_tree itemAtRow:[o_tree selectedRow]] showView:o_prefs_view advancedView:
        ( [o_advanced_ckb state] == NSOnState ) ? VLC_TRUE : VLC_FALSE];
}

- (void)loadConfigTree
{
}

- (void)outlineViewSelectionIsChanging:(NSNotification *)o_notification
{
}

/* update the document view to the view of the selected tree item */
- (void)outlineViewSelectionDidChange:(NSNotification *)o_notification
{
    [[o_tree itemAtRow:[o_tree selectedRow]] showView: o_prefs_view
        advancedView:( [o_advanced_ckb state] == NSOnState ) ?
        VLC_TRUE : VLC_FALSE];
}

@end

@implementation VLCPrefs (NSTableDataSource)

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item {
    return (item == nil) ? [[VLCTreeItem rootItem] numberOfChildren] :
                            [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return (item == nil) ? YES : ( ([item numberOfChildren] != -1) && 
                                   ([item numberOfChildren] != 0));
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item {
    return (item == nil) ? [[VLCTreeItem rootItem] childAtIndex:index] :
                            [item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView
    objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    return (item == nil) ? @"" : (id)[item getName];
}

@end

@implementation VLCTreeItem

static VLCTreeItem *o_root_item = nil;

#define IsALeafNode ((id)-1)

- (id)initWithName: (NSString *)o_item_name
    withTitle: (NSString *)o_item_title
    withHelp: (NSString *)o_item_help
    ID: (int)i_id
    parent:(VLCTreeItem *)o_parent_item
    children:(NSMutableArray *)o_children_array
    whithCategory: (int) i_category
{
    self = [super init];

    if( self != nil )
    {
        o_name = [o_item_name copy];
        o_title= [o_item_title copy];
        o_help= [o_item_help copy];
        i_object_id = i_id;
        o_parent = o_parent_item;
        o_children = o_children_array;
        i_object_category = i_category;
        o_subviews = nil;
    }
    return( self );
}

+ (VLCTreeItem *)rootItem
{
   if (o_root_item == nil)
        o_root_item = [[VLCTreeItem alloc] initWithName:@"main" withTitle:@"main" withHelp:@"" ID:0
            parent:nil children:[[NSMutableArray alloc] initWithCapacity:10]
            whithCategory: -1];
   return o_root_item;
}

- (void)dealloc
{
    if (o_children != IsALeafNode) [o_children release];
    [o_name release];
    [o_title release];
    [o_help release];
    [super dealloc];
}

/* Creates and returns the array of children
 * Loads children incrementally */
- (NSArray *)children
{
    if( o_children == IsALeafNode )
        return o_children;
    if( [ o_children count] == 0 )
    {
        intf_thread_t   *p_intf = VLCIntf;
        vlc_list_t      *p_list;
        module_t        *p_module = NULL;
        module_config_t *p_item;
        int             i_index;

        /* List the modules */
        p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
        if( !p_list ) return nil;

        if( [[self getName] isEqualToString: @"main"] )
        {
            /*
            * Find the main module
            */
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    break;
            }
            if( p_module == NULL )
            {
                msg_Err( p_intf,
                    "could not find the main module in our preferences" );
                return nil;
            }
            if( i_index < p_list->i_count )
            {
                /* We found the main module */
                /* Enumerate config categories and store a reference so we can
                 * generate their config panel them when it is asked by the user. */
                VLCTreeItem *p_last_category = NULL;
                p_item = p_module->p_config;
                o_children = [[NSMutableArray alloc] initWithCapacity:10];
                if( p_item ) do
                {
                    NSString *o_child_name;
                    NSString *o_child_title;
                    NSString *o_child_help;
                    switch( p_item->i_type )
                    {
                    case CONFIG_CATEGORY:
			if( p_item->i_value == -1 ) break;
                        o_child_name = [[VLCMain sharedInstance]
                            localizedString: config_CategoryNameGet( p_item->i_value ) ];
                        o_child_title = o_child_name;
                        o_child_help = [[VLCMain sharedInstance]
                            localizedString: config_CategoryHelpGet( p_item->i_value ) ];
                        p_last_category = [VLCTreeItem alloc];
                        [o_children addObject:[p_last_category
                            initWithName: o_child_name
                            withTitle: o_child_title
                            withHelp: o_child_help
                            ID: p_item->i_value
                            parent:self
                            children:[[NSMutableArray alloc]
                                initWithCapacity:10]
                            whithCategory: p_item - p_module->p_config]];
                        break;
                    case CONFIG_SUBCATEGORY:
			if( p_item->i_value == -1 ) break;
                        o_child_name = [[VLCMain sharedInstance]
                            localizedString: config_CategoryNameGet( p_item->i_value ) ];
                        o_child_title = o_child_name;
                        o_child_help = [[VLCMain sharedInstance]
                            localizedString: config_CategoryHelpGet( p_item->i_value ) ];
                        if( p_item->i_value != SUBCAT_PLAYLIST_GENERAL &&
                            p_item->i_value != SUBCAT_VIDEO_GENERAL &&
                            p_item->i_value != SUBCAT_AUDIO_GENERAL )
                            [p_last_category->o_children
                                addObject:[[VLCTreeItem alloc]
                                initWithName: o_child_name
                                withTitle: o_child_title
                                withHelp: o_child_help
                                ID: p_item->i_value
                                parent:p_last_category
                                children:[[NSMutableArray alloc]
                                    initWithCapacity:10]
                                whithCategory: p_item - p_module->p_config]];
                        break;
                    default:
                        break;
                    }
                } while( p_item->i_type != CONFIG_HINT_END && p_item++ );
            }

            /* Build a tree of the plugins */
            /* Add the capabilities */
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;

                /* Exclude the main module */
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    continue;

                /* Exclude empty plugins (submodules don't have config */
                /* options, they are stored in the parent module) */
                if( p_module->b_submodule )
                    continue;
                else
                    p_item = p_module->p_config;

                if( !p_item ) continue;
                int i_category = -1;
                int i_subcategory = -1;
                int i_options = 0;
                do
                {
                    if( p_item->i_type == CONFIG_CATEGORY )
                        i_category = p_item->i_value;
                    else if( p_item->i_type == CONFIG_SUBCATEGORY )
                        i_subcategory = p_item->i_value;

                    if( p_item->i_type & CONFIG_ITEM )
                        i_options ++;
                    if( i_options > 0 && i_category >= 0 && i_subcategory >= 0 )
                        break;
                } while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                if( !i_options ) continue;

                /* Find the right category item */

                long cookie;
                vlc_bool_t b_found = VLC_FALSE;
                unsigned int i;
                VLCTreeItem* p_category_item, * p_subcategory_item;
                for (i = 0 ; i < [o_children count] ; i++)
                {
                    p_category_item = [o_children objectAtIndex: i];
                    if( p_category_item->i_object_id == i_category )
                    {
                        b_found = VLC_TRUE;
                        break;
                    }
                }
                if( !b_found ) continue;

                /* Find subcategory item */
                b_found = VLC_FALSE;
                cookie = -1;
                for (i = 0 ; i < [p_category_item->o_children count] ; i++)
                {
                    p_subcategory_item = [p_category_item->o_children
                                            objectAtIndex: i];
                    if( p_subcategory_item->i_object_id == i_subcategory )
                    {
                        b_found = VLC_TRUE;
                        break;
                    }
                }
                if( !b_found )
                    p_subcategory_item = p_category_item;

                [p_subcategory_item->o_children addObject:[[VLCTreeItem alloc]
                    initWithName:[[VLCMain sharedInstance]
                        localizedString: p_module->psz_shortname ?
                        p_module->psz_shortname : p_module->psz_object_name ]
                    withTitle:[[VLCMain sharedInstance]
                        localizedString: p_module->psz_longname ?
                        p_module->psz_longname : p_module->psz_object_name ]
                    withHelp: @""
                    ID: p_module->i_object_id
                    parent:p_subcategory_item
                    children:IsALeafNode
                    whithCategory: -1]];
            }
        }
        vlc_list_release( p_list );
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

- (NSString *)getTitle
{
    return o_title;
}

- (NSString *)getHelp
{
    return o_help;
}

- (VLCTreeItem *)childAtIndex:(int)i_index
{
    return [[self children] objectAtIndex:i_index];
}

- (int)numberOfChildren {
    id i_tmp = [self children];
    return (i_tmp == IsALeafNode) ? (-1) : (int)[i_tmp count];
}

- (BOOL)hasPrefs:(NSString *)o_module_name
{
    intf_thread_t *p_intf = VLCIntf;
    module_t *p_parser;
    vlc_list_t *p_list;
    char *psz_module_name;
    int i_index;

    psz_module_name = (char *)[o_module_name UTF8String];

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

- (NSView *)showView:(NSScrollView *)o_prefs_view
    advancedView:(vlc_bool_t) b_advanced
{
    NSRect          s_vrc;
    NSView          *o_view;

    [[VLCPrefs sharedInstance] setTitle: [self getTitle]];
    /* NSLog( [self getHelp] ); */ 
    s_vrc = [[o_prefs_view contentView] bounds]; s_vrc.size.height -= 4;
    o_view = [[VLCFlippedView alloc] initWithFrame: s_vrc];
    [o_view setAutoresizingMask: NSViewWidthSizable | NSViewMinYMargin |
                                    NSViewMaxYMargin];

/* Create all subviews if it isn't already done because we cannot use */
/* setHiden for MacOS < 10.3*/
    if( o_subviews == nil )
    {
        intf_thread_t   *p_intf = VLCIntf;
        vlc_list_t      *p_list;
        module_t        *p_parser = NULL;
        module_config_t *p_item;

        o_subviews = [[NSMutableArray alloc] initWithCapacity:10];
        /* Get a pointer to the module */
        if( i_object_category == -1 )
        {
            p_parser = (module_t *) vlc_object_get( p_intf, i_object_id );
            if( !p_parser || p_parser->i_object_type != VLC_OBJECT_MODULE )
            {
                /* 0OOoo something went really bad */
                return nil;
            }
            p_item = p_parser->p_config;
            int i = 0;

            p_item = p_parser->p_config + 1;

            do
            {
                if( !p_item )
                {
                    msg_Err( p_intf, "null item found" );
                    break;
                }
                switch(p_item->i_type)
                {
                case CONFIG_SUBCATEGORY:
                    break;
                case CONFIG_CATEGORY:
                    break;
                case CONFIG_SECTION:
                    break;
                case CONFIG_HINT_END:
                    break;
                case CONFIG_HINT_USAGE:
                    break;
                default:
                {
                    VLCConfigControl *o_control = nil;
                    o_control = [VLCConfigControl newControl:p_item
                                                  withView:o_view];
                    if( o_control != nil )
                    {
                        [o_control setAutoresizingMask: NSViewMaxYMargin |
                            NSViewWidthSizable];
                        [o_subviews addObject: o_control];
                    }
                }
                    break;
                }
            } while( p_item++->i_type != CONFIG_HINT_END );

            vlc_object_release( p_parser );
        }
        else
        {
            int i = 0;
            int i_index;
            p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
            if( !p_list ) return o_view;

            /*
            * Find the main module
            */
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_parser = (module_t *)p_list->p_values[i_index].p_object;
                if( !strcmp( p_parser->psz_object_name, "main" ) )
                    break;
            }
            if( p_parser == NULL )
            {
                msg_Err( p_intf, "could not find the main module in our "
                                    "preferences" );
                return o_view;
            }
            p_item = (p_parser->p_config + i_object_category);
            if( ( p_item->i_type == CONFIG_CATEGORY ) &&
              ( ( p_item->i_value == CAT_PLAYLIST )  ||
                ( p_item->i_value == CAT_AUDIO )  ||
                ( p_item->i_value == CAT_VIDEO ) ) )
                p_item++;

            do
            {
                p_item++;
                if( !p_item )
                {
                    msg_Err( p_intf, "null item found" );
                    break;
                }
                switch( p_item->i_type )
                {
                case CONFIG_SUBCATEGORY:
                    break;
                case CONFIG_CATEGORY:
                    break;
                case CONFIG_SECTION:
                    break;
                case CONFIG_HINT_END:
                    break;
                case CONFIG_HINT_USAGE:
                    break;
                default:
                {
                    VLCConfigControl *o_control = nil;
                    o_control = [VLCConfigControl newControl:p_item
                                                  withView:o_view];
                    if( o_control != nil )
                    {
                        [o_control setAutoresizingMask: NSViewMaxYMargin |
                                                        NSViewWidthSizable];
                        [o_subviews addObject: o_control];
                    }
                    break;
                }
                }
            } while ( ( p_item->i_type != CONFIG_HINT_END ) &&
                      ( p_item->i_type != CONFIG_SUBCATEGORY ) );

            vlc_list_release( p_list );
        }
    }

    if( o_view != nil )
    {
        int i_lastItem = 0;
        int i_yPos = -2;
        int i_max_label = 0;
        int i_show_advanced = 0;

        NSEnumerator *enumerator = [o_subviews objectEnumerator];
        VLCConfigControl *o_widget;
        NSRect o_frame;
        
        while( ( o_widget = [enumerator nextObject] ) )
            if( ( [o_widget isAdvanced] ) && (! b_advanced) )
                continue;
            else if( i_max_label < [o_widget getLabelSize] )
                i_max_label = [o_widget getLabelSize];

        enumerator = [o_subviews objectEnumerator];
        while( ( o_widget = [enumerator nextObject] ) )
        {
            int i_widget;
            if( ( [o_widget isAdvanced] ) && (! b_advanced) )
            {
                i_show_advanced++;
                continue;
            }

            i_widget = [o_widget getViewType];
            i_yPos += [VLCConfigControl calcVerticalMargin:i_widget
                lastItem:i_lastItem];
            [o_widget setYPos:i_yPos];
            o_frame = [o_widget frame];
            o_frame.size.width = [o_view frame].size.width -
                                    LEFTMARGIN - RIGHTMARGIN;
            [o_widget setFrame:o_frame];
            [o_widget alignWithXPosition: i_max_label];
            i_yPos += [o_widget frame].size.height;
            i_lastItem = i_widget;
            [o_view addSubview:o_widget];
         }
        if( i_show_advanced != 0 )
        {
            /* We add the advanced notice... */
            NSRect s_rc = [o_view frame];
            NSTextField *o_label;
            s_rc.size.height = 17;
            s_rc.origin.x = LEFTMARGIN;
            s_rc.origin.y = i_yPos += [VLCConfigControl
                                        calcVerticalMargin:CONFIG_ITEM_STRING
                                        lastItem:i_lastItem];
            o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
            [o_label setDrawsBackground: NO];
            [o_label setBordered: NO];
            [o_label setEditable: NO];
            [o_label setSelectable: NO];
            [o_label setStringValue: _NS("Some options are available but " \
                                "hidden. Check \"Advanced\" to see them.")];
            [o_label setFont:[NSFont systemFontOfSize:10]];
            [o_label sizeToFit];
            [o_view addSubview:o_label];
            i_yPos += [o_label frame].size.height;
        }
        o_frame = [o_view frame];
        o_frame.size.height = i_yPos;
        [o_view setFrame:o_frame];
        [o_prefs_view setDocumentView:o_view];

    }
    return o_view;
}

- (void)applyChanges
{
    unsigned int i;
    if( o_subviews != nil )
        //Item has been shown
        for( i = 0 ; i < [o_subviews count] ; i++ )
            [[o_subviews objectAtIndex:i] applyChanges];

    if( o_children != IsALeafNode )
        for( i = 0 ; i < [o_children count] ; i++ )
            [[o_children objectAtIndex:i] applyChanges];
}

- (void)resetView
{
    unsigned int i;
    if( o_subviews != nil )
    {
        //Item has been shown
        [o_subviews release];
        o_subviews = nil;
    }

    if( o_children != IsALeafNode )
        for( i = 0 ; i < [o_children count] ; i++ )
            [[o_children objectAtIndex:i] resetView];
}

@end


@implementation VLCFlippedView

- (BOOL)isFlipped
{
    return( YES );
}

@end
