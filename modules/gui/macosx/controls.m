/*****************************************************************************
 * controls.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: controls.m,v 1.5 2003/01/04 04:11:08 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>

#include <Cocoa/Cocoa.h> 
#include <CoreAudio/AudioHardware.h>

#include "intf.h"
#include "vout.h"

/*****************************************************************************
 * VLCControls interface 
 *****************************************************************************/
@interface VLCControls : NSObject
{
    IBOutlet id o_open;
}

- (IBAction)play:(id)sender;
- (IBAction)pause:(id)sender;
- (IBAction)stop:(id)sender;
- (IBAction)faster:(id)sender;
- (IBAction)slower:(id)sender;

- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)loop:(id)sender;

- (IBAction)volumeUp:(id)sender;
- (IBAction)volumeDown:(id)sender;
- (IBAction)mute:(id)sender;
- (IBAction)fullscreen:(id)sender;
- (IBAction)deinterlace:(id)sender;

- (IBAction)toggleProgram:(id)sender;
- (IBAction)toggleTitle:(id)sender;
- (IBAction)toggleChapter:(id)sender;
- (IBAction)toggleLanguage:(id)sender;
- (IBAction)toggleVar:(id)sender;

@end

/*****************************************************************************
 * VLCControls implementation 
 *****************************************************************************/
@implementation VLCControls

- (IBAction)play:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    /* If the playlist is empty, open a file requester instead */
    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );

        [o_open openFile: nil];
    }
}

- (IBAction)pause:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    if( p_intf->p_sys->p_input == NULL )
    {
        return;
    }

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PAUSE );
}

- (IBAction)stop:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
}

- (IBAction)faster:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    if( p_intf->p_sys->p_input == NULL )
    {
        return;
    }

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_FASTER );
}

- (IBAction)slower:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    if( p_intf->p_sys->p_input == NULL )
    {
        return;
    }

    input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_SLOWER );
}

- (IBAction)prev:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}

- (IBAction)next:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
}

- (IBAction)loop:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    if( p_intf->p_sys->b_loop )
    {
        [o_mi setState: NSOffState];
        config_PutInt( p_playlist, "loop", 0 );
    }
    else
    {
        [o_mi setState: NSOnState];
        config_PutInt( p_playlist, "loop", 1 );
    }

    p_intf->p_sys->b_loop = !p_intf->p_sys->b_loop;

    vlc_object_release( p_playlist );
}

- (IBAction)volumeUp:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );

    if ( p_aout != NULL )
    {
        aout_VolumeUp( p_aout, 1, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }
}

- (IBAction)volumeDown:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );

    if ( p_aout != NULL )
    {
        aout_VolumeDown( p_aout, 1, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }
}

- (IBAction)mute:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );

    if ( p_aout != NULL )
    {
        aout_VolumeMute( p_aout, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }

    NSMenuItem * o_mi = (NSMenuItem *)sender;
    p_intf->p_sys->b_mute = !p_intf->p_sys->b_mute;
    [o_mi setState: p_intf->p_sys->b_mute ? NSOnState : NSOffState];
}

- (IBAction)fullscreen:(id)sender
{
    id o_window = [NSApp keyWindow];

    if( [[o_window className] isEqualToString: @"VLCWindow"] )
    {
        [o_window toggleFullscreen];
    }
}

- (IBAction)deinterlace:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    BOOL bEnable = [sender state] == NSOffState;

    if( bEnable )
    {
        config_PutPsz( p_intf, "filter", "deinterlace" );
        config_PutPsz( p_intf, "deinterlace-mode", 
                       [[sender title] lossyCString] );
    }
    else
    {
        config_PutPsz( p_intf, "filter", NULL );
    }
}

- (IBAction)toggleProgram:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    if( [o_mi state] == NSOffState )
    {
        u16 i_program_id = [o_mi tag];

        input_ChangeProgram( p_intf->p_sys->p_input, i_program_id );
        input_SetStatus( p_intf->p_sys->p_input, INPUT_STATUS_PLAY );
    }
}

- (IBAction)toggleTitle:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    if( [o_mi state] == NSOffState )
    {
        int i_title = [o_mi tag];

#define p_input p_intf->p_sys->p_input
        input_ChangeArea( p_input, p_input->stream.pp_areas[i_title] );
        input_SetStatus( p_input, INPUT_STATUS_PLAY );
#undef p_input
    }
}

