/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <vlc_common.h>
#include <vlc_keys.h>
#include <vlc_dialog.h>
#include <unistd.h> /* execl() */

#import "intf.h"
#import "fspanel.h"
#import "vout.h"
#import "prefs.h"
#import "playlist.h"
#import "playlistinfo.h"
#import "controls.h"
#import "about.h"
#import "open.h"
#import "wizard.h"
#import "extended.h"
#import "bookmarks.h"
#import "coredialogs.h"
#import "embeddedwindow.h"
#import "AppleRemote.h"
#import "eyetv.h"
#import "simple_prefs.h"

#import <AddressBook/AddressBook.h>         /* for crashlog send mechanism */
#import <IOKit/hidsystem/ev_keymap.h>         /* for the media key support */
#import <Sparkle/Sparkle.h>                 /* we're the update delegate */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

static void * ManageThread( void *user_data );

static unichar VLCKeyToCocoa( unsigned int i_key );
static unsigned int VLCModifiersToCocoa( unsigned int i_key );

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

static void MsgCallback( msg_cb_data_t *, msg_item_t *, unsigned );

#pragma mark -
#pragma mark VLC Interface Object Callbacks

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;

    memset( p_intf->p_sys, 0, sizeof( *p_intf->p_sys ) );

    /* subscribe to LibVLCCore's messages */
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf->p_libvlc, MsgCallback, NULL );
    p_intf->pf_run = Run;
    p_intf->b_should_run_on_first_thread = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static NSLock * o_appLock = nil;    // controls access to f_appExit
static int f_appExit = 0;           // set to 1 when application termination signaled

static void Run( intf_thread_t *p_intf )
{
    sigset_t set;

    /* Do it again - for some unknown reason, vlc_thread_create() often
     * fails to go to real-time priority with the first launched thread
     * (???) --Meuuh */
    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );

    /* Make sure the "force quit" menu item does quit instantly.
     * VLC overrides SIGTERM which is sent by the "force quit"
     * menu item to make sure deamon mode quits gracefully, so
     * we un-override SIGTERM here. */
    sigemptyset( &set );
    sigaddset( &set, SIGTERM );
    pthread_sigmask( SIG_UNBLOCK, &set, NULL );

    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    o_appLock = [[NSLock alloc] init];

    [VLCApplication sharedApplication];

    [[VLCMain sharedInstance] setIntf: p_intf];
    [NSBundle loadNibNamed: @"MainMenu" owner: NSApp];

    [NSApp run];
    [[VLCMain sharedInstance] applicationWillTerminate:nil];

    [o_pool release];
}

#pragma mark -
#pragma mark Variables Callback

/*****************************************************************************
 * MsgCallback: Callback triggered by the core once a new debug message is
 * ready to be displayed. We store everything in a NSArray in our Cocoa part
 * of this file, so we are forwarding everything through notifications.
 *****************************************************************************/
static void MsgCallback( msg_cb_data_t *data, msg_item_t *item, unsigned int i )
{
    int canc = vlc_savecancel();
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    /* this may happen from time to time, let's bail out as info would be useless anyway */
    if( !item->psz_module || !item->psz_msg )
        return;

    NSDictionary *o_dict = [NSDictionary dictionaryWithObjectsAndKeys:
                                [NSString stringWithUTF8String: item->psz_module], @"Module",
                                [NSString stringWithUTF8String: item->psz_msg], @"Message",
                                [NSNumber numberWithInt: item->i_type], @"Type", nil];

    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCCoreMessageReceived"
                                                        object: nil
                                                      userInfo: o_dict];

    [o_pool release];
    vlc_restorecancel( canc );
}

/*****************************************************************************
 * playlistChanged: Callback triggered by the intf-change playlist
 * variable, to let the intf update the playlist.
 *****************************************************************************/
static int PlaylistChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    if( p_intf && p_intf->p_sys )
    {
        p_intf->p_sys->b_intf_update = true;
        p_intf->p_sys->b_playlist_update = true;
        p_intf->p_sys->b_playmode_update = true;
        p_intf->p_sys->b_current_title_update = true;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ShowController: Callback triggered by the show-intf playlist variable
 * through the ShowIntf-control-intf, to let us show the controller-win;
 * usually when in fullscreen-mode
 *****************************************************************************/
static int ShowController( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    if( p_intf && p_intf->p_sys )
        p_intf->p_sys->b_intf_show = true;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * FullscreenChanged: Callback triggered by the fullscreen-change playlist
 * variable, to let the intf update the controller.
 *****************************************************************************/
static int FullscreenChanged( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t * p_intf = VLCIntf;
    if( p_intf && p_intf->p_sys )
        p_intf->p_sys->b_fullscreen_update = true;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DialogCallback: Callback triggered by the "dialog-*" variables
 * to let the intf display error and interaction dialogs
 *****************************************************************************/
static int DialogCallback( vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data )
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    VLCMain *interface = (VLCMain *)data;

    if( [[NSString stringWithUTF8String: type] isEqualToString: @"dialog-progress-bar"] )
    {
        /* the progress panel needs to update itself and therefore wants special treatment within this context */
        dialog_progress_bar_t *p_dialog = (dialog_progress_bar_t *)value.p_address;

        p_dialog->pf_update = updateProgressPanel;
        p_dialog->pf_check = checkProgressPanel;
        p_dialog->pf_destroy = destroyProgressPanel;
        p_dialog->p_sys = VLCIntf->p_libvlc;
    }

    NSValue *o_value = [NSValue valueWithPointer:value.p_address];
    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCNewCoreDialogEventNotification" object:[interface coreDialogProvider] userInfo:[NSDictionary dictionaryWithObjectsAndKeys: o_value, @"VLCDialogPointer", [NSString stringWithUTF8String: type], @"VLCDialogType", nil]];

    [o_pool release];
    return VLC_SUCCESS;
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSString *o_txt;
    if( text != NULL )
        o_txt = [NSString stringWithUTF8String: text];
    else
        o_txt = @"";

    [[[VLCMain sharedInstance] coreDialogProvider] updateProgressPanelWithText: o_txt andNumber: (double)(value * 1000.)];

    [o_pool release];
}

void destroyProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [[[VLCMain sharedInstance] coreDialogProvider] destroyProgressPanel];
    [o_pool release];
}

bool checkProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    return [[[VLCMain sharedInstance] coreDialogProvider] progressCancelled];
    [o_pool release];
}

#pragma mark -
#pragma mark Helpers

input_thread_t *getInput(void)
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NULL;
    return pl_CurrentInput(p_intf);
}

vout_thread_t *getVout(void)
{
    input_thread_t *p_input = getInput();
    if (!p_input)
        return NULL;
    vout_thread_t *p_vout = input_GetVout(p_input);
    vlc_object_release(p_input);
    return p_vout;
}

aout_instance_t *getAout(void)
{
    input_thread_t *p_input = getInput();
    if (!p_input)
        return NULL;
    aout_instance_t *p_aout = input_GetAout(p_input);
    vlc_object_release(p_input);
    return p_aout;
}

#pragma mark -
#pragma mark Private

@interface VLCMain ()
- (void)_removeOldPreferences;
@end

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

#pragma mark -
#pragma mark Initialization

static VLCMain *_o_sharedMainInstance = nil;

+ (VLCMain *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if( _o_sharedMainInstance)
    {
        [self dealloc];
        return _o_sharedMainInstance;
    }
    else
        _o_sharedMainInstance = [super init];

    p_intf = NULL;

    o_msg_lock = [[NSLock alloc] init];
    o_msg_arr = [[NSMutableArray arrayWithCapacity: 600] retain];
    /* subscribe to LibVLC's debug messages as early as possible (for us) */
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(libvlcMessageReceived:) name: @"VLCCoreMessageReceived" object: nil];

    o_about = [[VLAboutBox alloc] init];
    o_prefs = nil;
    o_open = [[VLCOpen alloc] init];
    o_wizard = [[VLCWizard alloc] init];
    o_extended = nil;
    o_bookmarks = [[VLCBookmarks alloc] init];
    o_embedded_list = [[VLCEmbeddedList alloc] init];
    o_coredialogs = [[VLCCoreDialogProvider alloc] init];
    o_info = [[VLCInfo alloc] init];

    i_lastShownVolume = -1;

    o_eyetv = [[VLCEyeTVController alloc] init];

    /* announce our launch to a potential eyetv plugin */
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"VLCOSXGUIInit"
                                                                   object: @"VLCEyeTVSupport"
                                                                 userInfo: NULL
                                                       deliverImmediately: YES];

    return _o_sharedMainInstance;
}

- (void)setIntf: (intf_thread_t *)p_mainintf {
    p_intf = p_mainintf;
}

- (intf_thread_t *)intf {
    return p_intf;
}

