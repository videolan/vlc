/*****************************************************************************
 * intf_vlc_wrapper.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.c,v 1.8 2002/02/18 01:34:44 jlj Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <videolan/vlc.h>

#include "interface.h"
#include "intf_playlist.h"

#include "video.h"
#include "video_output.h"
#include "stream_control.h"
#include "input_ext-intf.h"

#include "macosx.h"
#include "intf_vlc_wrapper.h"

@implementation Intf_VlcWrapper

/* Initialization */

+ (Intf_VlcWrapper *)instance
{
    static bool b_initialized = 0;
    static Intf_VlcWrapper* o_vlc = nil;
    
    if( !b_initialized )
    {
        o_vlc = [[Intf_VlcWrapper alloc] init];
        b_initialized = TRUE;
    }
        
    return o_vlc;
}
    
- (Intf_VlcWrapper *)initWithDelegate:(id)_o_delegate
{
    e_speed = SPEED_NORMAL;
    o_delegate = _o_delegate;
        
    return self;
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

    return( 0 );
}

- (void)quit
{
    p_main->p_intf->b_die = 1;
}

/* Vout requests */
- (void)handlePortMessage:(NSPortMessage *)o_msg
{
    NSData *o_req;
    struct vout_req_s *p_req;

    o_req = [[o_msg components] lastObject];
    p_req = *((struct vout_req_s **)[o_req bytes]);

    [p_req->o_lock lock];

    if( p_req->i_type == VOUT_REQ_CREATE_WINDOW )
    {
        VLCView *o_view;

        p_req->p_vout->p_sys->o_window = [VLCWindow alloc];
        [p_req->p_vout->p_sys->o_window setVout: p_req->p_vout];
        [p_req->p_vout->p_sys->o_window setReleasedWhenClosed: YES];

        if( p_req->p_vout->b_fullscreen )
        {
            [p_req->p_vout->p_sys->o_window 
                initWithContentRect: [[NSScreen mainScreen] frame] 
                styleMask: NSBorderlessWindowMask 
                backing: NSBackingStoreBuffered
                defer: NO screen: [NSScreen mainScreen]];

            [p_req->p_vout->p_sys->o_window 
                setLevel: CGShieldingWindowLevel()];
        }
        else
        {
            unsigned int i_stylemask = NSTitledWindowMask |
                                       NSMiniaturizableWindowMask |
                                       NSResizableWindowMask;

            [p_req->p_vout->p_sys->o_window 
                initWithContentRect: p_req->p_vout->p_sys->s_rect 
                styleMask: i_stylemask
                backing: NSBackingStoreBuffered
                defer: NO screen: [NSScreen mainScreen]];

            if( !p_req->p_vout->p_sys->b_pos_saved )
            {
                [p_req->p_vout->p_sys->o_window center];
            }
        }

        o_view = [[VLCView alloc] initWithVout: p_req->p_vout];
        [p_req->p_vout->p_sys->o_window setContentView: o_view];

        [o_view lockFocus];
        p_req->p_vout->p_sys->p_qdport = [o_view qdPort];
        [o_view unlockFocus];

        [p_req->p_vout->p_sys->o_window setTitle: [NSString 
            stringWithCString: VOUT_TITLE]];
        [p_req->p_vout->p_sys->o_window setAcceptsMouseMovedEvents: YES];
        [p_req->p_vout->p_sys->o_window makeKeyAndOrderFront: nil];

        p_req->i_result = 1;
    }
    else if( p_req->i_type == VOUT_REQ_DESTROY_WINDOW )
    {
        if( !p_req->p_vout->b_fullscreen )
        {
            NSRect s_rect;

            s_rect = [[p_req->p_vout->p_sys->o_window contentView] frame];
            p_req->p_vout->p_sys->s_rect.size = s_rect.size;

            s_rect = [p_req->p_vout->p_sys->o_window frame];
            p_req->p_vout->p_sys->s_rect.origin = s_rect.origin;

            p_req->p_vout->p_sys->b_pos_saved = 1;
        }

        p_req->p_vout->p_sys->p_qdport = nil;
        [p_req->p_vout->p_sys->o_window close];
        p_req->p_vout->p_sys->o_window = nil;

        p_req->i_result = 1;
    }

    [p_req->o_lock unlockWithCondition: 1];
}

- (NSPort *)sendPort
{
    return( p_main->p_intf->p_sys->o_port );
}

/* Playback control */
- (void)setSpeed:(intf_speed_t)_e_speed
{
    e_speed = _e_speed;
    [self playlistPlayCurrent];
}

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
    if( p_input_bank->pp_input[0] == NULL )
    {
        return( 0.0 );
    }    

    return( (float)p_area->i_tell / (float)p_area->i_size );
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

/* Playlist control */

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
    
- (bool)playlistPlayCurrent
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        switch (e_speed)
        {
            case SPEED_SLOW:
                input_SetStatus( p_input_bank->pp_input[0], 
                                 INPUT_STATUS_SLOWER );
            break;

            case SPEED_NORMAL:
                input_SetStatus( p_input_bank->pp_input[0], 
                                 INPUT_STATUS_PLAY );
            break;

            case SPEED_FAST:
                input_SetStatus( p_input_bank->pp_input[0], 
                                 INPUT_STATUS_FASTER );
            break;
        }

        p_main->p_playlist->b_stopped = 0;
    }
    else if( p_main->p_playlist->b_stopped )
    {
        if( p_main->p_playlist->i_size )
        {
            intf_PlaylistJumpto( p_main->p_playlist, 
                                 p_main->p_playlist->i_index );
        }
        else
        {
            return FALSE;
        }
    }
        
    return TRUE;
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

- (void)playlistPlayNext
{
    [self playlistStop];

    vlc_mutex_lock( &p_main->p_playlist->change_lock );
    p_main->p_playlist->i_index++;
    vlc_mutex_unlock( &p_main->p_playlist->change_lock );

    [self playlistPlayCurrent];
}

- (void)playlistPlayPrev
{
    [self playlistStop];

    vlc_mutex_lock( &p_main->p_playlist->change_lock );
    p_main->p_playlist->i_index--;
    vlc_mutex_unlock( &p_main->p_playlist->change_lock );

    [self playlistPlayCurrent];    
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

- (bool)playlistPlaying
{
    return( p_main->p_playlist->b_stopped );
}

@end
