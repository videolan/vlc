/*****************************************************************************
 * intf.m: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf.m,v 1.17 2003/01/04 04:11:08 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>

#include <QuickTime/QuickTime.h>

#include "intf.h"
#include "vout.h"
#include "prefs.h"
#include "playlist.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{   
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    }

    memset( p_intf->p_sys, 0, sizeof( *p_intf->p_sys ) );

    p_intf->p_sys->o_pool = [[NSAutoreleasePool alloc] init];
    p_intf->p_sys->o_sendport = [[NSPort port] retain];

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    p_intf->pf_run = Run;

    [[VLCApplication sharedApplication] autorelease];
    [NSApp initIntlSupport];
    [NSApp setIntf: p_intf];

    [NSBundle loadNibNamed: @"MainMenu" owner: NSApp];

    return( 0 );
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    [p_intf->p_sys->o_sendport release];
    [p_intf->p_sys->o_pool release];

    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    [NSApp run];
}

/*****************************************************************************
 * VLCApplication implementation 
 *****************************************************************************/
@implementation VLCApplication

- (id)init
{
    /* default encoding: ISO-8859-1 */
    i_encoding = NSISOLatin1StringEncoding;

    return( [super init] );
}

- (void)initIntlSupport
{
    char *psz_lang = getenv( "LANG" );

    if( psz_lang == NULL )
    {
        return;
    }

    if( strncmp( psz_lang, "pl", 2 ) == 0 )
    {
        i_encoding = NSISOLatin2StringEncoding;
    }
    else if( strncmp( psz_lang, "ja", 2 ) == 0 ) 
    {
        i_encoding = NSJapaneseEUCStringEncoding;
    }
    else if( strncmp( psz_lang, "ru", 2 ) == 0 )
    {
#define CFSENC2NSSENC(e) CFStringConvertEncodingToNSStringEncoding(e)
        i_encoding = CFSENC2NSSENC( kCFStringEncodingKOI8_R ); 
#undef CFSENC2NSSENC
    }
}

- (NSString *)localizedString:(char *)psz
{
    UInt32 uiLength = (UInt32)strlen( psz );
    NSData * o_data = [NSData dataWithBytes: psz length: uiLength];
    NSString *o_str = [[NSString alloc] initWithData: o_data
                                        encoding: i_encoding];
    return( [o_str autorelease] );
}

- (void)setIntf:(intf_thread_t *)_p_intf
{
    p_intf = _p_intf;
}

- (intf_thread_t *)getIntf
{
    return( p_intf );
}

- (void)terminate:(id)sender
{
    [self getIntf]->p_vlc->b_die = VLC_TRUE;
}

@end

/*****************************************************************************
 * VLCMain implementation 
 *****************************************************************************/
@implementation VLCMain

- (id)init
{
    self = [super init];

    if( self != nil )
    {
        o_prefs = nil;
    }

    return( self ); 
}

