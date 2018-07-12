/*****************************************************************************
 * applescript.m: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
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
#import "VLCMain.h"
#import "applescript.h"
#import "VLCCoreInteraction.h"
#import "VLCPlaylist.h"
#import <vlc_url.h>

/*****************************************************************************
 * VLGetURLScriptCommand implementation
 *****************************************************************************/
@implementation VLGetURLScriptCommand

- (id)performDefaultImplementation {
    NSString *o_command = [[self commandDescription] commandName];
    NSString *o_urlString = [self directParameter];

    if ([o_command isEqualToString:@"GetURL"] || [o_command isEqualToString:@"OpenURL"]) {
        if (o_urlString) {

            NSDictionary *o_dic = [NSDictionary dictionaryWithObject:o_urlString forKey:@"ITEM_URL"];
            NSArray* item = [NSArray arrayWithObject:o_dic];

            [[[VLCMain sharedInstance] playlist] addPlaylistItems:item tryAsSubtitle:YES];
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

- (id)performDefaultImplementation {
    NSString *o_command = [[self commandDescription] commandName];
    NSString *o_parameter = [self directParameter];

    if ([o_command isEqualToString:@"play"])
        [[VLCCoreInteraction sharedInstance] playOrPause];
    else if ([o_command isEqualToString:@"stop"])
        [[VLCCoreInteraction sharedInstance] stop];
    else if ([o_command isEqualToString:@"previous"])
        [[VLCCoreInteraction sharedInstance] previous];
    else if ([o_command isEqualToString:@"next"])
        [[VLCCoreInteraction sharedInstance] next];
    else if ([o_command isEqualToString:@"fullscreen"])
        [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    else if ([o_command isEqualToString:@"mute"])
        [[VLCCoreInteraction sharedInstance] toggleMute];
    else if ([o_command isEqualToString:@"volumeUp"])
        [[VLCCoreInteraction sharedInstance] volumeUp];
    else if ([o_command isEqualToString:@"volumeDown"])
        [[VLCCoreInteraction sharedInstance] volumeDown];
    else if ([o_command isEqualToString:@"moveMenuFocusUp"])
        [[VLCCoreInteraction sharedInstance] moveMenuFocusUp];
    else if ([o_command isEqualToString:@"moveMenuFocusDown"])
        [[VLCCoreInteraction sharedInstance] moveMenuFocusDown];
    else if ([o_command isEqualToString:@"moveMenuFocusLeft"])
        [[VLCCoreInteraction sharedInstance] moveMenuFocusLeft];
    else if ([o_command isEqualToString:@"moveMenuFocusRight"])
        [[VLCCoreInteraction sharedInstance] moveMenuFocusRight];
    else if ([o_command isEqualToString:@"menuFocusActivate"])
        [[VLCCoreInteraction sharedInstance] menuFocusActivate];
    else if ([o_command isEqualToString:@"stepForward"]) {
        //default: forwardShort
        if (o_parameter) {
            int i_parameter = [o_parameter intValue];
            switch (i_parameter) {
                case 1:
                    [[VLCCoreInteraction sharedInstance] forwardExtraShort];
                    break;
                case 2:
                    [[VLCCoreInteraction sharedInstance] forwardShort];
                    break;
                case 3:
                    [[VLCCoreInteraction sharedInstance] forwardMedium];
                    break;
                case 4:
                    [[VLCCoreInteraction sharedInstance] forwardLong];
                    break;
                default:
                    [[VLCCoreInteraction sharedInstance] forwardShort];
                    break;
            }
        } else
            [[VLCCoreInteraction sharedInstance] forwardShort];
    } else if ([o_command isEqualToString:@"stepBackward"]) {
        //default: backwardShort
        if (o_parameter) {
            int i_parameter = [o_parameter intValue];
            switch (i_parameter) {
                case 1:
                    [[VLCCoreInteraction sharedInstance] backwardExtraShort];
                    break;
                case 2:
                    [[VLCCoreInteraction sharedInstance] backwardShort];
                    break;
                case 3:
                    [[VLCCoreInteraction sharedInstance] backwardMedium];
                    break;
                case 4:
                    [[VLCCoreInteraction sharedInstance] backwardLong];
                    break;
                default:
                    [[VLCCoreInteraction sharedInstance] backwardShort];
                    break;
            }
        } else
            [[VLCCoreInteraction sharedInstance] backwardShort];
    }
   return nil;
}

@end

/*****************************************************************************
 * Category that adds AppleScript support to NSApplication
 *****************************************************************************/
@implementation NSApplication(ScriptSupport)

- (BOOL)scriptFullscreenMode {
    vout_thread_t * p_vout = getVoutForActiveWindow();
    if (!p_vout)
        return NO;
    BOOL b_value = var_GetBool(p_vout, "fullscreen");
    vlc_object_release(p_vout);
    return b_value;
}

- (void)setScriptFullscreenMode:(BOOL)mode {
    vout_thread_t * p_vout = getVoutForActiveWindow();
    if (!p_vout)
        return;
    if (var_GetBool(p_vout, "fullscreen") == mode) {
        vlc_object_release(p_vout);
        return;
    }
    vlc_object_release(p_vout);
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (BOOL) muted {
    return [[VLCCoreInteraction sharedInstance] mute];
}

- (BOOL) playing {
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return NO;

    input_thread_t * p_input = pl_CurrentInput(p_intf);
    if (!p_input)
        return NO;

    input_state_e i_state = var_GetInteger(p_input, "state");
    vlc_object_release(p_input);

    return ((i_state == OPENING_S) || (i_state == PLAYING_S));
}

- (int) audioVolume {
    return ([[VLCCoreInteraction sharedInstance] volume]);
}

- (void) setAudioVolume:(int)i_audioVolume {
    [[VLCCoreInteraction sharedInstance] setVolume:(int)i_audioVolume];
}

- (int) audioDesync {
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    vlc_tick_t i_delay;

    if(!p_input)
        return -1;

    i_delay = var_GetInteger(p_input, "audio-delay");
    vlc_object_release(p_input);

    return MS_FROM_VLC_TICK( i_delay );
}

- (void) setAudioDesync:(int)i_audioDesync {
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    if(!p_input)
        return;

    var_SetInteger(p_input, "audio-delay", VLC_TICK_FROM_MS( i_audioDesync ));
    vlc_object_release(p_input);
}

- (int) currentTime {
    input_thread_t * p_input = pl_CurrentInput(getIntf());
    int i_currentTime = -1;

    if (!p_input)
        return i_currentTime;

    i_currentTime = var_GetInteger(p_input, "time");
    vlc_object_release(p_input);

    return (int)SEC_FROM_VLC_TICK(i_currentTime);
}

- (void) setCurrentTime:(int)i_currentTime {
    if (i_currentTime) {
        int64_t i64_value = (int64_t)i_currentTime;
        input_thread_t * p_input = pl_CurrentInput(getIntf());

        if (!p_input)
            return;

        input_SetTime(p_input, vlc_tick_from_sec(i64_value),
                      var_GetBool(p_input, "input-fast-seek"));
        vlc_object_release(p_input);
    }
}

- (NSInteger) durationOfCurrentItem {
    return [[VLCCoreInteraction sharedInstance] durationOfCurrentPlaylistItem];
}

- (NSString*) pathOfCurrentItem {
    return [[[VLCCoreInteraction sharedInstance] URLOfCurrentPlaylistItem] path];
}

- (NSString*) nameOfCurrentItem {
    return [[VLCCoreInteraction sharedInstance] nameOfCurrentPlaylistItem];
}

- (BOOL)playbackShowsMenu {
    input_thread_t *p_input_thread = pl_CurrentInput(getIntf());

    if (!p_input_thread)
        return NO;

    int i_current_title = (int)var_GetInteger(p_input_thread, "title");

    input_title_t **p_input_title;
    int count;

    /* fetch data */
    int coreret = input_Control(p_input_thread, INPUT_GET_FULL_TITLE_INFO,
                                &p_input_title, &count);
    vlc_object_release(p_input_thread);

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