- (IBAction)toggleChapter:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    if( [o_mi state] == NSOffState )
    {
        int i_chapter = [o_mi tag];

#define p_input p_intf->p_sys->p_input
        p_input->stream.p_selected_area->i_part = i_chapter;
        input_ChangeArea( p_input, p_input->stream.p_selected_area );
        input_SetStatus( p_input, INPUT_STATUS_PLAY );
#undef p_input
    }
}

- (IBAction)toggleLanguage:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

#define p_input p_intf->p_sys->p_input

    if( !p_intf->p_sys->b_audio_update )
    {
        NSValue * o_value = [o_mi representedObject];
        es_descriptor_t * p_es = [o_value pointerValue];

        if( [o_mi state] == NSOnState )
        {
            /* we just have one ES to disable */
            input_ToggleES( p_input, p_es, 0 );
        }
        else
        {
            unsigned int i;
            int i_cat = [o_mi tag];

            vlc_mutex_lock( &p_input->stream.stream_lock );

#define ES p_input->stream.pp_selected_es[i]

            /* unselect the selected ES in the same class */
            for( i = 0; i < p_input->stream.i_selected_es_number; i++ )
            {
                if( ES->i_cat == i_cat )
                {
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    input_ToggleES( p_input, ES, 0 );
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    break;
                }
            }

#undef ES

            vlc_mutex_unlock( &p_input->stream.stream_lock );

            input_ToggleES( p_input, p_es, 1 );
        }
    }

#undef p_input
}

- (IBAction)toggleVar:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;

    if( [o_mi state] == NSOffState )
    {
        const char * psz_variable = (const char *)[o_mi tag];
        const char * psz_value = [[o_mi title] cString];
        vlc_object_t * p_object = (vlc_object_t *)
            [[o_mi representedObject] pointerValue];
        vlc_value_t val;
        /* psz_string sucks */
        val.psz_string = (char *)psz_value;

        if ( var_Set( p_object, psz_variable, val ) < 0 )
        {
            msg_Warn( p_object, "cannot set variable (%s)", psz_value );
        }
    }
}

@end

@implementation VLCControls (NSMenuValidation)
 
- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;
    NSMenu * o_menu = [o_mi menu];
    intf_thread_t * p_intf = [NSApp getIntf];

    if( [[o_mi title] isEqualToString: _NS("Pause")] ||
        [[o_mi title] isEqualToString: _NS("Faster")] ||
        [[o_mi title] isEqualToString: _NS("Slower")] )
    {
        if( p_intf->p_sys->p_input != NULL )
        {
#define p_input p_intf->p_sys->p_input
            vlc_mutex_lock( &p_input->stream.stream_lock );
            bEnabled = p_input->stream.b_pace_control;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
#undef p_input
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Stop")] )
    {
        bEnabled = p_intf->p_sys->p_input != NULL;
    }
    else if( [[o_mi title] isEqualToString: _NS("Previous")] ||
             [[o_mi title] isEqualToString: _NS("Next")] )
    {
        playlist_t * p_playlist = vlc_object_find( p_intf, 
                                                   VLC_OBJECT_PLAYLIST,
                                                   FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            bEnabled = FALSE;
        }
        else
        {
            vlc_mutex_lock( &p_playlist->object_lock );
            bEnabled = p_playlist->i_size > 1;
            vlc_mutex_unlock( &p_playlist->object_lock );
            vlc_object_release( p_playlist );
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Volume Up")] ||
             [[o_mi title] isEqualToString: _NS("Volume Down")] ||
             [[o_mi title] isEqualToString: _NS("Mute")] )
    {
        aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                    FIND_ANYWHERE );
 
        if ( p_aout != NULL )
        { 
            vlc_object_release( (vlc_object_t *)p_aout );
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Fullscreen")] )    
    {
        id o_window = [NSApp keyWindow];

        if( [[o_window className] isEqualToString: @"VLCWindow"] )
        {
            [o_mi setState: [o_window isFullscreen] ? 
                             NSOnState : NSOffState]; 
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( o_menu != nil && 
             [[o_menu title] isEqualToString: _NS("Deinterlace")] )
    { 
        char * psz_filter = config_GetPsz( p_intf, "filter" );

        if( psz_filter != NULL )
        {
            free( psz_filter );

            psz_filter = config_GetPsz( p_intf, "deinterlace-mode" );
        }

        if( psz_filter != NULL )
        {
            if( strcmp( psz_filter, [[o_mi title] lossyCString] ) == 0 )
            {
                [o_mi setState: NSOnState]; 
            }
            else
            {
                [o_mi setState: NSOffState];
            }

            free( psz_filter );
        } 
        else
        {
            [o_mi setState: NSOffState];
        }
    } 

    return( bEnabled );
}

@end
