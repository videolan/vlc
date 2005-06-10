/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
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
#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"
#include "prefs.h"
#include "playlist.h"
#include "controls.h"
#include "about.h"
#include "open.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

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

    /* Put Cocoa into multithread mode as soon as possible.
     * http://developer.apple.com/techpubs/macosx/Cocoa/
     * TasksAndConcepts/ProgrammingTopics/Multithreading/index.html
     * This thread does absolutely nothing at all. */
    [NSThread detachNewThreadSelector:@selector(self) toTarget:[NSString string] withObject:nil];

    p_intf->p_sys->o_sendport = [[NSPort port] retain];
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );
    p_intf->b_play = VLC_TRUE;
    p_intf->pf_run = Run;

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
    /* Do it again - for some unknown reason, vlc_thread_create() often
     * fails to go to real-time priority with the first launched thread
     * (???) --Meuuh */
    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );
    [[VLCMain sharedInstance] setIntf: p_intf];
    [NSBundle loadNibNamed: @"MainMenu" owner: NSApp];
    [NSApp run];
    [[VLCMain sharedInstance] terminate];
}

int ExecuteOnMainThread( id target, SEL sel, void * p_arg )
{
    int i_ret = 0;

    //NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    if( [target respondsToSelector: @selector(performSelectorOnMainThread:
                                             withObject:waitUntilDone:)] )
    {
        [target performSelectorOnMainThread: sel
                withObject: [NSValue valueWithPointer: p_arg]
                waitUntilDone: NO];
    }
    else if( NSApp != nil && [[VLCMain sharedInstance] respondsToSelector: @selector(getIntf)] )
    {
        NSValue * o_v1;
        NSValue * o_v2;
        NSArray * o_array;
        NSPort * o_recv_port;
        NSInvocation * o_inv;
        NSPortMessage * o_msg;
        intf_thread_t * p_intf;
        NSConditionLock * o_lock;
        NSMethodSignature * o_sig;

        id * val[] = { &o_lock, &o_v2 };

        p_intf = (intf_thread_t *)VLCIntf;

        o_recv_port = [[NSPort port] retain];
        o_v1 = [NSValue valueWithPointer: val];
        o_v2 = [NSValue valueWithPointer: p_arg];

        o_sig = [target methodSignatureForSelector: sel];
        o_inv = [NSInvocation invocationWithMethodSignature: o_sig];
        [o_inv setArgument: &o_v1 atIndex: 2];
        [o_inv setTarget: target];
        [o_inv setSelector: sel];

        o_array = [NSArray arrayWithObject:
            [NSData dataWithBytes: &o_inv length: sizeof(o_inv)]];
        o_msg = [[NSPortMessage alloc]
            initWithSendPort: p_intf->p_sys->o_sendport
            receivePort: o_recv_port components: o_array];

        o_lock = [[NSConditionLock alloc] initWithCondition: 0];
        [o_msg sendBeforeDate: [NSDate distantPast]];
        [o_lock lockWhenCondition: 1];
        [o_lock unlock];
        [o_lock release];

        [o_msg release];
        [o_recv_port release];
    }
    else
    {
        i_ret = 1;
    }

    //[o_pool release];

    return( i_ret );
}

/*****************************************************************************
 * playlistChanged: Callback triggered by the intf-change playlist
 * variable, to let the intf update the playlist.
 *****************************************************************************/
int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    p_intf->p_sys->b_playlist_update = TRUE;
    p_intf->p_sys->b_intf_update = TRUE;
    p_intf->p_sys->b_playmode_update = TRUE;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * FullscreenChanged: Callback triggered by the fullscreen-change playlist
 * variable, to let the intf update the controller.
 *****************************************************************************/
int FullscreenChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    p_intf->p_sys->b_fullscreen_update = TRUE;
    return VLC_SUCCESS;
}


static struct
{
    unichar i_nskey;
    unsigned int i_vlckey;
} nskeys_to_vlckeys[] =
{
    { NSUpArrowFunctionKey, KEY_UP },
    { NSDownArrowFunctionKey, KEY_DOWN },
    { NSLeftArrowFunctionKey, KEY_LEFT },
    { NSRightArrowFunctionKey, KEY_RIGHT },
    { NSF1FunctionKey, KEY_F1 },
    { NSF2FunctionKey, KEY_F2 },
    { NSF3FunctionKey, KEY_F3 },
    { NSF4FunctionKey, KEY_F4 },
    { NSF5FunctionKey, KEY_F5 },
    { NSF6FunctionKey, KEY_F6 },
    { NSF7FunctionKey, KEY_F7 },
    { NSF8FunctionKey, KEY_F8 },
    { NSF9FunctionKey, KEY_F9 },
    { NSF10FunctionKey, KEY_F10 },
    { NSF11FunctionKey, KEY_F11 },
    { NSF12FunctionKey, KEY_F12 },
    { NSHomeFunctionKey, KEY_HOME },
    { NSEndFunctionKey, KEY_END },
    { NSPageUpFunctionKey, KEY_PAGEUP },
    { NSPageDownFunctionKey, KEY_PAGEDOWN },
    { NSTabCharacter, KEY_TAB },
    { NSCarriageReturnCharacter, KEY_ENTER },
    { NSEnterCharacter, KEY_ENTER },
    { NSBackspaceCharacter, KEY_BACKSPACE },
    { (unichar) ' ', KEY_SPACE },
    { (unichar) 0x1b, KEY_ESC },
    {0,0}
};

unichar VLCKeyToCocoa( unsigned int i_key )
{
    unsigned int i;

    for( i = 0; nskeys_to_vlckeys[i].i_vlckey != 0; i++ )
    {
        if( nskeys_to_vlckeys[i].i_vlckey == (i_key & ~KEY_MODIFIER) )
        {
            return nskeys_to_vlckeys[i].i_nskey;
        }
    }
    return (unichar)(i_key & ~KEY_MODIFIER);
}

unsigned int CocoaKeyToVLC( unichar i_key )
{
    unsigned int i;

    for( i = 0; nskeys_to_vlckeys[i].i_nskey != 0; i++ )
    {
        if( nskeys_to_vlckeys[i].i_nskey == i_key )
        {
            return nskeys_to_vlckeys[i].i_vlckey;
        }
    }
    return (unsigned int)i_key;
}

