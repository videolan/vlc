/*****************************************************************************
 * prefs.m: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: prefs.m,v 1.39 2004/02/02 08:50:41 titer Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *	    Derk-Jan Hartman <hartman at videolan.org>
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
#include <string.h>

#include "intf.h"
#include "prefs.h"
#include "misc.h"
#include <vlc_help.h>

#define ROOT_ID 1241
#define GENERAL_ID 1242
#define MODULE_ID 1243
#define CAPABILITY_ID 1244

/*****************************************************************************
 * GetCapabilityHelp: Display the help for one capability.
 *****************************************************************************/
static char * GetCapabilityHelp( char *psz_capability, int i_type)
{
    if( psz_capability == NULL) return "";

    if( !strcasecmp(psz_capability,"access") )
        return i_type == 1 ? ACCESS_TITLE : ACCESS_HELP;
    if( !strcasecmp(psz_capability,"audio filter") )
        return i_type == 1 ? AUDIO_FILTER_TITLE : AUDIO_FILTER_HELP;
    if( !strcasecmp(psz_capability,"audio output") )
        return i_type == 1 ? AOUT_TITLE : AOUT_HELP;
    if( !strcasecmp(psz_capability,"audio encoder") )
        return i_type == 1 ? AOUT_ENC_TITLE : AOUT_ENC_HELP;
    if( !strcasecmp(psz_capability,"chroma") )
        return i_type == 1 ? CHROMA_TITLE : CHROMA_HELP;
    if( !strcasecmp(psz_capability,"decoder") )
        return i_type == 1 ? DECODER_TITLE : DECODER_HELP;
    if( !strcasecmp(psz_capability,"demux") )
        return i_type == 1 ? DEMUX_TITLE : DEMUX_HELP;
    if( !strcasecmp(psz_capability,"interface") )
        return i_type == 1 ? INTERFACE_TITLE : INTERFACE_HELP;
    if( !strcasecmp(psz_capability,"sout access") )
        return i_type == 1 ? SOUT_TITLE : SOUT_HELP;
    if( !strcasecmp(psz_capability,"subtitle demux") )
        return i_type == 1 ? SUBTITLE_DEMUX_TITLE : SUBTITLE_DEMUX_HELP;
    if( !strcasecmp(psz_capability,"text renderer") )
        return i_type == 1 ? TEXT_TITLE : TEXT_HELP;
    if( !strcasecmp(psz_capability,"video output") )
        return i_type == 1 ? VOUT__TITLE : VOUT_HELP;
    if( !strcasecmp(psz_capability,"video filter") )
        return i_type == 1 ? VIDEO_FILTER_TITLE : VIDEO_FILTER_HELP;

    return " ";
}

/*****************************************************************************
 * VLCPrefs implementation
 *****************************************************************************/
@implementation VLCPrefs

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        o_empty_view = [[[NSView alloc] init] retain];
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
    [o_prefs_view setRulersVisible: NO];
    [o_prefs_view setDocumentView: o_empty_view];
    [o_tree selectRow:0 byExtendingSelection:NO];
    [o_tree expandItem:[o_tree itemAtRow:0]];
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
    // show first tree item
    [[o_prefs_view window] center];
    [[o_prefs_view window] makeKeyAndOrderFront:self];
}

- (IBAction)savePrefs: (id)sender
{
    // walk trough all treeitems and tell them all to save
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
        // show first config treeitem
    }
}

- (IBAction)advancedToggle: (id)sender
{
    b_advanced = !b_advanced;
    [o_advanced_ckb setState: b_advanced];
    // walk trough all treeitems and set advanced state
}

- (void)outlineViewSelectionDidChange:(NSNotification *)o_notification
{
    // a tree item will be shown
}

- (void)showViewForID:(int)i_id
{
}

@end

