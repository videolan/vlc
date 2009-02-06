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

@interface VLCTreeItem : NSObject
{
    NSString *_name;
    NSMutableArray *_children;
    NSMutableArray *_subviews;
    module_config_t * _configItem;
}
- (id)initWithConfigItem:(module_config_t *)configItem;

- (id)initWithName:(NSString*)name andConfigItem:(module_config_t *)configItem;

- (int)numberOfChildren;
- (VLCTreeItem *)childAtIndex:(int)i_index;

- (module_config_t *)configItem;

- (NSString *)name;
- (NSMutableArray *)children;
- (void)showView:(NSScrollView *)o_prefs_view;
- (void)applyChanges;
- (void)resetView;

@end

@interface VLCTreeSubCategoryItem : VLCTreeItem
{
    int _subCategory;
}
+ (VLCTreeSubCategoryItem *)subCategoryTreeItemWithSubCategory:(int)category;
- (id)initWithSubCategory:(int)subCategory;
- (int)subCategory;
@end

@interface VLCTreeCategoryItem : VLCTreeItem
{
    int _category;
}
+ (VLCTreeCategoryItem *)categoryTreeItemWithCategory:(int)category;
- (id)initWithCategory:(int)category;
- (int)category;
- (VLCTreeSubCategoryItem *)itemRepresentingSubCategory:(int)category;
@end


@interface VLCTreeLeafItem : VLCTreeItem
{ }
@end

@interface VLCTreeMainItem : VLCTreeItem
{
    module_config_t * _configItems;
}
- (VLCTreeCategoryItem *)itemRepresentingCategory:(int)category;
@end

#pragma mark -

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
        _rootTreeItem = [[VLCTreeMainItem alloc] init];
    }

    return _o_sharedMainInstance;
}