- (void)awakeFromNib
{
    unsigned int i_key = 0;
    playlist_t *p_playlist;
    vlc_value_t val;

    if( !p_intf ) return;

    /* Check if we already did this once. Opening the other nibs calls it too,
       because VLCMain is the owner */
    if( nib_main_loaded ) return;

    [self initStrings];

    [o_window setExcludedFromWindowsMenu: YES];
    [o_msgs_panel setExcludedFromWindowsMenu: YES];
    [o_msgs_panel setDelegate: self];

    /* In code and not in Nib for 10.4 compat */
    NSToolbar * toolbar = [[[NSToolbar alloc] initWithIdentifier:@"mainControllerToolbar"] autorelease];
    [toolbar setDelegate:self];
    [toolbar setShowsBaselineSeparator:NO];
    [toolbar setAllowsUserCustomization:NO];
    [toolbar setDisplayMode:NSToolbarDisplayModeIconOnly];
    [toolbar setAutosavesConfiguration:YES];
    [o_window setToolbar:toolbar];

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
    i_key = config_GetInt( p_intf, "key-rate-normal" );
    [o_mi_normalSpeed setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_normalSpeed setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-prev" );
    [o_mi_previous setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_previous setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-next" );
    [o_mi_next setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_next setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump+short" );
    [o_mi_fwd setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fwd setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump-short" );
    [o_mi_bwd setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_bwd setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump+medium" );
    [o_mi_fwd1m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fwd1m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump-medium" );
    [o_mi_bwd1m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_bwd1m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump+long" );
    [o_mi_fwd5m setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fwd5m setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-jump-long" );
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
    i_key = config_GetInt( p_intf, "key-toggle-fullscreen" );
    [o_mi_fullscreen setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_fullscreen setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-snapshot" );
    [o_mi_snapshot setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_snapshot setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-random" );
    [o_mi_random setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_random setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-zoom-half" );
    [o_mi_half_window setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_half_window setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-zoom-original" );
    [o_mi_normal_window setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_normal_window setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];
    i_key = config_GetInt( p_intf, "key-zoom-double" );
    [o_mi_double_window setKeyEquivalent: [NSString stringWithFormat:@"%C", VLCKeyToCocoa( i_key )]];
    [o_mi_double_window setKeyEquivalentModifierMask: VLCModifiersToCocoa(i_key)];

    var_Create( p_intf, "intf-change", VLC_VAR_BOOL );

    [self setSubmenusEnabled: FALSE];
    [o_volumeslider setEnabled: YES];
    [self manageVolumeSlider];
    [o_window setDelegate: self];

    b_restore_size = false;

    // Set that here as IB seems to be buggy
    [o_window setContentMinSize:NSMakeSize(338., 30.)];

    if( [o_window contentRectForFrameRect:[o_window frame]].size.height <= 169. )
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
        NSRect contentRect = [o_window contentRectForFrameRect:[o_window frame]];
        [o_playlist_view setFrame: NSMakeRect( 0, 0, contentRect.size.width, contentRect.size.height - [o_window contentMinSize].height )];
        [o_playlist_view setNeedsDisplay:YES];
        [o_playlist_view setAutoresizesSubviews: YES];
        [[o_window contentView] addSubview: o_playlist_view];
    }

    [self updateTogglePlaylistState];

    o_size_with_playlist = [o_window contentRectForFrameRect:[o_window frame]].size;

    p_playlist = pl_Get( p_intf );

    val.b_bool = false;

    var_AddCallback( p_playlist, "fullscreen", FullscreenChanged, self);
    var_AddCallback( p_intf->p_libvlc, "intf-show", ShowController, self);

    /* load our Core Dialogs nib */
    nib_coredialogs_loaded = [NSBundle loadNibNamed:@"CoreDialogs" owner: NSApp];

    /* subscribe to various interactive dialogues */
    var_Create( p_intf, "dialog-error", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-error", DialogCallback, self );
    var_Create( p_intf, "dialog-critical", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-critical", DialogCallback, self );
    var_Create( p_intf, "dialog-login", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-login", DialogCallback, self );
    var_Create( p_intf, "dialog-question", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-question", DialogCallback, self );
    var_Create( p_intf, "dialog-progress-bar", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "dialog-progress-bar", DialogCallback, self );
    dialog_Register( p_intf );

    /* update the playmode stuff */
    p_intf->p_sys->b_playmode_update = true;

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(refreshVoutDeviceMenu:)
                                                 name: NSApplicationDidChangeScreenParametersNotification
                                               object: nil];

    /* take care of tint changes during runtime */
    o_img_play = [NSImage imageNamed: @"play"];
    o_img_pause = [NSImage imageNamed: @"pause"];
    [self controlTintChanged];
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector( controlTintChanged )
                                                 name: NSControlTintDidChangeNotification
                                               object: nil];

    /* init Apple Remote support */
    o_remote = [[AppleRemote alloc] init];
    [o_remote setClickCountEnabledButtons: kRemoteButtonPlay];
    [o_remote setDelegate: _o_sharedMainInstance];

    /* yeah, we are done */
    nib_main_loaded = TRUE;
}

- (void)applicationWillFinishLaunching:(NSNotification *)o_notification
{
    if( !p_intf ) return;

    /* FIXME: don't poll */
    interfaceTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.5
                                     target: self selector: @selector(manageIntf:)
                                   userInfo: nil repeats: FALSE] retain];

    /* Note: we use the pthread API to support pre-10.5 */
    pthread_create( &manage_thread, NULL, ManageThread, self );

    [o_controls setupVarMenuItem: o_mi_add_intf target: (vlc_object_t *)p_intf
        var: "intf-add" selector: @selector(toggleVar:)];

    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    if( !p_intf ) return;

    [self _removeOldPreferences];

    /* Handle sleep notification */
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(computerWillSleep:)
           name:NSWorkspaceWillSleepNotification object:nil];

    [NSThread detachNewThreadSelector:@selector(lookForCrashLog) toTarget:self withObject:nil];
}

- (void)initStrings
{
    if( !p_intf ) return;

    [o_window setTitle: _NS("VLC media player")];
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
    [o_msgs_crashlog_btn setTitle: _NS("Open CrashLog...")];
    [o_msgs_save_btn setTitle: _NS("Save this Log...")];

    /* main menu */
    [o_mi_about setTitle: [_NS("About VLC media player") \
        stringByAppendingString: @"..."]];
    [o_mi_checkForUpdate setTitle: _NS("Check for Update...")];
    [o_mi_prefs setTitle: _NS("Preferences...")];
    [o_mi_add_intf setTitle: _NS("Add Interface")];
    [o_mu_add_intf setTitle: _NS("Add Interface")];
    [o_mi_services setTitle: _NS("Services")];
    [o_mi_hide setTitle: _NS("Hide VLC")];
    [o_mi_hide_others setTitle: _NS("Hide Others")];
    [o_mi_show_all setTitle: _NS("Show All")];
    [o_mi_quit setTitle: _NS("Quit VLC")];

    [o_mu_file setTitle: _ANS("1:File")];
    [o_mi_open_generic setTitle: _NS("Advanced Open File...")];
    [o_mi_open_file setTitle: _NS("Open File...")];
    [o_mi_open_disc setTitle: _NS("Open Disc...")];
    [o_mi_open_net setTitle: _NS("Open Network...")];
    [o_mi_open_capture setTitle: _NS("Open Capture Device...")];
    [o_mi_open_recent setTitle: _NS("Open Recent")];
    [o_mi_open_recent_cm setTitle: _NS("Clear Menu")];
    [o_mi_open_wizard setTitle: _NS("Streaming/Exporting Wizard...")];

    [o_mu_edit setTitle: _NS("Edit")];
    [o_mi_cut setTitle: _NS("Cut")];
    [o_mi_copy setTitle: _NS("Copy")];
    [o_mi_paste setTitle: _NS("Paste")];
    [o_mi_clear setTitle: _NS("Clear")];
    [o_mi_select_all setTitle: _NS("Select All")];

    [o_mu_controls setTitle: _NS("Playback")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_stop setTitle: _NS("Stop")];
    [o_mi_faster setTitle: _NS("Faster")];
    [o_mi_slower setTitle: _NS("Slower")];
    [o_mi_normalSpeed setTitle: _NS("Normal rate")];
    [o_mi_previous setTitle: _NS("Previous")];
    [o_mi_next setTitle: _NS("Next")];
    [o_mi_random setTitle: _NS("Random")];
    [o_mi_repeat setTitle: _NS("Repeat One")];
    [o_mi_loop setTitle: _NS("Repeat All")];
    [o_mi_quitAfterPB setTitle: _NS("Quit after Playback")];
    [o_mi_fwd setTitle: _NS("Step Forward")];
    [o_mi_bwd setTitle: _NS("Step Backward")];

    [o_mi_program setTitle: _NS("Program")];
    [o_mu_program setTitle: _NS("Program")];
    [o_mi_title setTitle: _NS("Title")];
    [o_mu_title setTitle: _NS("Title")];
    [o_mi_chapter setTitle: _NS("Chapter")];
    [o_mu_chapter setTitle: _NS("Chapter")];

    [o_mu_audio setTitle: _NS("Audio")];
    [o_mi_vol_up setTitle: _NS("Increase Volume")];
    [o_mi_vol_down setTitle: _NS("Decrease Volume")];
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
    [o_mi_aspect_ratio setTitle: _NS("Aspect-ratio")];
    [o_mu_aspect_ratio setTitle: _NS("Aspect-ratio")];
    [o_mi_crop setTitle: _NS("Crop")];
    [o_mu_crop setTitle: _NS("Crop")];
    [o_mi_screen setTitle: _NS("Fullscreen Video Device")];
    [o_mu_screen setTitle: _NS("Fullscreen Video Device")];
    [o_mi_subtitle setTitle: _NS("Subtitles Track")];
    [o_mu_subtitle setTitle: _NS("Subtitles Track")];
    [o_mi_addSub setTitle: _NS("Open File...")];
    [o_mi_deinterlace setTitle: _NS("Deinterlace")];
    [o_mu_deinterlace setTitle: _NS("Deinterlace")];
    [o_mi_ffmpeg_pp setTitle: _NS("Post processing")];
    [o_mu_ffmpeg_pp setTitle: _NS("Post processing")];
    [o_mi_teletext setTitle: _NS("Teletext")];
    [o_mi_teletext_transparent setTitle: _NS("Transparent")];
    [o_mi_teletext_index setTitle: _NS("Index")];
    [o_mi_teletext_red setTitle: _NS("Red")];
    [o_mi_teletext_green setTitle: _NS("Green")];
    [o_mi_teletext_yellow setTitle: _NS("Yellow")];
    [o_mi_teletext_blue setTitle: _NS("Blue")];

    [o_mu_window setTitle: _NS("Window")];
    [o_mi_minimize setTitle: _NS("Minimize Window")];
    [o_mi_close_window setTitle: _NS("Close Window")];
    [o_mi_player setTitle: _NS("Player...")];
    [o_mi_controller setTitle: _NS("Controller...")];
    [o_mi_equalizer setTitle: _NS("Equalizer...")];
    [o_mi_extended setTitle: _NS("Extended Controls...")];
    [o_mi_bookmarks setTitle: _NS("Bookmarks...")];
    [o_mi_playlist setTitle: _NS("Playlist...")];
    [o_mi_info setTitle: _NS("Media Information...")];
    [o_mi_messages setTitle: _NS("Messages...")];
    [o_mi_errorsAndWarnings setTitle: _NS("Errors and Warnings...")];

    [o_mi_bring_atf setTitle: _NS("Bring All to Front")];

    [o_mu_help setTitle: _NS("Help")];
    [o_mi_help setTitle: _NS("VLC media player Help...")];
    [o_mi_readme setTitle: _NS("ReadMe / FAQ...")];
    [o_mi_license setTitle: _NS("License")];
    [o_mi_documentation setTitle: _NS("Online Documentation...")];
    [o_mi_website setTitle: _NS("VideoLAN Website...")];
    [o_mi_donation setTitle: _NS("Make a donation...")];
    [o_mi_forum setTitle: _NS("Online Forum...")];

    /* dock menu */
    [o_dmi_play setTitle: _NS("Play")];
    [o_dmi_stop setTitle: _NS("Stop")];
    [o_dmi_next setTitle: _NS("Next")];
    [o_dmi_previous setTitle: _NS("Previous")];
    [o_dmi_mute setTitle: _NS("Mute")];

    /* vout menu */
    [o_vmi_play setTitle: _NS("Play")];
    [o_vmi_stop setTitle: _NS("Stop")];
    [o_vmi_prev setTitle: _NS("Previous")];
    [o_vmi_next setTitle: _NS("Next")];
    [o_vmi_volup setTitle: _NS("Volume Up")];
    [o_vmi_voldown setTitle: _NS("Volume Down")];
    [o_vmi_mute setTitle: _NS("Mute")];
    [o_vmi_fullscreen setTitle: _NS("Fullscreen")];
    [o_vmi_snapshot setTitle: _NS("Snapshot")];

    /* crash reporter panel */
    [o_crashrep_send_btn setTitle: _NS("Send")];
    [o_crashrep_dontSend_btn setTitle: _NS("Don't Send")];
    [o_crashrep_title_txt setStringValue: _NS("VLC crashed previously")];
    [o_crashrep_win setTitle: _NS("VLC crashed previously")];
    [o_crashrep_desc_txt setStringValue: _NS("Do you want to send details on the crash to VLC's development team?\n\nIf you want, you can enter a few lines on what you did before VLC crashed along with other helpful information: a link to download a sample file, a URL of a network stream, ...")];
    [o_crashrep_includeEmail_ckb setTitle: _NS("I agree to be possibly contacted about this bugreport.")];
    [o_crashrep_includeEmail_txt setStringValue: _NS("Only your default E-Mail address will be submitted, including no further information.")];
}

#pragma mark -
#pragma mark Termination

- (void)releaseRepresentedObjects:(NSMenu *)the_menu
{
    if( !p_intf ) return;

    NSArray *menuitems_array = [the_menu itemArray];
    for( int i=0; i<[menuitems_array count]; i++ )
    {
        NSMenuItem *one_item = [menuitems_array objectAtIndex: i];
        if( [one_item hasSubmenu] )
            [self releaseRepresentedObjects: [one_item submenu]];

        [one_item setRepresentedObject:NULL];
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    playlist_t * p_playlist;
    vout_thread_t * p_vout;
    int returnedValue = 0;

    if( !p_intf )
        return;

    // don't allow a double termination call. If the user has
    // already invoked the quit then simply return this time.
    int isTerminating = false;

    [o_appLock lock];
    isTerminating = (f_appExit++ > 0 ? 1 : 0);
    [o_appLock unlock];

    if (isTerminating)
        return;

    msg_Dbg( p_intf, "Terminating" );

    pthread_join( manage_thread, NULL );

    /* Make sure the intf object is getting killed */
    vlc_object_kill( p_intf );

    /* Make sure the interfaceTimer is destroyed */
    [interfaceTimer invalidate];
    [interfaceTimer release];
    interfaceTimer = nil;

    /* make sure that the current volume is saved */
    config_PutInt( p_intf->p_libvlc, "volume", i_lastShownVolume );

    /* save the prefs if they were changed in the extended panel */
    if(o_extended && [o_extended configChanged])
    {
        [o_extended savePrefs];
    }

    /* unsubscribe from the interactive dialogues */
    dialog_Unregister( p_intf );
    var_DelCallback( p_intf, "dialog-error", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-critical", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-login", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-question", DialogCallback, self );
    var_DelCallback( p_intf, "dialog-progress-bar", DialogCallback, self );

    /* remove global observer watching for vout device changes correctly */
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    /* release some other objects here, because it isn't sure whether dealloc
     * will be called later on */
    if( nib_about_loaded )
        [o_about release];

    if( nib_prefs_loaded )
    {
        [o_sprefs release];
        [o_prefs release];
    }

    if( nib_open_loaded )
        [o_open release];

    if( nib_extended_loaded )
    {
        [o_extended release];
    }

    if( nib_bookmarks_loaded )
        [o_bookmarks release];

    if( o_info )
    {
        [o_info stopTimers];
        [o_info release];
    }

    if( nib_wizard_loaded )
        [o_wizard release];

    [crashLogURLConnection cancel];
    [crashLogURLConnection release];

    [o_embedded_list release];
    [o_coredialogs release];
    [o_eyetv release];

    [o_img_pause_pressed release];
    [o_img_play_pressed release];
    [o_img_pause release];
    [o_img_play release];

    /* unsubscribe from libvlc's debug messages */
    msg_Unsubscribe( p_intf->p_sys->p_sub );

    [o_msg_arr removeAllObjects];
    [o_msg_arr release];

    [o_msg_lock release];

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];

    /* Make sure the Menu doesn't have any references to vlc objects anymore */
    [self releaseRepresentedObjects:[NSApp mainMenu]];

    /* Kill the playlist, so that it doesn't accept new request
     * such as the play request from vlc.c (we are a blocking interface). */
    p_playlist = pl_Get( p_intf );
    vlc_object_kill( p_playlist );
    libvlc_Quit( p_intf->p_libvlc );

    [self setIntf:nil];
}

#pragma mark -
#pragma mark Sparkle delegate
/* received directly before the update gets installed, so let's shut down a bit */
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    [o_remote stopListening: self];
    var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_STOP );

    /* Close the window directly, because we do know that there
     * won't be anymore video. It's currently waiting a bit. */
    [[[o_controls voutView] window] orderOut:self];
}

#pragma mark -
#pragma mark Toolbar delegate

/* Our item identifiers */
static NSString * VLCToolbarMediaControl     = @"VLCToolbarMediaControl";

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return [NSArray arrayWithObjects:
//                        NSToolbarCustomizeToolbarItemIdentifier,
//                        NSToolbarFlexibleSpaceItemIdentifier,
//                        NSToolbarSpaceItemIdentifier,
//                        NSToolbarSeparatorItemIdentifier,
                        VLCToolbarMediaControl,
                        nil ];
}

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *) toolbar
{
    return [NSArray arrayWithObjects:
                        VLCToolbarMediaControl,
                        nil ];
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
    NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: itemIdentifier] autorelease];

    if( [itemIdentifier isEqual: VLCToolbarMediaControl] )
    {
        [toolbarItem setLabel:@"Media Controls"];
        [toolbarItem setPaletteLabel:@"Media Controls"];

        NSSize size = toolbarMediaControl.frame.size;
        [toolbarItem setView:toolbarMediaControl];
        [toolbarItem setMinSize:size];
        size.width += 1000.;
        [toolbarItem setMaxSize:size];

        // Hack: For some reason we need to make sure
        // that the those element are on top
        // Add them again will put them frontmost
        [toolbarMediaControl addSubview:o_scrollfield];
        [toolbarMediaControl addSubview:o_timeslider];
        [toolbarMediaControl addSubview:o_timefield];
        [toolbarMediaControl addSubview:o_main_pgbar];

        /* TODO: setup a menu */
    }
    else
    {
        /* itemIdentifier referred to a toolbar item that is not
         * provided or supported by us or Cocoa
         * Returning nil will inform the toolbar
         * that this kind of item is not supported */
        toolbarItem = nil;
    }
    return toolbarItem;
}

#pragma mark -
#pragma mark Other notification

- (void)controlTintChanged
{
    BOOL b_playing = NO;

    if( [o_btn_play alternateImage] == o_img_play_pressed )
        b_playing = YES;

    if( [NSColor currentControlTint] == NSGraphiteControlTint )
    {
        o_img_play_pressed = [NSImage imageNamed: @"play_graphite"];
        o_img_pause_pressed = [NSImage imageNamed: @"pause_graphite"];

        [o_btn_prev setAlternateImage: [NSImage imageNamed: @"previous_graphite"]];
        [o_btn_rewind setAlternateImage: [NSImage imageNamed: @"skip_previous_graphite"]];
        [o_btn_stop setAlternateImage: [NSImage imageNamed: @"stop_graphite"]];
        [o_btn_ff setAlternateImage: [NSImage imageNamed: @"skip_forward_graphite"]];
        [o_btn_next setAlternateImage: [NSImage imageNamed: @"next_graphite"]];
        [o_btn_fullscreen setAlternateImage: [NSImage imageNamed: @"fullscreen_graphite"]];
        [o_btn_playlist setAlternateImage: [NSImage imageNamed: @"playlistdrawer_graphite"]];
        [o_btn_equalizer setAlternateImage: [NSImage imageNamed: @"equalizerdrawer_graphite"]];
    }
    else
    {
        o_img_play_pressed = [NSImage imageNamed: @"play_blue"];
        o_img_pause_pressed = [NSImage imageNamed: @"pause_blue"];

        [o_btn_prev setAlternateImage: [NSImage imageNamed: @"previous_blue"]];
        [o_btn_rewind setAlternateImage: [NSImage imageNamed: @"skip_previous_blue"]];
        [o_btn_stop setAlternateImage: [NSImage imageNamed: @"stop_blue"]];
        [o_btn_ff setAlternateImage: [NSImage imageNamed: @"skip_forward_blue"]];
        [o_btn_next setAlternateImage: [NSImage imageNamed: @"next_blue"]];
        [o_btn_fullscreen setAlternateImage: [NSImage imageNamed: @"fullscreen_blue"]];
        [o_btn_playlist setAlternateImage: [NSImage imageNamed: @"playlistdrawer_blue"]];
        [o_btn_equalizer setAlternateImage: [NSImage imageNamed: @"equalizerdrawer_blue"]];
    }

    if( b_playing )
        [o_btn_play setAlternateImage: o_img_play_pressed];
    else
        [o_btn_play setAlternateImage: o_img_pause_pressed];
}

/* Listen to the remote in exclusive mode, only when VLC is the active
   application */
- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
    if( !p_intf ) return;
	if( config_GetInt( p_intf, "macosx-appleremote" ) == YES )
		[o_remote startListening: self];
}
- (void)applicationDidResignActive:(NSNotification *)aNotification
{
    if( !p_intf ) return;
    [o_remote stopListening: self];
}

/* Triggered when the computer goes to sleep */
- (void)computerWillSleep: (NSNotification *)notification
{
    /* Pause */
    if( p_intf && p_intf->p_sys->i_play_status == PLAYING_S )
    {
        var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_PLAY_PAUSE );
    }
}

#pragma mark -
#pragma mark File opening

- (BOOL)application:(NSApplication *)o_app openFile:(NSString *)o_filename
{
    BOOL b_autoplay = config_GetInt( VLCIntf, "macosx-autoplay" );
    NSDictionary *o_dic = [NSDictionary dictionaryWithObjectsAndKeys: o_filename, @"ITEM_URL", nil];
    if( b_autoplay )
        [o_playlist appendArray: [NSArray arrayWithObject: o_dic] atPos: -1 enqueue: NO];
    else
        [o_playlist appendArray: [NSArray arrayWithObject: o_dic] atPos: -1 enqueue: YES];

    return( TRUE );
}

/* When user click in the Dock icon our double click in the finder */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if(!hasVisibleWindows)
        [o_window makeKeyAndOrderFront:self];

    return YES;
}

#pragma mark -
#pragma mark Apple Remote Control

/* Helper method for the remote control interface in order to trigger forward/backward and volume
   increase/decrease as long as the user holds the left/right, plus/minus button */
- (void) executeHoldActionForRemoteButton: (NSNumber*) buttonIdentifierNumber
{
    if(b_remote_button_hold)
    {
        switch([buttonIdentifierNumber intValue])
        {
            case kRemoteButtonRight_Hold:
                  [o_controls forward: self];
            break;
            case kRemoteButtonLeft_Hold:
                  [o_controls backward: self];
            break;
            case kRemoteButtonVolume_Plus_Hold:
                [o_controls volumeUp: self];
            break;
            case kRemoteButtonVolume_Minus_Hold:
                [o_controls volumeDown: self];
            break;
        }
        if(b_remote_button_hold)
        {
            /* trigger event */
            [self performSelector:@selector(executeHoldActionForRemoteButton:)
                         withObject:buttonIdentifierNumber
                         afterDelay:0.25];
        }
    }
}

/* Apple Remote callback */
- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier
               pressedDown: (BOOL) pressedDown
                clickCount: (unsigned int) count
{
    switch( buttonIdentifier )
    {
        case kRemoteButtonPlay:
            if(count >= 2) {
                [o_controls toogleFullscreen:self];
            } else {
                [o_controls play: self];
            }
            break;
        case kRemoteButtonVolume_Plus:
            [o_controls volumeUp: self];
            break;
        case kRemoteButtonVolume_Minus:
            [o_controls volumeDown: self];
            break;
        case kRemoteButtonRight:
            [o_controls next: self];
            break;
        case kRemoteButtonLeft:
            [o_controls prev: self];
            break;
        case kRemoteButtonRight_Hold:
        case kRemoteButtonLeft_Hold:
        case kRemoteButtonVolume_Plus_Hold:
        case kRemoteButtonVolume_Minus_Hold:
            /* simulate an event as long as the user holds the button */
            b_remote_button_hold = pressedDown;
            if( pressedDown )
            {
                NSNumber* buttonIdentifierNumber = [NSNumber numberWithInt: buttonIdentifier];
                [self performSelector:@selector(executeHoldActionForRemoteButton:)
                           withObject:buttonIdentifierNumber];
            }
            break;
        case kRemoteButtonMenu:
            [o_controls showPosition: self];
            break;
        default:
            /* Add here whatever you want other buttons to do */
            break;
    }
}

#pragma mark -
#pragma mark String utility
// FIXME: this has nothing to do here

- (NSString *)localizedString:(const char *)psz
{
    NSString * o_str = nil;

    if( psz != NULL )
    {
        o_str = [[[NSString alloc] initWithUTF8String: psz] autorelease];

        if( o_str == NULL )
        {
            msg_Err( VLCIntf, "could not translate: %s", psz );
            return( @"" );
        }
    }
    else
    {
        msg_Warn( VLCIntf, "can't translate empty strings" );
        return( @"" );
    }

    return( o_str );
}



- (char *)delocalizeString:(NSString *)id
{
    NSData * o_data = [id dataUsingEncoding: NSUTF8StringEncoding
                          allowLossyConversion: NO];
    char * psz_string;

    if( o_data == nil )
    {
        o_data = [id dataUsingEncoding: NSUTF8StringEncoding
                     allowLossyConversion: YES];
        psz_string = malloc( [o_data length] + 1 );
        [o_data getBytes: psz_string];
        psz_string[ [o_data length] ] = '\0';
        msg_Err( VLCIntf, "cannot convert to the requested encoding: %s",
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
        if([o_wrapped lineRangeForRange:
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


#pragma mark -
#pragma mark Key Shortcuts

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
    { NSInsertFunctionKey, KEY_INSERT },
    { NSHomeFunctionKey, KEY_HOME },
    { NSEndFunctionKey, KEY_END },
    { NSPageUpFunctionKey, KEY_PAGEUP },
    { NSPageDownFunctionKey, KEY_PAGEDOWN },
    { NSMenuFunctionKey, KEY_MENU },
    { NSTabCharacter, KEY_TAB },
    { NSCarriageReturnCharacter, KEY_ENTER },
    { NSEnterCharacter, KEY_ENTER },
    { NSBackspaceCharacter, KEY_BACKSPACE },
    {0,0}
};

static unichar VLCKeyToCocoa( unsigned int i_key )
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

static unsigned int VLCModifiersToCocoa( unsigned int i_key )
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
 * hasDefinedShortcutKey: Check to see if the key press is a defined VLC
 * shortcut key.  If it is, pass it off to VLC for handling and return YES,
 * otherwise ignore it and return NO (where it will get handled by Cocoa).
 *****************************************************************************/
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;
    const struct hotkey *p_hotkeys;
    int i;

    val.i_int = 0;
    p_hotkeys = p_intf->p_libvlc->p_hotkeys;

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
            var_Set( p_intf->p_libvlc, "key-pressed", val );
            return YES;
        }
    }

    return NO;
}

#pragma mark -
#pragma mark Other objects getters

- (id)controls
{
    if( o_controls )
        return o_controls;

    return nil;
}

- (id)simplePreferences
{
    if( !o_sprefs )
        return nil;

    if( !nib_prefs_loaded )
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_sprefs;
}

- (id)preferences
{
    if( !o_prefs )
        return nil;

    if( !nib_prefs_loaded )
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_prefs;
}

- (id)playlist
{
    if( o_playlist )
        return o_playlist;

    return nil;
}

- (BOOL)isPlaylistCollapsed
{
    return ![o_btn_playlist state];
}

- (id)info
{
    if( o_info )
        return o_info;

    return nil;
}

- (id)wizard
{
    if( o_wizard )
        return o_wizard;

    return nil;
}

- (id)bookmarks
{
    if( o_bookmarks )
        return o_bookmarks;

    return nil;
}

- (id)embeddedList
{
    if( o_embedded_list )
        return o_embedded_list;

    return nil;
}

- (id)coreDialogProvider
{
    if( o_coredialogs )
        return o_coredialogs;

    return nil;
}

- (id)mainIntfPgbar
{
    if( o_main_pgbar )
        return o_main_pgbar;

    return nil;
}

- (id)controllerWindow
{
    if( o_window )
        return o_window;
    return nil;
}

- (id)voutMenu
{
    return o_vout_menu;
}

- (id)eyeTVController
{
    if( o_eyetv )
        return o_eyetv;

    return nil;
}

- (id)appleRemoteController
{
	return o_remote;
}

#pragma mark -
#pragma mark Polling

/*****************************************************************************
 * ManageThread: An ugly thread that polls
 *****************************************************************************/
static void * ManageThread( void *user_data )
{
    id self = user_data;

    [self manage];

    return NULL;
}

struct manage_cleanup_stack {
    intf_thread_t * p_intf;
    input_thread_t ** p_input;
    playlist_t * p_playlist;
    id self;
};

static void manage_cleanup( void * args )
{
    struct manage_cleanup_stack * manage_cleanup_stack = args;
    intf_thread_t * p_intf = manage_cleanup_stack->p_intf;
    input_thread_t * p_input = *manage_cleanup_stack->p_input;
    id self = manage_cleanup_stack->self;
    playlist_t * p_playlist = manage_cleanup_stack->p_playlist;

    var_DelCallback( p_playlist, "item-current", PlaylistChanged, self );
    var_DelCallback( p_playlist, "intf-change", PlaylistChanged, self );
    var_DelCallback( p_playlist, "item-change", PlaylistChanged, self );
    var_DelCallback( p_playlist, "playlist-item-append", PlaylistChanged, self );
    var_DelCallback( p_playlist, "playlist-item-deleted", PlaylistChanged, self );

    if( p_input ) vlc_object_release( p_input );
}

- (void)manage
{
    playlist_t * p_playlist;
    input_thread_t * p_input = NULL;

    /* new thread requires a new pool */

    vlc_thread_set_priority( p_intf, VLC_THREAD_PRIORITY_LOW );

    p_playlist = pl_Get( p_intf );

    var_AddCallback( p_playlist, "item-current", PlaylistChanged, self );
    var_AddCallback( p_playlist, "intf-change", PlaylistChanged, self );
    var_AddCallback( p_playlist, "item-change", PlaylistChanged, self );
    var_AddCallback( p_playlist, "playlist-item-append", PlaylistChanged, self );
    var_AddCallback( p_playlist, "playlist-item-deleted", PlaylistChanged, self );

    struct manage_cleanup_stack stack = { p_intf, &p_input, p_playlist, self };
    pthread_cleanup_push(manage_cleanup, &stack);

    bool exitLoop = false;
    while( !exitLoop )
    {
        NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

        if( !p_input )
        {
            p_input = playlist_CurrentInput( p_playlist );

            /* Refresh the interface */
            if( p_input )
            {
                msg_Dbg( p_intf, "input has changed, refreshing interface" );
                p_intf->p_sys->b_input_update = true;
            }
        }
        else if( !vlc_object_alive (p_input) || p_input->b_dead )
        {
            /* input stopped */
            p_intf->p_sys->b_intf_update = true;
            p_intf->p_sys->i_play_status = END_S;
            msg_Dbg( p_intf, "input has stopped, refreshing interface" );
            vlc_object_release( p_input );
            p_input = NULL;
        }
        else if( cachedInputState != input_GetState( p_input ) )
        {
            p_intf->p_sys->b_intf_update = true;
        }

        /* Manage volume status */
        [self manageVolumeSlider];

        msleep( INTF_IDLE_SLEEP );

        [pool release];

        [o_appLock lock];
        exitLoop = (f_appExit != 0 ? true : false);
        [o_appLock unlock];
    }

    pthread_cleanup_pop(1);
}

- (void)manageVolumeSlider
{
    audio_volume_t i_volume;
    playlist_t * p_playlist = pl_Get( p_intf );

    aout_VolumeGet( p_playlist, &i_volume );

    if( i_volume != i_lastShownVolume )
    {
        i_lastShownVolume = i_volume;
        p_intf->p_sys->b_volume_update = TRUE;
    }
}

- (void)manageIntf:(NSTimer *)o_timer
{
    vlc_value_t val;
    playlist_t * p_playlist;
    input_thread_t * p_input;

    if( p_intf->p_sys->b_input_update )
    {
        /* Called when new input is opened */
        p_intf->p_sys->b_current_title_update = true;
        p_intf->p_sys->b_intf_update = true;
        p_intf->p_sys->b_input_update = false;
        [self setupMenus]; /* Make sure input menu is up to date */

        /* update our info-panel to reflect the new item, if we don't show
         * the playlist or the selection is empty */
        if( [self isPlaylistCollapsed] == YES )
        {
            playlist_t * p_playlist = pl_Get( p_intf );
            PL_LOCK;
            playlist_item_t * p_item = playlist_CurrentPlayingItem( p_playlist );
            PL_UNLOCK;
            if( p_item )
                [[self info] updatePanelWithItem: p_item->p_input];
        }
    }
    if( p_intf->p_sys->b_intf_update )
    {
        bool b_input = false;
        bool b_plmul = false;
        bool b_control = false;
        bool b_seekable = false;
        bool b_chapters = false;

        playlist_t * p_playlist = pl_Get( p_intf );

        PL_LOCK;
        b_plmul = playlist_CurrentSize( p_playlist ) > 1;
        PL_UNLOCK;

        p_input = playlist_CurrentInput( p_playlist );

        bool b_buffering = NO;

        if( ( b_input = ( p_input != NULL ) ) )
        {
            /* seekable streams */
            cachedInputState = input_GetState( p_input );
            if ( cachedInputState == INIT_S ||
                 cachedInputState == OPENING_S )
            {
                b_buffering = YES;
            }

            /* seekable streams */
            b_seekable = var_GetBool( p_input, "can-seek" );

            /* check whether slow/fast motion is possible */
            b_control = var_GetBool( p_input, "can-rate" );

            /* chapters & titles */
            //b_chapters = p_input->stream.i_area_nb > 1;
            vlc_object_release( p_input );
        }

        if( b_buffering )
        {
            [o_main_pgbar startAnimation:self];
            [o_main_pgbar setIndeterminate:YES];
            [o_main_pgbar setHidden:NO];
        }
        else
        {
            [o_main_pgbar stopAnimation:self];
            [o_main_pgbar setHidden:YES];
        }

        [o_btn_stop setEnabled: b_input];
        [o_embedded_window setStop: b_input];
        [o_btn_ff setEnabled: b_seekable];
        [o_btn_rewind setEnabled: b_seekable];
        [o_btn_prev setEnabled: (b_plmul || b_chapters)];
        [o_embedded_window setPrev: (b_plmul || b_chapters)];
        [o_btn_next setEnabled: (b_plmul || b_chapters)];
        [o_embedded_window setNext: (b_plmul || b_chapters)];

        [o_timeslider setFloatValue: 0.0];
        [o_timeslider setEnabled: b_seekable];
        [o_timefield setStringValue: @"00:00"];
        [[[self controls] fspanel] setStreamPos: 0 andTime: @"00:00"];
        [[[self controls] fspanel] setSeekable: b_seekable];

        [o_embedded_window setSeekable: b_seekable];
        [o_embedded_window setTime:@"00:00" position:0.0];

        p_intf->p_sys->b_current_title_update = true;

        p_intf->p_sys->b_intf_update = false;
    }

    if( p_intf->p_sys->b_playmode_update )
    {
        [o_playlist playModeUpdated];
        p_intf->p_sys->b_playmode_update = false;
    }
    if( p_intf->p_sys->b_playlist_update )
    {
        [o_playlist playlistUpdated];
        p_intf->p_sys->b_playlist_update = false;
    }

    if( p_intf->p_sys->b_fullscreen_update )
    {
        p_intf->p_sys->b_fullscreen_update = false;
    }

    if( p_intf->p_sys->b_intf_show )
    {
        if( [[o_controls voutView] isFullscreen] && config_GetInt( VLCIntf, "macosx-fspanel" ) )
            [[o_controls fspanel] fadeIn];
        else
            [o_window makeKeyAndOrderFront: self];

        p_intf->p_sys->b_intf_show = false;
    }

    p_input = pl_CurrentInput( p_intf );
    if( p_input && vlc_object_alive (p_input) )
    {
        vlc_value_t val;

        if( p_intf->p_sys->b_current_title_update )
        {
            NSString *aString;
            input_item_t * p_item = input_GetItem( p_input );
            char * name = input_item_GetNowPlaying( p_item );

            if( !name )
                name = input_item_GetName( p_item );

            aString = [NSString stringWithUTF8String:name];

            free(name);

            [self setScrollField: aString stopAfter:-1];
            [[[self controls] fspanel] setStreamTitle: aString];

            [[o_controls voutView] updateTitle];

            [o_playlist updateRowSelection];

            p_intf->p_sys->b_current_title_update = FALSE;
        }

        if( [o_timeslider isEnabled] )
        {
            /* Update the slider */
            vlc_value_t time;
            NSString * o_time;
            vlc_value_t pos;
            char psz_time[MSTRTIME_MAX_SIZE];
            float f_updated;

            var_Get( p_input, "position", &pos );
            f_updated = 10000. * pos.f_float;
            [o_timeslider setFloatValue: f_updated];

            var_Get( p_input, "time", &time );

            mtime_t dur = input_item_GetDuration( input_GetItem( p_input ) );
            if( b_time_remaining && dur != -1 )
            {
                o_time = [NSString stringWithFormat: @"-%s", secstotimestr( psz_time, ((dur - time.i_time) / 1000000))];
            }
            else
                o_time = [NSString stringWithUTF8String: secstotimestr( psz_time, (time.i_time / 1000000) )];

            [o_timefield setStringValue: o_time];
            [[[self controls] fspanel] setStreamPos: f_updated andTime: o_time];
            [o_embedded_window setTime: o_time position: f_updated];
        }

        /* Manage Playing status */
        var_Get( p_input, "state", &val );
        if( p_intf->p_sys->i_play_status != val.i_int )
        {
            p_intf->p_sys->i_play_status = val.i_int;
            [self playStatusUpdated: p_intf->p_sys->i_play_status];
            [o_embedded_window playStatusUpdated: p_intf->p_sys->i_play_status];
        }
        vlc_object_release( p_input );
    }
    else if( p_input )
    {
        vlc_object_release( p_input );
    }
    else
    {
        p_intf->p_sys->i_play_status = END_S;
        [self playStatusUpdated: p_intf->p_sys->i_play_status];
        [o_embedded_window playStatusUpdated: p_intf->p_sys->i_play_status];
        [self setSubmenusEnabled: FALSE];
    }

    if( p_intf->p_sys->b_volume_update )
    {
        NSString *o_text;
        int i_volume_step = 0;
        o_text = [NSString stringWithFormat: _NS("Volume: %d%%"), i_lastShownVolume * 400 / AOUT_VOLUME_MAX];
        if( i_lastShownVolume != -1 )
        [self setScrollField:o_text stopAfter:1000000];
        i_volume_step = config_GetInt( p_intf->p_libvlc, "volume-step" );
        [o_volumeslider setFloatValue: (float)i_lastShownVolume / i_volume_step];
        [o_volumeslider setEnabled: TRUE];
        [o_embedded_window setVolumeSlider: (float)i_lastShownVolume / i_volume_step];
        [o_embedded_window setVolumeEnabled: TRUE];
        [[[self controls] fspanel] setVolumeLevel: (float)i_lastShownVolume / i_volume_step];
        p_intf->p_sys->b_mute = ( i_lastShownVolume == 0 );
        p_intf->p_sys->b_volume_update = FALSE;
    }

end:
    [self updateMessageDisplay];

    if( ((i_end_scroll != -1) && (mdate() > i_end_scroll)) || !p_input )
        [self resetScrollField];

    [interfaceTimer autorelease];

    interfaceTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.3
        target: self selector: @selector(manageIntf:)
        userInfo: nil repeats: FALSE] retain];
}

#pragma mark -
#pragma mark Interface update

- (void)setupMenus
{
    playlist_t * p_playlist = pl_Get( p_intf );
    input_thread_t * p_input = playlist_CurrentInput( p_playlist );
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

        /* special case for "Open File" inside the subtitles menu item */
        if( [o_mi_videotrack isEnabled] == YES )
            [o_mi_subtitle setEnabled: YES];

        aout_instance_t * p_aout = input_GetAout( p_input );
        if( p_aout != NULL )
        {
            [o_controls setupVarMenuItem: o_mi_channels target: (vlc_object_t *)p_aout
                var: "audio-channels" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_device target: (vlc_object_t *)p_aout
                var: "audio-device" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_visual target: (vlc_object_t *)p_aout
                var: "visual" selector: @selector(toggleVar:)];
            vlc_object_release( (vlc_object_t *)p_aout );
        }

        vout_thread_t * p_vout = input_GetVout( p_input );

        if( p_vout != NULL )
        {
            vlc_object_t * p_dec_obj;

            [o_controls setupVarMenuItem: o_mi_aspect_ratio target: (vlc_object_t *)p_vout
                var: "aspect-ratio" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_crop target: (vlc_object_t *) p_vout
                var: "crop" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_screen target: (vlc_object_t *)p_vout
                var: "video-device" selector: @selector(toggleVar:)];

            [o_controls setupVarMenuItem: o_mi_deinterlace target: (vlc_object_t *)p_vout
                var: "deinterlace-mode" selector: @selector(toggleVar:)];

#if 1
           [o_controls setupVarMenuItem: o_mi_ffmpeg_pp target:
                    (vlc_object_t *)p_vout var:"postprocess" selector:
                    @selector(toggleVar:)];

#endif
            vlc_object_release( (vlc_object_t *)p_vout );
        }
        vlc_object_release( p_input );
    }
}