- (void)awakeFromNib
{
    NSString * pTitle = [NSString
        stringWithCString: VOUT_TITLE " (Cocoa)"];

    [o_window setTitle: pTitle];

    /* button controls */
    [o_btn_playlist setToolTip: _NS("Playlist")];
    [o_btn_prev setToolTip: _NS("Previous")];
    [o_btn_slower setToolTip: _NS("Slower")];
    [o_btn_play setToolTip: _NS("Play")];
    [o_btn_pause setToolTip: _NS("Pause")];
    [o_btn_stop setToolTip: _NS("Stop")];
    [o_btn_faster setToolTip: _NS("Faster")];
    [o_btn_next setToolTip: _NS("Next")];
    [o_btn_prefs setToolTip: _NS("Preferences")];

    /* messages panel */ 
    [o_msgs_panel setTitle: _NS("Messages")];
    [o_msgs_btn_ok setTitle: _NS("Close")];

    /* main menu */
    [o_mi_about setTitle: _NS("About vlc")];
    [o_mi_prefs setTitle: _NS("Preferences")];
    [o_mi_hide setTitle: _NS("Hide vlc")];
    [o_mi_hide_others setTitle: _NS("Hide Others")];
    [o_mi_show_all setTitle: _NS("Show All")];
    [o_mi_quit setTitle: _NS("Quit vlc")];

    [o_mu_file setTitle: _NS("File")];
    [o_mi_open_generic setTitle: _NS("Open...")];
    [o_mi_open_file setTitle: _NS("Open File...")];
    [o_mi_open_disc setTitle: _NS("Open Disc...")];
    [o_mi_open_net setTitle: _NS("Open Network...")];
    [o_mi_open_recent setTitle: _NS("Open Recent")];
    [o_mi_open_recent_cm setTitle: _NS("Clear Menu")];

    [o_mu_edit setTitle: _NS("Edit")];
    [o_mi_cut setTitle: _NS("Cut")];
    [o_mi_copy setTitle: _NS("Copy")];
    [o_mi_paste setTitle: _NS("Paste")];
    [o_mi_clear setTitle: _NS("Clear")];
    [o_mi_select_all setTitle: _NS("Select All")];

    [o_mu_view setTitle: _NS("View")];
    [o_mi_playlist setTitle: _NS("Playlist")];
    [o_mi_messages setTitle: _NS("Messages")];

    [o_mu_controls setTitle: _NS("Controls")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_pause setTitle: _NS("Pause")];
    [o_mi_stop setTitle: _NS("Stop")];
    [o_mi_faster setTitle: _NS("Faster")];
    [o_mi_slower setTitle: _NS("Slower")];
    [o_mi_previous setTitle: _NS("Previous")];
    [o_mi_next setTitle: _NS("Next")];
    [o_mi_loop setTitle: _NS("Loop")];
    [o_mi_vol_up setTitle: _NS("Volume Up")];
    [o_mi_vol_down setTitle: _NS("Volume Down")];
    [o_mi_mute setTitle: _NS("Mute")];
    [o_mi_channels setTitle: _NS("Channels")];
    [o_mi_device setTitle: _NS("Device")];
    [o_mi_fullscreen setTitle: _NS("Fullscreen")];
    [o_mi_screen setTitle: _NS("Screen")];
    [o_mi_deinterlace setTitle: _NS("Deinterlace")];
    [o_mi_program setTitle: _NS("Program")];
    [o_mi_title setTitle: _NS("Title")];
    [o_mi_chapter setTitle: _NS("Chapter")];
    [o_mi_language setTitle: _NS("Language")];
    [o_mi_subtitle setTitle: _NS("Subtitles")];

    [o_mu_window setTitle: _NS("Window")];
    [o_mi_minimize setTitle: _NS("Minimize")];
    [o_mi_bring_atf setTitle: _NS("Bring All to Front")];

    /* dock menu */
    [o_dmi_play setTitle: _NS("Play")];
    [o_dmi_pause setTitle: _NS("Pause")];
    [o_dmi_stop setTitle: _NS("Stop")];

    [self manageMode];
}

- (void)applicationWillFinishLaunching:(NSNotification *)o_notification
{
    intf_thread_t * p_intf = [NSApp getIntf];

    f_slider_old = f_slider = 0.0;
    o_slider_lock = [[NSLock alloc] init];

    [NSThread detachNewThreadSelector: @selector(manage)
        toTarget: self withObject: nil];

    [p_intf->p_sys->o_sendport setDelegate: self];
    [[NSRunLoop currentRunLoop] 
        addPort: p_intf->p_sys->o_sendport
        forMode: NSDefaultRunLoopMode];
}

- (BOOL)application:(NSApplication *)o_app openFile:(NSString *)o_filename
{
    [o_playlist appendArray:
        [NSArray arrayWithObject: o_filename] atPos: -1];

    return( TRUE );
}

