/*****************************************************************************
 * prefs_widgets.m: Preferences controls
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "vlc_keys.h"

#include "intf.h"
#include "prefs_widgets.h"

#define PREFS_WRAP 300
#define OFFSET_RIGHT 20
#define OFFSET_BETWEEN 2

@implementation VLCConfigControl

- (id)initWithFrame: (NSRect)frame
{
    return [self initWithFrame: frame
                    item: nil
                    withObject: nil];
}

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    self = [super initWithFrame: frame];

    if( self != nil )
    {
        p_this = _p_this;
        p_item = _p_item;
        o_label = NULL;
        psz_name = strdup( p_item->psz_name );
        i_type = p_item->i_type;
        b_advanced = p_item->b_advanced;
        [self setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin ];
    }
    return (self);
}

- (void)dealloc
{
    if( o_label ) [o_label release];
    if( psz_name ) free( psz_name );
    [super dealloc];
}


+ (VLCConfigControl *)newControl: (module_config_t *)_p_item withView: (NSView *)o_parent_view withObject: (vlc_object_t *)_p_this offset:(NSPoint) offset
{
    VLCConfigControl *p_control = NULL;
    NSRect frame = [o_parent_view frame];
/*FIXME: Why do we need to divide by two ??? */
    frame.origin.x=offset.x;
    frame.origin.y=offset.y;
    frame.size.width-=OFFSET_RIGHT;
    switch( _p_item->i_type )
    {
#if 0
    case CONFIG_ITEM_MODULE:
        p_control = [[ModuleConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        break;
#endif
    case CONFIG_ITEM_STRING:
        if( !_p_item->i_list )
        {
            p_control = [[StringConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        else
        {
            p_control = [[StringListConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        break;
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
        p_control = [[FileConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        break;
    case CONFIG_ITEM_INTEGER:
        if( _p_item->i_list )
        {
            p_control = [[IntegerListConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        else if( _p_item->i_min != 0 || _p_item->i_max != 0 )
        {
            p_control = [[RangedIntegerConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        else
        {
            p_control = [[IntegerConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        break;
    case CONFIG_ITEM_KEY:
        p_control = [[KeyConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        break;
#if 0
    case CONFIG_ITEM_FLOAT:
        if( _p_item->f_min != 0 || _p_item->f_max != 0 )
        {
            p_control = [[RangedFloatConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        else
        {
            p_control = [[FloatConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        }
        break;

    case CONFIG_ITEM_BOOL:
        p_control = [[BoolConfigControl alloc] initWithFrame: frame item: _p_item withObject: _p_this ];
        break;
#endif        
    default:
        break;
    }
    return p_control;
}

- (NSString *)getName
{
    return [[VLCMain sharedInstance] localizedString: psz_name];
}

- (int)getType
{
    return i_type;
}

- (BOOL)isAdvanced
{
    return b_advanced;
}

- (int)intValue
{
    return 0;
}

- (float)floatValue
{
    return 0;
}

- (char *)stringValue
{
    return NULL;
}

@end

@implementation KeyConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 80;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        unsigned int i;

        o_matrix = [[[NSMatrix alloc] initWithFrame: s_rc mode: NSHighlightModeMatrix cellClass: [NSButtonCell class] numberOfRows:2 numberOfColumns:2] retain];
        NSArray *o_cells = [o_matrix cells];
        for( i = 0; i < [o_cells count]; i++ )
        {
            NSButtonCell *o_current_cell = [o_cells objectAtIndex:i];
            [o_current_cell setButtonType: NSSwitchButton];
            [o_current_cell setControlSize: NSSmallControlSize];
            [o_matrix setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext] toWidth: PREFS_WRAP] forCell: o_current_cell];

            switch( i )
            {
                case 0:
                    [o_current_cell setTitle:_NS("Command")];
                    [o_current_cell setState: p_item->i_value & KEY_MODIFIER_COMMAND];
                    break;
                case 1:
                    [o_current_cell setTitle:_NS("Control")];
                    [o_current_cell setState: p_item->i_value & KEY_MODIFIER_CTRL];
                    break;
                case 2:
                    [o_current_cell setTitle:_NS("Option/Alt")];
                    [o_current_cell setState: p_item->i_value & KEY_MODIFIER_ALT];
                    break;
                case 3:
                    [o_current_cell setTitle:_NS("Shift")];
                    [o_current_cell setState: p_item->i_value & KEY_MODIFIER_SHIFT];
                    break;
            }
        }
        [o_matrix sizeToCells];
        [o_matrix setAutoresizingMask:NSViewMaxXMargin ];
        [self addSubview: o_matrix];

        /* add the combo box */
        s_rc.origin.x += [o_matrix frame].size.width + OFFSET_BETWEEN;
        s_rc.size.height = 22;
        s_rc.size.width = 100;

        o_combo = [[[NSComboBox alloc] initWithFrame: s_rc] retain];
        [o_combo setAutoresizingMask:NSViewMaxXMargin ];
        [o_combo setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext] toWidth: PREFS_WRAP]];
        
        for( i = 0; i < sizeof(vlc_keys) / sizeof(key_descriptor_t); i++ )
        {
            
            if( vlc_keys[i].psz_key_string && *vlc_keys[i].psz_key_string )
            [o_combo addItemWithObjectValue: [[VLCMain sharedInstance] localizedString:vlc_keys[i].psz_key_string]];
        }
        
        [o_combo setStringValue: [[VLCMain sharedInstance] localizedString:KeyToString(( ((unsigned int)p_item->i_value) & ~KEY_MODIFIER ))]];
        [self addSubview: o_combo];
        
        /* add the label */
        s_rc.origin.y += 50;
        
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if ( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
    }
    return self;
}

- (void)dealloc
{
    [o_matrix release];
    [o_combo release];
    [super dealloc];
}

- (int)getIntValue
{
    unsigned int i, i_new_key = 0;
    NSButtonCell *o_current_cell;
    NSArray *o_cells = [o_matrix cells];

    for( i = 0; i < [o_cells count]; i++ )
    {
        o_current_cell = [o_cells objectAtIndex:i];
        if( [[o_current_cell title] isEqualToString:_NS("Command")] && 
            [o_current_cell state] == NSOnState )
        {
            i_new_key |= KEY_MODIFIER_COMMAND;
        }
        if( [[o_current_cell title] isEqualToString:_NS("Control")] && 
            [o_current_cell state] == NSOnState )
        {
            i_new_key |= KEY_MODIFIER_CTRL;
        }
        if( [[o_current_cell title] isEqualToString:_NS("Option/Alt")] && 
            [o_current_cell state] == NSOnState )
        {
            i_new_key |= KEY_MODIFIER_ALT;
        }
        if( [[o_current_cell title] isEqualToString:_NS("Shift")] && 
            [o_current_cell state] == NSOnState )
        {
            i_new_key |= KEY_MODIFIER_SHIFT;
        }
    }
    i_new_key |= StringToKey([[o_combo stringValue] cString]);
    return i_new_key;
}


@end
#if 0

@implementation ModuleConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 20;
    if( self = [super initWithFrame: frame item: p_item
                withObject: _p_this] )
    {
        vlc_list_t *p_list;
        module_t *p_parser;
        NSRect s_rc = frame;
        int i_index;

        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if ( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
        
        /* build the popup */
        s_rc.origin.x = s_rc.size.width - 200 - OFFSET_RIGHT;
        s_rc.size.width = 200;
        
        o_popup = [[[NSPopUpButton alloc] initWithFrame: s_rc] retain];
        [self addSubview: o_popup];
        [o_popup setAutoresizingMask:NSViewMinXMargin ];

        [o_popup setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [o_popup addItemWithTitle: _NS("Default")];
        [[o_popup lastItem] setTag: -1];
        [o_popup selectItem: [o_popup lastItem]];
        
        /* build a list of available modules */
        p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
        for( i_index = 0; i_index < p_list->i_count; i_index++ )
        {
            p_parser = (module_t *)p_list->p_values[i_index].p_object ;

            if( !strcmp( p_parser->psz_capability,
                        p_item->psz_type ) )
            {
                NSString *o_description = [NSApp
                    localizedString: p_parser->psz_longname];
                [o_popup addItemWithTitle: o_description];

                if( p_item->psz_value &&
                    !strcmp( p_item->psz_value, p_parser->psz_object_name ) )
                {
                    [o_popup selectItem:[o_popup lastItem]];
                }
            }
        }
        vlc_list_release( p_list );
    }
    return self;
}

- (void)dealloc
{
    [o_popup release];
    [super dealloc];
}

- (char *)stringValue
{
    NSString *newval = [o_popup stringValue];
    char *returnval;
    int i_index;
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;

    p_item = config_FindConfig( p_this, psz_name );
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_capability,
                    p_item->psz_type ) )
        {
            NSString *o_description = [NSApp
                localizedString: p_parser->psz_longname];
            
            if( [newval isEqualToString: o_description] )
            {
                returnval = strdup(p_parser->psz_object_name);
                break;
            }
        }
    }
    vlc_list_release( p_list );
    return returnval;
}

@end
#endif
@implementation StringConfigControl
- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 22;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        s_rc.size.height = 17;
        s_rc.origin.x = 0;
        s_rc.origin.y = 3;
        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
        
        /* build the textfield */
        s_rc.origin.x = s_rc.size.width - 200 - OFFSET_RIGHT;
        s_rc.origin.y = 0;
        s_rc.size.height = 22;
        s_rc.size.width = 200;
        
        o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield setAutoresizingMask:NSViewMinXMargin | NSViewWidthSizable ];

        [o_textfield setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        if( p_item->psz_value )
            [o_textfield setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_value]];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void)dealloc
{
    [o_textfield release];
    [super dealloc];
}

- (char *)stringValue
{
    return strdup( [NSApp delocalizeString:[o_textfield stringValue]] );
}

@end

@implementation StringListConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 24;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        int i_index;
        s_rc.size.height = 17;
        s_rc.origin.x = 0;
        s_rc.origin.y = 5;

        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
        
        /* build the textfield */
        s_rc.origin.x = s_rc.size.width - 200 - OFFSET_RIGHT;
        s_rc.origin.y = 0;
        s_rc.size.height = 26;
        s_rc.size.width = 200;
        
        o_combo = [[[NSComboBox alloc] initWithFrame: s_rc] retain];
        [o_combo setAutoresizingMask:NSViewMinXMargin | NSViewWidthSizable ];

        [o_combo setUsesDataSource:TRUE];
        [o_combo setDataSource:self];
        [o_combo setNumberOfVisibleItems:10];
        [o_combo setCompletes:YES];
        for( i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            if( p_item->psz_value && !strcmp( p_item->psz_value, p_item->ppsz_list[i_index] ) )
            {
                [o_combo selectItemAtIndex: i_index];
            }
        }

        [o_combo setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_combo];
    }
    return self;
}

- (void)dealloc
{
    [o_combo release];
    [super dealloc];
}

- (char *)stringValue
{
    if( [o_combo indexOfSelectedItem] >= 0 )
        return strdup( p_item->ppsz_list[[o_combo indexOfSelectedItem]] );
    else
        return strdup( [NSApp delocalizeString: [o_combo stringValue]] );
}

@end

@implementation StringListConfigControl (NSComboBoxDataSource)

- (int)numberOfItemsInComboBox:(NSComboBox *)aComboBox
{
        return p_item->i_list;
}

- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(int)i_index
{
    if( p_item->ppsz_list_text && p_item->ppsz_list_text[i_index] )
    {
        return [[VLCMain sharedInstance] localizedString: p_item->ppsz_list_text[i_index]];
    } else return [[VLCMain sharedInstance] localizedString: p_item->ppsz_list[i_index]];
}

@end
@implementation FileConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 53;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        s_rc.size.height = 17;
        s_rc.origin.x = 0;
        s_rc.origin.y = 36;

        /* is it a directory */
        b_directory = ( [self getType] == CONFIG_ITEM_DIRECTORY ) ? YES : NO;

        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
        
        /* build the button */
        s_rc.origin.y = 0;
        s_rc.size.height = 32;

        o_button = [[[NSButton alloc] initWithFrame: s_rc] retain];
        [o_button setButtonType: NSMomentaryPushInButton];
        [o_button setBezelStyle: NSRoundedBezelStyle];
        [o_button setTitle: _NS("Browse...")];
/*TODO: enlarge a bit the button...*/
        [o_button sizeToFit];
        [o_button setAutoresizingMask:NSViewMinXMargin];
        [o_button setFrameOrigin: NSMakePoint( s_rc.size.width - 
                    [o_button frame].size.width - OFFSET_RIGHT / 2, s_rc.origin.y)];
        [o_button setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];

        [o_button setTarget: self];
        [o_button setAction: @selector(openFileDialog:)];

        s_rc.origin.x = 15;
        s_rc.origin.y = 6;
        s_rc.size.height = 22;
        s_rc.size.width = s_rc.size.width - OFFSET_BETWEEN - [o_button frame].size.width - OFFSET_RIGHT / 2 - s_rc.origin.x;
        
        o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        
        if( p_item->psz_value )
            [o_textfield setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_value]];
        [o_textfield setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];

        [o_textfield setAutoresizingMask:NSViewWidthSizable];
               
        [self addSubview: o_textfield];
        [self addSubview: o_button];
    }
    return self;
}

- (void)dealloc
{
    [o_textfield release];
    [o_button release];
    [super dealloc];
}

- (IBAction)openFileDialog: (id)sender
{
    NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];
    
    [o_open_panel setTitle: (b_directory)?_NS("Select a directory"):_NS("Select a file")];
    [o_open_panel setPrompt: _NS("Select")];
    [o_open_panel setAllowsMultipleSelection: NO];
    [o_open_panel setCanChooseFiles: !b_directory];
    [o_open_panel setCanChooseDirectories: b_directory];
    [o_open_panel beginSheetForDirectory:nil
        file:nil
        types:nil
        modalForWindow:[sender window]
        modalDelegate: self
        didEndSelector: @selector(pathChosenInPanel: 
                        withReturn:
                        contextInfo:)
        contextInfo: nil];
}

- (void)pathChosenInPanel:(NSOpenPanel *)o_sheet withReturn:(int)i_return_code contextInfo:(void  *)o_context_info
{
    if( i_return_code == NSOKButton )
    {
        NSString *o_path = [[o_sheet filenames] objectAtIndex: 0];
        [o_textfield setStringValue: o_path];
    }
}

- (char *)stringValue
{
    return strdup( [[o_textfield stringValue] fileSystemRepresentation] );
}

@end

@implementation IntegerConfigControl
- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 25;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        s_rc.size.height = 17;
        s_rc.origin.x = 0;
        s_rc.origin.y = 6;
        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
        
        /* build the stepper */
        s_rc.origin.x = s_rc.size.width - 16 - OFFSET_RIGHT;
        s_rc.origin.y = 0;
        s_rc.size.height = 21;
        
        o_stepper = [[[NSStepper alloc] initWithFrame: s_rc] retain];
        [o_stepper sizeToFit];
        [o_stepper setAutoresizingMask:NSViewMinXMargin];

        [o_stepper setMaxValue: 1600];
        [o_stepper setMinValue: -1600];
        [o_stepper setIntValue: p_item->i_value];
        [o_stepper setTarget: self];
        [o_stepper setAction: @selector(stepperChanged:)];
        [o_stepper sendActionOn:NSLeftMouseUpMask|NSLeftMouseDownMask|NSLeftMouseDraggedMask];
        [o_stepper setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_stepper];
    
        /* build the textfield */
        s_rc.origin.x = s_rc.size.width - 42 - OFFSET_BETWEEN - 19 - OFFSET_RIGHT;
        s_rc.origin.y = 3;
        s_rc.size.width = 42;
        s_rc.size.height = 22;
        
        o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield setAutoresizingMask:NSViewMinXMargin | NSViewWidthSizable ];

        [o_textfield setIntValue: p_item->i_value];
        [o_textfield setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void)dealloc
{
    [o_stepper release];
    [o_textfield release];
    [super dealloc];
}

- (IBAction)stepperChanged:(id)sender
{
    [o_textfield setIntValue: [o_stepper intValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_stepper setIntValue: [o_textfield intValue]];
}

- (int)getIntValue
{
    return [o_stepper intValue];
}

@end

@implementation IntegerListConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 24;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        int i_index;
        s_rc.size.height = 17;
        s_rc.origin.x = 0;
        s_rc.origin.y = 5;

        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];
        
        /* build the textfield */
        s_rc.origin.x = s_rc.size.width - 200 - OFFSET_RIGHT;
        s_rc.origin.y = 0;
        s_rc.size.height = 26;
        s_rc.size.width = 200;
        
        o_combo = [[[NSComboBox alloc] initWithFrame: s_rc] retain];
        [o_combo setAutoresizingMask:NSViewMinXMargin | NSViewWidthSizable ];

        [o_combo setUsesDataSource:TRUE];
        [o_combo setDataSource:self];
        [o_combo setNumberOfVisibleItems:10];
        [o_combo setCompletes:YES];
        for( i_index = 0; i_index < p_item->i_list; i_index++ )
        {
            if( p_item->i_value == p_item->pi_list[i_index] )
            {
                [o_combo selectItemAtIndex: i_index];
            }
        }

        [o_combo setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_combo];
    }
    return self;
}

- (void)dealloc
{
    [o_combo release];
    [super dealloc];
}

- (int)intValue
{
    if( [o_combo indexOfSelectedItem] >= 0 )
        return p_item->pi_list[[o_combo indexOfSelectedItem]];
    else
        return [o_combo intValue];
}

@end

@implementation IntegerListConfigControl (NSComboBoxDataSource)

- (int)numberOfItemsInComboBox:(NSComboBox *)aComboBox
{
    return p_item->i_list;
}

- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(int)i_index
{
    if( p_item->ppsz_list_text && p_item->ppsz_list_text[i_index] )
    {
        return [[VLCMain sharedInstance] localizedString: p_item->ppsz_list_text[i_index]];
    } else return [NSString stringWithFormat: @"%i", p_item->pi_list[i_index]];
}

@end

@implementation RangedIntegerConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)_p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 51;
    if( ( self = [super initWithFrame: frame item: _p_item
                withObject: _p_this] ) )
    {
        NSRect s_rc = frame;
        s_rc.size.height = 17;
        s_rc.origin.x = 0;
        s_rc.origin.y = 32;
        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];

        /* current value textfield */
        s_rc.size.height = 22;
        s_rc.size.width = 40;
        s_rc.origin.x = [o_label frame].size.width + OFFSET_RIGHT;
        s_rc.origin.y = 29;
        o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield setAutoresizingMask:NSViewMinXMargin];

        [o_textfield setIntValue: p_item->i_value];
        [o_textfield setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];

        /* build the slider */
        /* min value textfield */
        s_rc.origin.x = 15;
        s_rc.origin.y = 0;
        s_rc.size.width = 40;

        o_textfield_min = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield_min setAutoresizingMask:NSViewMaxXMargin];
        [o_textfield_min setDrawsBackground: NO];
        [o_textfield_min setBordered: NO];
        [o_textfield_min setEditable: NO];
        [o_textfield_min setSelectable: NO];
        [o_textfield_min setIntValue: p_item->i_min];
        [o_textfield_min setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_textfield_min];

        /* max value textfield */
        s_rc.size.width = 40;
        s_rc.origin.x = [self bounds].size.width - OFFSET_RIGHT - 40;
        
        o_textfield_max = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield_max setAutoresizingMask:NSViewMinXMargin ];

        [o_textfield_max setDrawsBackground: NO];
        [o_textfield_max setBordered: NO];
        [o_textfield_max setEditable: NO];
        [o_textfield_max setSelectable: NO];
        [o_textfield_max setAlignment: NSRightTextAlignment];

        [o_textfield_max setIntValue: p_item->i_max];
        [o_textfield_max setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_textfield_max];
        
        /* the slider */
        s_rc.size.height = 21;
        s_rc.size.width = [self bounds].size.width - OFFSET_RIGHT - 2*OFFSET_BETWEEN - 2 * 40;
        s_rc.origin.x = 40 + OFFSET_BETWEEN;
        s_rc.origin.y = 1;

        o_slider = [[[NSSlider alloc] initWithFrame: s_rc] retain];
        [o_slider setAutoresizingMask:NSViewWidthSizable];

        [o_slider setMaxValue: p_item->i_max];
        [o_slider setMinValue: p_item->i_min];
        [o_slider setIntValue: p_item->i_value];
        [o_slider setTarget: self];
        [o_slider setAction: @selector(sliderChanged:)];
        [o_slider sendActionOn:NSLeftMouseUpMask|NSLeftMouseDownMask|NSLeftMouseDraggedMask];
        [o_slider setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_slider];
       
    }
    return self;
}

- (void)dealloc
{
    [o_textfield release];
    [o_textfield_min release];
    [o_textfield_max release];
    [o_slider release];
    [super dealloc];
}

- (IBAction)sliderChanged:(id)sender
{
    [o_textfield setIntValue: [o_slider intValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_slider setIntValue: [o_textfield intValue]];
}

- (int)intValue
{
    return [o_slider intValue];
}

@end
#if 0

@implementation FloatConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 20;
    if( self = [super initWithFrame: frame item: p_item
                withObject: _p_this] )
    {
        NSRect s_rc = frame;

        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];

        /* build the textfield */
        s_rc.origin.x = s_rc.size.width - 60 - OFFSET_RIGHT;
        s_rc.size.width = 60;
        
        o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield setAutoresizingMask:NSViewMinXMargin | NSViewWidthSizable ];

        [o_textfield setFloatValue: p_item->f_value];
        [o_textfield setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_textfield];
    }
    return self;
}

