/*****************************************************************************
 * controls.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Benjamin Pracht <bigben at videolan doit org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include "intf.h"
#include "vout.h"
#include "open.h"
#include "controls.h"
#include <vlc_osd.h>


/*****************************************************************************
 * VLCControls implementation 
 *****************************************************************************/
@implementation VLCControls

- (void)awakeFromNib
{
    [o_specificTime_mi setTitle: _NS("Jump To Time")];
    [o_specificTime_cancel_btn setTitle: _NS("Cancel")];
    [o_specificTime_ok_btn setTitle: _NS("OK")];
    [o_specificTime_sec_lbl setStringValue: _NS("sec.")];
    [o_specificTime_goTo_lbl setStringValue: _NS("Jump to time")];
}

- (IBAction)play:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                        FIND_ANYWHERE );
    if( p_playlist )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        if( p_playlist->i_size <= 0 )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            vlc_object_release( p_playlist );
            [o_main intfOpenFileGeneric: (id)sender];
        }
        else
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            vlc_object_release( p_playlist );
        }

    }
    val.i_int = config_GetInt( p_intf, "key-play-pause" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

/* Small helper method */

-(id) getVoutView
{
    id o_window;
    id o_vout_view = nil;
    id o_embedded_vout_list = [[VLCMain sharedInstance] getEmbeddedList];
    NSEnumerator *o_enumerator = [[NSApp orderedWindows] objectEnumerator];
    while( !o_vout_view && ( o_window = [o_enumerator nextObject] ) )
    {
        /* We have an embedded vout */
        if( [o_embedded_vout_list windowContainsEmbedded: o_window] )
        {
            o_vout_view = [o_embedded_vout_list getViewForWindow: o_window];
        }
        /* We have a detached vout */
        else if( [[o_window className] isEqualToString: @"VLCWindow"] )
        {
            msg_Dbg( VLCIntf, "detached vout controls.m call getVoutView" );
            o_vout_view = [o_window getVoutView];
        }
    }
    return o_vout_view;
}


- (IBAction)stop:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-stop" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

- (IBAction)faster:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-faster" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

- (IBAction)slower:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-slower" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

- (IBAction)prev:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-prev" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

- (IBAction)next:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-next" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

- (IBAction)random:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    var_Get( p_playlist, "random", &val );
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "random", val );
    if( val.b_bool )
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Random On" ) );
    }
    else
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Random Off" ) );
    }

    p_intf->p_sys->b_playmode_update = VLC_TRUE;
    p_intf->p_sys->b_intf_update = VLC_TRUE;
    vlc_object_release( p_playlist );
}

- (IBAction)repeat:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    var_Get( p_playlist, "repeat", &val );
    if (!val.b_bool)
    {
        var_Set( p_playlist, "loop", val );
    }
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "repeat", val );
    if( val.b_bool )
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat One" ) );
    }
    else
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat Off" ) );
    }

    p_intf->p_sys->b_playmode_update = VLC_TRUE;
    p_intf->p_sys->b_intf_update = VLC_TRUE;
    vlc_object_release( p_playlist );
}

- (IBAction)loop:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    var_Get( p_playlist, "loop", &val );
    if (!val.b_bool)
    {
        var_Set( p_playlist, "repeat", val );
    }
    val.b_bool = !val.b_bool;
    var_Set( p_playlist, "loop", val );
    if( val.b_bool )
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat All" ) );
    }
    else
    {
        vout_OSDMessage( p_intf, DEFAULT_CHAN, _( "Repeat Off" ) );
    }

    p_intf->p_sys->b_playmode_update = VLC_TRUE;
    p_intf->p_sys->b_intf_update = VLC_TRUE;
    vlc_object_release( p_playlist );
}

- (IBAction)forward:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-jump+short" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}

- (IBAction)backward:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-jump-short" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
}


- (IBAction)volumeUp:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-vol-up" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)volumeDown:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-vol-down" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)mute:(id)sender
{
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    val.i_int = config_GetInt( p_intf, "key-vol-mute" );
    var_Set( p_intf->p_vlc, "key-pressed", val );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)volumeSliderUpdated:(id)sender
{
    intf_thread_t * p_intf = VLCIntf;
    audio_volume_t i_volume = (audio_volume_t)[sender intValue];
    int i_volume_step = 0;
    i_volume_step = config_GetInt( p_intf->p_vlc, "volume-step" );
    aout_VolumeSet( p_intf, i_volume * i_volume_step );
    /* Manage volume status */
    [o_main manageVolumeSlider];
}

