/*****************************************************************************
 * intf_vlc_wrapper.c: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.m,v 1.6 2002/05/19 23:51:37 massiot Exp $
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
                p_main->p_intf->p_sys->b_disabled_menus = 0;
            }
        }

        vlc_mutex_unlock( &p_input_bank->pp_input[0]->stream.stream_lock );
    }
    else if ( !p_main->p_intf->p_sys->b_disabled_menus )
    {
        [self setupMenus];
        p_main->p_intf->p_sys->b_disabled_menus = 1;
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

- (void)maxvolume
{
    if( p_aout_bank->pp_aout[0] == NULL ) return;

    if( p_main->p_intf->p_sys->b_mute )
    {
        p_main->p_intf->p_sys->i_saved_volume = VOLUME_MAX;
    }
    else
    {
        p_aout_bank->pp_aout[0]->i_volume = VOLUME_MAX;
    }
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

    if ( p_input_bank->pp_input[0] != NULL &&
         (p_input_bank->pp_input[0]->stream.i_method == INPUT_METHOD_VCD ||
          p_input_bank->pp_input[0]->stream.i_method == INPUT_METHOD_DVD ||
          p_input_bank->pp_input[0]->stream.i_method == INPUT_METHOD_DISC) )
    {
        intf_ErrMsg("error: cannot eject the disc while you're reading from it");
        return;
    }

    if ( o_devices == nil )
    {
        o_devices = GetEjectableMediaOfClass(kIOCDMediaClass);
    }

    if ( o_devices != nil && [o_devices count] )
    { 
        psz_device = [[o_devices objectAtIndex:0] cString];
        intf_Eject( psz_device );
    }
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

    if( p_input_bank->pp_input[0] != NULL )
    {
        f_time = (float)p_area->i_tell / (float)p_area->i_size;
    }    

    return( f_time );
}

- (void)setTimeAsFloat:(float)f_position
{
    if( p_input_bank->pp_input[0] != NULL )
    {
        input_Seek( p_input_bank->pp_input[0], p_area->i_size * f_position );
    }
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

- (void)toggleProgram:(id)sender
{
    NSMenuItem * o_item = (NSMenuItem *)sender;
    input_thread_t * p_input = p_input_bank->pp_input[0];

    if( [o_item state] == NSOffState )
    {
        u16 i_program_id = [o_item tag];

        input_ChangeProgram( p_input, i_program_id );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        [self setupMenus];
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        input_SetStatus( p_input, INPUT_STATUS_PLAY );
    }
}

- (void)toggleTitle:(id)sender
{
    NSMenuItem * o_item = (NSMenuItem *)sender;
    input_thread_t * p_input = p_input_bank->pp_input[0];

    if( [o_item state] == NSOffState )
    {
        int i_title = [o_item tag];

        input_ChangeArea( p_input,
                          p_input->stream.pp_areas[i_title] );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        [self setupMenus];
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        input_SetStatus( p_input, INPUT_STATUS_PLAY );
    }
}

- (void)toggleChapter:(id)sender
{
    NSMenuItem * o_item = (NSMenuItem *)sender;
    input_thread_t * p_input = p_input_bank->pp_input[0];

    if( [o_item state] == NSOffState )
    {
        int i_chapter = [o_item tag];

        p_input->stream.p_selected_area->i_part = i_chapter;
        input_ChangeArea( p_input,
                          p_input->stream.p_selected_area );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        [self setupMenus];
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        input_SetStatus( p_input, INPUT_STATUS_PLAY );
    }
}

- (void)toggleLanguage:(id)sender
{
    NSMenuItem * o_item = (NSMenuItem *)sender;
    input_thread_t * p_input = p_input_bank->pp_input[0];

    int i_es = [o_item tag];

    if( [o_item state] == NSOnState )
    {
        /* We just have one ES to disable */
        input_ToggleES( p_input, p_input->stream.pp_es[i_es], 0 );
    }
    else
    {
        /* Unselect the selected ES in the same class */
        int i;
        vlc_mutex_lock( &p_input->stream.stream_lock );
        for( i = 0; i < p_input->stream.i_selected_es_number; i++ )
        {
            if( p_input->stream.pp_selected_es[i]->i_cat == AUDIO_ES )
            {
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                input_ToggleES( p_input, p_input->stream.pp_selected_es[i], 0 );
                vlc_mutex_lock( &p_input->stream.stream_lock );
                break;
            }
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        /* Select the wanted ES */
        input_ToggleES( p_input, p_input->stream.pp_es[i_es], 1 );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    [self setupMenus];
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    input_SetStatus( p_input, INPUT_STATUS_PLAY );
}

- (void)toggleSubtitle:(id)sender
{
    NSMenuItem * o_item = (NSMenuItem *)sender;
    input_thread_t * p_input = p_input_bank->pp_input[0];

    int i_es = [o_item tag];

    if( [o_item state] == NSOnState )
    {
        /* We just have one ES to disable */
        input_ToggleES( p_input, p_input->stream.pp_es[i_es], 0 );
    }
    else
    {
        /* Unselect the selected ES in the same class */
        int i;
        vlc_mutex_lock( &p_input->stream.stream_lock );
        for( i = 0; i < p_input->stream.i_selected_es_number; i++ )
        {
            if( p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
            {
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                input_ToggleES( p_input, p_input->stream.pp_selected_es[i], 0 );
                vlc_mutex_lock( &p_input->stream.stream_lock );
                break;
            }
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );

        /* Select the wanted ES */
        input_ToggleES( p_input, p_input->stream.pp_es[i_es], 1 );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    [self setupMenus];
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    input_SetStatus( p_input, INPUT_STATUS_PLAY );
}

- (void)setupMenus
{
    NSMenu *o_main_menu;
    NSMenuItem *o_controls_item;
    NSMenuItem *o_program_item, *o_title_item, *o_chapter_item, *o_language_item,
               *o_subtitle_item;
    input_thread_t * p_input = p_input_bank->pp_input[0];

    o_main_menu  = [NSApp mainMenu];
    o_controls_item  = [o_main_menu itemWithTitle: @"Controls"];
    o_program_item = [[o_controls_item submenu] itemWithTitle: @"Program"]; 
    o_title_item = [[o_controls_item submenu] itemWithTitle: @"Title"]; 
    o_chapter_item = [[o_controls_item submenu] itemWithTitle: @"Chapter"]; 
    o_language_item = [[o_controls_item submenu] itemWithTitle: @"Language"]; 
    o_subtitle_item = [[o_controls_item submenu] itemWithTitle: @"Subtitles"]; 

    if( p_input == NULL )
    {
        [o_program_item setEnabled:0];
        [o_title_item setEnabled:0];
        [o_chapter_item setEnabled:0];
        [o_language_item setEnabled:0];
        [o_subtitle_item setEnabled:0];
    }
    else
    {
        NSMenu *o_program, *o_title, *o_chapter, *o_language, *o_subtitle;
        SEL pf_toggle_program, pf_toggle_title, pf_toggle_chapter,
            pf_toggle_language, pf_toggle_subtitle;

        int i, i_nb_items;
        pgrm_descriptor_t * p_pgrm;

        /* ----- PROGRAMS ----- */
        if( p_input->stream.i_pgrm_number < 2 )
        {
            [o_program_item setEnabled:0];
        }
        else
        {
            [o_program_item setEnabled:1];
            o_program = [o_program_item submenu];
            pf_toggle_program = @selector(toggleProgram:);
    
            /* Remove previous program menu */
            i_nb_items = [o_program numberOfItems];
            for( i = 0; i < i_nb_items; i++ )
            {
                [o_program removeItemAtIndex:0];
            }
    
            if( p_input->stream.p_new_program )
            {
                p_pgrm = p_input->stream.p_new_program;
            }
            else
            {
                p_pgrm = p_input->stream.p_selected_program;
            }
    
            /* Create program menu */
            for( i = 0 ; i < p_input->stream.i_pgrm_number ; i++ )
            {
                char psz_title[ 256 ];
                NSString * o_menu_title;
                NSMenuItem * o_item;
    
                snprintf( psz_title, sizeof(psz_title), "id %d",
                    p_input->stream.pp_programs[i]->i_number );
                psz_title[sizeof(psz_title) - 1] = '\0';
    
                o_menu_title = [NSString stringWithCString: psz_title];
    
                o_item = [o_program addItemWithTitle: o_menu_title
                        action: pf_toggle_program keyEquivalent: @""];
                [o_item setTarget: self];
                [o_item setTag: p_input->stream.pp_programs[i]->i_number];
                if( p_pgrm == p_input->stream.pp_programs[i] )
                {
                    [o_item setState: 1];
                }
            }
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* ----- TITLES ----- */
        if( p_input->stream.i_area_nb < 2 )
        {
            [o_title_item setEnabled:0];
        }
        else
        {
            [o_title_item setEnabled:1];
            o_title = [o_title_item submenu];
            pf_toggle_title = @selector(toggleTitle:);
    
            /* Remove previous title menu */
            i_nb_items = [o_title numberOfItems];
            for( i = 0; i < i_nb_items; i++ )
            {
                [o_title removeItemAtIndex:0];
            }
    
            /* Create title menu */
            for( i = 1 ; i < p_input->stream.i_area_nb ; i++ )
            {
                char psz_title[ 256 ];
                NSString * o_menu_title;
                NSMenuItem * o_item;
    
                snprintf( psz_title, sizeof(psz_title), "Title %d (%d)", i,
                    p_input->stream.pp_areas[i]->i_part_nb );
                psz_title[sizeof(psz_title) - 1] = '\0';
    
                o_menu_title = [NSString stringWithCString: psz_title];
    
                o_item = [o_title addItemWithTitle: o_menu_title
                        action: pf_toggle_title keyEquivalent: @""];
                [o_item setTag: i];
                [o_item setTarget: self];
                if( ( p_input->stream.pp_areas[i] ==
                    p_input->stream.p_selected_area ) )
                {
                    [o_item setState: 1];
                }
            }
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* ----- CHAPTERS ----- */
        if( p_input->stream.p_selected_area->i_part_nb < 2 )
        {
            [o_chapter_item setEnabled:0];
        }
        else
        {
            [o_chapter_item setEnabled:1];
            o_chapter = [o_chapter_item submenu];
            pf_toggle_chapter = @selector(toggleChapter:);
    
            /* Remove previous chapter menu */
            i_nb_items = [o_chapter numberOfItems];
            for( i = 0; i < i_nb_items; i++ )
            {
                [o_chapter removeItemAtIndex:0];
            }
    
            /* Create chapter menu */
            for( i = 0 ; i < p_input->stream.p_selected_area->i_part_nb ; i++ )
            {
                char psz_title[ 256 ];
                NSString * o_menu_title;
                NSMenuItem * o_item;
    
                snprintf( psz_title, sizeof(psz_title), "Chapter %d", i + 1 );
                psz_title[sizeof(psz_title) - 1] = '\0';
    
                o_menu_title = [NSString stringWithCString: psz_title];
    
                o_item = [o_chapter addItemWithTitle: o_menu_title
                        action: pf_toggle_chapter keyEquivalent: @""];
                [o_item setTag: i];
                [o_item setTarget: self];
                if( ( p_input->stream.p_selected_area->i_part == i + 1 ) )
                {
                    [o_item setState: 1];
                }
            }
        }
        p_main->p_intf->p_sys->i_part = p_input->stream.p_selected_area->i_part;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* ----- LANGUAGES & SUBTITLES ----- */
        o_language = [o_language_item submenu];
        o_subtitle = [o_subtitle_item submenu];
        pf_toggle_language = @selector(toggleLanguage:);
        pf_toggle_subtitle = @selector(toggleSubtitle:);

        /* Remove previous language menu */
        i_nb_items = [o_language numberOfItems];
        for( i = 0; i < i_nb_items; i++ )
        {
            [o_language removeItemAtIndex:0];
        }

        /* Remove previous subtitle menu */
        i_nb_items = [o_subtitle numberOfItems];
        for( i = 0; i < i_nb_items; i++ )
        {
            [o_subtitle removeItemAtIndex:0];
        }

        /* Create language & subtitles menus */
        for( i = 0 ; i < p_input->stream.i_es_number ; i++ )
        {
            es_descriptor_t * p_es = p_input->stream.pp_es[i];
            if( p_es->p_pgrm != NULL
                 && p_es->p_pgrm != p_input->stream.p_selected_program )
            {
                continue;
            }

            if( p_es->i_cat == AUDIO_ES )
            {
                NSString * o_menu_title;
                NSMenuItem * o_item;

                if( *p_es->psz_desc )
                {
                    o_menu_title = [NSString stringWithCString: p_es->psz_desc];
                }
                else
                {
                    char psz_title[ 256 ];
                    snprintf( psz_title, sizeof(psz_title), "Language 0x%x",
                              p_es->i_id );
                    psz_title[sizeof(psz_title) - 1] = '\0';
    
                    o_menu_title = [NSString stringWithCString: psz_title];
                }
    
                o_item = [o_language addItemWithTitle: o_menu_title
                        action: pf_toggle_language keyEquivalent: @""];
                [o_item setTag: i];
                [o_item setTarget: self];
                if( p_es->p_decoder_fifo != NULL )
                {
                    [o_item setState: 1];
                }
            }
            else if( p_es->i_cat == SPU_ES )
            {
                NSString * o_menu_title;
                NSMenuItem * o_item;

                if( *p_es->psz_desc )
                {
                    o_menu_title = [NSString stringWithCString: p_es->psz_desc];
                }
                else
                {
                    char psz_title[ 256 ];
                    snprintf( psz_title, sizeof(psz_title), "Subtitle 0x%x",
                              p_es->i_id );
                    psz_title[sizeof(psz_title) - 1] = '\0';
    
                    o_menu_title = [NSString stringWithCString: psz_title];
                }
    
                o_item = [o_subtitle addItemWithTitle: o_menu_title
                        action: pf_toggle_subtitle keyEquivalent: @""];
                [o_item setTag: i];
                [o_item setTarget: self];
                if( p_es->p_decoder_fifo != NULL )
                {
                    [o_item setState: 1];
                }
            }
        }

        if( [o_language numberOfItems] )
        {
            [o_language_item setEnabled: 1];
        }
        else
        {
            [o_language_item setEnabled: 0];
        }
        if( [o_subtitle numberOfItems] )
        {
            [o_subtitle_item setEnabled: 1];
        }
        else
        {
            [o_subtitle_item setEnabled: 0];
        }
        p_input->stream.b_changed = 0;
    }
}

@end
