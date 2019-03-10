/*****************************************************************************
 * applescript.m: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#import "applescript.h"

#import <vlc_common.h>
#import <vlc_url.h>

#import "main/VLCMain.h"
#import "coreinteraction/VLCCoreInteraction.h"
#import "playlist/VLCPlaylistController.h"
#import "windows/VLCOpenInputMetadata.h"

/*****************************************************************************
 * VLGetURLScriptCommand implementation
 *****************************************************************************/
@implementation VLGetURLScriptCommand

- (id)performDefaultImplementation
{
    NSString *commandString = [[self commandDescription] commandName];
    NSString *parameterString = [self directParameter];

    if ([commandString isEqualToString:@"GetURL"] || [commandString isEqualToString:@"OpenURL"]) {
        if (parameterString) {
            VLCOpenInputMetadata *inputMetadata = [[VLCOpenInputMetadata alloc] init];
            inputMetadata.MRLString = parameterString;

            [[[VLCMain sharedInstance] playlistController] addPlaylistItems:@[inputMetadata]];
        }
    }
    return nil;
}

@end


/*****************************************************************************
 * VLControlScriptCommand implementation
 *****************************************************************************/
/*
 * This entire control command needs a better design. more object oriented.
 * Applescript developers would be very welcome (hartman)
 */
@implementation VLControlScriptCommand

- (id)performDefaultImplementation
{
    NSString *commandString = [[self commandDescription] commandName];
    NSString *parameterString = [self directParameter];
    VLCCoreInteraction *coreInteractionInstance = [VLCCoreInteraction sharedInstance];

    if ([commandString isEqualToString:@"play"])
        [coreInteractionInstance playOrPause];
    else if ([commandString isEqualToString:@"stop"])
        [coreInteractionInstance stop];
    else if ([commandString isEqualToString:@"previous"])
        [coreInteractionInstance previous];
    else if ([commandString isEqualToString:@"next"])
        [coreInteractionInstance next];
    else if ([commandString isEqualToString:@"fullscreen"])
        [coreInteractionInstance toggleFullscreen];
    else if ([commandString isEqualToString:@"mute"])
        [coreInteractionInstance toggleMute];
    else if ([commandString isEqualToString:@"volumeUp"])
        [coreInteractionInstance volumeUp];
    else if ([commandString isEqualToString:@"volumeDown"])
        [coreInteractionInstance volumeDown];
    else if ([commandString isEqualToString:@"moveMenuFocusUp"])
        [coreInteractionInstance moveMenuFocusUp];
    else if ([commandString isEqualToString:@"moveMenuFocusDown"])
        [coreInteractionInstance moveMenuFocusDown];
    else if ([commandString isEqualToString:@"moveMenuFocusLeft"])
        [coreInteractionInstance moveMenuFocusLeft];
    else if ([commandString isEqualToString:@"moveMenuFocusRight"])
        [coreInteractionInstance moveMenuFocusRight];
    else if ([commandString isEqualToString:@"menuFocusActivate"])
        [coreInteractionInstance menuFocusActivate];
    else if ([commandString isEqualToString:@"stepForward"]) {
        //default: forwardShort
        if (parameterString) {
            int parameterInt = [parameterString intValue];
            switch (parameterInt) {
                case 1:
                    [coreInteractionInstance forwardExtraShort];
                    break;
                case 2:
                    [coreInteractionInstance forwardShort];
                    break;
                case 3:
                    [coreInteractionInstance forwardMedium];
                    break;
                case 4:
                    [coreInteractionInstance forwardLong];
                    break;
                default:
                    [coreInteractionInstance forwardShort];
                    break;
            }
        } else
            [coreInteractionInstance forwardShort];
    } else if ([commandString isEqualToString:@"stepBackward"]) {
        //default: backwardShort
        if (parameterString) {
            int parameterInt = [parameterString intValue];
            switch (parameterInt) {
                case 1:
                    [coreInteractionInstance backwardExtraShort];
                    break;
                case 2:
                    [coreInteractionInstance backwardShort];
                    break;
                case 3:
                    [coreInteractionInstance backwardMedium];
                    break;
                case 4:
                    [coreInteractionInstance backwardLong];
                    break;
                default:
                    [coreInteractionInstance backwardShort];
                    break;
            }
        } else
            [coreInteractionInstance backwardShort];
    }
   return nil;
}

