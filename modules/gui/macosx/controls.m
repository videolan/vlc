/*****************************************************************************
 * controls.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: controls.m,v 1.22 2003/02/08 17:26:00 massiot Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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
#include "vout.h"

/*****************************************************************************
 * VLCControls interface 
 *****************************************************************************/
@interface VLCControls : NSObject
{
    IBOutlet id o_open;
    IBOutlet id o_main;

    IBOutlet id o_volumeslider;
}

- (IBAction)play:(id)sender;
- (IBAction)stop:(id)sender;
- (IBAction)faster:(id)sender;
- (IBAction)slower:(id)sender;

- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)loop:(id)sender;

- (IBAction)volumeUp:(id)sender;
- (IBAction)volumeDown:(id)sender;
- (IBAction)mute:(id)sender;
- (IBAction)volumeSliderUpdated:(id)sender;
- (void)updateVolumeSlider;

- (IBAction)halfWindow:(id)sender;
- (IBAction)normalWindow:(id)sender;
- (IBAction)doubleWindow:(id)sender;
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

    if( playlist_IsPlaying( p_playlist ) )
    {
        playlist_Pause( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        if( !playlist_IsEmpty( p_playlist ) )
        {
            playlist_Play( p_playlist );
            vlc_object_release( p_playlist );
        }
        else
        {
            vlc_object_release( p_playlist );
            [o_open openFile: nil];
        }
    }
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

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->p_input != NULL )
    {
        input_SetStatus( p_playlist->p_input, INPUT_STATUS_FASTER );
    } 
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );
}

- (IBAction)slower:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->p_input != NULL )
    {
        input_SetStatus( p_playlist->p_input, INPUT_STATUS_SLOWER );
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    vlc_object_release( p_playlist );
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

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->p_input == NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );  
        return;
    }

    vlc_mutex_lock( &p_playlist->p_input->stream.stream_lock );

#define p_area p_playlist->p_input->stream.p_selected_area

    if( p_area->i_part_nb > 1 && p_area->i_part > 1 )
    {
        p_area->i_part--;

        vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
        input_ChangeArea( p_playlist->p_input, p_area );
        vlc_mutex_unlock( &p_playlist->object_lock );

        p_intf->p_sys->b_chapter_update = VLC_TRUE;
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Prev( p_playlist );
    }

#undef p_area

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

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->p_input == NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );  
        return;
    }

    vlc_mutex_lock( &p_playlist->p_input->stream.stream_lock );

#define p_area p_playlist->p_input->stream.p_selected_area

    if( p_area->i_part_nb > 1 && p_area->i_part + 1 < p_area->i_part_nb )
    {
        p_area->i_part++;

        vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
        input_ChangeArea( p_playlist->p_input, p_area );
        vlc_mutex_unlock( &p_playlist->object_lock );

        p_intf->p_sys->b_chapter_update = VLC_TRUE;
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Next( p_playlist );
    }

#undef p_area

    vlc_object_release( p_playlist );
}

- (IBAction)loop:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    config_PutInt( p_playlist, "loop",
                   !config_GetInt( p_playlist, "loop" ) );

    vlc_object_release( p_playlist );
}

- (IBAction)volumeUp:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                        FIND_ANYWHERE );
    if( p_aout != NULL )
    {
        if( p_intf->p_sys->b_mute )
        {
            [self mute: nil];
        }

        aout_VolumeUp( p_aout, 1, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }

    [self updateVolumeSlider];
}

- (IBAction)volumeDown:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                        FIND_ANYWHERE );
    if( p_aout != NULL )
    {
        if( p_intf->p_sys->b_mute )
        {
            [self mute: nil];
        }

        aout_VolumeDown( p_aout, 1, NULL );
        vlc_object_release( (vlc_object_t *)p_aout );
    }

    [self updateVolumeSlider];
}

- (IBAction)mute:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                        FIND_ANYWHERE );

    if ( p_aout != NULL )
    {
        audio_volume_t i_volume;

        aout_VolumeMute( p_aout, &i_volume );
        vlc_object_release( (vlc_object_t *)p_aout );

        p_intf->p_sys->b_mute = ( i_volume == 0 );
    }

    [self updateVolumeSlider];
}