- (IBAction)windowAction:(id)sender
{
    NSString *o_title = [sender title];

    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
    if( p_vout != NULL )
    {
        id o_vout_view = [self getVoutView];
        if( o_vout_view )
        {
            if( [o_title isEqualToString: _NS("Half Size") ] )
                [o_vout_view scaleWindowWithFactor: 0.5];
            else if( [o_title isEqualToString: _NS("Normal Size") ] )
                [o_vout_view scaleWindowWithFactor: 1.0];
            else if( [o_title isEqualToString: _NS("Double Size") ] )
                [o_vout_view scaleWindowWithFactor: 2.0];
            else if( [o_title isEqualToString: _NS("Float on Top") ] )
                [o_vout_view toggleFloatOnTop];
            else if( [o_title isEqualToString: _NS("Fit to Screen") ] )
            {
                id o_window = [o_vout_view getWindow];
                if( ![o_window isZoomed] )
                    [o_window performZoom:self];
            }
            else if( [o_title isEqualToString: _NS("Snapshot") ] )
            {
                [o_vout_view snapshot];
            }
            else
            {
                [o_vout_view toggleFullscreen];
            }
        }
        vlc_object_release( (vlc_object_t *)p_vout );
    }
    else
    {
        playlist_t * p_playlist = vlc_object_find( VLCIntf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );

        if( p_playlist && ( [o_title isEqualToString: _NS("Fullscreen")] ||
            [sender isKindOfClass:[NSButton class]] ) )
        {
            vlc_value_t val;
            var_Get( p_playlist, "fullscreen", &val );
            var_Set( p_playlist, "fullscreen", (vlc_value_t)!val.b_bool );
        }
        if( p_playlist ) vlc_object_release( (vlc_object_t *)p_playlist );
    }

}

- (BOOL)keyEvent:(NSEvent *)o_event
{
    BOOL eventHandled = NO;
    unichar key = [[o_event charactersIgnoringModifiers] characterAtIndex: 0];

    if( key )
    {
        vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
        if( p_vout != NULL )
        {
            /* Escape */
            if( key == (unichar) 0x1b )
            {
                id o_vout_view = [self getVoutView];
                if( o_vout_view && [o_vout_view isFullscreen] )
                {
                    [o_vout_view toggleFullscreen];
                    eventHandled = YES;
                }
            }
            else if( key == ' ' )
            {
                [self play:self];
                eventHandled = YES;
            }
            vlc_object_release( (vlc_object_t *)p_vout );
        }
    }
    return eventHandled;
}

- (void)setupVarMenuItem:(NSMenuItem *)o_mi
                    target:(vlc_object_t *)p_object
                    var:(const char *)psz_variable
                    selector:(SEL)pf_callback
{
    vlc_value_t val, text;
    int i_type = var_Type( p_object, psz_variable );

    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
    case VLC_VAR_BOOL:
    case VLC_VAR_VARIABLE:
    case VLC_VAR_STRING:
    case VLC_VAR_INTEGER:
        break;
    default:
        /* Variable doesn't exist or isn't handled */
        return;
    }
    
    /* Make sure we want to display the variable */
    if( i_type & VLC_VAR_HASCHOICE )
    {
        var_Change( p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 ) return;
        if( (i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE && val.i_int == 1 )
            return;
    }
    
    /* Get the descriptive name of the variable */
    var_Change( p_object, psz_variable, VLC_VAR_GETTEXT, &text, NULL );
    [o_mi setTitle: [[VLCMain sharedInstance] localizedString: text.psz_string ?
                                        text.psz_string : strdup( psz_variable ) ]];

    var_Get( p_object, psz_variable, &val );
    if( i_type & VLC_VAR_HASCHOICE )
    {
        NSMenu *o_menu = [o_mi submenu];

        [self setupVarMenu: o_menu forMenuItem: o_mi target:p_object
                        var:psz_variable selector:pf_callback];
        
        if( text.psz_string ) free( text.psz_string );
        return;
    }

    VLCMenuExt *o_data;
    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
        o_data = [[VLCMenuExt alloc] initWithVar: psz_variable Object: p_object->i_object_id
                Value: val ofType: i_type];
        [o_mi setRepresentedObject: [NSValue valueWithPointer:[o_data retain]]];
        break;

    case VLC_VAR_BOOL:
        o_data = [[VLCMenuExt alloc] initWithVar: psz_variable Object: p_object->i_object_id
                Value: val ofType: i_type];
        [o_mi setRepresentedObject: [NSValue valueWithPointer:[o_data retain]]];
        if( !( i_type & VLC_VAR_ISCOMMAND ) )
            [o_mi setState: val.b_bool ? TRUE : FALSE ];
        break;

    default:
        if( text.psz_string ) free( text.psz_string );
        return;
    }

    if( ( i_type & VLC_VAR_TYPE ) == VLC_VAR_STRING ) free( val.psz_string );
    if( text.psz_string ) free( text.psz_string );
}