@implementation VLCPrefs (NSTableDataSource)

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item {
    return (item == nil) ? [[VLCTreeItem rootItem] numberOfChildren] : [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return (item == nil) ? YES : ([item numberOfChildren] >= 0);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item {
    return (item == nil) ? [[VLCTreeItem rootItem] childAtIndex:index] : [item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item {
    return (item == nil) ? @"" : (id)[item name];
}

@end

@implementation VLCTreeItem

static VLCTreeItem *o_root_item = nil;

#define IsALeafNode ((id)-1)

- (id)initWithID: (int)i_id parent: (VLCTreeItem *)o_parent_item
{
    self = [super init];

    if( self != nil )
    {
        i_object_id = i_id;
        o_parent = o_parent_item;
    }
    return( self );
}

+ (VLCTreeItem *)rootItem {
   if (o_root_item == nil) o_root_item = [[VLCTreeItem alloc] initWithID: ROOT_ID parent:nil];
   return o_root_item;       
}

- (void)dealloc
{
    if( psz_help ) free( psz_help );
    if( psz_section ) free( psz_section );
    if (o_name) [o_name release];
    if (o_children != IsALeafNode) [o_children release];
    [super dealloc];
}

/* Creates and returns the array of children
 * Loads children incrementally */
- (NSArray *)children
{
    if (o_children == NULL)
    {
        intf_thread_t   *p_intf = [NSApp getIntf];
        vlc_list_t      *p_list = NULL;
        module_t        *p_module = NULL;
        module_config_t *p_item = NULL;
        int             i_index;

        /* List the modules */
        p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
        if( !p_list ) return nil;

        if( [self objectID] == ROOT_ID )
        {
            /* Create the General Settings and Modules items */
            o_children = [[NSMutableArray alloc] initWithCapacity:2];
            o_name = @"root";
            [o_children addObject:[[VLCTreeItem alloc] initWithID: GENERAL_ID parent:self]];
            [o_children addObject:[[VLCTreeItem alloc] initWithID: MODULE_ID parent:self]];
            [o_children retain];
        }
        else if( [self objectID] == GENERAL_ID )
        {
            /*
             * Build a tree of the main options
             */
            o_name = [_NS("General Settings") copy];
            psz_help = strdup( GENERAL_HELP );
            psz_section = strdup( GENERAL_TITLE );
            
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
                /* Enumerate config categories and store a reference so we can
                 * generate their config panel them when it is asked by the user. */
                p_item = p_module->p_config;
                o_children = [[NSMutableArray alloc] initWithCapacity:10];

                if( p_item ) do
                {
                    VLCTreeItem *o_now;

                    switch( p_item->i_type )
                    {
                    case CONFIG_HINT_CATEGORY:
                        o_now = [[VLCTreeItem alloc] initWithID: p_module->i_object_id parent:self];
                        [o_now setName: [[NSApp localizedString: p_item->psz_text] retain]];
                        [o_children addObject: o_now];
                        break;
                    }
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                [o_children retain];
                //[o_children sortUsingSelector:@selector(caseInsensitiveCompare:)];
            }
        }
        else if( [self objectID] == MODULE_ID )
        {
            int i_counter;
            int i_total;
            BOOL b_found;

            /* Build a list of the capabilities */
            o_name = [_NS("Modules") copy];
            psz_help = strdup( PLUGIN_HELP );
            psz_section = strdup( PLUGIN_TITLE );
            
            o_children = [[NSMutableArray alloc] initWithCapacity:10];
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;
                
                /* Exclude the main module */
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    continue;

                /* Exclude empty modules */
                if( p_module->b_submodule )
                    p_item = ((module_t *)p_module->p_parent)->p_config;
                else
                    p_item = p_module->p_config;
                
                if( !p_item ) continue;
                do
                {
                    if( p_item->i_type & CONFIG_ITEM )
                        break;
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                if( p_item->i_type == CONFIG_HINT_END ) continue;
                
                i_total = [o_children count];
                b_found = FALSE;
                
                for( i_counter = 0; i_counter < i_total; i_counter++ )
                {
                    if( [[[o_children objectAtIndex: i_counter] name] isEqualToString:
                        [NSApp localizedString: p_module->psz_capability]] )
                    {
                        b_found = TRUE;
                        break;
                    }
                }
                
                if( !b_found )
                {
                    VLCTreeItem *o_now = [[VLCTreeItem alloc] initWithID: CAPABILITY_ID parent:self];
                    [o_now setName: [NSApp localizedString: p_module->psz_capability]];
                    [o_children addObject:o_now];
                }
            }
            [o_children retain];
            //[o_children sortUsingSelector:@selector(caseInsensitiveCompare:)];
        }
        else if( [self objectID] == CAPABILITY_ID )
        {
            /* add the modules */
            o_children = [[NSMutableArray alloc] initWithCapacity:3];
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_module = (module_t *)p_list->p_values[i_index].p_object;
                
                /* Exclude the main module */
                if( !strcmp( p_module->psz_object_name, "main" ) )
                    continue;

                /* Exclude empty modules */
                if( p_module->b_submodule )
                    p_item = ((module_t *)p_module->p_parent)->p_config;
                else
                    p_item = p_module->p_config;
                
                if( !p_item ) continue;
                do
                {
                    if( p_item->i_type & CONFIG_ITEM )
                        break;
                }
                while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                if( p_item->i_type == CONFIG_HINT_END ) continue;

                if( [[self name] isEqualToString:
                    [NSApp localizedString: p_module->psz_capability]] )
                {
                    VLCTreeItem *o_now = [[VLCTreeItem alloc] initWithID: p_module->i_object_id parent:self];
                    psz_help = strdup(GetCapabilityHelp( p_module->psz_capability, 1));
                    psz_section = strdup(GetCapabilityHelp( p_module->psz_capability, 2));
                    [o_now setName: [[NSApp localizedString:p_module->psz_object_name] retain]];
                    [o_children addObject:o_now];
                }
            }
            [o_children retain];
        }
        else
        {
            /* all the other stuff are leafs */
            o_children = IsALeafNode;
        }
    }
    return o_children;
}

- (int)objectID
{
    return i_object_id;
}

- (NSString *)name
{
    if( o_name ) return o_name;
}

- (void)setName:(NSString *)a_name;
{
    if( o_name ) [o_name release];
    o_name = [a_name copy];
}

- (VLCTreeItem *)childAtIndex:(int)i_index
{
    return [[self children] objectAtIndex:i_index];
}

- (int)numberOfChildren
{
    id i_tmp = [self children];
    return (i_tmp == IsALeafNode) ? (-1) : (int)[i_tmp count];
}

@end