unsigned int VLCModifiersToCocoa( unsigned int i_key )
{
    unsigned int new = 0;
    if( i_key & KEY_MODIFIER_COMMAND )
        new |= NSCommandKeyMask;
    if( i_key & KEY_MODIFIER_ALT )
        new |= NSAlternateKeyMask;
    if( i_key & KEY_MODIFIER_SHIFT )
        new |= NSShiftKeyMask;
    if( i_key & KEY_MODIFIER_CTRL )
        new |= NSControlKeyMask;
    return new;
}

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

static VLCMain *_o_sharedMainInstance = nil;

+ (VLCMain *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedMainInstance) {
        [self dealloc];
    } else {
        _o_sharedMainInstance = [super init];
    }
    
    o_about = [[VLAboutBox alloc] init];
    o_prefs = nil;
    o_open = [[VLCOpen alloc] init];

    i_lastShownVolume = -1;
    return _o_sharedMainInstance;
}

- (void)setIntf: (intf_thread_t *)p_mainintf {
    p_intf = p_mainintf;
}

- (intf_thread_t *)getIntf {
    return p_intf;
}

- (void)awakeFromNib
{
    unsigned int i_key = 0;
    playlist_t *p_playlist;
    vlc_value_t val;

    [self initStrings];
    [o_window setExcludedFromWindowsMenu: TRUE];
    [o_msgs_panel setExcludedFromWindowsMenu: TRUE];
    [o_msgs_panel setDelegate: self];

    i_key = config_GetInt( p_intf, "key-quit" );
    [o_mi_quit setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_quit setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-play-pause" );
    [o_mi_play setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_play setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-stop" );
    [o_mi_stop setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_stop setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-faster" );
    [o_mi_faster setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_faster setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-slower" );
    [o_mi_slower setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_slower setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-prev" );
    [o_mi_previous setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_previous setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-next" );
    [o_mi_next setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_next setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump+10sec" );
    [o_mi_fwd setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fwd setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump-10sec" );
    [o_mi_bwd setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_bwd setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump+1min" );
    [o_mi_fwd1m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fwd1m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump-1min" );
    [o_mi_bwd1m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_bwd1m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump+5min" );
    [o_mi_fwd5m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fwd5m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump-5min" );
    [o_mi_bwd5m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_bwd5m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-vol-up" );
    [o_mi_vol_up setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_vol_up setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-vol-down" );
    [o_mi_vol_down setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_vol_down setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-vol-mute" );
    [o_mi_mute setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_mute setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-fullscreen" );
    [o_mi_fullscreen setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fullscreen setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-snapshot" );
    [o_mi_snapshot setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_snapshot setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];

    var_Create( p_intf, "intf-change", VLC_VAR_BOOL );

    [self setSubmenusEnabled: FALSE];
    [self manageVolumeSlider];
    [o_window setDelegate: self];
    
    if( [o_window frame].size.height <= 200 )
    {
        b_small_window = YES;
        [o_window setFrame: NSMakeRect( [o_window frame].origin.x,
            [o_window frame].origin.y, [o_window frame].size.width,
            [o_window minSize].height ) display: YES animate:YES];
        [o_playlist_view setAutoresizesSubviews: NO];
    }
    else
    {
        b_small_window = NO;
        [o_playlist_view setFrame: NSMakeRect( 10, 10, [o_window frame].size.width - 20, [o_window frame].size.height - 105 )];
        [o_playlist_view setNeedsDisplay:YES];
        [o_playlist_view setAutoresizesSubviews: YES];
        [[o_window contentView] addSubview: o_playlist_view];
    }
    [self updateTogglePlaylistState];

    o_size_with_playlist = [o_window frame].size;

    p_playlist = (playlist_t *) vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist )
    {
        /* Check if we need to start playing */
        if( p_intf->b_play )
        {
            playlist_Play( p_playlist );
        }
        var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
        val.b_bool = VLC_FALSE;

        var_AddCallback( p_playlist, "fullscreen", FullscreenChanged, self);

        [o_btn_fullscreen setState: ( var_Get( p_playlist, "fullscreen", &val )>=0 && val.b_bool )];
        vlc_object_release( p_playlist );
    }
}

- (void)initStrings
{
    [o_window setTitle: _NS("VLC - Controller")];
    [self setScrollField:_NS("VLC media player") stopAfter:-1];

    /* button controls */
    [o_btn_prev setToolTip: _NS("Previous")];
    [o_btn_rewind setToolTip: _NS("Rewind")];
    [o_btn_play setToolTip: _NS("Play")];
    [o_btn_stop setToolTip: _NS("Stop")];
    [o_btn_ff setToolTip: _NS("Fast Forward")];
    [o_btn_next setToolTip: _NS("Next")];
    [o_btn_fullscreen setToolTip: _NS("Fullscreen")];
    [o_volumeslider setToolTip: _NS("Volume")];
    [o_timeslider setToolTip: _NS("Position")];
    [o_btn_playlist setToolTip: _NS("Playlist")];

    /* messages panel */
    [o_msgs_panel setTitle: _NS("Messages")];
    [o_msgs_btn_crashlog setTitle: _NS("Open CrashLog")];

    /* main menu */
    [o_mi_about setTitle: _NS("About VLC media player")];
    [o_mi_prefs setTitle: _NS("Preferences...")];
    [o_mi_add_intf setTitle: _NS("Add Interface")];
    [o_mu_add_intf setTitle: _NS("Add Interface")];
    [o_mi_services setTitle: _NS("Services")];
    [o_mi_hide setTitle: _NS("Hide VLC")];
    [o_mi_hide_others setTitle: _NS("Hide Others")];
    [o_mi_show_all setTitle: _NS("Show All")];
    [o_mi_quit setTitle: _NS("Quit VLC")];

    [o_mu_file setTitle: _ANS("1:File")];
    [o_mi_open_generic setTitle: _NS("Open File...")];
    [o_mi_open_file setTitle: _NS("Quick Open File...")];
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

    [o_mu_controls setTitle: _NS("Controls")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_stop setTitle: _NS("Stop")];
    [o_mi_faster setTitle: _NS("Faster")];
    [o_mi_slower setTitle: _NS("Slower")];
    [o_mi_previous setTitle: _NS("Previous")];
    [o_mi_next setTitle: _NS("Next")];
    [o_mi_random setTitle: _NS("Random")];
    [o_mi_repeat setTitle: _NS("Repeat One")];
    [o_mi_loop setTitle: _NS("Repeat All")];
    [o_mi_fwd setTitle: _NS("Step Forward")];
    [o_mi_bwd setTitle: _NS("Step Backward")];

    [o_mi_program setTitle: _NS("Program")];
    [o_mu_program setTitle: _NS("Program")];
    [o_mi_title setTitle: _NS("Title")];
    [o_mu_title setTitle: _NS("Title")];
    [o_mi_chapter setTitle: _NS("Chapter")];
    [o_mu_chapter setTitle: _NS("Chapter")];

    [o_mu_audio setTitle: _NS("Audio")];
    [o_mi_vol_up setTitle: _NS("Volume Up")];
    [o_mi_vol_down setTitle: _NS("Volume Down")];
    [o_mi_mute setTitle: _NS("Mute")];
    [o_mi_audiotrack setTitle: _NS("Audio Track")];
    [o_mu_audiotrack setTitle: _NS("Audio Track")];
    [o_mi_channels setTitle: _NS("Audio Channels")];
    [o_mu_channels setTitle: _NS("Audio Channels")];
    [o_mi_device setTitle: _NS("Audio Device")];
    [o_mu_device setTitle: _NS("Audio Device")];
    [o_mi_visual setTitle: _NS("Visualizations")];
    [o_mu_visual setTitle: _NS("Visualizations")];

    [o_mu_video setTitle: _NS("Video")];
    [o_mi_half_window setTitle: _NS("Half Size")];
    [o_mi_normal_window setTitle: _NS("Normal Size")];
    [o_mi_double_window setTitle: _NS("Double Size")];
    [o_mi_fittoscreen setTitle: _NS("Fit to Screen")];
    [o_mi_fullscreen setTitle: _NS("Fullscreen")];
    [o_mi_floatontop setTitle: _NS("Float on Top")];
    [o_mi_snapshot setTitle: _NS("Snapshot")];
    [o_mi_videotrack setTitle: _NS("Video Track")];
    [o_mu_videotrack setTitle: _NS("Video Track")];
    [o_mi_screen setTitle: _NS("Video Device")];
    [o_mu_screen setTitle: _NS("Video Device")];
    [o_mi_subtitle setTitle: _NS("Subtitles Track")];
    [o_mu_subtitle setTitle: _NS("Subtitles Track")];
    [o_mi_deinterlace setTitle: _NS("Deinterlace")];
    [o_mu_deinterlace setTitle: _NS("Deinterlace")];
    [o_mi_ffmpeg_pp setTitle: _NS("Post processing")];
    [o_mu_ffmpeg_pp setTitle: _NS("Post processing")];

    [o_mu_window setTitle: _NS("Window")];
    [o_mi_minimize setTitle: _NS("Minimize Window")];
    [o_mi_close_window setTitle: _NS("Close Window")];
    [o_mi_controller setTitle: _NS("Controller")];
    [o_mi_equalizer setTitle: _NS("Equalizer")];
    [o_mi_playlist setTitle: _NS("Playlist")];
    [o_mi_info setTitle: _NS("Info")];
    [o_mi_messages setTitle: _NS("Messages")];

    [o_mi_bring_atf setTitle: _NS("Bring All to Front")];

    [o_mu_help setTitle: _NS("Help")];
    [o_mi_readme setTitle: _NS("ReadMe...")];
    [o_mi_documentation setTitle: _NS("Online Documentation")];
    [o_mi_reportabug setTitle: _NS("Report a Bug")];
    [o_mi_website setTitle: _NS("VideoLAN Website")];
    [o_mi_license setTitle: _NS("License")];

    /* dock menu */
    [o_dmi_play setTitle: _NS("Play")];
    [o_dmi_stop setTitle: _NS("Stop")];
    [o_dmi_next setTitle: _NS("Next")];
    [o_dmi_previous setTitle: _NS("Previous")];
    [o_dmi_mute setTitle: _NS("Mute")];

    /* error panel */
    [o_error setTitle: _NS("Error")];
    [o_err_lbl setStringValue: _NS("An error has occurred which probably prevented the execution of your request:")];
    [o_err_bug_lbl setStringValue: _NS("If you believe that it is a bug, please follow the instructions at:")];
    [o_err_btn_msgs setTitle: _NS("Open Messages Window")];
    [o_err_btn_dismiss setTitle: _NS("Dismiss")];
    [o_err_ckbk_surpress setTitle: _NS("Suppress further errors")];

    [o_info_window setTitle: _NS("Info")];
}

- (void)applicationWillFinishLaunching:(NSNotification *)o_notification
{
    o_msg_lock = [[NSLock alloc] init];
    o_msg_arr = [[NSMutableArray arrayWithCapacity: 200] retain];

    o_img_play = [[NSImage imageNamed: @"play"] retain];
    o_img_play_pressed = [[NSImage imageNamed: @"play_blue"] retain];
    o_img_pause = [[NSImage imageNamed: @"pause"] retain];
    o_img_pause_pressed = [[NSImage imageNamed: @"pause_blue"] retain];

    [p_intf->p_sys->o_sendport setDelegate: self];
    [[NSRunLoop currentRunLoop]
        addPort: p_intf->p_sys->o_sendport
        forMode: NSDefaultRunLoopMode];

    [NSTimer scheduledTimerWithTimeInterval: 0.5
        target: self selector: @selector(manageIntf:)
        userInfo: nil repeats: FALSE];

    [NSThread detachNewThreadSelector: @selector(manage)
        toTarget: self withObject: nil];

    [o_controls setupVarMenuItem: o_mi_add_intf target: (vlc_object_t *)p_intf
        var: "intf-add" selector: @selector(toggleVar:)];

    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );
}

- (BOOL)application:(NSApplication *)o_app openFile:(NSString *)o_filename
{
    NSDictionary *o_dic = [NSDictionary dictionaryWithObjectsAndKeys: o_filename, @"ITEM_URL", nil];
    [o_playlist appendArray:
        [NSArray arrayWithObject: o_dic] atPos: -1 enqueue: NO];

    return( TRUE );
}

- (NSString *)localizedString:(char *)psz
{
    NSString * o_str = nil;

    if( psz != NULL )
    {
        o_str = [[[NSString alloc] initWithUTF8String: psz] autorelease];
    }
    if ( o_str == NULL )
    {
        msg_Err( VLCIntf, "could not translate: %s", psz );
    }

    return( o_str );
}

- (char *)delocalizeString:(NSString *)id
{
    NSData * o_data = [id dataUsingEncoding: NSUTF8StringEncoding
                          allowLossyConversion: NO];
    char * psz_string;

    if ( o_data == nil )
    {
        o_data = [id dataUsingEncoding: NSUTF8StringEncoding
                     allowLossyConversion: YES];
        psz_string = malloc( [o_data length] + 1 );
        [o_data getBytes: psz_string];
        psz_string[ [o_data length] ] = '\0';
        msg_Err( VLCIntf, "cannot convert to wanted encoding: %s",
                 psz_string );
    }
    else
    {
        psz_string = malloc( [o_data length] + 1 );
        [o_data getBytes: psz_string];
        psz_string[ [o_data length] ] = '\0';
    }

    return psz_string;
}

/* i_width is in pixels */
- (NSString *)wrapString: (NSString *)o_in_string toWidth: (int) i_width
{
    NSMutableString *o_wrapped;
    NSString *o_out_string;
    NSRange glyphRange, effectiveRange, charRange;
    NSRect lineFragmentRect;
    unsigned glyphIndex, breaksInserted = 0;

    NSTextStorage *o_storage = [[NSTextStorage alloc] initWithString: o_in_string
        attributes: [NSDictionary dictionaryWithObjectsAndKeys:
        [NSFont labelFontOfSize: 0.0], NSFontAttributeName, nil]];
    NSLayoutManager *o_layout_manager = [[NSLayoutManager alloc] init];
    NSTextContainer *o_container = [[NSTextContainer alloc]
        initWithContainerSize: NSMakeSize(i_width, 2000)];

    [o_layout_manager addTextContainer: o_container];
    [o_container release];
    [o_storage addLayoutManager: o_layout_manager];
    [o_layout_manager release];

    o_wrapped = [o_in_string mutableCopy];
    glyphRange = [o_layout_manager glyphRangeForTextContainer: o_container];

    for( glyphIndex = glyphRange.location ; glyphIndex < NSMaxRange(glyphRange) ;
            glyphIndex += effectiveRange.length) {
        lineFragmentRect = [o_layout_manager lineFragmentRectForGlyphAtIndex: glyphIndex
                                            effectiveRange: &effectiveRange];
        charRange = [o_layout_manager characterRangeForGlyphRange: effectiveRange
                                    actualGlyphRange: &effectiveRange];
        if ([o_wrapped lineRangeForRange:
                NSMakeRange(charRange.location + breaksInserted, charRange.length)].length > charRange.length) {
            [o_wrapped insertString: @"\n" atIndex: NSMaxRange(charRange) + breaksInserted];
            breaksInserted++;
        }
    }
    o_out_string = [NSString stringWithString: o_wrapped];
    [o_wrapped release];
    [o_storage release];

    return o_out_string;
}


/*****************************************************************************
 * hasDefinedShortcutKey: Check to see if the key press is a defined VLC
 * shortcut key.  If it is, pass it off to VLC for handling and return YES,
 * otherwise ignore it and return NO (where it will get handled by Cocoa).
 *****************************************************************************/
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;
    struct hotkey *p_hotkeys;
    int i;

    val.i_int = 0;
    p_hotkeys = p_intf->p_vlc->p_hotkeys;

    i_pressed_modifiers = [o_event modifierFlags];

    if( i_pressed_modifiers & NSShiftKeyMask )
        val.i_int |= KEY_MODIFIER_SHIFT;
    if( i_pressed_modifiers & NSControlKeyMask )
        val.i_int |= KEY_MODIFIER_CTRL;
    if( i_pressed_modifiers & NSAlternateKeyMask )
        val.i_int |= KEY_MODIFIER_ALT;
    if( i_pressed_modifiers & NSCommandKeyMask )
        val.i_int |= KEY_MODIFIER_COMMAND;

    key = [[o_event charactersIgnoringModifiers] characterAtIndex: 0];

    switch( key )
    {
        case NSDeleteCharacter:
        case NSDeleteFunctionKey:
        case NSDeleteCharFunctionKey:
        case NSBackspaceCharacter:
            return YES;
        case NSUpArrowFunctionKey:
        case NSDownArrowFunctionKey:
        case NSRightArrowFunctionKey:
        case NSLeftArrowFunctionKey:
        case NSEnterCharacter:
        case NSCarriageReturnCharacter:
            return NO;
    }

    val.i_int |= CocoaKeyToVLC( key );

    for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
    {
        if( p_hotkeys[i].i_key == val.i_int )
        {
            var_Set( p_intf->p_vlc, "key-pressed", val );
            return YES;
        }
    }

    return NO;
}

- (id)getControls
{
    if ( o_controls )
    {
        return o_controls;
    }
    return nil;
}

- (id)getPlaylist
{
    if ( o_playlist )
    {
        return o_playlist;
    }
    return nil;
}

- (id)getInfo
{
    if ( o_info )
    {
        return o_info;
    }
    return  nil;
}

- (void)manage
{
    NSDate * o_sleep_date;
    playlist_t * p_playlist;

    /* new thread requires a new pool */
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );

    if( p_playlist != NULL )
    {
        var_AddCallback( p_playlist, "intf-change", PlaylistChanged, self );
        var_AddCallback( p_playlist, "item-change", PlaylistChanged, self );
        var_AddCallback( p_playlist, "item-append", PlaylistChanged, self );
        var_AddCallback( p_playlist, "item-deleted", PlaylistChanged, self );
        var_AddCallback( p_playlist, "playlist-current", PlaylistChanged, self );

        vlc_object_release( p_playlist );
    }

    while( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->change_lock );

#define p_input p_intf->p_sys->p_input

        if( p_input == NULL )
        {
            p_input = (input_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                           FIND_ANYWHERE );

            /* Refresh the interface */
            if( p_input )
            {
                msg_Dbg( p_intf, "input has changed, refreshing interface" );
                p_intf->p_sys->b_input_update = VLC_TRUE;
            }
        }
        else if( p_input->b_die || p_input->b_dead )
        {
            /* input stopped */
            p_intf->p_sys->b_intf_update = VLC_TRUE;
            p_intf->p_sys->i_play_status = END_S;
            [self setScrollField: _NS("VLC media player") stopAfter:-1];
            vlc_object_release( p_input );
            p_input = NULL;
        }
#undef p_input

        vlc_mutex_unlock( &p_intf->change_lock );

        o_sleep_date = [NSDate dateWithTimeIntervalSinceNow: .1];
        [NSThread sleepUntilDate: o_sleep_date];
    }

    [self terminate];
    [o_pool release];
}

- (void)manageIntf:(NSTimer *)o_timer
{
    vlc_value_t val;

    if( p_intf->p_vlc->b_die == VLC_TRUE )
    {
        [o_timer invalidate];
        return;
    }

#define p_input p_intf->p_sys->p_input
    if( p_intf->p_sys->b_input_update )
    {
        /* Called when new input is opened */
        p_intf->p_sys->b_current_title_update = VLC_TRUE;
        p_intf->p_sys->b_intf_update = VLC_TRUE;
        p_intf->p_sys->b_input_update = VLC_FALSE;
    }
    if( p_intf->p_sys->b_intf_update )
    {
        vlc_bool_t b_input = VLC_FALSE;
        vlc_bool_t b_plmul = VLC_FALSE;
        vlc_bool_t b_control = VLC_FALSE;
        vlc_bool_t b_seekable = VLC_FALSE;
        vlc_bool_t b_chapters = VLC_FALSE;

        playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                   FIND_ANYWHERE );
        b_plmul = p_playlist->i_size > 1;

        vlc_object_release( p_playlist );

        if( ( b_input = ( p_input != NULL ) ) )
        {
            /* seekable streams */
            var_Get( p_input, "seekable", &val);
            b_seekable = val.b_bool;

            /* check wether slow/fast motion is possible*/
            b_control = p_input->input.b_can_pace_control;

            /* chapters & titles */
            //b_chapters = p_input->stream.i_area_nb > 1;
        }

        [o_btn_stop setEnabled: b_input];
        [o_btn_ff setEnabled: b_seekable];
        [o_btn_rewind setEnabled: b_seekable];
        [o_btn_prev setEnabled: (b_plmul || b_chapters)];
        [o_btn_next setEnabled: (b_plmul || b_chapters)];

        [o_timeslider setFloatValue: 0.0];
        [o_timeslider setEnabled: b_seekable];
        [o_timefield setStringValue: @"0:00:00"];

        p_intf->p_sys->b_intf_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_playmode_update )
    {
        [o_playlist playModeUpdated];
        p_intf->p_sys->b_playmode_update = VLC_FALSE;
    }
    if( p_intf->p_sys->b_playlist_update )
    {
        [o_playlist playlistUpdated];
        p_intf->p_sys->b_playlist_update = VLC_FALSE;
    }

    if( p_intf->p_sys->b_fullscreen_update )
    {
        playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                   FIND_ANYWHERE );
        var_Get( p_playlist, "fullscreen", &val );
        [o_btn_fullscreen setState: val.b_bool];
        vlc_object_release( p_playlist );

        p_intf->p_sys->b_fullscreen_update = VLC_FALSE;
    }

    if( p_input && !p_input->b_die )
    {
        vlc_value_t val;

        if( p_intf->p_sys->b_current_title_update )
        {
            NSString *o_temp;
            vout_thread_t *p_vout;
            playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );

            if( p_playlist == NULL || p_playlist->status.p_item == NULL )
            {
                return;
            }
            o_temp = [NSString stringWithUTF8String:
                p_playlist->status.p_item->input.psz_name];
            if( o_temp == NULL )
                o_temp = [NSString stringWithCString:
                    p_playlist->status.p_item->input.psz_name];
            [self setScrollField: o_temp stopAfter:-1];

            p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                    FIND_ANYWHERE );
            if( p_vout != NULL )
            {
                id o_vout_wnd;
                NSEnumerator * o_enum = [[NSApp orderedWindows] objectEnumerator];

                while( ( o_vout_wnd = [o_enum nextObject] ) )
                {
                    if( [[o_vout_wnd className] isEqualToString: @"VLCWindow"] )
                    {
                        [o_vout_wnd updateTitle];
                    }
                }
                vlc_object_release( (vlc_object_t *)p_vout );
            }
            [o_playlist updateRowSelection];
            vlc_object_release( p_playlist );
            p_intf->p_sys->b_current_title_update = FALSE;
        }

        if( p_input && [o_timeslider isEnabled] )
        {
            /* Update the slider */
            vlc_value_t time;
            NSString * o_time;
            mtime_t i_seconds;
            vlc_value_t pos;
            float f_updated;

            var_Get( p_input, "position", &pos );
            f_updated = 10000. * pos.f_float;
            [o_timeslider setFloatValue: f_updated];

            var_Get( p_input, "time", &time );
            i_seconds = time.i_time / 1000000;

            o_time = [NSString stringWithFormat: @"%d:%02d:%02d",
                            (int) (i_seconds / (60 * 60)),
                            (int) (i_seconds / 60 % 60),
                            (int) (i_seconds % 60)];
            [o_timefield setStringValue: o_time];
        }

        /* Manage Playing status */
        var_Get( p_input, "state", &val );
        if( p_intf->p_sys->i_play_status != val.i_int )
        {
            p_intf->p_sys->i_play_status = val.i_int;
            [self playStatusUpdated: p_intf->p_sys->i_play_status];
        }
    }
    else
    {
        p_intf->p_sys->i_play_status = END_S;
        p_intf->p_sys->b_intf_update = VLC_TRUE;
        [self playStatusUpdated: p_intf->p_sys->i_play_status];
        [self setSubmenusEnabled: FALSE];
    }

#undef p_input

    [self updateMessageArray];

    if( (i_end_scroll != -1) && (mdate() > i_end_scroll) )
        [self resetScrollField];

    /* Manage volume status */
    [self manageVolumeSlider];

    [NSTimer scheduledTimerWithTimeInterval: 0.3
        target: self selector: @selector(manageIntf:)
        userInfo: nil repeats: FALSE];
}

- (void)setupMenus
{
#define p_input p_intf->p_sys->p_input
    if( p_input != NULL )
    {
        [o_controls setupVarMenuItem: o_mi_program target: (vlc_object_t *)p_input
            var: "program" selector: @selector(toggleVar:)];

        [o_controls setupVarMenuItem: o_mi_title target: (vlc_object_t *)p_input
            var: "title" selector: @selector(toggleVar:)];

        [o_controls setupVarMenuItem: o_mi_chapter target: (vlc_object_t *)p_input
            var: "chapter" selector: @selector(toggleVar:)];

        [o_controls setupVarMenuItem: o_mi_audiotrack target: (vlc_object_t *)p_input
            var: "audio-es" selector: @selector(toggleVar:)];

        [o_controls setupVarMenuItem: o_mi_videotrack target: (vlc_object_t *)p_input
            var: "video-es" selector: @selector(toggleVar:)];

        [o_controls setupVarMenuItem: o_mi_subtitle target: (vlc_object_t *)p_input
            var: "spu-es" selector: @selector(toggleVar:)];

        aout_instance_t * p_aout = vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                    FIND_ANYWHERE );
        if ( p_aout != NULL )
        {
            [o_controls setupVarMenuItem: o_mi_channels target: (vlc_object_t *)p_aout
                var: "audio-channels" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_device target: (vlc_object_t *)p_aout
                var: "audio-device" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_visual target: (vlc_object_t *)p_aout
                var: "visual" selector: @selector(toggleVar:)];
            vlc_object_release( (vlc_object_t *)p_aout );
        }

        vout_thread_t * p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                            FIND_ANYWHERE );

        if ( p_vout != NULL )
        {
            vlc_object_t * p_dec_obj;

            [o_controls setupVarMenuItem: o_mi_screen target: (vlc_object_t *)p_vout
                var: "video-device" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_deinterlace target: (vlc_object_t *)p_vout
                var: "deinterlace" selector: @selector(toggleVar:)];

            p_dec_obj = (vlc_object_t *)vlc_object_find(
                                                 (vlc_object_t *)p_vout,
                                                 VLC_OBJECT_DECODER,
                                                 FIND_PARENT );
            if ( p_dec_obj != NULL )
            {
               [o_controls setupVarMenuItem: o_mi_ffmpeg_pp target:
                    (vlc_object_t *)p_dec_obj var:"ffmpeg-pp-q" selector:
                    @selector(toggleVar:)];

                vlc_object_release(p_dec_obj);
            }
            vlc_object_release( (vlc_object_t *)p_vout );
        }
    }
#undef p_input
}

- (void)setScrollField:(NSString *)o_string stopAfter:(int)timeout
{
    if( timeout != -1 )
        i_end_scroll = mdate() + timeout;
    else
        i_end_scroll = -1;
    [o_scrollfield setStringValue: o_string];
}

- (void)resetScrollField
{
    i_end_scroll = -1;
#define p_input p_intf->p_sys->p_input
    if( p_input && !p_input->b_die )
    {
        NSString *o_temp;
        playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                   FIND_ANYWHERE );
        if( p_playlist == NULL )
        {
            return;
        }
        o_temp = [NSString stringWithUTF8String:
                  p_playlist->status.p_item->input.psz_name];
        if( o_temp == NULL )
            o_temp = [NSString stringWithCString:
                    p_playlist->status.p_item->input.psz_name];
        [self setScrollField: o_temp stopAfter:-1];
        vlc_object_release( p_playlist );
        return;
    }
#undef p_input
    [self setScrollField: _NS("VLC media player") stopAfter:-1];
}

- (void)updateMessageArray
{
    int i_start, i_stop;
    vlc_value_t quiet;

    vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
    i_stop = *p_intf->p_sys->p_sub->pi_stop;
    vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );

    if( p_intf->p_sys->p_sub->i_start != i_stop )
    {
        NSColor *o_white = [NSColor whiteColor];
        NSColor *o_red = [NSColor redColor];
        NSColor *o_yellow = [NSColor yellowColor];
        NSColor *o_gray = [NSColor grayColor];

        NSColor * pp_color[4] = { o_white, o_red, o_yellow, o_gray };
        static const char * ppsz_type[4] = { ": ", " error: ",
                                             " warning: ", " debug: " };

        for( i_start = p_intf->p_sys->p_sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            NSString *o_msg;
            NSDictionary *o_attr;
            NSAttributedString *o_msg_color;

            int i_type = p_intf->p_sys->p_sub->p_msg[i_start].i_type;

            [o_msg_lock lock];

            if( [o_msg_arr count] + 2 > 400 )
            {
                unsigned rid[] = { 0, 1 };
                [o_msg_arr removeObjectsFromIndices: (unsigned *)&rid
                           numIndices: sizeof(rid)/sizeof(rid[0])];
            }

            o_attr = [NSDictionary dictionaryWithObject: o_gray
                forKey: NSForegroundColorAttributeName];
            o_msg = [NSString stringWithFormat: @"%s%s",
                p_intf->p_sys->p_sub->p_msg[i_start].psz_module,
                ppsz_type[i_type]];
            o_msg_color = [[NSAttributedString alloc]
                initWithString: o_msg attributes: o_attr];
            [o_msg_arr addObject: [o_msg_color autorelease]];

            o_attr = [NSDictionary dictionaryWithObject: pp_color[i_type]
                forKey: NSForegroundColorAttributeName];
            o_msg = [NSString stringWithFormat: @"%s\n",
                p_intf->p_sys->p_sub->p_msg[i_start].psz_msg];
            o_msg_color = [[NSAttributedString alloc]
                initWithString: o_msg attributes: o_attr];
            [o_msg_arr addObject: [o_msg_color autorelease]];

            [o_msg_lock unlock];

            var_Get( p_intf->p_vlc, "verbose", &quiet );

            if( i_type == 1 && quiet.i_int > -1 )
            {
                NSString *o_my_msg = [NSString stringWithFormat: @"%s: %s\n",
                    p_intf->p_sys->p_sub->p_msg[i_start].psz_module,
                    p_intf->p_sys->p_sub->p_msg[i_start].psz_msg];

                NSRange s_r = NSMakeRange( [[o_err_msg string] length], 0 );
                [o_err_msg setEditable: YES];
                [o_err_msg setSelectedRange: s_r];
                [o_err_msg insertText: o_my_msg];

                [o_error makeKeyAndOrderFront: self];
                [o_err_msg setEditable: NO];
            }
        }

        vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
        p_intf->p_sys->p_sub->i_start = i_start;
        vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );
    }
}

- (void)playStatusUpdated:(int)i_status
{
    if( i_status == PLAYING_S )
    {
        [o_btn_play setImage: o_img_pause];
        [o_btn_play setAlternateImage: o_img_pause_pressed];
        [o_btn_play setToolTip: _NS("Pause")];
        [o_mi_play setTitle: _NS("Pause")];
        [o_dmi_play setTitle: _NS("Pause")];
    }
    else
    {
        [o_btn_play setImage: o_img_play];
        [o_btn_play setAlternateImage: o_img_play_pressed];
        [o_btn_play setToolTip: _NS("Play")];
        [o_mi_play setTitle: _NS("Play")];
        [o_dmi_play setTitle: _NS("Play")];
    }
}

- (void)setSubmenusEnabled:(BOOL)b_enabled
{
    [o_mi_program setEnabled: b_enabled];
    [o_mi_title setEnabled: b_enabled];
    [o_mi_chapter setEnabled: b_enabled];
    [o_mi_audiotrack setEnabled: b_enabled];
    [o_mi_visual setEnabled: b_enabled];
    [o_mi_videotrack setEnabled: b_enabled];
    [o_mi_subtitle setEnabled: b_enabled];
    [o_mi_channels setEnabled: b_enabled];
    [o_mi_deinterlace setEnabled: b_enabled];
    [o_mi_ffmpeg_pp setEnabled: b_enabled];
    [o_mi_device setEnabled: b_enabled];
    [o_mi_screen setEnabled: b_enabled];
}

- (void)manageVolumeSlider
{
    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );

    if( i_volume != i_lastShownVolume )
    {
        NSString *o_text;
        o_text = [NSString stringWithFormat: _NS("Volume: %d"), i_volume * 200 / AOUT_VOLUME_MAX];
        if( i_lastShownVolume != -1 )
            [self setScrollField:o_text stopAfter:1000000];
    
        [o_volumeslider setFloatValue: (float)i_volume / AOUT_VOLUME_STEP];
        [o_volumeslider setEnabled: TRUE];
        i_lastShownVolume = i_volume;
    }

    p_intf->p_sys->b_mute = ( i_volume == 0 );
}

- (IBAction)timesliderUpdate:(id)sender
{
#define p_input p_intf->p_sys->p_input
    float f_updated;

    switch( [[NSApp currentEvent] type] )
    {
        case NSLeftMouseUp:
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
            f_updated = [sender floatValue];
            break;

        default:
            return;
    }

    if( p_input != NULL )
    {
        vlc_value_t time;
        vlc_value_t pos;
        mtime_t i_seconds;
        NSString * o_time;

        pos.f_float = f_updated / 10000.;
        var_Set( p_input, "position", pos );
        [o_timeslider setFloatValue: f_updated];

        var_Get( p_input, "time", &time );
        i_seconds = time.i_time / 1000000;

        o_time = [NSString stringWithFormat: @"%d:%02d:%02d",
                        (int) (i_seconds / (60 * 60)),
                        (int) (i_seconds / 60 % 60),
                        (int) (i_seconds % 60)];
        [o_timefield setStringValue: o_time];
    }
#undef p_input
}

- (void)terminate
{
    playlist_t * p_playlist;
    vout_thread_t * p_vout;

    /* Stop playback */
    if( ( p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                        FIND_ANYWHERE ) ) )
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
    }

    /* FIXME - Wait here until all vouts are terminated because
       libvlc's VLC_CleanUp destroys interfaces before vouts, which isn't
       good on OS X. We definitly need a cleaner way to handle this,
       but this may hopefully be good enough for now.
         -- titer 2003/11/22 */
    while( ( p_vout = vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                       FIND_ANYWHERE ) ) )
    {
        vlc_object_release( p_vout );
        msleep( 100000 );
    }
    msleep( 500000 );

    if( o_img_pause_pressed != nil )
    {
        [o_img_pause_pressed release];
        o_img_pause_pressed = nil;
    }

    if( o_img_pause_pressed != nil )
    {
        [o_img_pause_pressed release];
        o_img_pause_pressed = nil;
    }

    if( o_img_pause != nil )
    {
        [o_img_pause release];
        o_img_pause = nil;
    }

    if( o_img_play != nil )
    {
        [o_img_play release];
        o_img_play = nil;
    }

    if( o_msg_arr != nil )
    {
        [o_msg_arr removeAllObjects];
        [o_msg_arr release];
        o_msg_arr = nil;
    }

    if( o_msg_lock != nil )
    {
        [o_msg_lock release];
        o_msg_lock = nil;
    }

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];

    p_intf->b_die = VLC_TRUE;
    [NSApp stop:NULL];
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

