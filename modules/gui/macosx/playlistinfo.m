/*****************************************************************************
 * playlistinfo.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id: playlistinfo.m 7015 2004-03-08 15:22:58Z bigben $
 *
 * Authors: Benjmaib Pracht <bigben at videolan dot org>
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

#include "intf.h"
#include "playlistinfo.h"
#include "playlist.h"

/*****************************************************************************
 * VLCPlaylistInfo Implementation
 *****************************************************************************/

@implementation VLCPlaylistInfo

- (void)awakeFromNib
{

    [o_info_window setExcludedFromWindowsMenu: TRUE];
    [o_tbv_info setOutlineTableColumn:0];
    [o_tbv_info setDataSource: self];

    [o_info_window setTitle: _NS("Proprieties")];
    [o_uri_lbl setStringValue: _NS("URI")];
    [o_title_lbl setStringValue: _NS("Title")];
    [o_author_lbl setStringValue: _NS("Author")];
    [o_btn_info_ok setTitle: _NS("OK")];
    [o_btn_info_cancel setTitle: _NS("Cancel")];
}

- (IBAction)togglePlaylistInfoPanel:(id)sender
{
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist;

    if( [o_info_window isVisible] )
    {
        [o_info_window orderOut: sender];
    }
    else
    {
        p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );

        if (p_playlist)
        {
            /*fill uri / title / author info */
            int i_item = [o_vlc_playlist selectedPlaylistItem];
            [o_uri_txt setStringValue:[NSString stringWithUTF8String: p_playlist->pp_items[i_item]->psz_uri]];
            [o_title_txt setStringValue:[NSString stringWithUTF8String: p_playlist->pp_items[i_item]->psz_name]];
            [o_author_txt setStringValue:[NSString stringWithUTF8String: playlist_GetInfo(p_playlist, i_item ,_("General"),_("Author") )]];

            /*fill info outline*/
//            [o_tbv_info 
            vlc_object_release ( p_playlist );
        }
        [o_info_window makeKeyAndOrderFront: sender];
    }
}

- (IBAction)infoCancel:(id)sender
{
    [self togglePlaylistInfoPanel:self];
}


- (IBAction)infoOk:(id)sender
{
    int i_item = [o_vlc_playlist selectedPlaylistItem];
    intf_thread_t * p_intf = [NSApp getIntf];
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                          FIND_ANYWHERE );
    vlc_value_t val;

    if (p_playlist)                       
    {
        vlc_mutex_lock(&p_playlist->pp_items[i_item]->lock);
        
        p_playlist->pp_items[i_item]->psz_uri = strdup([[o_uri_txt stringValue] cString]);
        p_playlist->pp_items[i_item]->psz_name = strdup([[o_title_txt stringValue] cString]);
        playlist_ItemAddInfo(p_playlist->pp_items[i_item],_("General"),_("Author"), [[o_author_txt stringValue] cString]);
 
        vlc_mutex_unlock(&p_playlist->pp_items[i_item]->lock);
        val.b_bool = VLC_TRUE;
        var_Set( p_playlist,"int-change",val );
        vlc_object_release ( p_playlist );
    } 
    [self togglePlaylistInfoPanel:self];
    
}   

@end

