/*****************************************************************************
 * prefs.m: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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

#include "intf.h"
#include "prefs.h"
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
    [o_prefs_view setRulersVisible: NO];
    [o_prefs_view setDocumentView: o_empty_view];
    [o_tree selectRow:0 byExtendingSelection:NO];
}

- (void)showPrefs
{
    /* load our nib (if not already loaded) */
    [NSBundle loadNibNamed:@"Preferences" owner:self];
    
    /* Show View for the currently select treeitem */
    /* [self showViewForID: [[o_tree itemAtRow:[o_tree selectedRow]] getObjectID]
        andName: [[o_tree itemAtRow:[o_tree selectedRow]] getName]]; */
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
    config_SaveConfigFile( p_intf, NULL );
    [o_prefs_window orderOut:self];
}

- (IBAction)closePrefs: (id)sender
{
    [o_prefs_window orderOut:self];
}

- (IBAction)resetAll: (id)sender
{
    NSBeginInformationalAlertSheet(_NS("Reset Preferences"), _NS("Cancel"), _NS("Continue"), 
        nil, o_prefs_window, self, @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil,
        _NS("Beware this will reset your VLC media player preferences.\n"
            "Are you sure you want to continue?") );
}

- (void)sheetDidEnd:(NSWindow *)o_sheet returnCode:(int)i_return contextInfo:(void *)o_context
{
    if( i_return == NSAlertAlternateReturn )
    {
        config_ResetAll( p_intf );
        [self showViewForID: [[o_tree itemAtRow:[o_tree selectedRow]] getObjectID]
            andName: [[o_tree itemAtRow:[o_tree selectedRow]] getName]];
    }
}

- (IBAction)advancedToggle: (id)sender
{
    b_advanced = !b_advanced;
    [o_advanced_ckb setState: b_advanced];
    /* refresh the view of the current treeitem */
    /* [self showViewForID: [[o_tree itemAtRow:[o_tree selectedRow]] getObjectID]
        andName: [[o_tree itemAtRow:[o_tree selectedRow]] getName]]; */
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
    /*
    [self showViewForID: [[o_tree itemAtRow:[o_tree selectedRow]] getObjectID]
        andName: [[o_tree itemAtRow:[o_tree selectedRow]] getName]];*/
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
- (NSArray *)children
{
    if( o_children == NULL )
    {
        intf_thread_t *p_intf = VLCIntf;
        vlc_list_t      *p_list;
        module_t        *p_module = NULL;
        module_config_t *p_item;
        int i_index,j;

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
                msg_Err( p_intf, "could not find the main module in our preferences" );
                return nil;
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
                        o_child_name = [[VLCMain sharedInstance] localizedString: p_item->psz_text];
                        [o_children addObject:[[VLCTreeItem alloc] initWithName: o_child_name
                            ID: p_module->i_object_id parent:self]];
                        break;
                    }
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                
                /* Add the modules item */
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
        
                /* Exclude empty modules */
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
                o_capability = [[VLCMain sharedInstance] localizedString: p_module->psz_capability];
                if( !p_module->psz_capability || !*p_module->psz_capability )
                {
                    /* Empty capability ? Let's look at the submodules */
                    module_t * p_submodule;
                    for( j = 0; j < p_module->i_children; j++ )
                    {
                        p_submodule = (module_t*)p_module->pp_children[ j ];
                        if( p_submodule->psz_capability && *p_submodule->psz_capability )
                        {
                            o_capability = [[VLCMain sharedInstance] localizedString: p_submodule->psz_capability];
                            BOOL b_found = FALSE;
                            for( j = 0; j < (int)[o_children count]; j++ )
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
                for( j = 0; j < (int)[o_children count]; j++ )
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
        
                /* Exclude empty modules */
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
                o_capability = [[VLCMain sharedInstance] localizedString: p_module->psz_capability];
                if( !p_module->psz_capability || !*p_module->psz_capability )
                {
                    /* Empty capability ? Let's look at the submodules */
                    module_t * p_submodule;
                    for( j = 0; j < p_module->i_children; j++ )
                    {
                        p_submodule = (module_t*)p_module->pp_children[ j ];
                        if( p_submodule->psz_capability && *p_submodule->psz_capability )
                        {
                            o_capability = [[VLCMain sharedInstance] localizedString: p_submodule->psz_capability];
                            if( [o_capability isEqualToString: [self getName]] )
                            {
                            [o_children addObject:[[VLCTreeItem alloc] initWithName:
                                [[VLCMain sharedInstance] localizedString: p_module->psz_object_name ]
                                ID: p_module->i_object_id parent:self]];
                            }
                        }
                    }
                }
                else if( [o_capability isEqualToString: [self getName]] )
                {
                    [o_children addObject:[[VLCTreeItem alloc] initWithName:
                        [[VLCMain sharedInstance] localizedString: p_module->psz_object_name ]
                        ID: p_module->i_object_id parent:self]];
                }
            }
        }
        else
        {
            /* all the other stuff are leafs */
            o_children = IsALeafNode;
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

- (VLCTreeItem *)childAtIndex:(int)i_index {
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

@end


@implementation VLCFlippedView

- (BOOL)isFlipped
{
    return( YES );
}

@end
