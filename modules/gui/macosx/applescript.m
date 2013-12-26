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
#import "intf.h"
#import "applescript.h"
#import "CoreInteraction.h"
#import "playlist.h"
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
            BOOL b_autoplay = config_GetInt(VLCIntf, "macosx-autoplay");
            NSURL * o_url = [NSURL fileURLWithPath: o_urlString];
            if (o_url != nil)
                [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: o_url];

            input_thread_t * p_input = pl_CurrentInput(VLCIntf);
            BOOL b_returned = NO;

            if (p_input) {
                b_returned = input_AddSubtitle(p_input, [o_urlString UTF8String], true);
                vlc_object_release(p_input);
                if (!b_returned)
                    return nil;
            }

            NSDictionary *o_dic;
            NSArray *o_array;
            o_dic = [NSDictionary dictionaryWithObject:o_urlString forKey:@"ITEM_URL"];
            o_array = [NSArray arrayWithObject: o_dic];

            if (b_autoplay)
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue: NO];
            else
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue: YES];
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

    intf_thread_t * p_intf = VLCIntf;
    playlist_t * p_playlist = pl_Get(p_intf);

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
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NO;

    input_thread_t * p_input = pl_CurrentInput(p_intf);
    if (!p_input)
        return NO;

    input_state_e i_state = ERROR_S;
    input_Control(p_input, INPUT_GET_STATE, &i_state);
    vlc_object_release(p_input);

    return ((i_state == OPENING_S) || (i_state == PLAYING_S));
}

- (int) audioVolume {
    return ([[VLCCoreInteraction sharedInstance] volume]);
}

- (void) setAudioVolume:(int)i_audioVolume {
    [[VLCCoreInteraction sharedInstance] setVolume:(int)i_audioVolume];
}

- (int) currentTime {
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);
    int64_t i_currentTime = -1;

    if (!p_input)
        return i_currentTime;

    input_Control(p_input, INPUT_GET_TIME, &i_currentTime);
    vlc_object_release(p_input);

    return (int)(i_currentTime / 1000000);
}

- (void) setCurrentTime:(int)i_currentTime {
    if (i_currentTime) {
        int64_t i64_value = (int64_t)i_currentTime;
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);

        if (!p_input)
            return;

        input_Control(p_input, INPUT_SET_TIME, (int64_t)(i64_value * 1000000));
        vlc_object_release(p_input);
    }
}

- (int) durationOfCurrentItem {
    return [[VLCCoreInteraction sharedInstance] durationOfCurrentPlaylistItem];
}

- (NSString*) pathOfCurrentItem {
    return [[[VLCCoreInteraction sharedInstance] URLOfCurrentPlaylistItem] path];
}

- (NSString*) nameOfCurrentItem {
    return [[VLCCoreInteraction sharedInstance] nameOfCurrentPlaylistItem];
}

@end