- (void)refreshVoutDeviceMenu:(NSNotification *)o_notification
{
    int x, y = 0;
    vout_thread_t * p_vout = getVout();
    if( !p_vout )
        return;

    /* clean the menu before adding new entries */
    if( [o_mi_screen hasSubmenu] )
    {
        y = [[o_mi_screen submenu] numberOfItems] - 1;
        msg_Dbg( VLCIntf, "%i items in submenu", y );
        while( x != y )
        {
            msg_Dbg( VLCIntf, "removing item %i of %i", x, y );
            [[o_mi_screen submenu] removeItemAtIndex: x];
            x++;
        }
    }

    [o_controls setupVarMenuItem: o_mi_screen target: (vlc_object_t *)p_vout
                             var: "video-device" selector: @selector(toggleVar:)];
    vlc_object_release( (vlc_object_t *)p_vout );
}

- (void)setScrollField:(NSString *)o_string stopAfter:(int)timeout
{
    if( timeout != -1 )
        i_end_scroll = mdate() + timeout;
    else
        i_end_scroll = -1;
    [o_scrollfield setStringValue: o_string];
    [o_embedded_window setScrollString: o_string];
}

- (void)resetScrollField
{
    playlist_t * p_playlist = pl_Get( p_intf );
    input_thread_t * p_input = playlist_CurrentInput( p_playlist );

    i_end_scroll = -1;
    if( p_input && vlc_object_alive (p_input) )
    {
        NSString *o_temp;
        PL_LOCK;
        playlist_item_t * p_item = playlist_CurrentPlayingItem( p_playlist );
        if( input_item_GetNowPlaying( p_item->p_input ) )
            o_temp = [NSString stringWithUTF8String:input_item_GetNowPlaying( p_item->p_input )];
        else
            o_temp = [NSString stringWithUTF8String:p_item->p_input->psz_name];
        PL_UNLOCK;
        [self setScrollField: o_temp stopAfter:-1];
        [[[self controls] fspanel] setStreamTitle: o_temp];
        vlc_object_release( p_input );
        return;
    }
    [self setScrollField: _NS("VLC media player") stopAfter:-1];
}

