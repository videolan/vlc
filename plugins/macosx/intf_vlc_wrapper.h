/*****************************************************************************
 * intf_vlc_wrapper.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.h,v 1.4 2002/03/19 03:33:52 jlj Exp $
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

typedef enum intf_speed_e
{
    SPEED_SLOW = 0,
    SPEED_NORMAL,
    SPEED_FAST
} intf_speed_t;

/* Intf_VLCWrapper is a singleton class
    (only one instance at any time) */

@interface Intf_VLCWrapper : NSObject
{    
    intf_speed_t e_speed;
}

/* Initialization */
+ (Intf_VLCWrapper *)instance;

- (bool)manage;
- (void)quit;

/* Playback control */
- (void)setSpeed:(intf_speed_t)e_speed;
- (NSString *)getTimeAsString;
- (float)getTimeAsFloat;
- (void)setTimeAsFloat:(float)i_offset;

/* Playlist control */
- (NSArray *)playlistAsArray;
- (int)playlistLength;
- (NSString *)playlistItem:(int)i_pos;
- (bool)playlistPlayCurrent;
- (void)playlistPause;
- (void)playlistStop;
- (void)playlistPlayNext;
- (void)playlistPlayPrev;
- (void)playlistPlayItem:(int)i_item;
- (void)playlistAdd:(NSString *)o_filename;
- (void)clearPlaylist;
- (bool)playlistPlaying;

@end