- (IBAction)volumeSliderUpdated:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                        FIND_ANYWHERE );
    if( p_aout != NULL )
    {
        audio_volume_t i_volume;

        i_volume = (audio_volume_t)[sender intValue];

        aout_VolumeSet( p_aout, i_volume * AOUT_VOLUME_STEP );
        vlc_object_release( (vlc_object_t *)p_aout );
    }
}

- (void)updateVolumeSlider
{
    intf_thread_t * p_intf = [NSApp getIntf];

    aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                        FIND_ANYWHERE );

    if ( p_aout != NULL )
    {
        audio_volume_t i_volume;

        aout_VolumeGet( p_aout, &i_volume );
        vlc_object_release( (vlc_object_t *)p_aout );

        [o_volumeslider setFloatValue: (float)(i_volume / AOUT_VOLUME_STEP)]; 
    }
}

- (IBAction)halfWindow:(id)sender
{
    id o_window = [NSApp keyWindow];
    NSArray *o_windows = [NSApp windows];
    NSEnumerator *o_enumerator = [o_windows objectEnumerator];
    
    while ((o_window = [o_enumerator nextObject]))
    {
        if( [[o_window className] isEqualToString: @"VLCWindow"] )
        {
            [o_window scaleWindowWithFactor: 0.5];
        }
    }
}

- (IBAction)normalWindow:(id)sender
{
    id o_window = [NSApp keyWindow];
    NSArray *o_windows = [NSApp windows];
    NSEnumerator *o_enumerator = [o_windows objectEnumerator];
    
    while ((o_window = [o_enumerator nextObject]))
    {
        if( [[o_window className] isEqualToString: @"VLCWindow"] )
        {
            [o_window scaleWindowWithFactor: 1];
        }
    }
}

- (IBAction)doubleWindow:(id)sender
{
    id o_window = [NSApp keyWindow];
    NSArray *o_windows = [NSApp windows];
    NSEnumerator *o_enumerator = [o_windows objectEnumerator];
    
    while ((o_window = [o_enumerator nextObject]))
    {
        if( [[o_window className] isEqualToString: @"VLCWindow"] )
        {
            [o_window scaleWindowWithFactor: 2];
        }
    }
}


- (IBAction)fullscreen:(id)sender
{
    id o_window = [NSApp keyWindow];
    NSArray *o_windows = [NSApp windows];
    NSEnumerator *o_enumerator = [o_windows objectEnumerator];
    
    while ((o_window = [o_enumerator nextObject]))
    {
        if( [[o_window className] isEqualToString: @"VLCWindow"] )
        {
            [o_window toggleFullscreen];
        }
    }
}

- (IBAction)deinterlace:(id)sender
{

}

- (IBAction)toggleProgram:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->p_input == NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        return;
    }

    if( [o_mi state] == NSOffState )
    {
        u16 i_program_id = [o_mi tag];

        input_ChangeProgram( p_playlist->p_input, i_program_id );
        input_SetStatus( p_playlist->p_input, INPUT_STATUS_PLAY );
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );
}

- (IBAction)toggleTitle:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->p_input == NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        return;
    }

    if( [o_mi state] == NSOffState )
    {
        int i_title = [o_mi tag];

#define p_input p_playlist->p_input
        input_ChangeArea( p_input, p_input->stream.pp_areas[i_title] );
        input_SetStatus( p_input, INPUT_STATUS_PLAY );
#undef p_input
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );
}

- (IBAction)toggleChapter:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->p_input == NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        return;
    }

    if( [o_mi state] == NSOffState )
    {
        int i_chapter = [o_mi tag];

#define p_input p_playlist->p_input
        p_input->stream.p_selected_area->i_part = i_chapter;
        input_ChangeArea( p_input, p_input->stream.p_selected_area );
        input_SetStatus( p_input, INPUT_STATUS_PLAY );
#undef p_input
    }

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );
}

- (IBAction)toggleLanguage:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    if( p_playlist->p_input == NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        return;
    }