- (void)playStatusUpdated:(int)i_status
{
    if( i_status == PLAYING_S )
    {
        [[[self controls] fspanel] setPause];
        [o_btn_play setImage: o_img_pause];
        [o_btn_play setAlternateImage: o_img_pause_pressed];
        [o_btn_play setToolTip: _NS("Pause")];
        [o_mi_play setTitle: _NS("Pause")];
        [o_dmi_play setTitle: _NS("Pause")];
        [o_vmi_play setTitle: _NS("Pause")];
    }
    else
    {
        [[[self controls] fspanel] setPlay];
        [o_btn_play setImage: o_img_play];
        [o_btn_play setAlternateImage: o_img_play_pressed];
        [o_btn_play setToolTip: _NS("Play")];
        [o_mi_play setTitle: _NS("Play")];
        [o_dmi_play setTitle: _NS("Play")];
        [o_vmi_play setTitle: _NS("Play")];
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
    [o_mi_aspect_ratio setEnabled: b_enabled];
    [o_mi_crop setEnabled: b_enabled];
    [o_mi_teletext setEnabled: b_enabled];
}

- (IBAction)timesliderUpdate:(id)sender
{
    float f_updated;
    playlist_t * p_playlist;
    input_thread_t * p_input;

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
    p_playlist = pl_Get( p_intf );
    p_input = playlist_CurrentInput( p_playlist );
    if( p_input != NULL )
    {
        vlc_value_t time;
        vlc_value_t pos;
        NSString * o_time;
        char psz_time[MSTRTIME_MAX_SIZE];

        pos.f_float = f_updated / 10000.;
        var_Set( p_input, "position", pos );
        [o_timeslider setFloatValue: f_updated];

        var_Get( p_input, "time", &time );

        mtime_t dur = input_item_GetDuration( input_GetItem( p_input ) );
        if( b_time_remaining && dur != -1 )
        {
            o_time = [NSString stringWithFormat: @"-%s", secstotimestr( psz_time, ((dur - time.i_time) / 1000000) )];
        }
        else
            o_time = [NSString stringWithUTF8String: secstotimestr( psz_time, (time.i_time / 1000000) )];

        [o_timefield setStringValue: o_time];
        [[[self controls] fspanel] setStreamPos: f_updated andTime: o_time];
        [o_embedded_window setTime: o_time position: f_updated];
        vlc_object_release( p_input );
    }
}

- (IBAction)timeFieldWasClicked:(id)sender
{
    b_time_remaining = !b_time_remaining;
}


#pragma mark -
#pragma mark Recent Items

- (IBAction)clearRecentItems:(id)sender
{
    [[NSDocumentController sharedDocumentController]
                          clearRecentDocuments: nil];
}

- (void)openRecentItem:(id)sender
{
    [self application: nil openFile: [sender title]];
}

#pragma mark -
#pragma mark Panels

- (IBAction)intfOpenFile:(id)sender
{
    if( !nib_open_loaded )
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];
        [o_open awakeFromNib];
        [o_open openFile];
    } else {
        [o_open openFile];
    }
}

