/*****************************************************************************
 * info.m: MacOS X info panel
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: info.m,v 1.1 2003/02/17 10:52:07 hartman Exp $
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#include "intf.h"

#import "info.h"

/*****************************************************************************
 * VLCInfo implementation 
 *****************************************************************************/
@implementation VLCInfo

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        p_intf = [NSApp getIntf];
        
        o_info_strings = [[NSMutableDictionary alloc] init];
    }

    return( self );
}

- (void)dealloc
{
    [o_info_strings release];

    [super dealloc];
}

- (IBAction)toggleInfoPanel:(id)sender
{
    if ( [o_info_window isVisible] )
    {
        [o_info_window orderOut:sender];
    }
    else
    {
        [o_info_window orderFront:sender];
        [self updateInfo];
    }
}

- (IBAction)showCategory:(id)sender
{
    NSString *o_selected = [o_info_selector titleOfSelectedItem];
    [o_info_view setString:(NSString *)[o_info_strings objectForKey: o_selected]];
    [o_info_view setNeedsDisplay: YES];
}

- (void)updateInfo
{
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    
    if( p_playlist == NULL )
    {
        [o_info_window orderOut:self];
        return;
    }
    
    if ( p_playlist->p_input == NULL )
    {
        [o_info_window orderOut:self];
        vlc_object_release( p_playlist );
        return;
    }

    [o_info_strings removeAllObjects];
    [o_info_selector removeAllItems];
    [o_info_view setDrawsBackground: NO];
    [[[o_info_view superview] superview] setDrawsBackground: NO];
    [o_info_window setExcludedFromWindowsMenu:YES];
    
    vlc_mutex_lock( &p_playlist->p_input->stream.stream_lock );
    input_info_category_t *p_category = p_playlist->p_input->stream.p_info;
    while ( p_category )
    {
        [self createInfoView: p_category ];
        p_category = p_category->p_next;
    }
    vlc_mutex_unlock( &p_playlist->p_input->stream.stream_lock );
    vlc_object_release( p_playlist );
    
    [o_info_selector selectItemAtIndex: 0];
    [self showCategory:o_info_selector];
}

- (void)createInfoView:(input_info_category_t *)p_category
{
    /* Add a catecory */
    NSString *title = [NSString stringWithCString: p_category->psz_name];
    [o_info_selector addItemWithTitle: title];
    
    /* Create the textfield content */
    NSMutableString *catString = [NSMutableString string];
    
    /* Add the fields */
    input_info_t *p_info = p_category->p_info;
    while ( p_info )
    {
        [catString appendFormat: @"%s: %s\n\n", p_info->psz_name, p_info->psz_value];
        p_info = p_info->p_next;
    }
    [o_info_strings setObject: catString forKey: title];
}

@end

@implementation VLCInfo (NSMenuValidation)
 
- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;

    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
    }

    if( [[o_mi title] isEqualToString: _NS("Info")] )
    {
        if( p_playlist == NULL || p_playlist->p_input == NULL )
        {
            bEnabled = FALSE;
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