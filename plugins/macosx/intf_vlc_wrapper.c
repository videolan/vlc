/*****************************************************************************
 * intf_vlc_wrapper.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.c,v 1.11 2002/05/06 22:59:46 massiot Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>

#include <videolan/vlc.h>

#include "interface.h"
#include "intf_playlist.h"
#include "intf_eject.h"

#include "video.h"
#include "video_output.h"
#include "audio_output.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "macosx.h"
#include "intf_open.h"
#include "intf_vlc_wrapper.h"

#include "netutils.h"

@implementation Intf_VLCWrapper

static Intf_VLCWrapper *o_intf = nil;

/* Initialization */

+ (Intf_VLCWrapper *)instance
{
    if( o_intf == nil )
    {
        o_intf = [[[Intf_VLCWrapper alloc] init] autorelease];
    }

    return( o_intf );
}

- (void)dealloc
{
    o_intf = nil;
    [super dealloc];
}

- (bool)manage
{
    p_main->p_intf->pf_manage( p_main->p_intf );

    if( p_main->p_intf->b_die )
    {
        /* Vout depends on intf */
        input_EndBank();
        vout_EndBank();
        input_InitBank();
        vout_InitBank();

        return( 1 );
    }

    if( p_input_bank->pp_input[0] != NULL )
    {
        vlc_mutex_lock( &p_input_bank->pp_input[0]->stream.stream_lock );

        if( !p_input_bank->pp_input[0]->b_die )
        {
            /* New input or stream map change */
            if( p_input_bank->pp_input[0]->stream.b_changed ||
                p_main->p_intf->p_sys->i_part !=
                p_input_bank->pp_input[0]->stream.p_selected_area->i_part )
            {
                [self setupMenus];
            }
        }

        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
    else
    {
        [self setupMenus];
    }

    return( 0 );
}

- (void)quit
{
    p_main->p_intf->b_die = 1;
}

/* playlist control */
    
- (bool)playlistPlay
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PLAY );
        p_main->p_playlist->b_stopped = 0;
    }
    else
    {
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        if( p_main->p_playlist->b_stopped )
        {
            if( p_main->p_playlist->i_size )
            {
                vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                intf_PlaylistJumpto( p_main->p_playlist,
                                     p_main->p_playlist->i_index );
            }
            else
            {
                vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                [[Intf_Open instance] openFile: nil];
            }
        }
        else
        {
            vlc_mutex_unlock( &p_main->p_playlist->change_lock );
        }
    }

    return( TRUE );
}

- (void)playlistPause
{
    if ( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_PAUSE );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}
    
- (void)playlistStop
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        /* end playing item */
        p_input_bank->pp_input[0]->b_eof = 1;

        /* update playlist */
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        p_main->p_playlist->i_index--;
        p_main->p_playlist->b_stopped = 1;

        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

- (void)playlistNext
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }
}

- (void)playlistPrev
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        /* FIXME: temporary hack */
        intf_PlaylistPrev( p_main->p_playlist );
        intf_PlaylistPrev( p_main->p_playlist );
        p_input_bank->pp_input[0]->b_eof = 1;
    }
}

- (void)playSlower
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_SLOWER );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

- (void)playFaster
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_SetStatus( p_input_bank->pp_input[0], INPUT_STATUS_FASTER );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }
}

- (void)mute
{
    if( p_aout_bank->pp_aout[0] == NULL ) return;

    if( p_main->p_intf->p_sys->b_mute )
    {
        p_aout_bank->pp_aout[0]->i_volume = 
                            p_main->p_intf->p_sys->i_saved_volume;
    }
    else
    {
        p_main->p_intf->p_sys->i_saved_volume = 
                            p_aout_bank->pp_aout[0]->i_volume;
        p_aout_bank->pp_aout[0]->i_volume = 0;
    }
    p_main->p_intf->p_sys->b_mute = !p_main->p_intf->p_sys->b_mute;
}

- (void)fullscreen
{
    if( p_vout_bank->pp_vout[0] != NULL )
    {
        p_vout_bank->pp_vout[0]->i_changes |= VOUT_FULLSCREEN_CHANGE;
    }
}

- (void)eject
{
    /* FIXME : this will only eject the first drive found */
    NSArray * o_devices = GetEjectableMediaOfClass(kIODVDMediaClass);
    const char * psz_device;

    if ( o_devices == nil )
    {
        o_devices = GetEjectableMediaOfClass(kIOCDMediaClass);
    }

    psz_device = [[o_devices objectAtIndex:0] cString];
    intf_Eject( psz_device );
}

/* playback info */

#define p_area p_input_bank->pp_input[0]->stream.p_selected_area

- (NSString *)getTimeAsString
{
    static char psz_currenttime[ OFFSETTOTIME_MAX_SIZE ];
        
    if( p_input_bank->pp_input[0] == NULL )
    {
        return [NSString stringWithCString:"00:00:00"];
    }     
   
    input_OffsetToTime( p_input_bank->pp_input[0], 
                        psz_currenttime, p_area->i_tell );        

    return( [NSString stringWithCString: psz_currenttime] );
}
    
- (float)getTimeAsFloat
{
    float f_time = 0.0;

    vlc_mutex_lock( &p_input_bank->lock );

    if( p_input_bank->pp_input[0] != NULL )
    {
        f_time = (float)p_area->i_tell / (float)p_area->i_size;
    }    

    vlc_mutex_unlock( &p_input_bank->lock );

    return( f_time );
}