- (void)dealloc
{
    [o_empty_view release];
    [_rootTreeItem release];
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
    [_rootTreeItem applyChanges];
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
        [_rootTreeItem resetView];
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

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return (item == nil) ? [_rootTreeItem numberOfChildren] : [item numberOfChildren];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return ![item isKindOfClass:[VLCTreeSubCategoryItem class]];
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    return (item == nil) ? (id)[_rootTreeItem childAtIndex:index]: (id)[item childAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView
    objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    return (item == nil) ? @"" : [item name];
}

@end

#pragma mark -
@implementation VLCTreeMainItem

- (VLCTreeCategoryItem *)itemRepresentingCategory:(int)category
{
    for( int i = 0; i < [[self children] count]; i++ )
    {
        VLCTreeCategoryItem * categoryItem = [[self children] objectAtIndex:i];
        if( [categoryItem category] == category )
            return categoryItem;
    }
    return nil;
}

- (void)dealloc
{
    module_config_free( _configItems );
    [super dealloc];
}

/* Creates and returns the array of children
 * Loads children incrementally */
- (NSMutableArray *)children
{
    if( _children ) return _children;
    _children = [[NSMutableArray alloc] init];

    intf_thread_t   *p_intf = VLCIntf;

    /* List the modules */
    size_t count, i;
    module_t ** modules = module_list_get( &count );
    if( !modules ) return nil;

    /* Build a tree of the plugins */
    /* Add the capabilities */
    for( i = 0; i < count; i++ )
    {
        module_t * p_module = modules[i];

        /* Exclude empty plugins (submodules don't have config */
        /* options, they are stored in the parent module) */
        unsigned int confsize;
        _configItems = module_config_get( p_module, &confsize );

        VLCTreeCategoryItem * categoryItem = nil;
        VLCTreeSubCategoryItem * subCategoryItem = nil;

        unsigned int j;
        for( j = 0; j < confsize; j++ )
        {
            int configType = _configItems[j].i_type;
            if( configType == CONFIG_CATEGORY )
            {
                categoryItem = [self itemRepresentingCategory:_configItems[j].value.i];
                if(!categoryItem)
                {
                    categoryItem = [VLCTreeCategoryItem categoryTreeItemWithCategory:_configItems[j].value.i];
                    if(categoryItem) [[self children] addObject:categoryItem];
                }
            }
            else if( configType == CONFIG_SUBCATEGORY && categoryItem )
            {
                subCategoryItem = [categoryItem itemRepresentingSubCategory:_configItems[j].value.i];
                if(!subCategoryItem)
                {
                    subCategoryItem = [VLCTreeSubCategoryItem subCategoryTreeItemWithSubCategory:_configItems[j].value.i];
                    if(subCategoryItem) [[categoryItem children] addObject:subCategoryItem];
                }
            }
            else if( (configType & CONFIG_ITEM) && subCategoryItem )
            {
                [[subCategoryItem children] addObject:[[VLCTreeLeafItem alloc] initWithConfigItem:&_configItems[j]]];
            }

        }
    }
    module_list_free( modules );
    return _children;
}
@end

#pragma mark -
@implementation VLCTreeCategoryItem
+ (VLCTreeCategoryItem *)categoryTreeItemWithCategory:(int)category
{
    return [[[[self class] alloc] initWithCategory:category] autorelease];
}
- (id)initWithCategory:(int)category
{
    if(!config_CategoryNameGet( category )) return nil;
    NSString * name = [[VLCMain sharedInstance] localizedString: config_CategoryNameGet( category )];
    if(self = [super initWithName:name andConfigItem:nil])
    {
        _category = category;
        //_help = [[[VLCMain sharedInstance] localizedString: config_CategoryHelpGet( category )] retain];
    }
    return self;
}

- (VLCTreeSubCategoryItem *)itemRepresentingSubCategory:(int)subCategory
{
    assert( [self isKindOfClass:[VLCTreeCategoryItem class]] );
    for( int i = 0; i < [[self children] count]; i++ )
    {
        VLCTreeSubCategoryItem * subCategoryItem = [[self children] objectAtIndex:i];
        if( [subCategoryItem subCategory] == subCategory )
            return subCategoryItem;
    }
    return nil;
}

- (int)category
{
    return _category;
}
@end

#pragma mark -
@implementation VLCTreeSubCategoryItem
- (id)initWithSubCategory:(int)subCategory
{
    if(!config_CategoryNameGet( subCategory )) return nil;
    NSString * name = [[VLCMain sharedInstance] localizedString: config_CategoryNameGet( subCategory )];
    if(self = [super initWithName:name andConfigItem:NULL])
    {
        _subCategory = subCategory;
        //_help = [[[VLCMain sharedInstance] localizedString: config_CategoryHelpGet( subCategory )] retain];
    }
    return self;
}

+ (VLCTreeSubCategoryItem *)subCategoryTreeItemWithSubCategory:(int)subCategory
{
    return [[[[self class] alloc] initWithSubCategory:subCategory] autorelease];
}

- (int)subCategory
{
    return _subCategory;
}

@end

#pragma mark -
@implementation VLCTreeLeafItem
@end

#pragma mark -
#pragma mark (Root class for all TreeItems)
@implementation VLCTreeItem

- (id)initWithConfigItem: (module_config_t *) configItem
{
    NSString * name = [[VLCMain sharedInstance] localizedString:configItem->psz_name];
    return [self initWithName:name andConfigItem:configItem];
}

- (id)initWithName:(NSString*)name andConfigItem:(module_config_t *)configItem
{
    self = [super init];
    if( self != nil )
    {
        _name = [name retain];
        _configItem = configItem;
    }
    return self;
}

- (void)dealloc
{
    [_children release];
    [_name release];
    [_subviews release];
    [super dealloc];
}

- (VLCTreeItem *)childAtIndex:(int)i_index
{
    return [[self children] objectAtIndex:i_index];
}

- (int)numberOfChildren
{
    return [[self children] count];
}

- (NSString *)name
{
    return [[_name retain] autorelease];
}

- (void)showView:(NSScrollView *)prefsView
{
    NSRect          s_vrc;
    NSView          *view;

    [[VLCPrefs sharedInstance] setTitle: [self name]];
    s_vrc = [[prefsView contentView] bounds]; s_vrc.size.height -= 4;
    view = [[VLCFlippedView alloc] initWithFrame: s_vrc];
    [view setAutoresizingMask: NSViewWidthSizable | NSViewMinYMargin | NSViewMaxYMargin];

    if(!_subviews)
    {
        _subviews = [[NSMutableArray alloc] initWithCapacity:10];

        long i;
        for( i = 0; i < [[self children] count]; i++)
        {
            VLCTreeItem * item = [[self children] objectAtIndex:i];
            if(![item isKindOfClass:[VLCTreeLeafItem class]]) continue;

            VLCConfigControl *control;
            control = [VLCConfigControl newControl:[item configItem] withView:view];
            if( control )
            {
                [control setAutoresizingMask: NSViewMaxYMargin | NSViewWidthSizable];
                [_subviews addObject: control];
            }
        }
    }

    assert(view);
    
    int i_lastItem = 0;
    int i_yPos = -2;
    int i_max_label = 0;

    NSEnumerator *enumerator = [_subviews objectEnumerator];
    VLCConfigControl *widget;
    NSRect frame;

    while( ( widget = [enumerator nextObject] ) )
        if( i_max_label < [widget getLabelSize] )
            i_max_label = [widget getLabelSize];

    enumerator = [_subviews objectEnumerator];
    while( ( widget = [enumerator nextObject] ) )
    {
        int i_widget;

        i_widget = [widget getViewType];
        i_yPos += [VLCConfigControl calcVerticalMargin:i_widget lastItem:i_lastItem];
        [widget setYPos:i_yPos];
        frame = [widget frame];
        frame.size.width = [view frame].size.width - LEFTMARGIN - RIGHTMARGIN;
        [widget setFrame:frame];
        [widget alignWithXPosition: i_max_label];
        i_yPos += [widget frame].size.height;
        i_lastItem = i_widget;
        [view addSubview:widget];
    }

    frame = [view frame];
    frame.size.height = i_yPos;
    [view setFrame:frame];
    [prefsView setDocumentView:view];
}

- (void)applyChanges
{
    unsigned int i;
    for( i = 0 ; i < [_subviews count] ; i++ )
        [[_subviews objectAtIndex:i] applyChanges];

    for( i = 0 ; i < [_children count] ; i++ )
        [[_children objectAtIndex:i] applyChanges];
}

- (void)resetView
{
    [_subviews release];
    _subviews = nil;

    unsigned int i;
    for( i = 0 ; i < [_children count] ; i++ )
        [[_children objectAtIndex:i] resetView];
}

- (NSMutableArray *)children
{
    if(!_children) _children = [[NSMutableArray alloc] init];
    return _children;
}

- (module_config_t *)configItem
{
    return _configItem;
}
@end

#pragma mark -
@implementation VLCFlippedView

- (BOOL)isFlipped
{
    return( YES );
}

@end
