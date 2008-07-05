/*****************************************************************************
 * prefs.m: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_config_cat.h>

#import "intf.h"
#import "prefs.h"
#import "simple_prefs.h"
#import "prefs_widgets.h"
#import "vlc_keys.h"

/* /!\ Warning: Unreadable code :/ */

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

    [self initStrings];
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
    [[o_basicFull_matrix cellAtRow:0 column:0] setState: NSOffState];
    [[o_basicFull_matrix cellAtRow:0 column:1] setState: NSOnState];
    
    [o_prefs_window center];
    [o_prefs_window makeKeyAndOrderFront:self];
}

- (void)initStrings
{
    [o_prefs_window setTitle: _NS("Preferences")];
    [o_save_btn setTitle: _NS("Save")];
    [o_cancel_btn setTitle: _NS("Cancel")];
    [o_reset_btn setTitle: _NS("Reset All")];
    [[o_basicFull_matrix cellAtRow: 0 column: 0] setStringValue: _NS("Basic")];
    [[o_basicFull_matrix cellAtRow: 0 column: 1] setStringValue: _NS("All")];
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
        _NS("Beware this will reset the VLC media player preferences.\n"
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
            showView:o_prefs_view];
    }
}

- (IBAction)buttonAction: (id)sender
{
    [o_prefs_window orderOut: self];
    [[o_basicFull_matrix cellAtRow:0 column:0] setState: NSOnState];
    [[o_basicFull_matrix cellAtRow:0 column:1] setState: NSOffState];
    [[[VLCMain sharedInstance] getSimplePreferences] showSimplePrefs];
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
    [[o_tree itemAtRow:[o_tree selectedRow]] showView: o_prefs_view];
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
                            (id)[item childAtIndex:index];
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
        module_t        *p_main_module;
        module_config_t *p_items;
        int             i = 0;
        if( [[self getName] isEqualToString: @"main"] )
        {
            p_main_module = module_GetMainModule( p_intf );
            assert( p_main_module );

            /* We found the main module */
            /* Enumerate config categories and store a reference so we can
             * generate their config panel them when it is asked by the user. */
            VLCTreeItem *p_last_category = NULL;
            unsigned int i_confsize;
            p_items = module_GetConfig( p_main_module, &i_confsize );
            o_children = [[NSMutableArray alloc] initWithCapacity:10];
            for( int i = 0; i < i_confsize; i++ )
            {
                NSString *o_child_name;
                NSString *o_child_title;
                NSString *o_child_help;
                switch( p_items[i].i_type )
                {
                    case CONFIG_CATEGORY:
                        if( p_items[i].value.i == -1 ) break;

                        o_child_name = [[VLCMain sharedInstance]
                            localizedString: config_CategoryNameGet( p_items[i].value.i )];
                        o_child_title = o_child_name;
                        o_child_help = [[VLCMain sharedInstance]
                            localizedString: config_CategoryHelpGet( p_items[i].value.i )];
                        p_last_category = [VLCTreeItem alloc];
                        [o_children addObject:[p_last_category
                            initWithName: o_child_name
                            withTitle: o_child_title
                            withHelp: o_child_help
                            ID: ((vlc_object_t*)p_main_module)->i_object_id
                            parent:self
                            children:[[NSMutableArray alloc]
                                initWithCapacity:10]
                            whithCategory: p_items[i].value.i]];
                        break;
                    case CONFIG_SUBCATEGORY:
                        if( p_items[i].value.i == -1 ) break;

                        if( p_items[i].value.i != SUBCAT_PLAYLIST_GENERAL &&
                            p_items[i].value.i != SUBCAT_VIDEO_GENERAL &&
                            p_items[i].value.i != SUBCAT_INPUT_GENERAL &&
                            p_items[i].value.i != SUBCAT_INTERFACE_GENERAL &&
                            p_items[i].value.i != SUBCAT_SOUT_GENERAL &&
                            p_items[i].value.i != SUBCAT_ADVANCED_MISC &&
                            p_items[i].value.i != SUBCAT_AUDIO_GENERAL )
                        {
                            o_child_name = [[VLCMain sharedInstance]
                                localizedString: config_CategoryNameGet( p_items[i].value.i ) ];
                            o_child_title = o_child_name;
                            o_child_help = [[VLCMain sharedInstance]
                                localizedString: config_CategoryHelpGet( p_items[i].value.i ) ];

                            [p_last_category->o_children
                                addObject:[[VLCTreeItem alloc]
                                initWithName: o_child_name
                                withTitle: o_child_title
                                withHelp: o_child_help
                                ID: ((vlc_object_t*)p_main_module)->i_object_id
                                parent:p_last_category
                                children:[[NSMutableArray alloc]
                                    initWithCapacity:10]
                                whithCategory: p_items[i].value.i]];
                        }

                        break;
                    default:
                        break;
                }
            }

            vlc_object_release( (vlc_object_t *)p_main_module );

            /* List the modules */
            p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
            if( !p_list ) return nil;

            /* Build a tree of the plugins */
            /* Add the capabilities */
            for( i = 0; i < p_list->i_count; i++ )
            {
                unsigned int confsize;
                p_module = (module_t *)p_list->p_values[i].p_object;

                /* Exclude the main module */
                if( module_IsMainModule( p_module ) )
                    continue;

                /* Exclude empty plugins (submodules don't have config */
                /* options, they are stored in the parent module) */
                p_items = module_GetConfig( p_module, &confsize );

                unsigned int j;

                int i_category = -1;
                int i_subcategory = -1;
                bool b_item = false;

                for( j = 0; j < confsize; j++ )
                {
                    if( p_items[j].i_type == CONFIG_CATEGORY )
                        i_category = p_items[j].value.i;
                    else if( p_items[j].i_type == CONFIG_SUBCATEGORY )
                        i_subcategory = p_items[j].value.i;

                    if( p_items[j].i_type & CONFIG_ITEM )
                        b_item = true;
            
                    if( b_item && i_category >= 0 && i_subcategory >= 0 )
                        break;
                }
    
                if( !b_item ) continue;

                /* Find the right category item */

                long cookie;
                bool b_found = false;

                VLCTreeItem* p_category_item, * p_subcategory_item;
                for (j = 0 ; j < [o_children count] ; j++)
                {
                    p_category_item = [o_children objectAtIndex: j];
                    if( p_category_item->i_object_category == i_category )
                    {
                        b_found = true;
                        break;
                    }
                }
                if( !b_found ) continue;

                /* Find subcategory item */
                b_found = false;
                cookie = -1;
                for (j = 0 ; j < [p_category_item->o_children count] ; j++)
                {
                    p_subcategory_item = [p_category_item->o_children
                                            objectAtIndex: j];
                    if( p_subcategory_item->i_object_category == i_subcategory )
                    {
                        b_found = true;
                        break;
                    }
                }
                if( !b_found )
                    p_subcategory_item = p_category_item;

                [p_subcategory_item->o_children addObject:[[VLCTreeItem alloc]
                    initWithName:[[VLCMain sharedInstance]
                        localizedString: module_GetName( p_module, false ) ]
                    withTitle:[[VLCMain sharedInstance]
                        localizedString:  module_GetLongName( p_module ) ]
                    withHelp: @""
                    ID: ((vlc_object_t*)p_module)->i_object_id
                    parent:p_subcategory_item
                    children:IsALeafNode
                    whithCategory: -1]];
                }
            vlc_list_release( p_list );
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

        if( !strcmp( module_GetObjName( p_parser ), psz_module_name ) )
        {
            unsigned int confsize;
            module_GetConfig( p_parser, &confsize );
            BOOL b_has_prefs = confsize != 0;
            vlc_list_release( p_list );
            return( b_has_prefs );
        }
    }

    vlc_list_release( p_list );

    return( NO );
}

