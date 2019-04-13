/*****************************************************************************
 * VLCMainWindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCMainWindow.h"

#import <math.h>

#import <vlc_url.h>
#import <vlc_strings.h>
#import <vlc_services_discovery.h>
#import <vlc_actions.h>
#import <vlc_plugin.h>
#import <vlc_modules.h>

#import "main/VLCMain.h"
#import "main/CompatibilityFixes.h"
#import "menus/VLCMainMenu.h"
#import "panels/VLCAudioEffectsWindowController.h"
#import "windows/VLCOpenWindowController.h"
#import "windows/mainwindow/VLCSourceListItem.h"
#import "windows/mainwindow/VLCSourceListTableCellView.h"
#import "windows/mainwindow/VLCMainWindowControlsBar.h"
#import "windows/video/VLCDetachedVideoWindow.h"
#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCFSPanelController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

@interface VLCMainWindow() <NSOutlineViewDataSource, NSOutlineViewDelegate, NSWindowDelegate, NSAnimationDelegate, NSSplitViewDelegate>
{
    BOOL videoPlaybackEnabled;
    BOOL dropzoneActive;
    BOOL splitViewRemoved;
    BOOL minimizedView;

    BOOL b_video_playback_enabled;
    BOOL b_dropzone_active;
    BOOL b_splitview_removed;
    BOOL b_minimized_view;

    CGFloat f_lastSplitViewHeight;
    CGFloat f_lastLeftSplitViewWidth;

    NSRect frameBeforePlayback;
    NSArray *_usedHotkeys;
}
- (void)makeSplitViewVisible;
- (void)makeSplitViewHidden;
@end

static const float f_min_window_height = 307.;

@implementation VLCMainWindow

#pragma mark -
#pragma mark Initialization

- (BOOL)isEvent:(NSEvent *)anEvent forKey:(const char *)keyString
{
    char *key = config_GetPsz(keyString);
    unsigned int keyModifiers = VLCModifiersToCocoa(key);
    NSString *vlcKeyString = VLCKeyToString(key);
    FREENULL(key);

    NSString *characters = [anEvent charactersIgnoringModifiers];
    if ([characters length] > 0) {
        return [[characters lowercaseString] isEqualToString: vlcKeyString] &&
        (keyModifiers & NSShiftKeyMask)     == ([anEvent modifierFlags] & NSShiftKeyMask) &&
        (keyModifiers & NSControlKeyMask)   == ([anEvent modifierFlags] & NSControlKeyMask) &&
        (keyModifiers & NSAlternateKeyMask) == ([anEvent modifierFlags] & NSAlternateKeyMask) &&
        (keyModifiers & NSCommandKeyMask)   == ([anEvent modifierFlags] & NSCommandKeyMask);
    }
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)anEvent
{
    BOOL b_force = NO;
    // these are key events which should be handled by vlc core, but are attached to a main menu item
    if (![self isEvent: anEvent forKey: "key-vol-up"] &&
        ![self isEvent: anEvent forKey: "key-vol-down"] &&
        ![self isEvent: anEvent forKey: "key-vol-mute"] &&
        ![self isEvent: anEvent forKey: "key-prev"] &&
        ![self isEvent: anEvent forKey: "key-next"] &&
        ![self isEvent: anEvent forKey: "key-jump+short"] &&
        ![self isEvent: anEvent forKey: "key-jump-short"]) {
        /* We indeed want to prioritize some Cocoa key equivalent against libvlc,
         so we perform the menu equivalent now. */
        if ([[NSApp mainMenu] performKeyEquivalent:anEvent])
            return TRUE;
    } else {
        b_force = YES;
    }

    return [self hasDefinedShortcutKey:anEvent force:b_force] ||
           [self keyEvent:anEvent];
}

- (BOOL)keyEvent:(NSEvent *)o_event
{
    BOOL eventHandled = NO;
    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        unichar key = [characters characterAtIndex: 0];

        if (key) {
            VLCPlayerController *playerController = [[[VLCMain sharedInstance] playlistController] playerController];
            vout_thread_t *p_vout = [playerController mainVideoOutputThread];
            if (p_vout != NULL) {
                /* Escape */
                if (key == (unichar) 0x1b) {
                    if (var_GetBool(p_vout, "fullscreen")) {
                        [playerController toggleFullscreen];
                        eventHandled = YES;
                    }
                }
                vout_Release(p_vout);
            }
        }
    }
    return eventHandled;
}

- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event force:(BOOL)b_force
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return NO;

    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;

    val.i_int = 0;
    i_pressed_modifiers = [o_event modifierFlags];

    if (i_pressed_modifiers & NSControlKeyMask)
        val.i_int |= KEY_MODIFIER_CTRL;

    if (i_pressed_modifiers & NSAlternateKeyMask)
        val.i_int |= KEY_MODIFIER_ALT;

    if (i_pressed_modifiers & NSShiftKeyMask)
        val.i_int |= KEY_MODIFIER_SHIFT;

    if (i_pressed_modifiers & NSCommandKeyMask)
        val.i_int |= KEY_MODIFIER_COMMAND;

    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        key = [[characters lowercaseString] characterAtIndex: 0];

        /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
        if (key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask) {
            [[[[VLCMain sharedInstance] playlistController] playerController] toggleFullscreen];
            return YES;
        }

        if (!b_force) {
            switch(key) {
                case NSDeleteCharacter:
                case NSDeleteFunctionKey:
                case NSDeleteCharFunctionKey:
                case NSBackspaceCharacter:
                case NSUpArrowFunctionKey:
                case NSDownArrowFunctionKey:
                case NSEnterCharacter:
                case NSCarriageReturnCharacter:
                    return NO;
            }
        }

        val.i_int |= CocoaKeyToVLC(key);

        BOOL b_found_key = NO;
        NSUInteger numberOfUsedHotkeys = [_usedHotkeys count];
        for (NSUInteger i = 0; i < numberOfUsedHotkeys; i++) {
            const char *str = [[_usedHotkeys objectAtIndex:i] UTF8String];
            unsigned int i_keyModifiers = VLCModifiersToCocoa((char *)str);

            if ([[characters lowercaseString] isEqualToString:VLCKeyToString((char *)str)] &&
                (i_keyModifiers & NSShiftKeyMask)     == (i_pressed_modifiers & NSShiftKeyMask) &&
                (i_keyModifiers & NSControlKeyMask)   == (i_pressed_modifiers & NSControlKeyMask) &&
                (i_keyModifiers & NSAlternateKeyMask) == (i_pressed_modifiers & NSAlternateKeyMask) &&
                (i_keyModifiers & NSCommandKeyMask)   == (i_pressed_modifiers & NSCommandKeyMask)) {
                b_found_key = YES;
                break;
            }
        }

        if (b_found_key) {
            var_SetInteger(vlc_object_instance(p_intf), "key-pressed", val.i_int);
            return YES;
        }
    }

    return NO;
}