- (void)setTimeAsFloat:(float)f_position
{
    vlc_mutex_lock( &p_input_bank->lock );

    if( p_input_bank->pp_input[0] != NULL )
    {
        input_Seek( p_input_bank->pp_input[0], p_area->i_size * f_position );
    }

    vlc_mutex_unlock( &p_input_bank->lock );
}

#undef p_area

- (bool)playlistPlaying
{
    return( !p_main->p_playlist->b_stopped );
}

- (NSArray *)playlistAsArray
{
    int i;
    NSMutableArray* p_list = 
        [NSMutableArray arrayWithCapacity: p_main->p_playlist->i_size];
    
    vlc_mutex_lock( &p_main->p_playlist->change_lock );

    for( i = 0; i < p_main->p_playlist->i_size; i++ )
    {
        [p_list addObject: [NSString 
            stringWithCString: p_main->p_playlist->p_item[i].psz_name]];
    }

    vlc_mutex_unlock( &p_main->p_playlist->change_lock );
        
    return( [NSArray arrayWithArray: p_list] );
}

/*
- (int)playlistLength
{
    return( p_main->p_playlist->i_size );
}

- (NSString*)playlistItem:(int)i_pos
{
    NSString *o_item = nil;

    vlc_mutex_lock( &p_main->p_playlist->change_lock );
    
    if( i_pos < p_main->p_playlist->i_size )
    {
        o_item = [NSString 
            stringWithCString: p_main->p_playlist->p_item[i_pos].psz_name];
    }

    vlc_mutex_unlock( &p_main->p_playlist->change_lock );

    return( o_item );
}

- (void)playlistPlayItem:(int)i_item
{
    [self playlistStop];

    vlc_mutex_lock( &p_main->p_playlist->change_lock );

    if( i_item<p_main->p_playlist->i_size )
    {
        p_main->p_playlist->i_index--;
    }

    vlc_mutex_unlock( &p_main->p_playlist->change_lock );        

    [self playlistPlayCurrent];
}
    
- (void)playlistAdd:(NSString *)o_filename
{
    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, 
                      [o_filename lossyCString] );
}
    
- (void)clearPlaylist
{
    int i;
    
    vlc_mutex_lock( &p_main->p_playlist->change_lock );

    for( i = 0; i < p_main->p_playlist->i_size; i++ )
    {
        intf_PlaylistDelete( p_main->p_playlist, i );
    }

    vlc_mutex_unlock( &p_main->p_playlist->change_lock );        
}
*/

/* open file/disc/network */

- (void)openFiles:(NSArray*)o_files
{
    NSString *o_file;
    int i_end = p_main->p_playlist->i_size;
    NSEnumerator *o_enum = [o_files objectEnumerator];

    while( ( o_file = (NSString *)[o_enum nextObject] ) )
    {
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END, 
                          [o_file lossyCString] );
    }

    /* end current item, select first added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}

- (void)openDisc:(NSString*)o_type device:(NSString*)o_device title:(int)i_title chapter:(int)i_chapter
{
    NSString *o_source;
    int i_end = p_main->p_playlist->i_size;

    o_source = [NSString stringWithFormat: @"%@:%@@%d,%d", 
                    o_type, o_device, i_title, i_chapter];

    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END,
                      [o_source lossyCString] );

    /* stop current item, select added item */
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}

- (void)openNet:(NSString*)o_protocol addr:(NSString*)o_addr port:(int)i_port baddr:(NSString*)o_baddr
{
    NSString *o_source;
    int i_end = p_main->p_playlist->i_size;

    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    config_PutIntVariable( "network_channel", 0 );

    if( o_baddr != nil )
    {
        o_source = [NSString stringWithFormat: @"%@://%@@:%i/%@",
                        o_protocol, o_addr, i_port, o_baddr];
    }
    else
    {
        o_source = [NSString stringWithFormat: @"%@://%@@:%i",
                        o_protocol, o_addr, i_port];
    }

    intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END,
                      [o_source lossyCString] );

    intf_PlaylistJumpto( p_main->p_playlist, i_end - 1 );
}

- (void)openNetChannel:(NSString*)o_addr port:(int)i_port
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        p_input_bank->pp_input[0]->b_eof = 1;
    }

    config_PutIntVariable( "network_channel", 1 );

    if( p_main->p_channel == NULL )
    {
        network_ChannelCreate();
    }

    config_PutPszVariable( "channel_server", (char*)[o_addr lossyCString] );
    config_PutIntVariable( "channel_port", i_port ); 
}

- (void)setupMenus
{
#if 0
    NSMenu * o_main_menu = [NSApp mainMenu];
    NSMenuItem * o_program_item = [o_main_menu itemWithTitle:@"Program"];

    if( p_input_bank->pp_input[0] == NULL )
    {
        NSMenu * o_program = [o_program_item submenu];
        [o_program_item setEnabled:0];
        [o_program removeItemAtIndex:0];
    }
    else
    {
        NSMenu * o_program = [o_program_item submenu];
        [o_program_item setEnabled:1];
        [o_program removeItemAtIndex:0];
    }
#endif
}

@end