- (void)manage
{
    NSDate * o_sleep_date;
    intf_thread_t * p_intf = [NSApp getIntf];
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    while( !p_intf->b_die )
    {
        int i_start, i_stop;

        vlc_mutex_lock( &p_intf->change_lock );

        /* update the input */
        if( p_intf->p_sys->p_input == NULL )
        {
            p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                              FIND_ANYWHERE );
        }
        else if( p_intf->p_sys->p_input->b_dead )
        {
            vlc_object_release( p_intf->p_sys->p_input );
            p_intf->p_sys->p_input = NULL;
        }

        if( p_intf->p_sys->p_input != NULL )
        {
            vlc_bool_t b_need_menus = 0;
            input_thread_t * p_input = p_intf->p_sys->p_input;
            aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                        FIND_ANYWHERE );
            vout_thread_t * p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                      FIND_ANYWHERE );

            /* Disable screen saver. */
            UpdateSystemActivity( UsrActivity );

            vlc_mutex_lock( &p_input->stream.stream_lock );

            if( !p_input->b_die )
            {
                [self displayTime];

                /* New input or stream map change */
                if( p_input->stream.b_changed )
                {
                    [self manageMode];
                    b_need_menus = 1;
                    p_intf->p_sys->b_playing = 1;
                }

                if( p_intf->p_sys->i_part !=
                    p_input->stream.p_selected_area->i_part )
                {
                    p_intf->p_sys->b_chapter_update = 1;
                    b_need_menus = 1;
                }
            }

            if ( p_aout != NULL )
            {
                vlc_value_t val;
                if ( var_Get( (vlc_object_t *)p_aout, "intf-change", &val )
                      >= 0 && val.b_bool )
                {
                    p_intf->p_sys->b_aout_update = 1;
                    b_need_menus = 1;
                }
                vlc_object_release( (vlc_object_t *)p_aout );
            }

            if ( p_vout != NULL )
            {
                vlc_value_t val;
                if ( var_Get( (vlc_object_t *)p_vout, "intf-change", &val )
                      >= 0 && val.b_bool )
                {
                    p_intf->p_sys->b_vout_update = 1;
                    b_need_menus = 1;
                }
                vlc_object_release( (vlc_object_t *)p_vout );
            }

            if ( b_need_menus )
                [self setupMenus];

            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        else if( p_intf->p_sys->b_playing && !p_intf->b_die )
        {
            [self displayTime];
            [self manageMode];
            p_intf->p_sys->b_playing = 0;
        }

        /* update the log window */
        vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
        i_stop = *p_intf->p_sys->p_sub->pi_stop;
        vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );

        if( p_intf->p_sys->p_sub->i_start != i_stop )
        {
            NSColor *o_white = [NSColor whiteColor];
            NSColor *o_red = [NSColor redColor];
            NSColor *o_yellow = [NSColor yellowColor];
            NSColor *o_gray = [NSColor grayColor];

            unsigned int ui_length = [[o_messages string] length];

            NSColor * pp_color[4] = { o_white, o_red, o_yellow, o_gray };
            static const char * ppsz_type[4] = { ": ", " error: ", 
                                                 " warning: ", " debug: " }; 
        
            [o_messages setEditable: YES];
            [o_messages setSelectedRange: NSMakeRange( ui_length, 0 )];
            [o_messages scrollRangeToVisible: NSMakeRange( ui_length, 0 )];

            for( i_start = p_intf->p_sys->p_sub->i_start;
                 i_start != i_stop;
                 i_start = (i_start+1) % VLC_MSG_QSIZE )
            {
                NSString *o_msg;
                NSDictionary *o_attr;
                NSAttributedString *o_msg_color;
                int i_type = p_intf->p_sys->p_sub->p_msg[i_start].i_type;

                o_attr = [NSDictionary dictionaryWithObject: o_gray
                    forKey: NSForegroundColorAttributeName];
                o_msg = [NSString stringWithFormat: @"%s%s",
                    p_intf->p_sys->p_sub->p_msg[i_start].psz_module, 
                    ppsz_type[i_type]];
                o_msg_color = [[NSAttributedString alloc]
                    initWithString: o_msg attributes: o_attr];
                [o_messages insertText: o_msg_color];

                o_attr = [NSDictionary dictionaryWithObject: pp_color[i_type]
                    forKey: NSForegroundColorAttributeName];
                o_msg = [NSString stringWithCString:
                    p_intf->p_sys->p_sub->p_msg[i_start].psz_msg];
                o_msg_color = [[NSAttributedString alloc]
                    initWithString: o_msg attributes: o_attr];
                [o_messages insertText: o_msg_color];

                [o_messages insertText: @"\n"];
            }

            [o_messages setEditable: NO];

            vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
            p_intf->p_sys->p_sub->i_start = i_start;
            vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );
        }

        vlc_mutex_unlock( &p_intf->change_lock );

        o_sleep_date = [NSDate dateWithTimeIntervalSinceNow: 0.1];
        [NSThread sleepUntilDate: o_sleep_date];
    }

    [self terminate];

    [o_pool release];
}