- (void)dealloc
{
    [o_textfield release];
    [super dealloc];
}

- (float)floatValue
{
    return [o_textfield floatValue];
}

@end

@implementation RangedFloatConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 50;
    if( self = [super initWithFrame: frame item: p_item
                withObject: _p_this] )
    {
        NSRect s_rc = frame;
        s_rc.size.height = 20;
        s_rc.origin.y = 30;
    
        /* add the label */
        o_label = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_label setDrawsBackground: NO];
        [o_label setBordered: NO];
        [o_label setEditable: NO];
        [o_label setSelectable: NO];
        if( p_item->psz_text )
            [o_label setStringValue: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];

        [o_label sizeToFit];
        [self addSubview: o_label];
        [o_label setAutoresizingMask:NSViewMaxXMargin ];

        /* build the slider */
        /* min value textfield */
        s_rc.origin.y = 0;
        s_rc.origin.x = 0;
        s_rc.size.width = 40;

        o_textfield_min = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield_min setAutoresizingMask:NSViewMaxXMargin];
        [o_textfield_min setDrawsBackground: NO];
        [o_textfield_min setBordered: NO];
        [o_textfield_min setEditable: NO];
        [o_textfield_min setSelectable: NO];

        [o_textfield_min setFloatValue: p_item->f_min];
        [o_textfield_min setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_textfield_min];

        /* the slider */
        s_rc.size.width = [self bounds].size.width - OFFSET_RIGHT - 2*OFFSET_BETWEEN - 3*40;
        s_rc.origin.x = 40 + OFFSET_BETWEEN;

        o_slider = [[[NSStepper alloc] initWithFrame: s_rc] retain];
        [o_slider setAutoresizingMask:NSViewWidthSizable];

        [o_slider setMaxValue: p_item->f_max];
        [o_slider setMinValue: p_item->f_min];
        [o_slider setFloatValue: p_item->f_value];
        [o_slider setTarget: self];
        [o_slider setAction: @selector(sliderChanged:)];
        [o_slider sendActionOn:NSLeftMouseUpMask|NSLeftMouseDownMask|NSLeftMouseDraggedMask];
        [o_slider setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_slider];
        
        /* max value textfield */
        s_rc.size.width = 40;
        s_rc.origin.x = [self bounds].size.width - OFFSET_RIGHT - OFFSET_BETWEEN - 2*40;
        
        o_textfield_max = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield_max setAutoresizingMask:NSViewMinXMargin ];

        [o_textfield_max setDrawsBackground: NO];
        [o_textfield_max setBordered: NO];
        [o_textfield_max setEditable: NO];
        [o_textfield_max setSelectable: NO];

        [o_textfield_max setFloatValue: p_item->f_max];
        [o_textfield_max setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_textfield_max];
        
        /* current value textfield */
        s_rc.size.width = 40;
        s_rc.origin.x = [self bounds].size.width - OFFSET_RIGHT - 40;

        o_textfield = [[[NSTextField alloc] initWithFrame: s_rc] retain];
        [o_textfield setAutoresizingMask:NSViewMinXMargin];

        [o_textfield setFloatValue: p_item->f_value];
        [o_textfield setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [o_textfield setDelegate: self];
        [[NSNotificationCenter defaultCenter] addObserver: self
            selector: @selector(textfieldChanged:)
            name: NSControlTextDidChangeNotification
            object: o_textfield];
        [self addSubview: o_textfield];

    }
    return self;
}