- (void)updateCurrentlyUsedHotkeys
{
    NSMutableArray *mutArray = [[NSMutableArray alloc] init];
    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert(p_main);
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    for (size_t i = 0; i < confsize; i++) {
        module_config_t *p_item = p_config + i;

        if (CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
            && !strncmp(p_item->psz_name , "key-", 4)
            && !EMPTY_STR(p_item->psz_text)) {
            if (p_item->value.psz)
                [mutArray addObject:toNSStr(p_item->value.psz)];
        }
    }
    module_config_free (p_config);

    _usedHotkeys = [[NSArray alloc] initWithArray:mutArray copyItems:YES];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    /*
     * General setup
     */

    BOOL splitViewShouldBeHidden = NO;

    [self setDelegate:self];
    [self setRestorable:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setAcceptsMouseMovedEvents:YES];
    [self setFrameAutosaveName:@"mainwindow"];

    _nativeFullscreenMode = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");
    b_dropzone_active = YES;

    /*
     * Set up translatable strings for the UI elements
     */

    // Window title
    [self setTitle:_NS("VLC media player")];

    // Search Field
    [_searchField setToolTip:_NS("Search in Playlist")];
    [_searchField.cell setPlaceholderString:_NS("Search")];
    _searchField.accessibilityLabel = _NS("Search the playlist. Results will be selected in the table.");

    // Dropzone
    [_dropzoneLabel setStringValue:_NS("Drop media here")];
    [_dropzoneImageView setImage:imageFromRes(@"dropzone")];
    [_dropzoneButton setTitle:_NS("Open media...")];
    _dropzoneButton.accessibilityLabel = _NS("Open a dialog to select the media to play");

    // Podcast view
    [_podcastAddButton setTitle:_NS("Subscribe")];
    [_podcastRemoveButton setTitle:_NS("Unsubscribe")];

    // Podcast subscribe window
    [_podcastSubscribeTitle setStringValue:_NS("Subscribe to a podcast")];
    [_podcastSubscribeSubtitle setStringValue:_NS("Enter URL of the podcast to subscribe to:")];
    [_podcastSubscribeOkButton setTitle:_NS("Subscribe")];
    [_podcastSubscribeCancelButton setTitle:_NS("Cancel")];

    // Podcast unsubscribe window
    [_podcastUnsubscirbeTitle setStringValue:_NS("Unsubscribe from a podcast")];
    [_podcastUnsubscribeSubtitle setStringValue:_NS("Select the podcast you would like to unsubscribe from:")];
    [_podcastUnsubscribeOkButton setTitle:_NS("Unsubscribe")];
    [_podcastUnsubscribeCancelButton setTitle:_NS("Cancel")];

    /* interface builder action */
    CGFloat f_threshold_height = f_min_video_height + [self.controlsBar height];

    if ([[self contentView] frame].size.height < f_threshold_height)
        splitViewShouldBeHidden = YES;

    // Set that here as IB seems to be buggy
    [self setContentMinSize:NSMakeSize(604., f_min_window_height)];

    _fspanel = [[VLCFSPanelController alloc] init];
    [_fspanel showWindow:self];

    // Check for first run and show metadata network access question
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:@"VLCFirstRun"]) {
        [defaults setObject:[NSDate date] forKey:@"VLCFirstRun"];

        NSAlert *albumArtAlert = [[NSAlert alloc] init];
        [albumArtAlert setMessageText:_NS("Check for album art and metadata?")];
        [albumArtAlert setInformativeText:_NS("VLC can check online for album art and metadata to enrich your playback experience, e.g. by providing track information when playing Audio CDs. To provide this functionality, VLC will send information about your contents to trusted services in an anonymized form.")];
        [albumArtAlert addButtonWithTitle:_NS("Enable Metadata Retrieval")];
        [albumArtAlert addButtonWithTitle:_NS("No, Thanks")];

        NSInteger returnValue = [albumArtAlert runModal];
        config_PutInt("metadata-network-access", returnValue == NSAlertFirstButtonReturn);
    }

    [_playlistScrollView setBorderType:NSNoBorder];
    [_sidebarScrollView setBorderType:NSNoBorder];

    // Register for NSNotifications about Window and SplitView changes
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(someWindowWillClose:)
                          name:NSWindowWillCloseNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(someWindowWillMiniaturize:)
                          name:NSWindowWillMiniaturizeNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(applicationWillTerminate:)
                          name:NSApplicationWillTerminateNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(mainSplitViewDidResizeSubviews:)
                          name:NSSplitViewDidResizeSubviewsNotification
                        object:_splitView];

    if (splitViewShouldBeHidden) {
        [self hideSplitView:YES];
        f_lastSplitViewHeight = 300;
    }

    // Resize MainWindow to the screen size, if it exceeds the screens size
    NSSize windowSize = self.frame.size;
    NSSize screenSize = self.screen.frame.size;
    if (screenSize.width <= windowSize.width || screenSize.height <= windowSize.height) {
        self.nativeVideoSize = screenSize;
        [self resizeWindow];
    }

    /* restore split view */
    f_lastLeftSplitViewWidth = 200;
    [[[VLCMain sharedInstance] mainMenu] updateSidebarMenuItem: ![_splitView isSubviewCollapsed:_splitViewLeft]];

    [self updateCurrentlyUsedHotkeys];
}

#pragma mark -
#pragma mark appearance management

// Show split view and hide the video view
- (void)makeSplitViewVisible
{
    [self setContentMinSize: NSMakeSize(604., f_min_window_height)];

    NSRect old_frame = [self frame];
    CGFloat newHeight = [self minSize].height;
    if (old_frame.size.height < newHeight) {
        NSRect new_frame = old_frame;
        new_frame.origin.y = old_frame.origin.y + old_frame.size.height - newHeight;
        new_frame.size.height = newHeight;

        [[self animator] setFrame:new_frame display:YES animate:YES];
    }

    [self.videoView setHidden:YES];
    [_splitView setHidden:NO];
    if (self.nativeFullscreenMode && [self fullscreen]) {
        [self showControlsBar];
        [self.fspanel setNonActive];
    }

    [self makeFirstResponder:_playlistScrollView];
}