- (void)setupVarMenu:(NSMenu *)o_menu
                    forMenuItem: (NSMenuItem *)o_parent
                    target:(vlc_object_t *)p_object
                    var:(const char *)psz_variable
                    selector:(SEL)pf_callback
{
    vlc_value_t val, val_list, text_list;
    int i_type, i, i_nb_items;

    /* remove previous items */
    i_nb_items = [o_menu numberOfItems];
    for( i = 0; i < i_nb_items; i++ )
    {
        [o_menu removeItemAtIndex: 0];
    }

    /* Check the type of the object variable */
    i_type = var_Type( p_object, psz_variable );

    /* Make sure we want to display the variable */
    if( i_type & VLC_VAR_HASCHOICE )
    {
        var_Change( p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 ) return;
        if( (i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE && val.i_int == 1 )
            return;
    }
    else
    {
        return;
    }

    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
    case VLC_VAR_BOOL:
    case VLC_VAR_VARIABLE:
    case VLC_VAR_STRING:
    case VLC_VAR_INTEGER:
        break;
    default:
        /* Variable doesn't exist or isn't handled */
        return;
    }

    if( var_Get( p_object, psz_variable, &val ) < 0 )
    {
        return;
    }

    if( var_Change( p_object, psz_variable, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING ) free( val.psz_string );
        return;
    }

    /* make (un)sensitive */
    [o_parent setEnabled: ( val_list.p_list->i_count > 1 )];

    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        vlc_value_t another_val;
        NSMenuItem * o_lmi;
        NSString *o_title = @"";
        VLCMenuExt *o_data;

        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_STRING:
            another_val.psz_string =
                strdup(val_list.p_list->p_values[i].psz_string);

            o_title = [[VLCMain sharedInstance] localizedString: text_list.p_list->p_values[i].psz_string ?
                text_list.p_list->p_values[i].psz_string : val_list.p_list->p_values[i].psz_string ];

            o_lmi = [o_menu addItemWithTitle: o_title action: pf_callback keyEquivalent: @""];
            o_data = [[VLCMenuExt alloc] initWithVar: strdup(psz_variable) Object: p_object->i_object_id
                    Value: another_val ofType: i_type];
            [o_lmi setRepresentedObject: [NSValue valueWithPointer:[o_data retain]]];
            [o_lmi setTarget: self];

            if( !strcmp( val.psz_string, val_list.p_list->p_values[i].psz_string ) && !( i_type & VLC_VAR_ISCOMMAND ) )
                [o_lmi setState: TRUE ];

            break;

        case VLC_VAR_INTEGER:

             o_title = text_list.p_list->p_values[i].psz_string ?
                                 [[VLCMain sharedInstance] localizedString: strdup( text_list.p_list->p_values[i].psz_string )] :
                                 [NSString stringWithFormat: @"%d",
                                 val_list.p_list->p_values[i].i_int];

            o_lmi = [[o_menu addItemWithTitle: o_title action: pf_callback keyEquivalent: @""] retain ];
            o_data = [[VLCMenuExt alloc] initWithVar: strdup(psz_variable) Object: p_object->i_object_id
                    Value: val_list.p_list->p_values[i] ofType: i_type];
            [o_lmi setRepresentedObject: [NSValue valueWithPointer:[ o_data retain]]];
            [o_lmi setTarget: self];

            if( val_list.p_list->p_values[i].i_int == val.i_int && !( i_type & VLC_VAR_ISCOMMAND ) )
                [o_lmi setState: TRUE ];
            break;

        default:
          break;
        }
    }

    /* clean up everything */
    if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING ) free( val.psz_string );
    var_Change( p_object, psz_variable, VLC_VAR_FREELIST, &val_list, &text_list );
}