- (void)terminate
{
    NSEvent * pEvent;
    vout_thread_t * p_vout;
    playlist_t * p_playlist;
    intf_thread_t * p_intf = [NSApp getIntf];

    /* release input */
    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    /*
     * Free playlists
     */
    msg_Dbg( p_intf, "removing all playlists" );
    while( (p_playlist = vlc_object_find( p_intf->p_vlc, VLC_OBJECT_PLAYLIST,
                                          FIND_CHILD )) )
    {
        vlc_object_detach( p_playlist );
        vlc_object_release( p_playlist );
        playlist_Destroy( p_playlist );
    }

    /*
     * Free video outputs
     */
    msg_Dbg( p_intf, "removing all video outputs" );
    while( (p_vout = vlc_object_find( p_intf->p_vlc, 
                                      VLC_OBJECT_VOUT, FIND_CHILD )) )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        vout_Destroy( p_vout );
    }

    [o_slider_lock release];

    if( o_prefs != nil )
    {
        [o_prefs release];
        o_prefs = nil;
    }

    [NSApp stop: nil];

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];

    /* send a dummy event to break out of the event loop */
    pEvent = [NSEvent mouseEventWithType: NSLeftMouseDown
                location: NSMakePoint( 1, 1 ) modifierFlags: 0
                timestamp: 1 windowNumber: [[NSApp mainWindow] windowNumber]
                context: [NSGraphicsContext currentContext] eventNumber: 1
                clickCount: 1 pressure: 0.0];
    [NSApp postEvent: pEvent atStart: YES];
}

- (void)manageMode
{
    vlc_bool_t b_input;
    vlc_bool_t b_plmul = 0;
    vlc_bool_t b_control = 0;
    playlist_t * p_playlist = NULL;
    intf_thread_t * p_intf = [NSApp getIntf];

    if( ( b_input = ( p_intf->p_sys->p_input != NULL ) ) )
    {
        /* control buttons for free pace streams */
        b_control = p_intf->p_sys->p_input->stream.b_pace_control;

        /* get ready for menu regeneration */
        p_intf->p_sys->b_program_update = 1;
        p_intf->p_sys->b_title_update = 1;
        p_intf->p_sys->b_chapter_update = 1;
        p_intf->p_sys->b_audio_update = 1;
        p_intf->p_sys->b_spu_update = 1;
        p_intf->p_sys->i_part = 0;

        p_intf->p_sys->p_input->stream.b_changed = 0;
        msg_Dbg( p_intf, "stream has changed, refreshing interface" );
    }
    else
    {
        /* unsensitize menus */
        [o_mi_program setEnabled: FALSE];
        [o_mi_title setEnabled: FALSE];
        [o_mi_chapter setEnabled: FALSE];
        [o_mi_language setEnabled: FALSE];
        [o_mi_subtitle setEnabled: FALSE];
        [o_mi_channels setEnabled: FALSE];
        [o_mi_device setEnabled: FALSE];
        [o_mi_screen setEnabled: FALSE];
    }

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, 
                                              FIND_ANYWHERE ); 
    if( p_playlist != NULL )
    {
        vlc_mutex_lock( &p_playlist->object_lock );
        b_plmul = p_playlist->i_size > 1;
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
    }

    /* set control items */
    [o_btn_stop setEnabled: b_input];
    [o_btn_pause setEnabled: b_control];
    [o_btn_faster setEnabled: b_control];
    [o_btn_slower setEnabled: b_control];
    [o_btn_prev setEnabled: b_plmul];
    [o_btn_next setEnabled: b_plmul];

    if ( (p_intf->p_sys->b_loop = config_GetInt( p_intf, "loop" )) )
    {
        [o_mi_loop setState: NSOnState];
    }
    else
    {
        [o_mi_loop setState: NSOffState];
    }
}