// Hides the split view and makes the vout view in foreground
- (void)makeSplitViewHidden
{
    [self setContentMinSize: NSMakeSize(604., f_min_video_height)];

    [_splitView setHidden:YES];
    [self.videoView setHidden:NO];
    if (self.nativeFullscreenMode && [self fullscreen]) {
        [self hideControlsBar];
        [self.fspanel setActive];
    }

    if ([[self.videoView subviews] count] > 0)
        [self makeFirstResponder: [[self.videoView subviews] firstObject]];
}

- (void)changePlaylistState:(VLCPlaylistStateEvent)event
{
    // Beware, this code is really ugly

    msg_Dbg(getIntf(), "toggle playlist from state: removed splitview %i, minimized view %i. Event %i", b_splitview_removed, b_minimized_view, event);
    if (![self isVisible] && event == psUserMenuEvent) {
        [self makeKeyAndOrderFront: nil];
        return;
    }

    BOOL b_activeVideo = [[VLCMain sharedInstance] activeVideoPlayback];
    BOOL b_restored = NO;

    // ignore alt if triggered through main menu shortcut
    BOOL b_have_alt_key = ([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) != 0;
    if (event == psUserMenuEvent)
        b_have_alt_key = NO;

    // eUserMenuEvent is now handled same as eUserEvent
    if(event == psUserMenuEvent)
        event = psUserEvent;

    if (b_dropzone_active && b_have_alt_key) {
        [self hideDropZone];
        return;
    }

    if (!(self.nativeFullscreenMode && self.fullscreen) && !b_splitview_removed && ((b_have_alt_key && b_activeVideo)
                                                                              || (self.nonembedded && event == psUserEvent)
                                                                              || (!b_activeVideo && event == psUserEvent)
                                                                              || (b_minimized_view && event == psVideoStartedOrStoppedEvent))) {
        // for starting playback, window is resized through resized events
        // for stopping playback, resize through reset to previous frame
        [self hideSplitView: event != psVideoStartedOrStoppedEvent];
        b_minimized_view = NO;
    } else {
        if (b_splitview_removed) {
            if (!self.nonembedded || (event == psUserEvent && self.nonembedded))
                [self showSplitView: event != psVideoStartedOrStoppedEvent];

            if (event != psUserEvent)
                b_minimized_view = YES;
            else
                b_minimized_view = NO;

            if (b_activeVideo)
                b_restored = YES;
        }

        if (!self.nonembedded) {
            if (([self.videoView isHidden] && b_activeVideo) || b_restored || (b_activeVideo && event != psUserEvent))
                [self makeSplitViewHidden];
            else
                [self makeSplitViewVisible];
        } else {
            [_splitView setHidden: NO];
            [_playlistScrollView setHidden: NO];
            [self.videoView setHidden: YES];
            [self showControlsBar];
        }
    }

    msg_Dbg(getIntf(), "toggle playlist to state: removed splitview %i, minimized view %i", b_splitview_removed, b_minimized_view);
}

- (IBAction)dropzoneButtonAction:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileGeneric];
}

#pragma mark -
#pragma mark overwritten default functionality

- (void)windowResizedOrMoved:(NSNotification *)notification
{
    [self saveFrameUsingName:[self frameAutosaveName]];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    [self saveFrameUsingName:[self frameAutosaveName]];
}


- (void)someWindowWillClose:(NSNotification *)notification
{
    id obj = [notification object];

    // hasActiveVideo is defined for VLCVideoWindowCommon and subclasses
    if ([obj respondsToSelector:@selector(hasActiveVideo)] && [obj hasActiveVideo]) {
        if ([[VLCMain sharedInstance] activeVideoPlayback])
            [[[VLCMain sharedInstance] playlistController] stopPlayback];
    }
}

- (void)someWindowWillMiniaturize:(NSNotification *)notification
{
    if (config_GetInt("macosx-pause-minimized")) {
        id obj = [notification object];

        if ([obj class] == [VLCVideoWindowCommon class] || [obj class] == [VLCDetachedVideoWindow class] || ([obj class] == [VLCMainWindow class] && !self.nonembedded)) {
            if ([[VLCMain sharedInstance] activeVideoPlayback])
                [[[VLCMain sharedInstance] playlistController] pausePlayback];
        }
    }
}

#pragma mark -
#pragma mark Update interface and respond to foreign events
- (void)showDropZone
{
    b_dropzone_active = YES;
    [_dropzoneView setHidden:NO];
    [_playlistScrollView setHidden:YES];
}

- (void)hideDropZone
{
    b_dropzone_active = NO;
    [_dropzoneView setHidden:YES];
    [_playlistScrollView setHidden:NO];
}