@end

/*****************************************************************************
 * Category that adds AppleScript support to NSApplication
 *****************************************************************************/
@implementation NSApplication(ScriptSupport)

- (BOOL)scriptFullscreenMode
{
    vout_thread_t * p_vout = getVoutForActiveWindow();
    if (!p_vout)
        return NO;
    BOOL b_value = var_GetBool(p_vout, "fullscreen");
    vout_Release(p_vout);
    return b_value;
}

- (void)setScriptFullscreenMode:(BOOL)mode
{
    vout_thread_t * p_vout = getVoutForActiveWindow();
    if (!p_vout)
        return;
    if (var_GetBool(p_vout, "fullscreen") == mode) {
        vout_Release(p_vout);
        return;
    }
    vout_Release(p_vout);
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (BOOL)muted
{
    return [[VLCCoreInteraction sharedInstance] mute];
}

- (BOOL)playing
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return NO;

    input_thread_t * p_input = pl_CurrentInput(p_intf);
    if (!p_input)
        return NO;

    input_state_e i_state = var_GetInteger(p_input, "state");
    input_Release(p_input);

    return ((i_state == OPENING_S) || (i_state == PLAYING_S));
}

- (int)audioVolume
{
    return ([[VLCCoreInteraction sharedInstance] volume]);
}

- (void)setAudioVolume:(int)volume
{
    [[VLCCoreInteraction sharedInstance] setVolume:volume];
}

- (long long)audioDesync
{
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    vlc_tick_t i_delay;

    if (!p_input)
        return -1;

    i_delay = var_GetInteger(p_input, "audio-delay");
    input_Release(p_input);

    return MS_FROM_VLC_TICK( i_delay );
}

- (void)setAudioDesync:(long long)audioDelay
{
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    if (!p_input)
        return;

    var_SetInteger(p_input, "audio-delay", VLC_TICK_FROM_MS( audioDelay ));
    input_Release(p_input);
}

- (int)currentTime
{
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    vlc_tick_t i_currentTime;

    if (!p_input)
        return -1;

    i_currentTime = var_GetInteger(p_input, "time");
    input_Release(p_input);

    return (int)SEC_FROM_VLC_TICK(i_currentTime);
}

- (void)setCurrentTime:(int)currenTime
{
    if (currenTime) {
        input_thread_t * p_input = pl_CurrentInput(getIntf());

        if (!p_input)
            return;

        input_SetTime(p_input, vlc_tick_from_sec(currenTime),
                      var_GetBool(p_input, "input-fast-seek"));
        input_Release(p_input);
    }
}

- (NSInteger)durationOfCurrentItem
{
    return [[VLCCoreInteraction sharedInstance] durationOfCurrentPlaylistItem];
}

- (NSString *)pathOfCurrentItem
{
    return [[[VLCCoreInteraction sharedInstance] URLOfCurrentPlaylistItem] path];
}

- (NSString *)nameOfCurrentItem
{
    return [[VLCCoreInteraction sharedInstance] nameOfCurrentPlaylistItem];
}

- (BOOL)playbackShowsMenu
{
    input_thread_t *p_input_thread = pl_CurrentInput(getIntf());

    if (!p_input_thread)
        return NO;

    int i_current_title = (int)var_GetInteger(p_input_thread, "title");

    input_title_t **p_input_title;
    int count;

    /* fetch data */
    int coreret = input_Control(p_input_thread, INPUT_GET_FULL_TITLE_INFO,
                                &p_input_title, &count);
    input_Release(p_input_thread);

    if (coreret != VLC_SUCCESS)
        return NO;

    BOOL ret = NO;

    if (count > 0 && i_current_title < count) {
        ret = p_input_title[i_current_title]->i_flags & INPUT_TITLE_MENU;
    }

    /* free array */
    for (int i = 0; i < count; i++) {
        vlc_input_title_Delete(p_input_title[i]);
    }
    free(p_input_title);

    return ret;
}

@end