- (void)setupMenus
{
    unsigned int i, i_nb_items;
    NSMenuItem * o_item;
    NSString * o_menu_title;
    char psz_title[ 256 ];

    es_descriptor_t * p_audio_es = NULL;
    es_descriptor_t * p_spu_es = NULL;

    intf_thread_t * p_intf = [NSApp getIntf];

    p_intf->p_sys->b_chapter_update |= p_intf->p_sys->b_title_update;
    p_intf->p_sys->b_audio_update |= p_intf->p_sys->b_title_update |
                                     p_intf->p_sys->b_program_update;
    p_intf->p_sys->b_spu_update |= p_intf->p_sys->b_title_update |
                                   p_intf->p_sys->b_program_update;

#define p_input (p_intf->p_sys->p_input)

    if( p_intf->p_sys->b_program_update )
    {
        NSMenu * o_program;
        SEL pf_toggle_program;
        pgrm_descriptor_t * p_pgrm;

        if( p_input->stream.p_new_program )
        {
            p_pgrm = p_input->stream.p_new_program;
        }
        else
        {
            p_pgrm = p_input->stream.p_selected_program;
        }

        o_program = [o_mi_program submenu];
        pf_toggle_program = @selector(toggleProgram:);

        /* remove previous program items */
        i_nb_items = [o_program numberOfItems];
        for( i = 0; i < i_nb_items; i++ )
        {
            [o_program removeItemAtIndex: 0];
        }

        /* make (un)sensitive */
        [o_mi_program setEnabled: 
            p_input->stream.i_pgrm_number > 1];

        /* add program items */
        for( i = 0 ; i < p_input->stream.i_pgrm_number ; i++ )
        {
            snprintf( psz_title, sizeof(psz_title), "id %d",
                p_input->stream.pp_programs[i]->i_number );
            psz_title[sizeof(psz_title) - 1] = '\0';

            o_menu_title = [NSString stringWithCString: psz_title];

            o_item = [o_program addItemWithTitle: o_menu_title
                action: pf_toggle_program keyEquivalent: @""];
            [o_item setTag: p_input->stream.pp_programs[i]->i_number];
            [o_item setTarget: o_controls];

            if( p_pgrm == p_input->stream.pp_programs[i] )
            {
                [o_item setState: NSOnState];
            }
        }

        p_intf->p_sys->b_program_update = 0;
    }

    if( p_intf->p_sys->b_title_update )
    {
        NSMenu * o_title;
        SEL pf_toggle_title;

        o_title = [o_mi_title submenu];
        pf_toggle_title = @selector(toggleTitle:);

        /* remove previous title items */
        i_nb_items = [o_title numberOfItems];
        for( i = 0; i < i_nb_items; i++ )
        {
            [o_title removeItemAtIndex: 0];
        }

        /* make (un)sensitive */
        [o_mi_title setEnabled: 
            p_input->stream.i_area_nb > 1];

        /* add title items */
        for( i = 1 ; i < p_input->stream.i_area_nb ; i++ )
        {
            snprintf( psz_title, sizeof(psz_title), "Title %d (%d)", i,
                p_input->stream.pp_areas[i]->i_part_nb );
            psz_title[sizeof(psz_title) - 1] = '\0';

            o_menu_title = [NSString stringWithCString: psz_title];

            o_item = [o_title addItemWithTitle: o_menu_title
                action: pf_toggle_title keyEquivalent: @""];
            [o_item setTag: i];
            [o_item setTarget: o_controls];

            if( ( p_input->stream.pp_areas[i] ==
                p_input->stream.p_selected_area ) )
            {
                [o_item setState: NSOnState];
            }
        }

        p_intf->p_sys->b_title_update = 0;
    }

    if( p_intf->p_sys->b_chapter_update )
    {
        NSMenu * o_chapter;
        SEL pf_toggle_chapter;

        o_chapter = [o_mi_chapter submenu];
        pf_toggle_chapter = @selector(toggleChapter:);

        /* remove previous chapter items */
        i_nb_items = [o_chapter numberOfItems];
        for( i = 0; i < i_nb_items; i++ )
        {
            [o_chapter removeItemAtIndex: 0];
        }

        /* make (un)sensitive */
        [o_mi_chapter setEnabled: 
            p_input->stream.p_selected_area->i_part_nb > 1];

        /* add chapter items */
        for( i = 0 ; i < p_input->stream.p_selected_area->i_part_nb ; i++ )
        {
            snprintf( psz_title, sizeof(psz_title), "Chapter %d", i + 1 );
            psz_title[sizeof(psz_title) - 1] = '\0';

            o_menu_title = [NSString stringWithCString: psz_title];

            o_item = [o_chapter addItemWithTitle: o_menu_title
                action: pf_toggle_chapter keyEquivalent: @""];
            [o_item setTag: i + 1];
            [o_item setTarget: o_controls];

            if( ( p_input->stream.p_selected_area->i_part == i + 1 ) )
            {
                [o_item setState: NSOnState];
            }
        }

        p_intf->p_sys->i_part =
                p_input->stream.p_selected_area->i_part;

        p_intf->p_sys->b_chapter_update = 0;
    }

    for( i = 0 ; i < p_input->stream.i_selected_es_number ; i++ )
    {
        if( p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_audio_es = p_input->stream.pp_selected_es[i];
        }
        else if( p_input->stream.pp_selected_es[i]->i_cat == SPU_ES )
        {
            p_spu_es = p_input->stream.pp_selected_es[i];
        }
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( p_intf->p_sys->b_audio_update )
    {
        [self setupLangMenu: o_mi_language es: p_audio_es
            category: AUDIO_ES selector: @selector(toggleLanguage:)];

        p_intf->p_sys->b_audio_update = 0;
    }

    if( p_intf->p_sys->b_spu_update )
    {
        [self setupLangMenu: o_mi_subtitle es: p_spu_es
            category: SPU_ES selector: @selector(toggleLanguage:)];

        p_intf->p_sys->b_spu_update = 0;
    }

    if ( p_intf->p_sys->b_aout_update )
    {
        aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                    FIND_ANYWHERE );

        if ( p_aout != NULL )
        {
            vlc_value_t val;
            val.b_bool = 0;

            var_Set( (vlc_object_t *)p_aout, "intf-change", val );

            [self setupVarMenu: o_mi_channels target: (vlc_object_t *)p_aout
                var: "audio-channels" selector: @selector(toggleVar:)];

            [self setupVarMenu: o_mi_device target: (vlc_object_t *)p_aout
                var: "audio-device" selector: @selector(toggleVar:)];

            vlc_object_release( (vlc_object_t *)p_aout );
        }

        p_intf->p_sys->b_aout_update = 0;
    }

    if ( p_intf->p_sys->b_vout_update )
    {
        vout_thread_t * p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                  FIND_ANYWHERE );

        if ( p_vout != NULL )
        {
            vlc_value_t val;
            val.b_bool = 0;

            var_Set( (vlc_object_t *)p_vout, "intf-change", val );

            [self setupVarMenu: o_mi_screen target: (vlc_object_t *)p_vout
                var: "video-device" selector: @selector(toggleVar:)];

            vlc_object_release( (vlc_object_t *)p_vout );
        }

        p_intf->p_sys->b_vout_update = 0;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

#undef p_input
}

