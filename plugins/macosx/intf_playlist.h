/*****************************************************************************
 * intf_playlist.h: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf_playlist.h,v 1.1 2002/07/15 01:54:03 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * VLCPlaylistView interface 
 *****************************************************************************/
@interface VLCPlaylistView : NSTableView
{

}

@end

/*****************************************************************************
 * VLCPlaylist interface 
 *****************************************************************************/
@interface VLCPlaylist : NSObject
{
    IBOutlet id o_panel; 
    IBOutlet id o_btn_close;
    IBOutlet id o_table_view;
}

- (void)appendArray:(NSArray*)o_array atPos:(int)i_pos;

@end