- (IBAction)toggleVar:(id)sender
{
    NSMenuItem *o_mi = (NSMenuItem *)sender;
    VLCMenuExt *o_data = [[o_mi representedObject] pointerValue];
    [NSThread detachNewThreadSelector: @selector(toggleVarThread:)
        toTarget: self withObject: o_data];

    return;
}

- (int)toggleVarThread: (id)_o_data
{
    vlc_object_t *p_object;
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    VLCMenuExt *o_data = (VLCMenuExt *)_o_data;

    vlc_thread_set_priority( VLCIntf , VLC_THREAD_PRIORITY_LOW );

    p_object = (vlc_object_t *)vlc_object_get( VLCIntf,
                                    [o_data objectID] );

    if( p_object != NULL )
    {
        var_Set( p_object, strdup([o_data name]), [o_data value] );
        vlc_object_release( p_object );
        [o_pool release];
        return VLC_TRUE;
    }
    [o_pool release];
    return VLC_EGENERIC;
}

- (IBAction)goToSpecificTime:(id)sender
{
    if( sender == o_specificTime_cancel_btn )
    {
        [NSApp endSheet: o_specificTime_win];
        [o_specificTime_win close];
    }
    else if( sender == o_specificTime_ok_btn )
    {
        input_thread_t * p_input = (input_thread_t *)vlc_object_find( VLCIntf, \
            VLC_OBJECT_INPUT, FIND_ANYWHERE );
        if( p_input )
        {
            unsigned int timeInSec = 0;
            NSString * fieldContent = [o_specificTime_enter_fld stringValue];
            if( [[fieldContent componentsSeparatedByString: @":"] count] > 1 && 
                [[fieldContent componentsSeparatedByString: @":"] count] <= 3 )
            {
                NSArray * ourTempArray = \
                    [fieldContent componentsSeparatedByString: @":"];

                if( [[fieldContent componentsSeparatedByString: @":"] count] == 3 )
                {
                    timeInSec += ([[ourTempArray objectAtIndex: 0] intValue] * 3600); //h
                    timeInSec += ([[ourTempArray objectAtIndex: 1] intValue] * 60); //m
                    timeInSec += [[ourTempArray objectAtIndex: 2] intValue];        //s
                }
                else
                {
                    timeInSec += ([[ourTempArray objectAtIndex: 0] intValue] * 60); //m
                    timeInSec += [[ourTempArray objectAtIndex: 1] intValue]; //s
                }
            }
            else
                timeInSec = [fieldContent intValue];

            input_Control( p_input, INPUT_SET_TIME, (int64_t)(timeInSec * 1000000));
            vlc_object_release( p_input );
        }
    
        [NSApp endSheet: o_specificTime_win];
        [o_specificTime_win close];
    }
    else
    {
        input_thread_t * p_input = (input_thread_t *)vlc_object_find( VLCIntf, \
            VLC_OBJECT_INPUT, FIND_ANYWHERE );
        if( p_input )
        {
            /* we can obviously only do that if an input is available */
            vlc_value_t pos, length;
            var_Get( p_input, "time", &pos );
            [o_specificTime_enter_fld setIntValue: (pos.i_time / 1000000)];
            var_Get( p_input, "length", &length );
            [o_specificTime_stepper setMaxValue: (length.i_time / 1000000)];

            [NSApp beginSheet: o_specificTime_win modalForWindow: \
                [NSApp mainWindow] modalDelegate: self didEndSelector: nil \
                contextInfo: nil];
            [o_specificTime_win makeKeyWindow];
            vlc_object_release( p_input );
        }
    }
}

@end

@implementation VLCControls (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;
    vlc_value_t val;
    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
    }
    else return FALSE;