- (void)dealloc
{
    [o_textfield release];
    [o_textfield_min release];
    [o_textfield_max release];
    [o_slider release];
    [super dealloc];
}

- (IBAction)sliderChanged:(id)sender
{
    [o_textfield setFloatValue: [o_slider floatValue]];
}

- (void)textfieldChanged:(NSNotification *)o_notification
{
    [o_slider setFloatValue: [o_textfield floatValue]];
}

- (float)floatValue
{
    return [o_slider floatValue];
}

@end


@implementation BoolConfigControl

- (id)initWithFrame: (NSRect)frame
        item: (module_config_t *)p_item
        withObject: (vlc_object_t *)_p_this
{
    frame.size.height = 20;
    if( self = [super initWithFrame: frame item: p_item
            withObject: _p_this] )
    {
        NSRect s_rc = frame;
        s_rc.size.height = 20;
        
        o_checkbox = [[[NSButton alloc] initWithFrame: s_rc] retain];
        [o_checkbox setButtonType: NSSwitchButton];
        [o_checkbox setIntValue: p_item->i_value];
        [o_checkbox setTitle: [[VLCMain sharedInstance] localizedString: p_item->psz_text]];
        [o_checkbox setToolTip: [[VLCMain sharedInstance] wrapString: [[VLCMain sharedInstance] localizedString: p_item->psz_longtext ] toWidth: PREFS_WRAP]];
        [self addSubview: o_checkbox];
    }
    return self;
}

- (void)dealloc
{
    [o_checkbox release];
    [super dealloc];
}

- (int)intValue
{
    [o_checkbox intValue];
}

@end
#endif