- (IBAction)intfOpenFileGeneric:(id)sender
{
    if( !nib_open_loaded )
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];
        [o_open awakeFromNib];
        [o_open openFileGeneric];
    } else {
        [o_open openFileGeneric];
    }
}

- (IBAction)intfOpenDisc:(id)sender
{
    if( !nib_open_loaded )
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];
        [o_open awakeFromNib];
        [o_open openDisc];
    } else {
        [o_open openDisc];
    }
}

- (IBAction)intfOpenNet:(id)sender
{
    if( !nib_open_loaded )
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];
        [o_open awakeFromNib];
        [o_open openNet];
    } else {
        [o_open openNet];
    }
}

- (IBAction)intfOpenCapture:(id)sender
{
    if( !nib_open_loaded )
    {
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];
        [o_open awakeFromNib];
        [o_open openCapture];
    } else {
        [o_open openCapture];
    }
}

- (IBAction)showWizard:(id)sender
{
    if( !nib_wizard_loaded )
    {
        nib_wizard_loaded = [NSBundle loadNibNamed:@"Wizard" owner: NSApp];
        [o_wizard initStrings];
        [o_wizard resetWizard];
        [o_wizard showWizard];
    } else {
        [o_wizard resetWizard];
        [o_wizard showWizard];
    }
}

