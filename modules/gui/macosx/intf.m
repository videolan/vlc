/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
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
#include <vlc_url.h>
#include <vlc_modules.h>
#include <unistd.h> /* execl() */

#import "intf.h"
#import "MainMenu.h"
#import "fspanel.h"
#import "vout.h"
#import "prefs.h"
#import "playlist.h"
#import "playlistinfo.h"
#import "controls.h"
#import "open.h"
#import "wizard.h"
#import "bookmarks.h"
#import "coredialogs.h"
#import "embeddedwindow.h"
#import "AppleRemote.h"
#import "eyetv.h"
#import "simple_prefs.h"
#import "CoreInteraction.h"

#import <AddressBook/AddressBook.h>         /* for crashlog send mechanism */
#import <Sparkle/Sparkle.h>                 /* we're the update delegate */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run ( intf_thread_t *p_intf );

static void * ManageThread( void *user_data );

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

static void MsgCallback( msg_cb_data_t *, const msg_item_t * );

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
static void MsgCallback( msg_cb_data_t *data, const msg_item_t *item )
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

    o_open = [[VLCOpen alloc] init];
    //o_embedded_list = [[VLCEmbeddedList alloc] init];
    o_coredialogs = [[VLCCoreDialogProvider alloc] init];
    o_info = [[VLCInfo alloc] init];
    o_mainmenu = [[VLCMainMenu alloc] init];
    o_coreinteraction = [[VLCCoreInteraction alloc] init];

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
    playlist_t *p_playlist;
    vlc_value_t val;
    var_Create( p_intf, "intf-change", VLC_VAR_BOOL );

    [o_volumeslider setEnabled: YES];
    [self manageVolumeSlider];
    [o_window setDelegate: self];
    [o_window setExcludedFromWindowsMenu: YES];
    [o_msgs_panel setExcludedFromWindowsMenu: YES];
    [o_msgs_panel setDelegate: self];

    // Set that here as IB seems to be buggy
    [o_window setContentMinSize:NSMakeSize(500., 200.)];

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

    /* take care of tint changes during runtime */
    o_img_play = [NSImage imageNamed: @"play"];
    o_img_pause = [NSImage imageNamed: @"pause"];

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
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    if( !p_intf ) return;

    /* init media key support */
    o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];
    b_mediaKeySupport = config_GetInt( VLCIntf, "macosx-mediakeys" );
    [o_mediaKeyController startWatchingMediaKeys];
    [o_mediaKeyController setShouldInterceptMediaKeyEvents:b_mediaKeySupport];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(coreChangedMediaKeySupportSetting:) name: @"VLCMediaKeySupportSettingChanged" object: nil];
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
                                                             [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
                                                             nil]];

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
    [o_mainmenu release];

    if( o_sprefs )
        [o_sprefs release];

    if( o_prefs )
        [o_prefs release];

    [o_open release];

    if( o_info )
    {
        [o_info stopTimers];
        [o_info release];
    }

    if( o_wizard )
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
    //FIXME: this should be moved to VLCMainMenu
    [o_mainmenu releaseRepresentedObjects:[NSApp mainMenu]];

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
    [[[o_coreinteraction voutView] window] orderOut:self];
}

#pragma mark -
#pragma mark Media Key support

-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event
{
    if( b_mediaKeySupport )
       {
        assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

        int keyCode = (([event data1] & 0xFFFF0000) >> 16);
        int keyFlags = ([event data1] & 0x0000FFFF);
        int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
        int keyRepeat = (keyFlags & 0x1);

        if( keyCode == NX_KEYTYPE_PLAY && keyState == 0 )
            var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_PLAY_PAUSE );

        if( keyCode == NX_KEYTYPE_FAST && !b_mediakeyJustJumped )
        {
            if( keyState == 0 && keyRepeat == 0 )
                var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_NEXT );
            else if( keyRepeat == 1 )
            {
                var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_SHORT );
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }

        if( keyCode == NX_KEYTYPE_REWIND && !b_mediakeyJustJumped )
        {
            if( keyState == 0 && keyRepeat == 0 )
                var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_PREV );
            else if( keyRepeat == 1 )
            {
                var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_SHORT );
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }
    }
}