- (void)setupLangMenu:(NSMenuItem *)o_mi
                      es:(es_descriptor_t *)p_es
                      category:(int)i_cat
                      selector:(SEL)pf_callback
{
    unsigned int i, i_nb_items;
    NSMenu * o_menu = [o_mi submenu];
    intf_thread_t * p_intf = [NSApp getIntf];

    /* remove previous language items */
    i_nb_items = [o_menu numberOfItems];
    for( i = 0; i < i_nb_items; i++ )
    {
        [o_menu removeItemAtIndex: 0];
    }

    /* make sensitive : we can't change it after we build the menu, and
     * before, we don't yet how many items we will have. So make it
     * always sensitive. --Meuuh */
    [o_mi setEnabled: TRUE];

    vlc_mutex_lock( &p_intf->p_sys->p_input->stream.stream_lock );

#define ES p_intf->p_sys->p_input->stream.pp_es[i]
    for( i = 0 ; i < p_intf->p_sys->p_input->stream.i_es_number ; i++ )
    {
        if( ( ES->i_cat == i_cat ) &&
            ( !ES->p_pgrm ||
              ES->p_pgrm ==
                 p_intf->p_sys->p_input->stream.p_selected_program ) )
        {
            NSMenuItem * o_lmi;
            NSString * o_title;

            if( *ES->psz_desc )
            {
                o_title = [NSString stringWithCString: ES->psz_desc];
            }
            else
            {
                char psz_title[ 256 ];

                snprintf( psz_title, sizeof(psz_title), "Language 0x%x",
                          ES->i_id );
                psz_title[sizeof(psz_title) - 1] = '\0';

                o_title = [NSString stringWithCString: psz_title];
            }

            o_lmi = [o_menu addItemWithTitle: o_title
                action: pf_callback keyEquivalent: @""];
            [o_lmi setRepresentedObject: 
                [NSValue valueWithPointer: ES]];
            [o_lmi setTarget: o_controls];
            [o_lmi setTag: i_cat];

            if( /*p_es == ES*/ ES->p_decoder_fifo != NULL )
            {
                [o_lmi setState: NSOnState];
            }
        }
    }
#undef ES

    vlc_mutex_unlock( &p_intf->p_sys->p_input->stream.stream_lock );
}