- (IBAction)showExtended:(id)sender
{
    if( o_extended == nil )
        o_extended = [[VLCExtended alloc] init];

    if( !nib_extended_loaded )
        nib_extended_loaded = [NSBundle loadNibNamed:@"Extended" owner: NSApp];

    [o_extended showPanel];
}

- (IBAction)showBookmarks:(id)sender
{
    /* we need the wizard-nib for the bookmarks's extract functionality */
    if( !nib_wizard_loaded )
    {
        nib_wizard_loaded = [NSBundle loadNibNamed:@"Wizard" owner: NSApp];
        [o_wizard initStrings];
    }

    if( !nib_bookmarks_loaded )
        nib_bookmarks_loaded = [NSBundle loadNibNamed:@"Bookmarks" owner: NSApp];

    [o_bookmarks showBookmarks];
}

- (IBAction)viewPreferences:(id)sender
{
    if( !nib_prefs_loaded )
    {
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];
        o_sprefs = [[VLCSimplePrefs alloc] init];
        o_prefs= [[VLCPrefs alloc] init];
    }

    [o_sprefs showSimplePrefs];
}

#pragma mark -
#pragma mark Help and Docs

- (IBAction)viewAbout:(id)sender
{
    if( !nib_about_loaded )
        nib_about_loaded = [NSBundle loadNibNamed:@"About" owner: NSApp];

    [o_about showAbout];
}