#pragma mark -
#pragma mark Other notification

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
    char *psz_uri = make_URI([o_filename UTF8String], "file" );
    if( !psz_uri )
        return( FALSE );

    NSDictionary *o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];

    free( psz_uri );

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
        case k2009RemoteButtonFullscreen:
            [o_controls toogleFullscreen:self];
            break;
        case k2009RemoteButtonPlay:
            [o_controls play:self];
            break;
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
        o_str = [[[NSString alloc] initWithFormat:@"%s", psz] autorelease];

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

- (unsigned int)VLCModifiersToCocoa:(NSString *)theString
{
    unsigned int new = 0;

    if([theString rangeOfString:@"Command"].location != NSNotFound)
        new |= NSCommandKeyMask;
    if([theString rangeOfString:@"Alt"].location != NSNotFound)
        new |= NSAlternateKeyMask;
    if([theString rangeOfString:@"Shift"].location != NSNotFound)
        new |= NSShiftKeyMask;
    if([theString rangeOfString:@"Ctrl"].location != NSNotFound)
        new |= NSControlKeyMask;
    return new;
}

- (NSString *)VLCKeyToString:(NSString *)theString
{
    if (![theString isEqualToString:@""]) {
        theString = [theString stringByReplacingOccurrencesOfString:@"Command" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Alt" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Shift" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"Ctrl" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"+" withString:@""];
        theString = [theString stringByReplacingOccurrencesOfString:@"-" withString:@""];
    }
    return theString;
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
    NSMutableString *tempString = [[[NSMutableString alloc] init] autorelease];
    NSMutableString *tempStringPlus = [[[NSMutableString alloc] init] autorelease];

    val.i_int = 0;
    p_hotkeys = p_intf->p_libvlc->p_hotkeys;

    i_pressed_modifiers = [o_event modifierFlags];

    if( i_pressed_modifiers & NSShiftKeyMask ) {
        val.i_int |= KEY_MODIFIER_SHIFT;
        [tempString appendString:@"Shift-"];
        [tempStringPlus appendString:@"Shift+"];
    }
    if( i_pressed_modifiers & NSControlKeyMask ) {
        val.i_int |= KEY_MODIFIER_CTRL;
        [tempString appendString:@"Ctrl-"];
        [tempStringPlus appendString:@"Ctrl+"];
    }
    if( i_pressed_modifiers & NSAlternateKeyMask ) {
        val.i_int |= KEY_MODIFIER_ALT;
        [tempString appendString:@"Alt-"];
        [tempStringPlus appendString:@"Alt+"];
    }
    if( i_pressed_modifiers & NSCommandKeyMask ) {
        val.i_int |= KEY_MODIFIER_COMMAND;
        [tempString appendString:@"Command-"];
        [tempStringPlus appendString:@"Command+"];
    }

    [tempString appendString:[[o_event charactersIgnoringModifiers] lowercaseString]];
    [tempStringPlus appendString:[[o_event charactersIgnoringModifiers] lowercaseString]];

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

    if( [o_usedHotkeys indexOfObject: tempString] != NSNotFound || [o_usedHotkeys indexOfObject: tempStringPlus] != NSNotFound )
    {
        var_SetInteger( p_intf->p_libvlc, "key-pressed", val.i_int );
        return YES;
    }

    return NO;
}

- (void)updateCurrentlyUsedHotkeys
{
    NSMutableArray *o_tempArray = [[NSMutableArray alloc] init];
    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert( p_main );
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    for (size_t i = 0; i < confsize; i++)
    {
        module_config_t *p_item = p_config + i;

        if( CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
           && !strncmp( p_item->psz_name , "key-", 4 )
           && !EMPTY_STR( p_item->psz_text ) )
        {
            if (p_item->value.psz)
                [o_tempArray addObject: [NSString stringWithUTF8String:p_item->value.psz]];
        }
    }
    module_config_free (p_config);
    module_release (p_main);
    o_usedHotkeys = [[NSArray alloc] initWithArray: o_usedHotkeys copyItems: YES];
}


#pragma mark -
#pragma mark Other objects getters

- (id)mainMenu
{
    return o_mainmenu;
}

- (id)controls
{
    if( o_controls )
        return o_controls;

    return nil;
}

- (id)bookmarks
{
    if (!o_bookmarks )
        o_bookmarks = [[VLCBookmarks alloc] init];

    if( !nib_bookmarks_loaded )
        nib_bookmarks_loaded = [NSBundle loadNibNamed:@"Bookmarks" owner: NSApp];

    return o_bookmarks;
}

