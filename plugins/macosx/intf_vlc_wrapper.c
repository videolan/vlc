/*****************************************************************************
 * intf_vlc_wrapper.c : MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.c,v 1.5 2001/12/07 18:33:07 sam Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
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

#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "interface.h"
#include "intf_playlist.h"

#include "modules.h"
#include "modules_export.h"

#define OSX_COM_STRUCT vout_sys_s
#define OSX_COM_TYPE vout_sys_t
#include "macosx.h"

#include "video.h"
#include "video_output.h"
#include "stream_control.h"
#include "input_ext-intf.h"

#import "intf_vlc_wrapper.h"

#define p_area p_main->p_intf->p_input->stream.p_selected_area

@interface Intf_VlcWrapper(Private) 
- (struct vout_thread_s*) lockVout ;
- (void) unlockVout ;
@end

@implementation Intf_VlcWrapper
//Initialization,.....
    + (Intf_VlcWrapper*) instance {
        static bool b_initialized = 0;
        static Intf_VlcWrapper* o_vlc = nil ;
    
        if (!b_initialized) {
            o_vlc = [[Intf_VlcWrapper alloc] init] ;
            b_initialized = TRUE ;
        }
        
        return o_vlc ;
    }
    
    - (Intf_VlcWrapper*) initWithDelegate:(id)_o_delegate {
            e_speed = SPEED_NORMAL ;
            o_delegate = _o_delegate ;
        
            return self ;
    }




    - (bool) manage {
        vout_thread_t *p_vout ;
        bool b_resize=FALSE, b_request=FALSE, b_release=FALSE;
        bool b_fullscreen=FALSE ;

        p_main->p_intf->pf_manage( p_main->p_intf ) ;
        
        if ((p_vout = [self lockVout])) {
            i_width = p_vout->i_width ;
            i_height = p_vout->i_height ;
            b_fullscreen = !!p_vout->b_fullscreen ;
            
            //Also mange the notifications for the output.
            if (p_vout->i_changes & (VOUT_SIZE_CHANGE | VOUT_FULLSCREEN_CHANGE)) b_resize = TRUE ;
            if (p_vout->p_sys->i_changes & OSX_VOUT_INTF_REQUEST_QDPORT) b_request = TRUE ;
            if (p_vout->p_sys->i_changes & OSX_VOUT_INTF_RELEASE_QDPORT) b_release = TRUE ;
            
            p_vout->i_changes &= ~(VOUT_SIZE_CHANGE | VOUT_FULLSCREEN_CHANGE) ;
            p_vout->p_sys->i_changes &= ~(OSX_VOUT_INTF_REQUEST_QDPORT | OSX_VOUT_INTF_RELEASE_QDPORT) ;

            [self unlockVout] ;
        }
        
        if (b_resize) [o_delegate resizeQDPortFullscreen:b_fullscreen] ;
        if (b_release) [o_delegate releaseQDPort] ;
        if (b_request) [o_delegate requestQDPortFullscreen:b_fullscreen] ;

        return p_main->p_intf->b_die ;
    }




//Function for the GUI. 
    - (void) setQDPort:(CGrafPtr)p_qdport {
        vout_thread_t *p_vout;
        
        if ((p_vout = [self lockVout])) {
            p_vout->p_sys->p_qdport = p_qdport ;
            p_vout->p_sys->i_changes |= OSX_INTF_VOUT_QDPORT_CHANGE ;
            [self unlockVout] ;
        }
    }
    
    - (void) sizeChangeQDPort {
        vout_thread_t *p_vout;
        
        if ((p_vout = [self lockVout])) {
            p_vout->p_sys->i_changes |= OSX_INTF_VOUT_SIZE_CHANGE ;
            [self unlockVout] ;
        }
    }    

    - (NSSize) videoSize {
        return NSMakeSize(i_width, i_height) ;
    }




//Playback control
    - (void) setSpeed:(intf_speed_t) _e_speed {
        e_speed = _e_speed ;
        [self playlistPlayCurrent] ;
    }
    
    - (NSString *) getTimeAsString {
        static char psz_currenttime[ OFFSETTOTIME_MAX_SIZE ] ;
        
        if (!p_main->p_intf->p_input) return [NSString stringWithCString:"00:00:00"] ;
        
        input_OffsetToTime( p_main->p_intf->p_input, psz_currenttime, p_area->i_tell ) ;        
        return [NSString stringWithCString:psz_currenttime] ;
    }
    
    - (float) getTimeAsFloat {
        if (!p_main->p_intf->p_input) return 0.0 ;
    
        return (float)p_area->i_tell / (float)p_area->i_size ;
    }

    - (void) setTimeAsFloat:(float) f_position {
        if (!p_main->p_intf->p_input) return ;
    
        input_Seek(p_main->p_intf->p_input, p_area->i_size * f_position) ;
    }
    
    

    
//Playlist control
    - (NSArray*) playlistAsArray {
        NSMutableArray* p_list = [NSMutableArray arrayWithCapacity:p_main->p_playlist->i_size] ;
        int i ;
    
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
        for (i=0; i < p_main->p_playlist->i_size; i++)
            [p_list addObject:[NSString stringWithCString:p_main->p_playlist->p_item[i].psz_name]] ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;
        
        return [NSArray arrayWithArray:p_list] ;
    }

     - (int) playlistLength {
        return p_main->p_playlist->i_size ;
    }
    
    - (NSString*) playlistItem:(int) i_pos {
        NSString* o_item = nil ;
    
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
            if (i_pos < p_main->p_playlist->i_size)
                o_item = [NSString stringWithCString:p_main->p_playlist->p_item[i_pos].psz_name] ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;
                
        return o_item ;
    }
    
    - (bool) playlistPlayCurrent {
        if (p_main->p_intf->p_input) {
            switch (e_speed)
            {
                case SPEED_SLOW:
                    input_SetStatus(p_main->p_intf->p_input, INPUT_STATUS_SLOWER) ;
                    break ;
                case SPEED_NORMAL:
                    input_SetStatus(p_main->p_intf->p_input, INPUT_STATUS_PLAY) ;
                    break ;
                case SPEED_FAST:
                    input_SetStatus(p_main->p_intf->p_input, INPUT_STATUS_FASTER) ;
                    break ;
            }
            p_main->p_playlist->b_stopped = 0 ;
        }
        else if (p_main->p_playlist->b_stopped) {
            if (p_main->p_playlist->i_size)
                intf_PlaylistJumpto(p_main->p_playlist, p_main->p_playlist->i_index) ;
            else
                return FALSE ;
        }
        
        return TRUE ;
    }

    - (void) playlistPause {
        if (p_main->p_intf->p_input)
            input_SetStatus(p_main->p_intf->p_input, INPUT_STATUS_PAUSE) ;
    }
    
    - (void) playlistStop {
        if (p_main->p_intf->p_input) p_main->p_intf->p_input->b_eof = 1 ;
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
            p_main->p_playlist->i_index-- ;
            p_main->p_playlist->b_stopped = 1 ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;
    }
    
    - (void) playlistPlayNext {
        [self playlistStop] ;
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
            p_main->p_playlist->i_index++ ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;
        [self playlistPlayCurrent] ;
    }
    
    - (void) playlistPlayPrev {
        [self playlistStop] ;
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
            p_main->p_playlist->i_index-- ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;
        [self playlistPlayCurrent] ;    
    }
    
    - (void) playlistPlayItem:(int)i_item {
        [self playlistStop] ;
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
            if (i_item < p_main->p_playlist->i_size)
                p_main->p_playlist->i_index-- ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;        
        [self playlistPlayCurrent] ;
    }
    
    - (void) playlistAdd:(NSString*)o_filename {
        intf_PlaylistAdd(p_main->p_playlist, PLAYLIST_END, [o_filename lossyCString]) ;
    }
    
    - (void) clearPlaylist {
        int i ;
    
        vlc_mutex_lock(&p_main->p_playlist->change_lock) ;
            for(i=0; i < p_main->p_playlist->i_size; i++)
                intf_PlaylistDelete(p_main->p_playlist, i) ;
        vlc_mutex_unlock(&p_main->p_playlist->change_lock) ;        
    }
    



// Private Functions. This are just some utilities for other functions
    - (struct vout_thread_s*) lockVout {
        vlc_mutex_lock(&p_vout_bank->lock) ;
        if (p_vout_bank->i_count) {
            vlc_mutex_lock(&p_vout_bank->pp_vout[0]->change_lock) ;
            return p_vout_bank->pp_vout[0] ;
        }
        else
        {
            vlc_mutex_unlock(&p_vout_bank->lock) ;
            return NULL ;
        }
    }
    
    - (void) unlockVout {
        vlc_mutex_unlock(&p_vout_bank->pp_vout[0]->change_lock) ;
        vlc_mutex_unlock(&p_vout_bank->lock) ;
    }
@end