- (void)setupVarMenu:(NSMenuItem *)o_mi
                     target:(vlc_object_t *)p_object
                     var:(const char *)psz_variable
                     selector:(SEL)pf_callback
{
    int i, i_nb_items;
    NSMenu * o_menu = [o_mi submenu];
    vlc_value_t val;
    char * psz_value;

    /* remove previous items */
    i_nb_items = [o_menu numberOfItems];
    for( i = 0; i < i_nb_items; i++ )
    {
        [o_menu removeItemAtIndex: 0];
    }

    if ( var_Get( p_object, psz_variable, &val ) < 0 )
    {
        return;
    }
    psz_value = val.psz_string;

    if ( var_Change( p_object, psz_variable,
                     VLC_VAR_GETLIST, &val ) < 0 )
    {
        free( psz_value );
        return;
    }

    /* make (un)sensitive */
    [o_mi setEnabled: (val.p_list->i_count > 0)];

    for ( i = 0; i < val.p_list->i_count; i++ )
    {
        NSMenuItem * o_lmi;
        NSString * o_title;

        o_title = [NSString stringWithCString: val.p_list->p_values[i].psz_string];
        o_lmi = [o_menu addItemWithTitle: o_title
                 action: pf_callback keyEquivalent: @""];
        /* FIXME: this isn't 64-bit clean ! */
        [o_lmi setTag: (int)psz_variable];
        [o_lmi setRepresentedObject:
            [NSValue valueWithPointer: p_object]];
        [o_lmi setTarget: o_controls];

        if ( !strcmp( psz_value, val.p_list->p_values[i].psz_string ) )
            [o_lmi setState: NSOnState];
    }

    var_Change( p_object, psz_variable, VLC_VAR_FREELIST,
                &val );

    free( psz_value );
}

- (IBAction)clearRecentItems:(id)sender
{
    [[NSDocumentController sharedDocumentController]
                          clearRecentDocuments: nil];
}

- (void)openRecentItem:(id)sender
{
    [self application: nil openFile: [sender title]]; 
}

- (IBAction)viewPreferences:(id)sender
{
    if( o_prefs == nil )
    {
        o_prefs = [[VLCPrefs alloc] init];
    }

    [o_prefs createPrefPanel: @"main"];
}

