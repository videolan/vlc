/*****************************************************************************
 * controls.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: controls.m,v 1.38 2003/05/11 18:41:27 hartman Exp $
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
#include "open.h"
#include "controls.h"

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
            [o_open openFileGeneric: nil];
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

        p_intf->p_sys->b_input_update = VLC_TRUE;
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

        p_intf->p_sys->b_input_update = VLC_TRUE;
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

- (IBAction)forward:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL || p_playlist->p_input == NULL )
    {
        if ( p_playlist != NULL ) vlc_object_release( p_playlist );
        return;
    }

    input_Seek( p_playlist->p_input, 5, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
    vlc_object_release( p_playlist );
}

- (IBAction)backward:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL || p_playlist->p_input == NULL )
    {
        if ( p_playlist != NULL ) vlc_object_release( p_playlist );
        return;
    }

    input_Seek( p_playlist->p_input, -5, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
    vlc_object_release( p_playlist );
}

- (IBAction)volumeUp:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    if( p_intf->p_sys->b_mute )
    {
        [self mute: nil];
    }

    aout_VolumeUp( p_intf, 1, NULL );

    [self updateVolumeSlider];
}

- (IBAction)volumeDown:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];

    if( p_intf->p_sys->b_mute )
    {
        [self mute: nil];
    }
    
    aout_VolumeDown( p_intf, 1, NULL );

    [self updateVolumeSlider];
}

- (IBAction)mute:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    audio_volume_t i_volume;

    aout_VolumeMute( p_intf, &i_volume );
    p_intf->p_sys->b_mute = ( i_volume == 0 );

    [self updateVolumeSlider];
}

- (IBAction)volumeSliderUpdated:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    audio_volume_t i_volume = (audio_volume_t)[sender intValue];

    aout_VolumeSet( p_intf, i_volume * AOUT_VOLUME_STEP );
}

- (void)updateVolumeSlider
{
    intf_thread_t * p_intf = [NSApp getIntf];
    audio_volume_t i_volume;

    aout_VolumeGet( p_intf, &i_volume );

    [o_volumeslider setFloatValue: (float)(i_volume / AOUT_VOLUME_STEP)]; 
}

- (IBAction)windowAction:(id)sender
{
    id o_window = [NSApp keyWindow];
    NSString *o_title = [sender title];
    NSArray *o_windows = [NSApp windows];
    NSEnumerator *o_enumerator = [o_windows objectEnumerator];
    vout_thread_t   *p_vout = vlc_object_find( [NSApp getIntf], VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );

    if( p_vout != NULL )
    {
        while ((o_window = [o_enumerator nextObject]))
        {
            if( [[o_window className] isEqualToString: @"VLCWindow"] )
            {
                if( [o_title isEqualToString: _NS("Fullscreen") ] )
                    [o_window toggleFullscreen];
                else if( [o_title isEqualToString: _NS("Half Size") ] )
                    [o_window scaleWindowWithFactor: 0.5];
                else if( [o_title isEqualToString: _NS("Normal Size") ] )
                    [o_window scaleWindowWithFactor: 1.0];
                else if( [o_title isEqualToString: _NS("Double Size") ] )
                    [o_window scaleWindowWithFactor: 2.0];
                else if( [o_title isEqualToString: _NS("Float On Top") ] )
                    [o_window toggleFloatOnTop];
                else if( [o_title isEqualToString: _NS("Fit To Screen") ] )
                {
                    if( ![o_window isZoomed] )
                        [o_window performZoom:self];
                }
            }
        }
        vlc_object_release( (vlc_object_t *)p_vout );
    }
}

- (IBAction)deinterlace:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    BOOL bEnable = [sender state] == NSOffState;

    if( bEnable && ![[sender title] isEqualToString: @"none"] )
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

- (IBAction)toggleVar:(id)sender
{
    NSMenuItem *o_mi = (NSMenuItem *)sender;
    NSMenu *o_mu = [o_mi menu];
    
    if( [o_mi state] == NSOffState )
    {
        const char * psz_variable = (const char *)
            [[[o_mu supermenu] itemWithTitle: [o_mu title]] tag];
        vlc_object_t * p_object = (vlc_object_t *)
            [[o_mi representedObject] pointerValue];
        vlc_value_t val;
        val.i_int = (int)[o_mi tag];

        if ( var_Set( p_object, psz_variable, val ) < 0 )
        {
            msg_Warn( p_object, "cannot set variable %s: with %d", psz_variable, val.i_int );
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
    else if( [[o_mi title] isEqualToString: _NS("Step Forward")] ||
             [[o_mi title] isEqualToString: _NS("Step Backward")] )
    {
        if( p_playlist != NULL && p_input != NULL )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            bEnabled = p_input->stream.b_seekable;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        else
        {
            bEnabled = FALSE;
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Mute")] ) 
    {
        [o_mi setState: p_intf->p_sys->b_mute ? NSOnState : NSOffState];
    }
    else if( [[o_mi title] isEqualToString: _NS("Fullscreen")] ||
                [[o_mi title] isEqualToString: _NS("Half Size")] ||
                [[o_mi title] isEqualToString: _NS("Normal Size")] ||
                [[o_mi title] isEqualToString: _NS("Double Size")] ||
                [[o_mi title] isEqualToString: _NS("Fit To Screen")] ||
                [[o_mi title] isEqualToString: _NS("Float On Top")] )
    {
        id o_window;
        NSArray *o_windows = [NSApp windows];
        NSEnumerator *o_enumerator = [o_windows objectEnumerator];
        bEnabled = FALSE;
        
        if ( [[o_mi title] isEqualToString: _NS("Float On Top")] )
        {
            int i_state = config_GetInt( p_playlist, "macosx-float" ) ?
                      NSOnState : NSOffState;
            [o_mi setState: i_state];
        }
        
        vout_thread_t   *p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                              FIND_ANYWHERE );
        if( p_vout != NULL )
        {
            while ((o_window = [o_enumerator nextObject]))
            {
                if( [[o_window className] isEqualToString: @"VLCWindow"] )
                {
                    bEnabled = TRUE;
                    break;
                }
            }
            vlc_object_release( (vlc_object_t *)p_vout );
        }
    }
    else if( [[o_mi title] isEqualToString: _NS("Float On Top")] )
    {
        
        bEnabled = TRUE;
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
            if( [[o_mi title] isEqualToString: @"none"] )
            {
                [o_mi setState: NSOnState];
            }
            else
            {
                [o_mi setState: NSOffState];
            }
        }
    }

    if( p_playlist != NULL )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
    }

    return( bEnabled );
}

@end