- (IBAction)showLicense:(id)sender
{
    if( !nib_about_loaded )
        nib_about_loaded = [NSBundle loadNibNamed:@"About" owner: NSApp];

    [o_about showGPL: sender];
}

- (IBAction)viewHelp:(id)sender
{
    if( !nib_about_loaded )
    {
        nib_about_loaded = [NSBundle loadNibNamed:@"About" owner: NSApp];
        [o_about showHelp];
    }
    else
        [o_about showHelp];
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

- (IBAction)openWebsite:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://www.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
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

#pragma mark -
#pragma mark Crash Log
- (void)sendCrashLog:(NSString *)crashLog withUserComment:(NSString *)userComment
{
    NSString *urlStr = @"http://jones.videolan.org/crashlog/sendcrashreport.php";
    NSURL *url = [NSURL URLWithString:urlStr];

    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];

    NSString * email;
    if( [o_crashrep_includeEmail_ckb state] == NSOnState )
    {
        ABPerson * contact = [[ABAddressBook sharedAddressBook] me];
        ABMultiValue *emails = [contact valueForProperty:kABEmailProperty];
        email = [emails valueAtIndex:[emails indexForIdentifier:
                    [emails primaryIdentifier]]];
    }
    else
        email = [NSString string];

    NSString *postBody;
    postBody = [NSString stringWithFormat:@"CrashLog=%@&Comment=%@&Email=%@\r\n",
            [crashLog stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
            [userComment stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
            [email stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];

    [req setHTTPBody:[postBody dataUsingEncoding:NSUTF8StringEncoding]];

    /* Released from delegate */
    crashLogURLConnection = [[NSURLConnection alloc] initWithRequest:req delegate:self];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    [crashLogURLConnection release];
    crashLogURLConnection = nil;
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    NSRunCriticalAlertPanel(_NS("Error when sending the Crash Report"), [error localizedDescription], @"OK", nil, nil);
    [crashLogURLConnection release];
    crashLogURLConnection = nil;
}

- (NSString *)latestCrashLogPathPreviouslySeen:(BOOL)previouslySeen
{
    NSString * crashReporter = [@"~/Library/Logs/CrashReporter" stringByExpandingTildeInPath];
    NSDirectoryEnumerator *direnum = [[NSFileManager defaultManager] enumeratorAtPath:crashReporter];
    NSString *fname;
    NSString * latestLog = nil;
    int year  = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportYear"] : 0;
    int month = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportMonth"]: 0;
    int day   = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportDay"]  : 0;
    int hours = !previouslySeen ? [[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportHours"]: 0;

    while (fname = [direnum nextObject])
    {
        [direnum skipDescendents];
        if([fname hasPrefix:@"VLC"] && [fname hasSuffix:@"crash"])
        {
            NSArray * compo = [fname componentsSeparatedByString:@"_"];
            if( [compo count] < 3 ) continue;
            compo = [[compo objectAtIndex:1] componentsSeparatedByString:@"-"];
            if( [compo count] < 4 ) continue;

            // Dooh. ugly.
            if( year < [[compo objectAtIndex:0] intValue] ||
                (year ==[[compo objectAtIndex:0] intValue] &&
                 (month < [[compo objectAtIndex:1] intValue] ||
                  (month ==[[compo objectAtIndex:1] intValue] &&
                   (day   < [[compo objectAtIndex:2] intValue] ||
                    (day   ==[[compo objectAtIndex:2] intValue] &&
                      hours < [[compo objectAtIndex:3] intValue] ))))))
            {
                year  = [[compo objectAtIndex:0] intValue];
                month = [[compo objectAtIndex:1] intValue];
                day   = [[compo objectAtIndex:2] intValue];
                hours = [[compo objectAtIndex:3] intValue];
                latestLog = [crashReporter stringByAppendingPathComponent:fname];
            }
        }
    }

    if(!(latestLog && [[NSFileManager defaultManager] fileExistsAtPath:latestLog]))
        return nil;

    if( !previouslySeen )
    {
        [[NSUserDefaults standardUserDefaults] setInteger:year  forKey:@"LatestCrashReportYear"];
        [[NSUserDefaults standardUserDefaults] setInteger:month forKey:@"LatestCrashReportMonth"];
        [[NSUserDefaults standardUserDefaults] setInteger:day   forKey:@"LatestCrashReportDay"];
        [[NSUserDefaults standardUserDefaults] setInteger:hours forKey:@"LatestCrashReportHours"];
    }
    return latestLog;
}

- (NSString *)latestCrashLogPath
{
    return [self latestCrashLogPathPreviouslySeen:YES];
}

- (void)lookForCrashLog
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    // This pref key doesn't exists? this VLC is an upgrade, and this crash log come from previous version
    BOOL areCrashLogsTooOld = ![[NSUserDefaults standardUserDefaults] integerForKey:@"LatestCrashReportYear"];
    NSString * latestLog = [self latestCrashLogPathPreviouslySeen:NO];
    if( latestLog && !areCrashLogsTooOld )
        [NSApp runModalForWindow: o_crashrep_win];
    [o_pool release];
}

- (IBAction)crashReporterAction:(id)sender
{
    if( sender == o_crashrep_send_btn )
        [self sendCrashLog:[NSString stringWithContentsOfFile: [self latestCrashLogPath] encoding: NSUTF8StringEncoding error: NULL] withUserComment: [o_crashrep_fld string]];

    [NSApp stopModal];
    [o_crashrep_win orderOut: sender];
}

- (IBAction)openCrashLog:(id)sender
{
    NSString * latestLog = [self latestCrashLogPath];
    if( latestLog )
    {
        [[NSWorkspace sharedWorkspace] openFile: latestLog withApplication: @"Console"];
    }
    else
    {
        NSBeginInformationalAlertSheet(_NS("No CrashLog found"), _NS("Continue"), nil, nil, o_msgs_panel, self, NULL, NULL, nil, _NS("Couldn't find any trace of a previous crash.") );
    }
}

#pragma mark -
#pragma mark Remove old prefs

- (void)_removeOldPreferences
{
    static NSString * kVLCPreferencesVersion = @"VLCPreferencesVersion";
    static const int kCurrentPreferencesVersion = 1;
    int version = [[NSUserDefaults standardUserDefaults] integerForKey:kVLCPreferencesVersion];
    if( version >= kCurrentPreferencesVersion ) return;

    NSArray *libraries = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
        NSUserDomainMask, YES);
    if( !libraries || [libraries count] == 0) return;
    NSString * preferences = [[libraries objectAtIndex:0] stringByAppendingPathComponent:@"Preferences"];

    /* File not found, don't attempt anything */
    if(![[NSFileManager defaultManager] fileExistsAtPath:[preferences stringByAppendingPathComponent:@"VLC"]] &&
       ![[NSFileManager defaultManager] fileExistsAtPath:[preferences stringByAppendingPathComponent:@"org.videolan.vlc.plist"]] )
    {
        [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        return;
    }

    int res = NSRunInformationalAlertPanel(_NS("Remove old preferences?"),
                _NS("We just found an older version of VLC's preferences files."),
                _NS("Move To Trash and Relaunch VLC"), _NS("Ignore"), nil, nil);
    if( res != NSOKButton )
    {
        [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        return;
    }

    NSArray * ourPreferences = [NSArray arrayWithObjects:@"org.videolan.vlc.plist", @"VLC", nil];

    /* Move the file to trash so that user can find them later */
    [[NSWorkspace sharedWorkspace] performFileOperation:NSWorkspaceRecycleOperation source:preferences destination:nil files:ourPreferences tag:0];

    /* really reset the defaults from now on */
    [NSUserDefaults resetStandardUserDefaults];

    [[NSUserDefaults standardUserDefaults] setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
    [[NSUserDefaults standardUserDefaults] synchronize];

    /* Relaunch now */
    const char * path = [[[NSBundle mainBundle] executablePath] UTF8String];

    /* For some reason we need to fork(), not just execl(), which reports a ENOTSUP then. */
    if(fork() != 0)
    {
        exit(0);
        return;
    }
    execl(path, path, NULL);
}

#pragma mark -
#pragma mark Errors, warnings and messages

- (IBAction)viewErrorsAndWarnings:(id)sender
{
    [[[self coreDialogProvider] errorPanel] showPanel];
}

- (IBAction)showMessagesPanel:(id)sender
{
    [o_msgs_panel makeKeyAndOrderFront: sender];
}

- (IBAction)showInformationPanel:(id)sender
{
    if(! nib_info_loaded )
        nib_info_loaded = [NSBundle loadNibNamed:@"MediaInfo" owner: NSApp];

    [o_info initPanel];
}

- (void)windowDidBecomeKey:(NSNotification *)o_notification
{
    if( [o_notification object] == o_msgs_panel )
        [self updateMessageDisplay];
}

- (void)updateMessageDisplay
{
    if( [o_msgs_panel isVisible] && b_msg_arr_changed )
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

        b_msg_arr_changed = NO;
        [o_msg_lock unlock];
    }
}

- (void)libvlcMessageReceived: (NSNotification *)o_notification
{
    NSColor *o_white = [NSColor whiteColor];
    NSColor *o_red = [NSColor redColor];
    NSColor *o_yellow = [NSColor yellowColor];
    NSColor *o_gray = [NSColor grayColor];

    NSColor * pp_color[4] = { o_white, o_red, o_yellow, o_gray };
    static const char * ppsz_type[4] = { ": ", " error: ",
    " warning: ", " debug: " };

    NSString *o_msg;
    NSDictionary *o_attr;
    NSAttributedString *o_msg_color;

    int i_type = [[[o_notification userInfo] objectForKey: @"Type"] intValue];

    [o_msg_lock lock];

    if( [o_msg_arr count] + 2 > 600 )
    {
        [o_msg_arr removeObjectAtIndex: 0];
        [o_msg_arr removeObjectAtIndex: 1];
    }

    o_attr = [NSDictionary dictionaryWithObject: o_gray
                                         forKey: NSForegroundColorAttributeName];
    o_msg = [NSString stringWithFormat: @"%@%s",
             [[o_notification userInfo] objectForKey: @"Module"],
             ppsz_type[i_type]];
    o_msg_color = [[NSAttributedString alloc]
                   initWithString: o_msg attributes: o_attr];
    [o_msg_arr addObject: [o_msg_color autorelease]];

    o_attr = [NSDictionary dictionaryWithObject: pp_color[i_type]
                                         forKey: NSForegroundColorAttributeName];
    o_msg = [[[o_notification userInfo] objectForKey: @"Message"] stringByAppendingString: @"\n"];
    o_msg_color = [[NSAttributedString alloc]
                   initWithString: o_msg attributes: o_attr];
    [o_msg_arr addObject: [o_msg_color autorelease]];

    b_msg_arr_changed = YES;
    [o_msg_lock unlock];
}

- (IBAction)saveDebugLog:(id)sender
{
    NSOpenPanel * saveFolderPanel = [[NSSavePanel alloc] init];

    [saveFolderPanel setCanChooseDirectories: NO];
    [saveFolderPanel setCanChooseFiles: YES];
    [saveFolderPanel setCanSelectHiddenExtension: NO];
    [saveFolderPanel setCanCreateDirectories: YES];
    [saveFolderPanel setRequiredFileType: @"rtfd"];
    [saveFolderPanel beginSheetForDirectory:nil file: [NSString stringWithFormat: _NS("VLC Debug Log (%s).rtfd"), VLC_Version()] modalForWindow: o_msgs_panel modalDelegate:self didEndSelector:@selector(saveDebugLogAsRTF:returnCode:contextInfo:) contextInfo:nil];
}

- (void)saveDebugLogAsRTF: (NSSavePanel *)sheet returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    BOOL b_returned;
    if( returnCode == NSOKButton )
    {
        b_returned = [o_messages writeRTFDToFile: [sheet filename] atomically: YES];
        if(! b_returned )
            msg_Warn( p_intf, "Error while saving the debug log" );
    }
}

#pragma mark -
#pragma mark Playlist toggling

- (IBAction)togglePlaylist:(id)sender
{
    NSRect contentRect = [o_window contentRectForFrameRect:[o_window frame]];
    NSRect o_rect = [o_window contentRectForFrameRect:[o_window frame]];
    /*First, check if the playlist is visible*/
    if( contentRect.size.height <= 169. )
    {
        o_restore_rect = contentRect;
        b_restore_size = true;
        b_small_window = YES; /* we know we are small, make sure this is actually set (see case below) */

        /* make large */
        if( o_size_with_playlist.height > 169. )
            o_rect.size.height = o_size_with_playlist.height;
        else
            o_rect.size.height = 500.;

        if( o_size_with_playlist.width >= [o_window contentMinSize].width )
            o_rect.size.width = o_size_with_playlist.width;
        else
            o_rect.size.width = [o_window contentMinSize].width;

        o_rect.origin.x = contentRect.origin.x;
        o_rect.origin.y = contentRect.origin.y - o_rect.size.height +
            [o_window contentMinSize].height;

        o_rect = [o_window frameRectForContentRect:o_rect];

        NSRect screenRect = [[o_window screen] visibleFrame];
        if( !NSContainsRect( screenRect, o_rect ) ) {
            if( NSMaxX(o_rect) > NSMaxX(screenRect) )
                o_rect.origin.x = ( NSMaxX(screenRect) - o_rect.size.width );
            if( NSMinY(o_rect) < NSMinY(screenRect) )
                o_rect.origin.y = ( NSMinY(screenRect) );
        }

        [o_btn_playlist setState: YES];
    }
    else
    {
        NSSize curSize = o_rect.size;
        if( b_restore_size )
        {
            o_rect = o_restore_rect;
            if( o_rect.size.height < [o_window contentMinSize].height )
                o_rect.size.height = [o_window contentMinSize].height;
            if( o_rect.size.width < [o_window contentMinSize].width )
                o_rect.size.width = [o_window contentMinSize].width;
        }
        else
        {
            NSRect contentRect = [o_window contentRectForFrameRect:[o_window frame]];
            /* make small */
            o_rect.size.height = [o_window contentMinSize].height;
            o_rect.size.width = [o_window contentMinSize].width;
            o_rect.origin.x = contentRect.origin.x;
            /* Calculate the position of the lower right corner after resize */
            o_rect.origin.y = contentRect.origin.y +
                contentRect.size.height - [o_window contentMinSize].height;
        }

        [o_playlist_view setAutoresizesSubviews: NO];
        [o_playlist_view removeFromSuperview];
        [o_btn_playlist setState: NO];
        b_small_window = NO; /* we aren't small here just yet. we are doing an animated resize after this */
        o_rect = [o_window frameRectForContentRect:o_rect];
    }

    [o_window setFrame: o_rect display:YES animate: YES];
}

- (void)updateTogglePlaylistState
{
    if( [o_window contentRectForFrameRect:[o_window frame]].size.height <= 169. )
        [o_btn_playlist setState: NO];
    else
        [o_btn_playlist setState: YES];

    [[self playlist] outlineViewSelectionDidChange: NULL];
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{

    /* Not triggered on a window resize or maxification of the window. only by window mouse dragging resize */

   /*Stores the size the controller one resize, to be able to restore it when
     toggling the playlist*/
    o_size_with_playlist = proposedFrameSize;

    NSRect rect;
    rect.size = proposedFrameSize;
    if( [o_window contentRectForFrameRect:rect].size.height <= 169. )
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

- (void)windowDidMove:(NSNotification *)notif
{
    b_restore_size = false;
}

- (void)windowDidResize:(NSNotification *)notif
{
    if( [o_window contentRectForFrameRect:[o_window frame]].size.height > 169. && b_small_window )
    {
        /* If large and coming from small then show */
        [o_playlist_view setAutoresizesSubviews: YES];
        NSRect contentRect = [o_window contentRectForFrameRect:[o_window frame]];
        [o_playlist_view setFrame: NSMakeRect( 0, 0, contentRect.size.width, contentRect.size.height - [o_window contentMinSize].height )];
        [o_playlist_view setNeedsDisplay:YES];
        [[o_window contentView] addSubview: o_playlist_view];
        b_small_window = NO;
    }
    [self updateTogglePlaylistState];
}

#pragma mark -

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

/*****************************************************************************
 * VLCApplication interface
 * exclusively used to implement media key support on Al Apple keyboards
 *   b_justJumped is required as the keyboard send its events faster than
 *    the user can actually jump through his media
 *****************************************************************************/

@implementation VLCApplication

- (void)awakeFromNib
{
	b_active = b_mediaKeySupport = config_GetInt( VLCIntf, "macosx-mediakeys" );
    b_activeInBackground = config_GetInt( VLCIntf, "macosx-mediakeys-background" );
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(coreChangedMediaKeySupportSetting:) name: @"VLCMediaKeySupportSettingChanged" object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(appGotActiveOrInactive:) name: @"NSApplicationDidBecomeActiveNotification" object: nil];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(appGotActiveOrInactive:) name: @"NSApplicationWillResignActiveNotification" object: nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [super dealloc];
}

- (void)appGotActiveOrInactive: (NSNotification *)o_notification
{
    if(( [[o_notification name] isEqualToString: @"NSApplicationWillResignActiveNotification"] && !b_activeInBackground ) || !b_mediaKeySupport)
        b_active = NO;
    else
        b_active = YES;
}

- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification
{
    b_active = b_mediaKeySupport = config_GetInt( VLCIntf, "macosx-mediakeys" );
    b_activeInBackground = config_GetInt( VLCIntf, "macosx-mediakeys-background" );
}


- (void)sendEvent: (NSEvent*)event
{
    if( b_active )
	{
        if( [event type] == NSSystemDefined && [event subtype] == 8 )
        {
            int keyCode = (([event data1] & 0xFFFF0000) >> 16);
            int keyFlags = ([event data1] & 0x0000FFFF);
            int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
            int keyRepeat = (keyFlags & 0x1);

            if( keyCode == NX_KEYTYPE_PLAY && keyState == 0 )
                var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PLAY_PAUSE );

            if( keyCode == NX_KEYTYPE_FAST && !b_justJumped )
            {
                if( keyState == 0 && keyRepeat == 0 )
                {
                    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_NEXT );
                }
                else if( keyRepeat == 1 )
                {
                    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_SHORT );
                    b_justJumped = YES;
                    [self performSelector:@selector(resetJump)
                               withObject: NULL
                               afterDelay:0.25];
                }
            }

            if( keyCode == NX_KEYTYPE_REWIND && !b_justJumped )
            {
                if( keyState == 0 && keyRepeat == 0 )
                {
                    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_PREV );
                }
                else if( keyRepeat == 1 )
                {
                    var_SetInteger( VLCIntf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_SHORT );
                    b_justJumped = YES;
                    [self performSelector:@selector(resetJump)
                               withObject: NULL
                               afterDelay:0.25];
                }
            }
        }
    }
	[super sendEvent: event];
}

- (void)resetJump
{
    b_justJumped = NO;
}

@end
