/*****************************************************************************
 * intf_vlc_wrapper.h : MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $$
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

#import <Cocoa/Cocoa.h>

typedef enum intf_speed_e {SPEED_SLOW=0, SPEED_NORMAL, SPEED_FAST} intf_speed_t ;
@protocol VlcWrapper_Delegate
    - (void) requestQDPortFullscreen:(bool)b_fullscreen ;
    - (void) releaseQDPort ;
    - (void) resizeQDPortFullscreen:(bool)b_fullscreen ;
@end


// Intf_VlcWrapper is a singleton class (there is only one instance at any time)
@interface Intf_VlcWrapper : NSObject {    
    id<VlcWrapper_Delegate> o_delegate ;
    intf_speed_t e_speed ;
    
    unsigned int i_width, i_height ;
}

// Initialization,.... 
+ (Intf_VlcWrapper*) instance ;
- (Intf_VlcWrapper*) initWithDelegate:(id)o_delegate ;

- (bool) manage ;

//Function for the GUI. 
- (void) setQDPort:(CGrafPtr)p_qdport ;
- (void) sizeChangeQDPort ;
- (NSSize) videoSize ;

// Playback control
- (void) setSpeed:(intf_speed_t)e_speed ;
- (NSString*) getTimeAsString ;
- (float) getTimeAsFloat ;
- (void) setTimeAsFloat:(float)i_offset ;

// Playlist control
- (NSArray*) playlistAsArray ;
- (int) playlistLength ;
- (NSString*) playlistItem:(int) i_pos ;
- (bool) playlistPlayCurrent ;
- (void) playlistPause ;
- (void) playlistStop ;
- (void) playlistPlayNext ;
- (void) playlistPlayPrev ;
- (void) playlistPlayItem:(int)i_item ;
- (void) playlistAdd:(NSString*)o_filename ;
- (void) clearPlaylist ;
@end