- (NSView *)showView:(NSScrollView *)o_prefs_view
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
        module_t        *p_module = NULL;
        module_t        *p_main_module;
        module_config_t *p_items;
        unsigned int confsize;

        o_subviews = [[NSMutableArray alloc] initWithCapacity:10];
        /* Get a pointer to the module */
        if( i_object_category == -1 )
        {
            p_module = (module_t *) vlc_object_get( i_object_id );
            assert( p_module );

            p_items = module_GetConfig( p_module, &confsize );

            for( unsigned int i = 0; i < confsize; i++ )
            {
                switch( p_items[i].i_type )
                {
                    case CONFIG_SUBCATEGORY:
                    case CONFIG_CATEGORY:
                    case CONFIG_SECTION:
                    case CONFIG_HINT_USAGE:
                        break;
                    default:
                    {
                        VLCConfigControl *o_control = nil;
                        o_control = [VLCConfigControl newControl:&p_items[i]
                                                      withView:o_view];
                        if( o_control )
                        {
                            [o_control setAutoresizingMask: NSViewMaxYMargin |
                                NSViewWidthSizable];
                            [o_subviews addObject: o_control];
                        }
                    }
                    break;
                }
            }
            vlc_object_release( (vlc_object_t*)p_module );
        }
        else
        {
            p_main_module = module_GetMainModule( p_intf );
            assert( p_main_module );
            module_config_t *p_items;

            unsigned int i, confsize;
            p_items = module_GetConfig( p_main_module, &confsize );

            /* We need to first, find the right (sub)category,
             * and then abort when we find a new (sub)category. Part of the Ugliness. */
            bool in_right_category = false;
            bool in_subcategory = false;
            bool done = false;
            for( i = 0; i < confsize; i++ )
            {
                if( !p_items[i].i_type )
                {
                    msg_Err( p_intf, "invalid preference item found" );
                    break;
                }

                switch( p_items[i].i_type )
                {
                    case CONFIG_CATEGORY:
                        if(!in_right_category && p_items[i].value.i == i_object_category)
                            in_right_category = true;
                        else if(in_right_category)
                            done = true;
                        break;
                    case CONFIG_SUBCATEGORY:
                        if(!in_right_category && p_items[i].value.i == i_object_category)
                        {
                            in_right_category = true;
                            in_subcategory = true;
                        }
                        else if(in_right_category && in_subcategory)
                            done = true;
                        break;
                    case CONFIG_SECTION:
                    case CONFIG_HINT_USAGE:
                        break;
                    default:
                    {
                        if(!in_right_category) break;

                        VLCConfigControl *o_control = nil;
                        o_control = [VLCConfigControl newControl:&p_items[i]
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
                if( done ) break;
            }
            vlc_object_release( (vlc_object_t*)p_main_module );
        }
    }

    if( o_view != nil )
    {
        int i_lastItem = 0;
        int i_yPos = -2;
        int i_max_label = 0;

        NSEnumerator *enumerator = [o_subviews objectEnumerator];
        VLCConfigControl *o_widget;
        NSRect o_frame;
 
        while( ( o_widget = [enumerator nextObject] ) )
            if( i_max_label < [o_widget getLabelSize] )
                i_max_label = [o_widget getLabelSize];

        enumerator = [o_subviews objectEnumerator];
        while( ( o_widget = [enumerator nextObject] ) )
        {
            int i_widget;

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
