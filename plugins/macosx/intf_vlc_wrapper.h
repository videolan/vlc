/*****************************************************************************
 * intf_vlc_wrapper.h: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.h,v 1.5 2002/04/23 03:21:21 jlj Exp $
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

/* Intf_VLCWrapper is a singleton class
    (only one instance at any time) */

@interface Intf_VLCWrapper : NSObject
{

}

/* initialization */
+ (Intf_VLCWrapper *)instance;

- (bool)manage;
- (void)quit;

/* playback control */
- (bool)playlistPlay;
- (void)playlistPause;
- (void)playlistStop;
- (void)playlistNext;
- (void)playlistPrev;

- (void)playSlower;
- (void)playFaster;

/* playback info */
- (NSString *)getTimeAsString;
- (float)getTimeAsFloat;
- (void)setTimeAsFloat:(float)i_offset;

- (bool)playlistPlaying;
- (NSArray *)playlistAsArray;

/*
- (int)playlistLength;
- (NSString *)playlistItem:(int)i_pos;
- (void)playlistPlayItem:(int)i_item;
- (void)playlistAdd:(NSString *)o_filename;
- (void)clearPlaylist;
*/

/* open file/disc/network */
- (void)openFiles:(NSArray*)o_files;
- (void)openDisc:(NSString*)o_type device:(NSString*)o_device title:(int)i_title chapter:(int)i_chapter;
- (void)openNet:(NSString*)o_protocol addr:(NSString*)o_addr port:(int)i_port baddr:(NSString*)o_baddr;
- (void)openNetChannel:(NSString*)o_addr port:(int)i_port;

@end