#if 0
    /* We do not use this code, because you need to start stop .avi for
     * it to work, so not very useful now  --hartman */
    if ( [o_mi state] == NSOffState && [o_mi tag] == 2000 )
    {
        NSOpenPanel *o_open_panel = [NSOpenPanel openPanel];
        
        [o_open_panel setAllowsMultipleSelection: NO];
        [o_open_panel setTitle: _NS("Open subtitlefile")];
        [o_open_panel setPrompt: _NS("Open")];
    
        if( [o_open_panel runModalForDirectory: nil 
                file: nil types: nil] == NSOKButton )
        {
            NSString *o_filename = [[o_open_panel filenames] objectAtIndex: 0];
            config_PutPsz( p_intf, "sub-file", strdup( [o_filename cString] ));
        }
    }
#endif

#define p_input p_playlist->p_input

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

    vlc_mutex_unlock( &p_playlist->object_lock );
    vlc_object_release( p_playlist );
}

- (IBAction)toggleVar:(id)sender
{
    NSMenuItem * o_mi = (NSMenuItem *)sender;
    
    if( [o_mi state] == NSOffState )
    {
        const char * psz_variable = (const char *)[o_mi tag];
        char * psz_value = [NSApp delocalizeString: [o_mi title]];
        vlc_object_t * p_object = (vlc_object_t *)
            [[o_mi representedObject] pointerValue];
        vlc_value_t val;
        /* psz_string sucks */
        val.psz_string = (char *)psz_value;

        if ( var_Set( p_object, psz_variable, val ) < 0 )
        {
            msg_Warn( p_object, "cannot set variable (%s)", psz_value );
        }

        free( psz_value );
    }
}

@end

@implementation VLCControls (NSMenuValidation)
 
- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;
    NSMenu * o_menu = [o_mi menu];
    intf_thread_t * p_intf = [NSApp getIntf];

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
    }

#define p_input p_playlist->p_input

    if( [[o_mi title] isEqualToString: _NS("Faster")] ||
        [[o_mi title] isEqualToString: _NS("Slower")] )
    {
        if( p_playlist != NULL && p_input != NULL )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            bEnabled = p_input->stream.b_pace_control;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Stop")] )
    {
        if( p_playlist == NULL || p_input == NULL )
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Previous")] ||
             [[o_mi title] isEqualToString: _NS("Next")] )
    {
        if( p_playlist == NULL )
        {
            bEnabled = FALSE;
        }
        else
        {
            bEnabled = p_playlist->i_size > 1;

            if( p_input != NULL )
            {
                vlc_mutex_lock( &p_input->stream.stream_lock );
                bEnabled |= p_input->stream.p_selected_area->i_part_nb > 1;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
            }
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Loop")] )
    {
        int i_state = config_GetInt( p_playlist, "loop" ) ?
                      NSOnState : NSOffState;

        [o_mi setState: i_state];
    }
    else if( [[o_mi title] isEqualToString: _NS("Volume Up")] ||
             [[o_mi title] isEqualToString: _NS("Volume Down")] ) 
    {
        aout_instance_t * p_aout;

        p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                          FIND_ANYWHERE );
        if( p_aout != NULL )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Mute")] ) 
    {
        aout_instance_t * p_aout;

        p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                          FIND_ANYWHERE );
        if( p_aout != NULL )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
        }
        else
        {
            bEnabled = FALSE;
        }

        [o_mi setState: p_intf->p_sys->b_mute ? NSOnState : NSOffState];
    }
    else if( [[o_mi title] isEqualToString: _NS("Fullscreen")] ||
                [[o_mi title] isEqualToString: _NS("Half Size")] ||
                [[o_mi title] isEqualToString: _NS("Normal Size")] ||
                [[o_mi title] isEqualToString: _NS("Double Size")])    
    {
        id o_window;
        NSArray *o_windows = [NSApp windows];
        NSEnumerator *o_enumerator = [o_windows objectEnumerator];
        bEnabled = FALSE;
        
        while ((o_window = [o_enumerator nextObject]))
        {
            if( [[o_window className] isEqualToString: @"VLCWindow"] )
            {
                bEnabled = TRUE;
                break;
            }
        }
    }
    else if( o_menu != nil && 
             [[o_menu title] isEqualToString: _NS("Deinterlace")] )
    { 

    } 

    if( p_playlist != NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
    }

    return( bEnabled );
}

@end