- (void)hideSplitView:(BOOL)resize
{
    if (resize) {
        NSRect winrect = [self frame];
        f_lastSplitViewHeight = [_splitView frame].size.height;
        winrect.size.height = winrect.size.height - f_lastSplitViewHeight;
        winrect.origin.y = winrect.origin.y + f_lastSplitViewHeight;
        [self setFrame:winrect display:YES animate:YES];
    }

    [self setContentMinSize: NSMakeSize(604., [self.controlsBar height])];
    [self setContentMaxSize: NSMakeSize(FLT_MAX, [self.controlsBar height])];

    b_splitview_removed = YES;
}

- (void)showSplitView:(BOOL)resize
{
    [self setContentMinSize:NSMakeSize(604., f_min_window_height)];
    [self setContentMaxSize: NSMakeSize(FLT_MAX, FLT_MAX)];

    if (resize) {
        NSRect winrect;
        winrect = [self frame];
        winrect.size.height = winrect.size.height + f_lastSplitViewHeight;
        winrect.origin.y = winrect.origin.y - f_lastSplitViewHeight;
        [self setFrame:winrect display:YES animate:YES];
    }

    b_splitview_removed = NO;
}

#pragma mark -
#pragma mark Video Output handling

- (void)videoplayWillBeStarted
{
    if (!self.fullscreen)
        frameBeforePlayback = [self frame];
}

- (void)setVideoplayEnabled
{
    BOOL b_videoPlayback = [[VLCMain sharedInstance] activeVideoPlayback];

    if (!b_videoPlayback) {
        if (!self.nonembedded && (!self.nativeFullscreenMode || (self.nativeFullscreenMode && !self.fullscreen)) && frameBeforePlayback.size.width > 0 && frameBeforePlayback.size.height > 0) {

            // only resize back to minimum view of this is still desired final state
            CGFloat f_threshold_height = f_min_video_height + [self.controlsBar height];
            if(frameBeforePlayback.size.height > f_threshold_height || b_minimized_view) {

                if ([[VLCMain sharedInstance] isTerminating])
                    [self setFrame:frameBeforePlayback display:YES];
                else
                    [[self animator] setFrame:frameBeforePlayback display:YES];

            }
        }

        frameBeforePlayback = NSMakeRect(0, 0, 0, 0);

        [self makeFirstResponder: _playlistScrollView];
        [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: NSNormalWindowLevel];

        // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
        [self setAlphaValue:1.0];
    }

    if (self.nativeFullscreenMode) {
        if ([self hasActiveVideo] && [self fullscreen] && b_videoPlayback) {
            [self hideControlsBar];
            [self.fspanel setActive];
        } else {
            [self showControlsBar];
            [self.fspanel setNonActive];
        }
    }
}

#pragma mark -
#pragma mark Fullscreen support

- (void)showFullscreenController
{
    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(hasActiveVideo)] && [currentWindow hasActiveVideo]) {
        if ([currentWindow respondsToSelector:@selector(fullscreen)] && [currentWindow fullscreen] && ![[currentWindow videoView] isHidden]) {

            if ([[VLCMain sharedInstance] activeVideoPlayback])
                [self.fspanel fadeIn];
        }
    }

}

#pragma mark -
#pragma mark split view delegate
- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0)
        return 300.;
    else
        return proposedMax;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex == 0)
        return 100.;
    else
        return proposedMin;
}

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
    return ([subview isEqual:_splitViewLeft]);
}

- (BOOL)splitView:(NSSplitView *)splitView shouldAdjustSizeOfSubview:(NSView *)subview
{
    return (![subview isEqual:_splitViewLeft]);
}

- (void)mainSplitViewDidResizeSubviews:(id)object
{
    f_lastLeftSplitViewWidth = [_splitViewLeft frame].size.width;
    [[[VLCMain sharedInstance] mainMenu] updateSidebarMenuItem: ![_splitView isSubviewCollapsed:_splitViewLeft]];
}

- (void)toggleLeftSubSplitView
{
    [_splitView adjustSubviews];
    if ([_splitView isSubviewCollapsed:_splitViewLeft])
        [_splitView setPosition:f_lastLeftSplitViewWidth ofDividerAtIndex:0];
    else
        [_splitView setPosition:[_splitView minPossiblePositionOfDividerAtIndex:0] ofDividerAtIndex:0];

    [[[VLCMain sharedInstance] mainMenu] updateSidebarMenuItem: ![_splitView isSubviewCollapsed:_splitViewLeft]];
}

@end
