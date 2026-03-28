/*****************************************************************************
 * macosx_dialogs.m
 *****************************************************************************
 * Copyright (C) 2026 the VideoLAN team
 *
 * Authors: Felix Paul Kühne <fkuehne@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Cocoa/Cocoa.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>

static void ShowDialog( intf_thread_t *, int, int, intf_dialog_args_t * );

static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    p_intf->pf_show_dialog = ShowDialog;
    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
}

static void OpenFilePanel( intf_thread_t *p_intf, bool b_play,
                           bool b_directories )
{
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            NSOpenPanel *panel = [NSOpenPanel openPanel];
            [panel setAllowsMultipleSelection:YES];
            [panel setCanChooseDirectories:b_directories];
            [panel setCanChooseFiles:!b_directories];

            if( b_directories )
                [panel setTitle:@"Open Directory"];
            else
                [panel setTitle:@"Open File"];

            if( [panel runModal] == NSModalResponseOK )
            {
                vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist( p_intf );
                vlc_playlist_Lock( playlist );

                for( NSURL *url in [panel URLs] )
                {
                    input_item_t *p_input = input_item_New(
                        [[url absoluteString] UTF8String], NULL );
                    if( p_input )
                    {
                        vlc_playlist_AppendOne( playlist, p_input );
                        input_item_Release( p_input );
                    }
                }

                if( b_play && vlc_playlist_Count( playlist ) > 0 )
                {
                    vlc_playlist_PlayAt( playlist,
                        vlc_playlist_Count( playlist ) - (int)[[panel URLs] count] );
                }

                vlc_playlist_Unlock( playlist );
            }
        }
    });
}

static void ShowDialog( intf_thread_t *p_intf, int i_dialog, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    switch( i_dialog )
    {
        case INTF_DIALOG_FILE_SIMPLE:
        case INTF_DIALOG_FILE:
            OpenFilePanel( p_intf, i_arg != 0, false );
            break;

        case INTF_DIALOG_DIRECTORY:
            OpenFilePanel( p_intf, i_arg != 0, true );
            break;

        case INTF_DIALOG_FILE_GENERIC:
            if( !p_arg )
                break;

            dispatch_sync(dispatch_get_main_queue(), ^{
                @autoreleasepool {
                    if( p_arg->b_save )
                    {
                        NSSavePanel *panel = [NSSavePanel savePanel];
                        if( p_arg->psz_title )
                            [panel setTitle:[NSString stringWithUTF8String:p_arg->psz_title]];

                        if( [panel runModal] == NSModalResponseOK )
                        {
                            const char *path = [[[panel URL] path] UTF8String];
                            p_arg->i_results = 1;
                            p_arg->psz_results = (char **)malloc( sizeof(char*) );
                            p_arg->psz_results[0] = strdup( path );
                        }
                    }
                    else
                    {
                        NSOpenPanel *panel = [NSOpenPanel openPanel];
                        if( p_arg->psz_title )
                            [panel setTitle:[NSString stringWithUTF8String:p_arg->psz_title]];
                        [panel setAllowsMultipleSelection:p_arg->b_multiple];
                        [panel setCanChooseDirectories:NO];
                        [panel setCanChooseFiles:YES];

                        if( [panel runModal] == NSModalResponseOK )
                        {
                            NSArray *urls = [panel URLs];
                            int count = (int)[urls count];
                            p_arg->i_results = count;
                            p_arg->psz_results = (char **)malloc( count * sizeof(char*) );
                            for( int i = 0; i < count; i++ )
                            {
                                p_arg->psz_results[i] = strdup(
                                    [[[urls objectAtIndex:i] path] UTF8String] );
                            }
                        }
                    }
                }
            });

            if( p_arg->pf_callback )
                p_arg->pf_callback( p_arg );
            break;

        default:
            break;
    }
}

vlc_module_begin()
    set_shortname( N_("macOS dialogs") )
    set_description( N_("macOS dialogs provider") )
    set_capability( "dialogs provider", 52 )
    set_callbacks( Open, Close )
vlc_module_end()
