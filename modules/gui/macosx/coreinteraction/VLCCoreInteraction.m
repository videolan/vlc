/*****************************************************************************
 * CoreInteraction.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2019 Felix Paul Kühne
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "VLCCoreInteraction.h"

#import <vlc_url.h>
#import <vlc_modules.h>
#import <vlc_plugin.h>
#import <vlc_actions.h>

#import "main/VLCMain.h"
#import "coreinteraction/VLCClickerManager.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistModel.h"
#import "windows/VLCOpenWindowController.h"

static int BossCallback(vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[VLCCoreInteraction sharedInstance] pause];
            [[NSApplication sharedApplication] hide:nil];
        });

        return VLC_SUCCESS;
    }
}

@interface VLCCoreInteraction ()
{
    float f_currentPlaybackRate;

    float f_maxVolume;

    NSArray *_usedHotkeys;

    VLCClickerManager *_clickerManager;
    VLCPlaylistController *_playlistController;
    VLCPlayerController *_playerController;
}
@end

@implementation VLCCoreInteraction

#pragma mark - Initialization

+ (VLCCoreInteraction *)sharedInstance
{
    static VLCCoreInteraction *sharedInstance = nil;
    static dispatch_once_t pred;

    dispatch_once(&pred, ^{
        sharedInstance = [VLCCoreInteraction new];
    });

    return sharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        intf_thread_t *p_intf = getIntf();

        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                          selector:@selector(applicationWillTerminate:)
                              name:NSApplicationWillTerminateNotification
                            object:nil];

        _clickerManager = [[VLCClickerManager alloc] init];
        _playlistController = [[VLCMain sharedInstance] playlistController];
        _playerController = [_playlistController playerController];

        // FIXME: this variable will live on the current libvlc instance now. Depends on a future patch
        var_AddCallback(p_intf, "intf-boss", BossCallback, (__bridge void *)self);
    }
    return self;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    // Dealloc is never called because this is a singleton, so we should cleanup manually before termination
    // FIXME: this variable will live on the current libvlc instance now. Depends on a future patch
    var_DelCallback(getIntf(), "intf-boss", BossCallback, (__bridge void *)self);
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    _clickerManager = nil;
    _usedHotkeys = nil;
}

#pragma mark - Playback Controls

- (void)play
{
    [_playlistController startPlaylist];
}

- (void)playOrPause
{
    input_item_t *p_input_item = _playlistController.currentlyPlayingInputItem;

    if (p_input_item) {
        [_playerController togglePlayPause];
        input_item_Release(p_input_item);
    } else {
        if (_playlistController.playlistModel.numberOfPlaylistItems == 0)
            [[[VLCMain sharedInstance] open] openFileGeneric];
        else
            [_playlistController startPlaylist];
    }
}

- (void)pause
{
    [_playlistController pausePlayback];
}

- (void)stop
{
    [_playlistController stopPlayback];
}

- (void)faster
{
    [_playerController incrementPlaybackRate];
}

- (void)slower
{
    [_playerController decrementPlaybackRate];
}

- (void)normalSpeed
{
    _playerController.playbackRate = 1.;
}

- (void)toggleRecord
{
    [_playerController toggleRecord];
}

- (void)setPlaybackRate:(int)i_value
{
    double speed = pow(2, (double)i_value / 17);
    if (f_currentPlaybackRate != speed) {
        _playerController.playbackRate = speed;
    }
    f_currentPlaybackRate = speed;
}

- (int)playbackRate
{
    f_currentPlaybackRate = _playerController.playbackRate;

    double value = 17 * log(f_currentPlaybackRate) / log(2.);
    int returnValue = (int) ((value > 0) ? value + .5 : value - .5);

    if (returnValue < -34)
        returnValue = -34;
    else if (returnValue > 34)
        returnValue = 34;

    return returnValue;
}

- (int)previous
{
    return [_playlistController playPreviousItem];
}

- (int)next
{
    return [_playlistController playNextItem];
}

- (NSInteger)durationOfCurrentPlaylistItem
{
    return SEC_FROM_VLC_TICK(_playerController.durationOfCurrentMediaItem);
}

- (NSURL*)URLOfCurrentPlaylistItem
{
    return _playerController.URLOfCurrentMediaItem;
}

- (NSString*)nameOfCurrentPlaylistItem
{
    return _playerController.nameOfCurrentMediaItem;
}

- (void)forwardExtraShort
{
    [_playerController jumpForwardExtraShort];
}

- (void)backwardExtraShort
{
    [_playerController jumpBackwardExtraShort];
}

- (void)forwardShort
{
    [_playerController jumpForwardShort];
}

- (void)backwardShort
{
    [_playerController jumpBackwardShort];
}

- (void)forwardMedium
{
    [_playerController jumpForwardMedium];
}

- (void)backwardMedium
{
    [_playerController jumpBackwardMedium];
}

- (void)forwardLong
{
    [_playerController jumpForwardLong];
}

- (void)backwardLong
{
    [_playerController jumpBackwardLong];
}

- (void)shuffle
{
    BOOL on = NO;
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
        on = YES;
    } else {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }
    config_PutInt("random", on);

    vout_thread_t *p_vout = [_playerController mainVideoOutputThread];
    if (!p_vout) {
        return;
    }
    if (on) {
        [_playerController displayOSDMessage:_NS("Random On")];
    } else {
        [_playerController displayOSDMessage:_NS("Random Off")];
    }

    vout_Release(p_vout);
}

- (void)repeatAll
{
    _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
    [_playerController displayOSDMessage:_NS("Repeat All")];
}

- (void)repeatOne
{
    _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
    [_playerController displayOSDMessage:_NS("Repeat One")];
}

- (void)repeatOff
{
    _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    [_playerController displayOSDMessage:_NS("Repeat Off")];
}

- (void)jumpToTime:(vlc_tick_t)time
{
    [_playerController setTimePrecise:time];
}

- (void)volumeUp
{
    [_playerController incrementVolume];
}

- (void)volumeDown
{
    [_playerController decrementVolume];
}

- (void)toggleMute
{
    [_playerController toggleMute];
}

- (BOOL)mute
{
    return _playerController.mute;
}

- (int)volume
{
    return (int)lroundf(_playerController.volume * AOUT_VOLUME_DEFAULT);
}

- (void)setVolume: (int)i_value
{
    if (i_value >= self.maxVolume)
        i_value = self.maxVolume;

    _playerController.volume = i_value / (float)AOUT_VOLUME_DEFAULT;
}

- (float)maxVolume
{
    if (f_maxVolume == 0.) {
        f_maxVolume = (float)var_InheritInteger(getIntf(), "macosx-max-volume") / 100. * AOUT_VOLUME_DEFAULT;
    }

    return f_maxVolume;
}

- (void)addSubtitlesToCurrentInput:(NSArray *)paths
{
    NSUInteger count = [paths count];
    for (int i = 0; i < count ; i++) {
        NSURL *url = [NSURL fileURLWithPath:paths[i]];
        [_playerController addAssociatedMediaToCurrentFromURL:url
                                                   ofCategory:SPU_ES
                                             shallSelectTrack:YES
                                              shallDisplayOSD:YES
                                         shallVerifyExtension:NO];
    }
}

- (void)showPosition
{
    [_playerController displayPosition];
}

#pragma mark - video output stuff

- (void)setAspectRatioIsLocked:(BOOL)b_value
{
    config_PutInt("macosx-lock-aspect-ratio", b_value);
}

- (BOOL)aspectRatioIsLocked
{
    return config_GetInt("macosx-lock-aspect-ratio");
}

- (void)toggleFullscreen
{
    BOOL b_fs = !_playerController.fullscreen;
    _playerController.fullscreen = b_fs;

    // FIXME: check whether this is still needed
    [[[VLCMain sharedInstance] voutProvider] setFullscreen:b_fs forWindow:nil withAnimation:YES];
}

#pragma mark - menu navigation
- (void)menuFocusActivate
{
    [_playerController navigateInInteractiveContent:VLC_PLAYER_NAV_ACTIVATE];
}

- (void)moveMenuFocusLeft
{
    [_playerController navigateInInteractiveContent:VLC_PLAYER_NAV_LEFT];
}

- (void)moveMenuFocusRight
{
    [_playerController navigateInInteractiveContent:VLC_PLAYER_NAV_RIGHT];
}

- (void)moveMenuFocusUp
{
    [_playerController navigateInInteractiveContent:VLC_PLAYER_NAV_UP];
}

- (void)moveMenuFocusDown
{
    [_playerController navigateInInteractiveContent:VLC_PLAYER_NAV_DOWN];
}

#pragma mark -
#pragma mark Key Shortcuts

/*****************************************************************************
 * hasDefinedShortcutKey: Check to see if the key press is a defined VLC
 * shortcut key.  If it is, pass it off to VLC for handling and return YES,
 * otherwise ignore it and return NO (where it will get handled by Cocoa).
 *****************************************************************************/

- (BOOL)keyEvent:(NSEvent *)o_event
{
    BOOL eventHandled = NO;
    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        unichar key = [characters characterAtIndex: 0];

        if (key) {
            vout_thread_t *p_vout = [_playerController mainVideoOutputThread];
            if (p_vout != NULL) {
                /* Escape */
                if (key == (unichar) 0x1b) {
                    if (var_GetBool(p_vout, "fullscreen")) {
                        [self toggleFullscreen];
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
            [self toggleFullscreen];
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
            NSString *str = [_usedHotkeys objectAtIndex:i];
            unsigned int i_keyModifiers = VLCModifiersToCocoa(str);

            if ([[characters lowercaseString] isEqualToString: VLCKeyToString(str)] &&
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

@end
