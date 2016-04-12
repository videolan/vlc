/*****************************************************************************
 * VLCStatusBarIcon.m: Mac OS X module for vlc
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Goran Dokic <vlc at 8hz dot com>
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

#import "VLCStatusBarIcon.h"

#import "MainMenu.h"
#import "intf.h"

#import <vlc_common.h>
#import <vlc_playlist.h>
#import <vlc_input.h>
#import <CoreInteraction.h>
#import <StringUtility.h>

#pragma mark -
#pragma mark Defines

#define playPauseMenuItemTag 74747
#define stopMenuItemTag 83838
#define randomMenuItemTag 63636
#define dataUpdateTimerInterval 1.0
#define NSInitialToolTipDelayIn_ms 20
// #define showURLInToolTip 1

@interface VLCStatusBarIcon ()
{
    NSMenuItem *_vlcStatusBarMenuItem;

    IBOutlet NSMenuItem *showMainWindowItem;
    IBOutlet NSMenuItem *playPauseItem;
    IBOutlet NSMenuItem *stopItem;
    IBOutlet NSMenuItem *nextItem;
    IBOutlet NSMenuItem *prevItem;
    IBOutlet NSMenuItem *randItem;
    IBOutlet NSMenuItem *quitItem;

    NSString *_nameToDisplay;
    NSString *_timeToDisplay;
    NSString *_durationToDisplay;
    NSString *_urlToDisplay;
    NSImage *_menuImagePlay;
    NSImage *_menuImagePause;
    NSImage *_menuImageStop;
}
@end

#pragma mark -
#pragma mark Implementation

@implementation VLCStatusBarIcon

#pragma mark -
#pragma mark Init

- (void)dealloc
{
    // cleanup
    [self.dataRefreshUpdateTimer invalidate];
    self.dataRefreshUpdateTimer = nil;
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    [self enableMenuIcon];

    // Populate menu items with localized strings
    [showMainWindowItem setTitle:_NS("Show Main Window")];
    [playPauseItem setTitle:_NS("Play")];
    [stopItem setTitle:_NS("Stop")];
    [nextItem setTitle:_NS("Next")];
    [prevItem setTitle:_NS("Previous")];
    [randItem setTitle:_NS("Random")];
    [quitItem setTitle:_NS("Quit")];

    // add the top menu item for dynamic data
    _vlcStatusBarMenuItem = [[NSMenuItem alloc] initWithTitle:_NS("URL/Path Options") action:@selector(updateMenuItemContent:) keyEquivalent:@""];

    [_vlcStatusBarMenuItem setToolTip:_NS("Misc functions with media URL or Path")];
    [_vlcStatusBarMenuItem setTarget:self];

    [_vlcStatusBarIconMenu insertItem:_vlcStatusBarMenuItem atIndex:0];

    // Set our selves up as delegate, to receive menuNeedsUpdate messages, so
    // we can update our menu as needed/before it's drawn
    [_vlcStatusBarIconMenu setDelegate:self];

    // Disable custom menu item initially
    // needs to be done with validateMenuItem (see below)
    [_vlcStatusBarMenuItem setEnabled:NO];

    // Increase toolTip speed, improves status usability
    // Tweak delay above, with '#define NSInitialToolTipDelayIn_ms x'

    [[NSUserDefaults standardUserDefaults] setObject: [NSNumber numberWithInt: NSInitialToolTipDelayIn_ms] forKey: @"NSInitialToolTipDelay"];

    // init _urlToDisplay
    _urlToDisplay = nil;

    // Load the menu icons
    _menuImagePlay = [NSImage imageNamed:@"playIcon"];
    _menuImagePause = [NSImage imageNamed:@"pauseIcon"];

    _menuImageStop = [NSImage imageNamed:@"stopIcon"];
    NSMenuItem *menuItemToChange = [_vlcStatusBarIconMenu itemWithTag:stopMenuItemTag];
    [menuItemToChange setImage:_menuImageStop];

    // I'd rather not use a timer and only update when mouse comes near
    // status icon in bar. But one can't tell without evil sourcery :(
    // Tweak update frequency above (#define)

    [self setDataUpdateTimer:dataUpdateTimerInterval];
}


#pragma mark -
#pragma mark Various callback functions

//---
// Menu delegate/callback for cocoa - called before menu is opened/displayed
// fire off menu item updates (dynamic item 0, play/pause, random)
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    // update dynamic menu 'item 0' (follows data gathered by timer handler)
    [self updateDynamicMenuItemText];

    // update play/pause status in status bar menu
    [self updateMenuItemPlayPause];

    // update random status in status bar menu
    [self updateMenuItemRandom];
}


//---
// Make sure we can enable/disable menu items (in our case index 0)
// override class method. Called every time before menu is drawn.
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    // disable the URL/Path options menu if there is no meaningful data
    if ((menuItem.action == @selector(updateMenuItemContent:)) && (!_urlToDisplay)) {
        return NO;
    }
    return YES;
}


//---
// callback for tooltip update timer
//
- (void)dataRefreshTimeHandler:(NSTimer *)timer
{
    [self gatherDataToDisplay];
    [self updateToolTipText];
}


#pragma mark -
#pragma mark Various functions


//---
// enables menu icon/status item, sets proporties like image, attach menu
//
- (void)enableMenuIcon
{
    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    [_statusItem setHighlightMode:YES];
    [_statusItem setEnabled:YES];
    [_statusItem setTarget:self];

    NSImage *menuIcon;
    // set black/white icon (or color, like originally)
    menuIcon = [NSImage imageNamed:@"statusBarIcon"];
    // Make sure the b/w statusItem icon is inverted on dark/light mode
    menuIcon.template = YES;
    [_statusItem setImage:menuIcon];
    [_statusItem setLength:24];

    // Attach pull-down menu
    [_statusItem setMenu:_vlcStatusBarIconMenu];
}


//---
// Make sure data is fresh, before displaying
//
- (void)gatherDataToDisplay
{
    mtime_t pos;

    // get name of current item - clear first!
    _nameToDisplay = nil;
    _nameToDisplay = [[VLCCoreInteraction sharedInstance] nameOfCurrentPlaylistItem];

    // If status is 'stopped' there is no useful data
    // Otherwise could have used 'if (_nameToDisplay == nil)'

    if ([self vlcPlayingStatus] == PLAYLIST_STOPPED) {
        _urlToDisplay = nil;
    } else {
        input_thread_t * p_input;
        p_input = pl_CurrentInput(getIntf());
        if (p_input) {
            pos = var_GetInteger(p_input, "time") / CLOCK_FREQ;
            vlc_object_release(p_input); // must release or get segfault on quit
        }

        // update our time counter
        _timeToDisplay = [[VLCStringUtility sharedInstance] stringForTime:(long long) pos];

        // get the duration (if it's there)
        int duration = [[VLCCoreInteraction sharedInstance] durationOfCurrentPlaylistItem];
        _durationToDisplay = [[VLCStringUtility sharedInstance] stringForTime:(long long) duration];

        // update the playing item's URL/Path
        _urlToDisplay =  [[[VLCCoreInteraction sharedInstance] URLOfCurrentPlaylistItem] absoluteString];
    }
}


//---
// Call for periodic updates of tooltip text
//
- (void)updateToolTipText
{
    NSString *toolTipText = nil;

    // craft the multiline string, for the tooltip, depending on play status

    if ([self vlcPlayingStatus] == PLAYLIST_STOPPED) {
        // nothing playing
        toolTipText = _NS("VLC media player\nNothing playing");
    } else {
#ifdef showURLInToolTip
        toolTipText = [NSString stringWithFormat:_NS("VLC media player\nName: %@\nDuration: %@\nTime: %@\nURL/Path: %@"), _nameToDisplay, _durationToDisplay, _timeToDisplay, _urlToDisplay];
#else
        toolTipText = [NSString stringWithFormat:_NS("VLC media player\nName: %@\nDuration: %@\nTime: %@"), _nameToDisplay, _durationToDisplay, _timeToDisplay];
        // Causes warning, we need NS_FORMAT_ARGUMENT(1) for the localize function
#endif
    }

    [_statusItem setToolTip:toolTipText];
}



//---
// Call for updating of dynamic menu item
//
- (void)updateDynamicMenuItemText
{
    NSString *menuString = nil;

    // create string for dynamic menu bit (sync?)
    if ([self vlcPlayingStatus] == PLAYLIST_STOPPED) {
        // put back our disabled menu item text.
        menuString =  _NS("URL/Path Options");
    } else {
        if ([_urlToDisplay hasPrefix:@"file://"]) {
            // offer to show 'file://' in finder
            menuString = _NS("Select File In Finder");
        } else {
            // offer to copy URL to clipboard
            menuString = _NS("Copy URL to clipboard");
        }
    }

    [_vlcStatusBarMenuItem setTitle:menuString];
}


//---
// set timer for tooltips updates and flee
//
- (void)setDataUpdateTimer:(float)interval
{
    self.dataRefreshUpdateTimer = [NSTimer scheduledTimerWithTimeInterval:interval
                                                                   target:self
                                                                 selector:@selector(dataRefreshTimeHandler:)
                                                                 userInfo:nil
                                                                  repeats:YES];
}


//---
//
//
- (void)updateMenuItemRandom
{
    // get current random status
    bool b_value;
    playlist_t *p_playlist = pl_Get(getIntf());
    b_value = var_GetBool(p_playlist, "random");

    // get menuitem 'Random'
    NSMenuItem* menuItemToChange = [_vlcStatusBarIconMenu itemWithTag:randomMenuItemTag];
    if (b_value) {
        [menuItemToChange setState:NSOnState];
    } else {
        [menuItemToChange setState:NSOffState];
    }
}


//---
//
//
- (void)updateMenuItemPlayPause
{
    if ([self vlcPlayingStatus] == PLAYLIST_RUNNING) {
        [playPauseItem setTitle:_NS("Pause")];
        [playPauseItem setImage:_menuImagePause];
    } else {
        [playPauseItem setTitle:_NS("Play")];
        [playPauseItem setImage:_menuImagePlay];
    }
}


#pragma mark -
#pragma mark Utility functions

//---
// Returns VLC playlist status
// Check for: constants PLAYLIST_RUNNING, PLAYLIST_STOPPED, PLAYLIST_PAUSED.
- (int)vlcPlayingStatus
{
    int res;
    // get the playlist 'playing' status
    playlist_t *p_playlist = pl_Get(getIntf());

    PL_LOCK;
    res = playlist_Status( p_playlist );
    PL_UNLOCK;

    return res;
}


//---
// Returns true if playing, false in all other cases.
//
- (BOOL)isVLCPlaying
{
    bool vlcPlaying = false;

    // get the playlist 'playing' status
    playlist_t *p_playlist = pl_Get(getIntf());

    PL_LOCK;
    if (playlist_Status( p_playlist ) == PLAYLIST_RUNNING) {
        vlcPlaying = true;
    }
    PL_UNLOCK;

    return vlcPlaying;
}



#pragma mark -
#pragma mark Menu item Actions

//-- action for dynamic menu index 0

- (IBAction) updateMenuItemContent:(id)sender
{
    // Here we offer to copy the url to the clipboard or
    // select/show a local file in the finder..(useful imo ;-)

    if ([self vlcPlayingStatus] != PLAYLIST_STOPPED) {
        if ([_urlToDisplay hasPrefix:@"file://"]) {
            // show local file in finder
            NSString *path=[_urlToDisplay substringFromIndex:7];
            [[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:path];
        } else {
            // copy remote URL to clipboard
            NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard clearContents];
            [pasteboard setString:_urlToDisplay forType:NSStringPboardType];
        }
    }
}


//-- action for 'main window'

- (IBAction)restoreMainWindow:(id)sender
{
    // force our window to go to front (huzzah) and restore window
    [[VLCApplication sharedApplication] activateIgnoringOtherApps:YES];
    [[[VLCMain sharedInstance] mainWindow] makeKeyAndOrderFront:sender];
}


//-- action for 'toggle play/pause'

- (IBAction)statusBarIconTogglePlayPause:(id)sender
{
    [[VLCCoreInteraction sharedInstance] playOrPause];
}


//-- action for 'stop'

- (IBAction)statusBarIconStop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] stop];
}


//-- action for 'Next track'

- (IBAction)statusBarIconNext:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}


//-- action for 'previous track'

- (IBAction)statusBarIconPrevious:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}


//-- action to actually 'toggle VLC randomize playorder status'

- (IBAction)statusBarIconToggleRandom:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];
}


//-- action voor 'quit'

- (IBAction)quitAction:(id)sender
{
    // clean timer, quit
    [self.dataRefreshUpdateTimer invalidate];
    [[NSApplication sharedApplication] terminate:nil];
}

@end