- (IBAction)intfOpenFile:(id)sender
{
    if (!nib_open_loaded)
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner:self];
        [o_open awakeFromNib];
        [o_open openFile];
    } else {
        [o_open openFile];
    }
}

- (IBAction)intfOpenFileGeneric:(id)sender
{
    if (!nib_open_loaded)
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner:self];
        [o_open awakeFromNib];
        [o_open openFileGeneric];
    } else {
        [o_open openFileGeneric];
    }
}

- (IBAction)intfOpenDisc:(id)sender
{
    if (!nib_open_loaded)
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner:self];
        [o_open awakeFromNib];
        [o_open openDisc];
    } else {
        [o_open openDisc];
    }
}

- (IBAction)intfOpenNet:(id)sender
{
    if (!nib_open_loaded)
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner:self];
        [o_open awakeFromNib];
        [o_open openNet];
    } else {
        [o_open openNet];
    }
}

- (IBAction)viewAbout:(id)sender
{
    [o_about showPanel];
}

- (IBAction)viewPreferences:(id)sender
{
/* GRUIIIIIIIK */
    if( o_prefs == nil )
        o_prefs = [[VLCPrefs alloc] init];
    [o_prefs showPrefs];
}

- (IBAction)closeError:(id)sender
{
    vlc_value_t val;

    if( [o_err_ckbk_surpress state] == NSOnState )
    {
        val.i_int = -1;
        var_Set( p_intf->p_vlc, "verbose", val );
    }
    [o_err_msg setString: @""];
    [o_error performClose: self];
}