- (id)open
{
    if (!o_open)
        return nil;

    if (!nib_open_loaded)
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];

    return o_open;
}

- (id)simplePreferences
{
    if (!o_sprefs)
        o_sprefs = [[VLCSimplePrefs alloc] init];

    if (!nib_prefs_loaded)
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_sprefs;
}

- (id)preferences
{
    if( !o_prefs )
        o_prefs = [[VLCPrefs alloc] init];

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
    if(! nib_info_loaded )
        nib_info_loaded = [NSBundle loadNibNamed:@"MediaInfo" owner: NSApp];

    if( o_info )
        return o_info;

    return nil;
}

- (id)wizard
{
    if( !o_wizard )
        o_wizard = [[VLCWizard alloc] init];

    if( !nib_wizard_loaded )
    {
        nib_wizard_loaded = [NSBundle loadNibNamed:@"Wizard" owner: NSApp];
        [o_wizard initStrings];
    }
    return o_wizard;
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

    i_volume = aout_VolumeGet( p_playlist );

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
        [o_mainmenu setupMenus]; /* Make sure input menu is up to date */

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
        if( [[o_coreinteraction voutView] isFullscreen] && config_GetInt( VLCIntf, "macosx-fspanel" ) )
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

            [[o_coreinteraction voutView] updateTitle];

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
        [o_mainmenu setSubmenusEnabled: FALSE];
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
        [[self mainMenu] setPause];
        [o_btn_play setImage: o_img_pause];
        [o_btn_play setAlternateImage: o_img_pause_pressed];
        [o_btn_play setToolTip: _NS("Pause")];
    }
    else
    {
        [[[self controls] fspanel] setPlay];
        [[self mainMenu] setPlay];
        [o_btn_play setImage: o_img_play];
        [o_btn_play setAlternateImage: o_img_play_pressed];
        [o_btn_play setToolTip: _NS("Play")];
    }
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

- (IBAction)showController:(id)sender
{
    //FIXME: temporary hack until we have VLCMainWindow
    [o_window makeKeyAndOrderFront:sender];
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
- (IBAction)showMessagesPanel:(id)sender
{
    [o_msgs_panel makeKeyAndOrderFront: sender];
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
    [saveFolderPanel setAllowedFileTypes: [NSArray arrayWithObject:@"rtfd"]];
    [saveFolderPanel beginSheetForDirectory:nil file: [NSString stringWithFormat: _NS("VLC Debug Log (%s).rtfd"), VERSION_MESSAGE] modalForWindow: o_msgs_panel modalDelegate:self didEndSelector:@selector(saveDebugLogAsRTF:returnCode:contextInfo:) contextInfo:nil];
}

- (void)saveDebugLogAsRTF: (NSSavePanel *)sheet returnCode: (int)returnCode contextInfo: (void *)contextInfo
{
    BOOL b_returned;
    if( returnCode == NSOKButton )
    {
        b_returned = [o_messages writeRTFDToFile: [[sheet URL] path] atomically: YES];
        if(! b_returned )
            msg_Warn( p_intf, "Error while saving the debug log" );
    }
}

#pragma mark -
#pragma mark Playlist toggling

- (IBAction)togglePlaylist:(id)sender
{
    NSLog( @"needs to be re-implemented" );
}

- (void)updateTogglePlaylistState
{
    [[self playlist] outlineViewSelectionDidChange: NULL];
}

#pragma mark -

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
- (void)resetMediaKeyJump
{
    b_mediakeyJustJumped = NO;
}
- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification
{
    b_mediaKeySupport = config_GetInt( VLCIntf, "macosx-mediakeys" );
    [o_mediaKeyController setShouldInterceptMediaKeyEvents:b_mediaKeySupport];
}

@end

/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@implementation VLCApplication
// when user selects the quit menu from dock it sends a terminate:
// but we need to send a stop: to properly exits libvlc.
// However, we are not able to change the action-method sent by this standard menu item.
// thus we override terminat: to send a stop:
// see [af97f24d528acab89969d6541d83f17ce1ecd580] that introduced the removal of setjmp() and longjmp()
- (void)terminate:(id)sender
{
    [self activateIgnoringOtherApps:YES];
    [self stop:sender];
}

@end