- (IBAction)timesliderUpdate:(id)sender
{
    switch( [[NSApp currentEvent] type] )
    {
        case NSLeftMouseDown:
            [o_slider_lock lock];
            break;

        case NSLeftMouseUp:
            f_slider = [sender floatValue];
            [o_slider_lock unlock];
            break;

        default:
            break;
    }
}

- (void)displayTime
{
    intf_thread_t * p_intf = [NSApp getIntf];
    input_thread_t * p_input = p_intf->p_sys->p_input;

    if( p_input == NULL )
    {
        [o_timeslider setEnabled: FALSE];
        [o_timeslider setFloatValue: 0.0];
        [o_timefield setStringValue: @"0:00:00"]; 

        return;
    }

#define p_area p_input->stream.p_selected_area

    if( p_input->stream.b_changed )
    {
        [o_timeslider setEnabled: p_input->stream.b_seekable];
    }
    else if( p_intf->p_sys->b_playing )
    {
        NSString * o_time;
        char psz_time[ OFFSETTOTIME_MAX_SIZE ];

        input_OffsetToTime( p_input, psz_time, p_area->i_tell );

        o_time = [NSString stringWithCString: psz_time];
        [o_timefield setStringValue: o_time];

        if( p_input->stream.b_seekable )
        {
            if( f_slider == f_slider_old )
            {
                float f_updated = ( 100. * p_area->i_tell ) /
                                           p_area->i_size;

                if( f_slider != f_updated && [o_slider_lock tryLock] )
                {
                    [o_timeslider setFloatValue: f_updated];
                    [o_slider_lock unlock]; 
                }
            }
            else
            {
                off_t i_seek = ( f_slider * p_area->i_size ) / 100;

                /* release the lock to be able to seek */
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                input_Seek( p_input, i_seek, INPUT_SEEK_SET );
                vlc_mutex_lock( &p_input->stream.stream_lock ); 

                /* Update the old value */
                f_slider_old = f_slider;
            }
        }
    }
#undef p_area
}

@end

@implementation VLCMain (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    BOOL bEnabled = TRUE;

    /* Recent Items Menu */

    if( [[o_mi title] isEqualToString: _NS("Clear Menu")] )
    {
        NSMenu * o_menu = [o_mi_open_recent submenu];
        int i_nb_items = [o_menu numberOfItems];
        NSArray * o_docs = [[NSDocumentController sharedDocumentController]
                                                       recentDocumentURLs];
        UInt32 i_nb_docs = [o_docs count];

        if( i_nb_items > 1 )
        {
            while( --i_nb_items )
            {
                [o_menu removeItemAtIndex: 0];
            }
        }

        if( i_nb_docs > 0 )
        {
            NSURL * o_url;
            NSString * o_doc;

            [o_menu insertItem: [NSMenuItem separatorItem] atIndex: 0];

            while( TRUE )
            {
                i_nb_docs--;

                o_url = [o_docs objectAtIndex: i_nb_docs];

                if( [o_url isFileURL] )
                {
                    o_doc = [o_url path];
                }
                else
                {
                    o_doc = [o_url absoluteString];
                }

                [o_menu insertItemWithTitle: o_doc
                    action: @selector(openRecentItem:)
                    keyEquivalent: @"" atIndex: 0]; 

                if( i_nb_docs == 0 )
                {
                    break;
                }
            } 
        }
        else
        {
            bEnabled = FALSE;
        }
    }

    return( bEnabled );
}

@end

@implementation VLCMain (Internal)

- (void)handlePortMessage:(NSPortMessage *)o_msg
{
    NSData * o_data;
    NSValue * o_value;
    NSInvocation * o_inv;
    vout_thread_t * p_vout;
 
    o_data = [[o_msg components] lastObject];
    o_inv = *((NSInvocation **)[o_data bytes]); 
    [o_inv getArgument: &o_value atIndex: 2];
    p_vout = (vout_thread_t *)[o_value pointerValue];

    [p_vout->p_sys->o_lock lock];
    [o_inv invoke];
    [p_vout->p_sys->o_lock unlockWithCondition: 1];
}

@end