- (IBAction)openReadMe:(id)sender
{
    NSString * o_path = [[NSBundle mainBundle]
        pathForResource: @"README.MacOSX" ofType: @"rtf"];

    [[NSWorkspace sharedWorkspace] openFile: o_path
                                   withApplication: @"TextEdit"];
}

- (IBAction)openDocumentation:(id)sender
{
    NSURL * o_url = [NSURL URLWithString:
        @"http://www.videolan.org/doc/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)reportABug:(id)sender
{
    NSURL * o_url = [NSURL URLWithString:
        @"http://www.videolan.org/support/bug-reporting.html"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openWebsite:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://www.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openLicense:(id)sender
{
    NSString * o_path = [[NSBundle mainBundle]
        pathForResource: @"COPYING" ofType: nil];

    [[NSWorkspace sharedWorkspace] openFile: o_path
                                   withApplication: @"TextEdit"];
}

- (IBAction)openForum:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://forum.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openDonate:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://www.videolan.org/contribute.html#paypal"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openCrashLog:(id)sender
{
    NSString * o_path = [@"~/Library/Logs/CrashReporter/VLC.crash.log"
                                    stringByExpandingTildeInPath];


    if ( [[NSFileManager defaultManager] fileExistsAtPath: o_path ] )
    {
        [[NSWorkspace sharedWorkspace] openFile: o_path
                                    withApplication: @"Console"];
    }
    else
    {
        NSBeginInformationalAlertSheet(_NS("No CrashLog found"), @"Continue", nil, nil, o_msgs_panel, self, NULL, NULL, nil, _NS("You haven't experienced any heavy crashes yet.") );

    }
}

- (void)windowDidBecomeKey:(NSNotification *)o_notification
{
    if( [o_notification object] == o_msgs_panel )
    {
        id o_msg;
        NSEnumerator * o_enum;

        [o_messages setString: @""];

        [o_msg_lock lock];

        o_enum = [o_msg_arr objectEnumerator];

        while( ( o_msg = [o_enum nextObject] ) != nil )
        {
            [o_messages insertText: o_msg];
        }

        [o_msg_lock unlock];
    }
}

- (IBAction)togglePlaylist:(id)sender
{
    NSRect o_rect = [o_window frame];
    /*First, check if the playlist is visible*/
    if( o_rect.size.height <= 200 )
    {
        b_small_window = YES; /* we know we are small, make sure this is actually set (see case below) */
        /* make large */
        if ( o_size_with_playlist.height > 200 )
        {
            o_rect.size.height = o_size_with_playlist.height;
        }
        else
        {
            o_rect.size.height = 500;
            if ( o_rect.size.width == [o_window minSize].width )
            {
                o_rect.size.width = 500;
            }

        }
        o_rect.size.height = (o_size_with_playlist.height > 200) ?
            o_size_with_playlist.height : 500;
        o_rect.origin.x = [o_window frame].origin.x;
        o_rect.origin.y = [o_window frame].origin.y - o_rect.size.height +
                                                [o_window minSize].height;
        [o_btn_playlist setState: YES];
    }
    else
    {
        /* make small */
        o_rect.size.height = [o_window minSize].height;
        o_rect.origin.x = [o_window frame].origin.x;
        /* Calculate the position of the lower right corner after resize */
        o_rect.origin.y = [o_window frame].origin.y +
            [o_window frame].size.height - [o_window minSize].height;

        [o_playlist_view setAutoresizesSubviews: NO];
        [o_playlist_view removeFromSuperview];
        [o_btn_playlist setState: NO];
        b_small_window = NO; /* we aren't small here just yet. we are doing an animated resize after this */
    }

    [o_window setFrame: o_rect display:YES animate: YES];
}

- (void)updateTogglePlaylistState
{
    if( [o_window frame].size.height <= 200 )
    {
        [o_btn_playlist setState: NO];
    }
    else
    {
        [o_btn_playlist setState: YES];
    }
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
    /* Not triggered on a window resize or maxification of the window. only by window mouse dragging resize */

   /*Stores the size the controller one resize, to be able to restore it when
     toggling the playlist*/
    o_size_with_playlist = proposedFrameSize;

    if( proposedFrameSize.height <= 200 )
    {
        if( b_small_window == NO )
        {
            /* if large and going to small then hide */
            b_small_window = YES;
            [o_playlist_view setAutoresizesSubviews: NO];
            [o_playlist_view removeFromSuperview];
        }
        return NSMakeSize( proposedFrameSize.width, [o_window minSize].height);
    }
    return proposedFrameSize;
}

- (void)windowDidResize:(NSNotification *)notif
{
    if( [o_window frame].size.height > 200 && b_small_window )
    {
        /* If large and coming from small then show */
        [o_playlist_view setAutoresizesSubviews: YES];
        [o_playlist_view setFrame: NSMakeRect( 10, 10, [o_window frame].size.width - 20, [o_window frame].size.height - [o_window minSize].height - 10 )];
        [o_playlist_view setNeedsDisplay:YES];
        [[o_window contentView] addSubview: o_playlist_view];
        b_small_window = NO;
    }
    [self updateTogglePlaylistState];
}

@end

@implementation VLCMain (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    NSString *o_title = [o_mi title];
    BOOL bEnabled = TRUE;

    /* Recent Items Menu */
    if( [o_title isEqualToString: _NS("Clear Menu")] )
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
    id ** val;
    NSData * o_data;
    NSValue * o_value;
    NSInvocation * o_inv;
    NSConditionLock * o_lock;

    o_data = [[o_msg components] lastObject];
    o_inv = *((NSInvocation **)[o_data bytes]);
    [o_inv getArgument: &o_value atIndex: 2];
    val = (id **)[o_value pointerValue];
    [o_inv setArgument: val[1] atIndex: 2];
    o_lock = *(val[0]);

    [o_lock lock];
    [o_inv invoke];
    [o_lock unlockWithCondition: 1];
}

@end