#define p_input p_playlist->p_input

    if( [[o_mi title] isEqualToString: _NS("Faster")] ||
        [[o_mi title] isEqualToString: _NS("Slower")] )
    {
        if( p_input != NULL )
        {
            bEnabled = p_input->input.b_can_pace_control;
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Stop")] )
    {
        if( p_input == NULL )
        {
            bEnabled = FALSE;
        }
        [o_main setupMenus]; /* Make sure input menu is up to date */
    }
    else if( [[o_mi title] isEqualToString: _NS("Previous")] ||
             [[o_mi title] isEqualToString: _NS("Next")] )
    {
            bEnabled = p_playlist->i_size > 1;
    }
    else if( [[o_mi title] isEqualToString: _NS("Random")] )
    {
        int i_state;
        var_Get( p_playlist, "random", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Repeat One")] )
    {
        int i_state;
        var_Get( p_playlist, "repeat", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Repeat All")] )
    {
        int i_state;
        var_Get( p_playlist, "loop", &val );
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Step Forward")] ||
             [[o_mi title] isEqualToString: _NS("Step Backward")] ||
             [[o_mi title] isEqualToString: _NS("Jump To Time")])
    {
        if( p_input != NULL )
        {
            var_Get( p_input, "seekable", &val);
            bEnabled = val.b_bool;
        }
        else bEnabled = FALSE;
    }
    else if( [[o_mi title] isEqualToString: _NS("Mute")] )
    {
        [o_mi setState: p_intf->p_sys->b_mute ? NSOnState : NSOffState];
        [o_main setupMenus]; /* Make sure audio menu is up to date */
    }
    else if( [[o_mi title] isEqualToString: _NS("Half Size")] ||
                [[o_mi title] isEqualToString: _NS("Normal Size")] ||
                [[o_mi title] isEqualToString: _NS("Double Size")] ||
                [[o_mi title] isEqualToString: _NS("Fit to Screen")] ||
                [[o_mi title] isEqualToString: _NS("Snapshot")] ||
                [[o_mi title] isEqualToString: _NS("Fullscreen")] ||
                [[o_mi title] isEqualToString: _NS("Float on Top")] )
    {
        id o_window;
        NSArray *o_windows = [NSApp orderedWindows];
        NSEnumerator *o_enumerator = [o_windows objectEnumerator];
        bEnabled = FALSE;
        
        vout_thread_t   *p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
        if( p_vout != NULL )
        {
            if( [[o_mi title] isEqualToString: _NS("Float on Top")] )
            {
                var_Get( p_vout, "video-on-top", &val );
                [o_mi setState: val.b_bool ?  NSOnState : NSOffState];
            }

            while( (o_window = [o_enumerator nextObject]))
            {
                if( [[o_window className] isEqualToString: @"VLCWindow"] ||
                            [[[VLCMain sharedInstance] getEmbeddedList]
                            windowContainsEmbedded: o_window])
                {
                    bEnabled = TRUE;
                    break;
                }
            }
            vlc_object_release( (vlc_object_t *)p_vout );
        }
        else if( [[o_mi title] isEqualToString: _NS("Fullscreen")] )
        {
            var_Get( p_playlist, "fullscreen", &val );
            [o_mi setState: val.b_bool];
            bEnabled = TRUE;
        }
        [o_main setupMenus]; /* Make sure video menu is up to date */
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );

    return( bEnabled );
}

@end

/*****************************************************************************
 * VLCMenuExt implementation 
 *****************************************************************************
 * Object connected to a playlistitem which remembers the data belonging to
 * the variable of the autogenerated menu
 *****************************************************************************/
@implementation VLCMenuExt

- (id)initWithVar: (const char *)_psz_name Object: (int)i_id
        Value: (vlc_value_t)val ofType: (int)_i_type
{
    self = [super init];

    if( self != nil )
    {
        psz_name = strdup( _psz_name );
        i_object_id = i_id;
        value = val;
        i_type = _i_type;
    }

    return( self );
}

- (void)dealloc
{
    free( psz_name );
    [super dealloc];
}

- (char *)name
{
    return psz_name;
}

- (int)objectID
{
    return i_object_id;
}

- (vlc_value_t)value
{
    return value;
}

- (int)type
{
    return i_type;
}

@end


/*****************************************************************************
 * VLCTimeField implementation 
 *****************************************************************************
 * we need this to catch our click-event in the controller window
 *****************************************************************************/

@implementation VLCTimeField
- (void)mouseDown: (NSEvent *)ourEvent
{
    if( [ourEvent clickCount] > 1 )
        [[[VLCMain sharedInstance] getControls] goToSpecificTime: nil];
}
@end
