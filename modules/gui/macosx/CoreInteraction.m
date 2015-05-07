/*****************************************************************************
 * CoreInteraction.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2014 Felix Paul Kühne
 * $Id$
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

#import "CoreInteraction.h"
#import "intf.h"
#import "open.h"
#import "playlist.h"
#import <math.h>
#import <vlc_playlist.h>
#import <vlc_input.h>
#import <vlc_keys.h>
#import <vlc_vout.h>
#import <vlc_vout_osd.h>
#import <vlc/vlc.h>
#import <vlc_strings.h>
#import <vlc_url.h>
#import <vlc_modules.h>
#import <vlc_charset.h>


@implementation VLCCoreInteraction
static VLCCoreInteraction *_o_sharedInstance = nil;

+ (VLCCoreInteraction *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

#pragma mark - Initialization

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
        return _o_sharedInstance;
    } else
        _o_sharedInstance = [super init];

    return _o_sharedInstance;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [super dealloc];
}


#pragma mark - Playback Controls

- (void)playOrPause
{
    input_thread_t *p_input = pl_CurrentInput(VLCIntf);
    playlist_t *p_playlist = pl_Get(VLCIntf);

    if (p_input) {
        playlist_TogglePause(p_playlist);
        vlc_object_release(p_input);

    } else {
        PLRootType root = [[[[VLCMain sharedInstance] playlist] model] currentRootType];
        if ([[[VLCMain sharedInstance] playlist] isSelectionEmpty] && (root == ROOT_TYPE_PLAYLIST || root == ROOT_TYPE_MEDIALIBRARY))
            [[[VLCMain sharedInstance] open] openFileGeneric];
        else
            [[[VLCMain sharedInstance] playlist] playItem:nil];
    }
}

- (void)pause
{
    playlist_t *p_playlist = pl_Get(VLCIntf);

    playlist_Pause(p_playlist);
}

- (void)stop
{
    playlist_Stop(pl_Get(VLCIntf));
}

- (void)faster
{
    var_TriggerCallback(pl_Get(VLCIntf), "rate-faster");
}

- (void)slower
{
    var_TriggerCallback(pl_Get(VLCIntf), "rate-slower");
}

- (void)normalSpeed
{
    var_SetFloat(pl_Get(VLCIntf), "rate", 1.);
}

- (void)toggleRecord
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    input_thread_t * p_input;
    p_input = pl_CurrentInput(p_intf);
    if (p_input) {
        var_ToggleBool(p_input, "record");
        vlc_object_release(p_input);
    }
}

- (void)setPlaybackRate:(int)i_value
{
    playlist_t * p_playlist = pl_Get(VLCIntf);

    double speed = pow(2, (double)i_value / 17);
    int rate = INPUT_RATE_DEFAULT / speed;
    if (i_currentPlaybackRate != rate)
        var_SetFloat(p_playlist, "rate", (float)INPUT_RATE_DEFAULT / (float)rate);
    i_currentPlaybackRate = rate;
}

- (int)playbackRate
{
    float f_rate;

    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return 0;

    input_thread_t * p_input;
    p_input = pl_CurrentInput(p_intf);
    if (p_input) {
        f_rate = var_GetFloat(p_input, "rate");
        vlc_object_release(p_input);
    }
    else
    {
        playlist_t * p_playlist = pl_Get(VLCIntf);
        f_rate = var_GetFloat(p_playlist, "rate");
    }

    double value = 17 * log(f_rate) / log(2.);
    int returnValue = (int) ((value > 0) ? value + .5 : value - .5);

    if (returnValue < -34)
        returnValue = -34;
    else if (returnValue > 34)
        returnValue = 34;

    i_currentPlaybackRate = returnValue;
    return returnValue;
}

- (void)previous
{
    playlist_Prev(pl_Get(VLCIntf));
}

- (void)next
{
    playlist_Next(pl_Get(VLCIntf));
}

- (int)durationOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return 0;

    input_thread_t * p_input = pl_CurrentInput(p_intf);
    int64_t i_duration = -1;
    if (!p_input)
        return i_duration;

    input_Control(p_input, INPUT_GET_LENGTH, &i_duration);
    vlc_object_release(p_input);

    return (int)(i_duration / 1000000);
}

- (NSURL*)URLOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return nil;

    input_thread_t *p_input = pl_CurrentInput(p_intf);
    if (!p_input)
        return nil;

    input_item_t *p_item = input_GetItem(p_input);
    if (!p_item) {
        vlc_object_release(p_input);
        return nil;
    }

    char *psz_uri = input_item_GetURI(p_item);
    if (!psz_uri) {
        vlc_object_release(p_input);
        return nil;
    }

    NSURL *o_url;
    o_url = [NSURL URLWithString:[NSString stringWithUTF8String:psz_uri]];
    vlc_object_release(p_input);

    return o_url;
}

- (NSString*)nameOfCurrentPlaylistItem
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return nil;

    input_thread_t *p_input = pl_CurrentInput(p_intf);
    if (!p_input)
        return nil;

    input_item_t *p_item = input_GetItem(p_input);
    if (!p_item) {
        vlc_object_release(p_input);
        return nil;
    }

    char *psz_uri = input_item_GetURI(p_item);
    if (!psz_uri) {
        vlc_object_release(p_input);
        return nil;
    }

    NSString *o_name = @"";
    char *format = var_InheritString(VLCIntf, "input-title-format");
    if (format) {
        char *formated = str_format_meta(p_input, format);
        free(format);
        o_name = toNSStr(formated);
        free(formated);
    }

    NSURL * o_url = [NSURL URLWithString:[NSString stringWithUTF8String:psz_uri]];
    free(psz_uri);

    if ([o_name isEqualToString:@""]) {
        if ([o_url isFileURL])
            o_name = [[NSFileManager defaultManager] displayNameAtPath:[o_url path]];
        else
            o_name = [o_url absoluteString];
    }
    vlc_object_release(p_input);
    return o_name;
}

- (void)forward
{
    //LEGACY SUPPORT
    [self forwardShort];
}

- (void)backward
{
    //LEGACY SUPPORT
    [self backwardShort];
}

- (void)jumpWithValue:(char *)p_value forward:(BOOL)b_value
{
    input_thread_t *p_input = pl_CurrentInput(VLCIntf);
    if (!p_input)
        return;

    int i_interval = var_InheritInteger( p_input, p_value );
    if (i_interval > 0) {
        mtime_t val = CLOCK_FREQ * i_interval;
        if (!b_value)
            val = val * -1;
        var_SetTime( p_input, "time-offset", val );
    }
    vlc_object_release(p_input);
}

- (void)forwardExtraShort
{
    [self jumpWithValue:"extrashort-jump-size" forward:YES];
}

- (void)backwardExtraShort
{
    [self jumpWithValue:"extrashort-jump-size" forward:NO];
}

- (void)forwardShort
{
    [self jumpWithValue:"short-jump-size" forward:YES];
}

- (void)backwardShort
{
    [self jumpWithValue:"short-jump-size" forward:NO];
}

- (void)forwardMedium
{
    [self jumpWithValue:"medium-jump-size" forward:YES];
}

- (void)backwardMedium
{
    [self jumpWithValue:"medium-jump-size" forward:NO];
}

- (void)forwardLong
{
    [self jumpWithValue:"long-jump-size" forward:YES];
}

- (void)backwardLong
{
    [self jumpWithValue:"long-jump-size" forward:NO];
}

- (void)shuffle
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    vlc_value_t val;
    playlist_t * p_playlist = pl_Get(p_intf);
    vout_thread_t *p_vout = getVout();

    var_Get(p_playlist, "random", &val);
    val.b_bool = !val.b_bool;
    var_Set(p_playlist, "random", val);
    if (val.b_bool) {
        if (p_vout) {
            vout_OSDMessage(p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Random On"));
            vlc_object_release(p_vout);
        }
        config_PutInt(p_playlist, "random", 1);
    }
    else
    {
        if (p_vout) {
            vout_OSDMessage(p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Random Off"));
            vlc_object_release(p_vout);
        }
        config_PutInt(p_playlist, "random", 0);
    }
}

- (void)repeatAll
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_t * p_playlist = pl_Get(p_intf);

    var_SetBool(p_playlist, "repeat", NO);
    var_SetBool(p_playlist, "loop", YES);
    config_PutInt(p_playlist, "repeat", NO);
    config_PutInt(p_playlist, "loop", YES);

    vout_thread_t *p_vout = getVout();
    if (p_vout) {
        vout_OSDMessage(p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Repeat All"));
        vlc_object_release(p_vout);
    }
}

- (void)repeatOne
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_t * p_playlist = pl_Get(p_intf);

    var_SetBool(p_playlist, "repeat", YES);
    var_SetBool(p_playlist, "loop", NO);
    config_PutInt(p_playlist, "repeat", YES);
    config_PutInt(p_playlist, "loop", NO);

    vout_thread_t *p_vout = getVout();
    if (p_vout) {
        vout_OSDMessage(p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Repeat One"));
        vlc_object_release(p_vout);
    }
}

- (void)repeatOff
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_t * p_playlist = pl_Get(p_intf);

    var_SetBool(p_playlist, "repeat", NO);
    var_SetBool(p_playlist, "loop", NO);
    config_PutInt(p_playlist, "repeat", NO);
    config_PutInt(p_playlist, "loop", NO);

    vout_thread_t *p_vout = getVout();
    if (p_vout) {
        vout_OSDMessage(p_vout, SPU_DEFAULT_CHANNEL, "%s", _("Repeat Off"));
        vlc_object_release(p_vout);
    }
}

- (void)setAtoB
{
    if (!timeA) {
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);
        if (p_input) {
            timeA = var_GetTime(p_input, "time");
            vlc_object_release(p_input);
        }
    } else if (!timeB) {
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);
        if (p_input) {
            timeB = var_GetTime(p_input, "time");
            vlc_object_release(p_input);
        }
    } else
        [self resetAtoB];
}

- (void)resetAtoB
{
    timeA = 0;
    timeB = 0;
}

- (void)updateAtoB
{
    if (timeB) {
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);
        if (p_input) {
            mtime_t currentTime = var_GetTime(p_input, "time");
            if ( currentTime >= timeB || currentTime < timeA)
                var_SetTime(p_input, "time", timeA);
            vlc_object_release(p_input);
        }
    }
}

- (void)volumeUp
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_VolumeUp(pl_Get(p_intf), 1, NULL);
}

- (void)volumeDown
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_VolumeDown(pl_Get(p_intf), 1, NULL);
}

- (void)toggleMute
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    playlist_MuteToggle(pl_Get(p_intf));
}

- (BOOL)mute
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NO;

    BOOL b_is_muted = NO;
    b_is_muted = playlist_MuteGet(pl_Get(p_intf)) > 0;

    return b_is_muted;
}

- (int)volume
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return 0;

    float volume = playlist_VolumeGet(pl_Get(p_intf));

    return lroundf(volume * AOUT_VOLUME_DEFAULT);
}

- (void)setVolume: (int)i_value
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    if (i_value >= self.maxVolume)
        i_value = self.maxVolume;

    float f_value = i_value / (float)AOUT_VOLUME_DEFAULT;

    playlist_VolumeSet(pl_Get(p_intf), f_value);
}

- (float)maxVolume
{
    if (f_maxVolume == 0.) {
        f_maxVolume = (float)var_InheritInteger(VLCIntf, "macosx-max-volume") / 100. * AOUT_VOLUME_DEFAULT;
    }

    return f_maxVolume;
}

- (void)addSubtitlesToCurrentInput:(NSArray *)paths
{
    input_thread_t * p_input = pl_CurrentInput(VLCIntf);
    if (!p_input)
        return;

    NSUInteger count = [paths count];

    for (int i = 0; i < count ; i++) {
        const char *path = [[[paths objectAtIndex:i] path] UTF8String];
        msg_Dbg(VLCIntf, "loading subs from %s", path);

        int i_result = input_AddSubtitleOSD(p_input, path, true, true);
        if (i_result != VLC_SUCCESS)
            msg_Warn(VLCIntf, "unable to load subtitles from '%s'", path);
    }
    vlc_object_release(p_input);
}

#pragma mark - drag and drop support for VLCVoutView, VLCDragDropView and VLCThreePartDropView
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObject:NSFilenamesPboardType];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];

    if (o_carried_data) {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType]) {
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
            NSUInteger count = [o_values count];

            input_thread_t * p_input = pl_CurrentInput(VLCIntf);

            if (count == 1 && p_input) {
                int i_result = input_AddSubtitleOSD(p_input, [[o_values objectAtIndex:0] UTF8String], true, true);
                vlc_object_release(p_input);
                if (i_result == VLC_SUCCESS)
                    return YES;
            }
            else if (p_input)
                vlc_object_release(p_input);

            for (NSUInteger i = 0; i < count; i++) {
                NSDictionary *o_dic;
                char *psz_uri = vlc_path2uri([[o_values objectAtIndex:i] UTF8String], NULL);
                if (!psz_uri)
                    continue;

                o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
                free(psz_uri);

                o_array = [o_array arrayByAddingObject: o_dic];
            }

            [[[VLCMain sharedInstance] playlist] addPlaylistItems:o_array];
            return YES;
        }
    }
    return NO;
}

#pragma mark - video output stuff

- (void)setAspectRatioIsLocked:(BOOL)b_value
{
    config_PutInt(VLCIntf, "macosx-lock-aspect-ratio", b_value);
}

- (BOOL)aspectRatioIsLocked
{
    return config_GetInt(VLCIntf, "macosx-lock-aspect-ratio");
}

- (void)toggleFullscreen
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    vout_thread_t *p_vout = getVoutForActiveWindow();
    if (p_vout) {
        BOOL b_fs = var_ToggleBool(p_vout, "fullscreen");
        var_SetBool(pl_Get(p_intf), "fullscreen", b_fs);
        vlc_object_release(p_vout);
    } else { // e.g. lion fullscreen toggle
        BOOL b_fs = var_ToggleBool(pl_Get(p_intf), "fullscreen");
        [[[VLCMain sharedInstance] voutController] setFullscreen:b_fs forWindow:nil withAnimation:YES];
    }
}

#pragma mark - uncommon stuff

- (BOOL)fixPreferences
{
    NSMutableString * o_workString;
    NSRange returnedRange;
    NSRange fullRange;
    BOOL b_needsRestart = NO;

    #define fixpref(pref) \
    o_workString = [[NSMutableString alloc] initWithFormat:@"%s", config_GetPsz(VLCIntf, pref)]; \
    if ([o_workString length] > 0) \
    { \
        returnedRange = [o_workString rangeOfString:@"macosx" options: NSCaseInsensitiveSearch]; \
        if (returnedRange.location != NSNotFound) \
        { \
            if ([o_workString isEqualToString:@"macosx"]) \
                [o_workString setString:@""]; \
            fullRange = NSMakeRange(0, [o_workString length]); \
            [o_workString replaceOccurrencesOfString:@":macosx" withString:@"" options: NSCaseInsensitiveSearch range: fullRange]; \
            fullRange = NSMakeRange(0, [o_workString length]); \
            [o_workString replaceOccurrencesOfString:@"macosx:" withString:@"" options: NSCaseInsensitiveSearch range: fullRange]; \
            \
            config_PutPsz(VLCIntf, pref, [o_workString UTF8String]); \
            b_needsRestart = YES; \
        } \
    } \
    [o_workString release]

    fixpref("control");
    fixpref("extraintf");
    #undef fixpref

    return b_needsRestart;
}

#pragma mark - video filter handling

- (const char *)getFilterType:(const char *)psz_name
{
    module_t *p_obj = module_find(psz_name);
    if (!p_obj) {
        return NULL;
    }

    if (module_provides(p_obj, "video splitter")) {
        return "video-splitter";
    } else if (module_provides(p_obj, "video filter2")) {
        return "video-filter";
    } else if (module_provides(p_obj, "sub source")) {
        return "sub-source";
    } else if (module_provides(p_obj, "sub filter")) {
        return "sub-filter";
    } else {
        msg_Err(VLCIntf, "Unknown video filter type.");
        return NULL;
    }
}

- (void)setVideoFilter: (const char *)psz_name on:(BOOL)b_on
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;
    char *psz_string, *psz_parser;

    const char *psz_filter_type = [self getFilterType:psz_name];
    if (!psz_filter_type) {
        msg_Err(p_intf, "Unable to find filter module \"%s\".", psz_name);
        return;
    }

    msg_Dbg(p_intf, "will set filter '%s'", psz_name);


    psz_string = config_GetPsz(p_intf, psz_filter_type);

    if (b_on) {
        if (psz_string == NULL) {
            psz_string = strdup(psz_name);
        } else if (strstr(psz_string, psz_name) == NULL) {
            char *psz_tmp = strdup([[NSString stringWithFormat: @"%s:%s", psz_string, psz_name] UTF8String]);
            free(psz_string);
            psz_string = psz_tmp;
        }
    } else {
        if (!psz_string)
            return;

        psz_parser = strstr(psz_string, psz_name);
        if (psz_parser) {
            if (*(psz_parser + strlen(psz_name)) == ':') {
                memmove(psz_parser, psz_parser + strlen(psz_name) + 1,
                        strlen(psz_parser + strlen(psz_name) + 1) + 1);
            } else {
                *psz_parser = '\0';
            }

            /* Remove trailing : : */
            if (strlen(psz_string) > 0 && *(psz_string + strlen(psz_string) -1) == ':')
                *(psz_string + strlen(psz_string) -1) = '\0';
        } else {
            free(psz_string);
            return;
        }
    }
    config_PutPsz(p_intf, psz_filter_type, psz_string);

    /* Try to set on the fly */
    if (!strcmp(psz_filter_type, "video-splitter")) {
        playlist_t *p_playlist = pl_Get(p_intf);
        var_SetString(p_playlist, psz_filter_type, psz_string);
    } else {
        vout_thread_t *p_vout = getVout();
        if (p_vout) {
            var_SetString(p_vout, psz_filter_type, psz_string);
            vlc_object_release(p_vout);
        }
    }

    free(psz_string);
}

- (void)restartFilterIfNeeded: (const char *)psz_filter option: (const char *)psz_name
{
    vout_thread_t *p_vout = getVout();
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    if (p_vout == NULL)
        return;
    else
        vlc_object_release(p_vout);

    vlc_object_t *p_filter = vlc_object_find_name(pl_Get(p_intf), psz_filter);
    if (p_filter) {

        /* we cannot rely on the p_filter existence.
         This filter might be just
         disabled, but the object still exists. Therefore, the string
         is checked, additionally.
         */
        const char *psz_filter_type = [self getFilterType:psz_filter];
        if (!psz_filter_type) {
            msg_Err(p_intf, "Unable to find filter module \"%s\".", psz_name);
            goto out;
        }

        char *psz_string = config_GetPsz(p_intf, psz_filter_type);
        if (!psz_string) {
            goto out;
        }
        if (strstr(psz_string, psz_filter) == NULL) {
            free(psz_string);
            goto out;
        }
        free(psz_string);

        int i_type;
        i_type = var_Type(p_filter, psz_name);
        if (i_type == 0)
            i_type = config_GetType(p_intf, psz_name);

        if (!(i_type & VLC_VAR_ISCOMMAND)) {
            msg_Warn(p_intf, "Brute-restarting filter '%s', because the last changed option isn't a command", psz_name);

            [self setVideoFilter: psz_filter on: NO];
            [self setVideoFilter: psz_filter on: YES];
        } else
            msg_Dbg(p_intf, "restart not needed");

        out:
        vlc_object_release(p_filter);
    }
}

- (void)setVideoFilterProperty: (const char *)psz_name forFilter: (const char *)psz_filter integer: (int)i_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    config_PutInt(p_intf, psz_name, i_value);

    if (p_vout) {
        p_filter = vlc_object_find_name(pl_Get(p_intf), psz_filter);

        if (!p_filter) {
            msg_Warn(p_intf, "filter '%s' isn't enabled", psz_filter);
            vlc_object_release(p_vout);
            return;
        }
        var_SetInteger(p_filter, psz_name, i_value);
        vlc_object_release(p_vout);
        vlc_object_release(p_filter);

        [self restartFilterIfNeeded: psz_filter option: psz_name];
    }
}

- (void)setVideoFilterProperty: (const char *)psz_name forFilter: (const char *)psz_filter float: (float)f_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    config_PutFloat(p_intf, psz_name, f_value);

    if (p_vout) {
        p_filter = vlc_object_find_name(pl_Get(p_intf), psz_filter);

        if (!p_filter) {
            msg_Warn(p_intf, "filter '%s' isn't enabled", psz_filter);
            vlc_object_release(p_vout);
            return;
        }
        var_SetFloat(p_filter, psz_name, f_value);
        vlc_object_release(p_vout);
        vlc_object_release(p_filter);

        [self restartFilterIfNeeded: psz_filter option: psz_name];
    }
}

- (void)setVideoFilterProperty: (const char *)psz_name forFilter: (const char *)psz_filter string: (const char *)psz_value
{
    char *psz_new_value = strdup(psz_value);
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    config_PutPsz(p_intf, psz_name, EnsureUTF8(psz_new_value));

    if (p_vout) {
        p_filter = vlc_object_find_name(pl_Get(p_intf), psz_filter);

        if (!p_filter) {
            msg_Warn(p_intf, "filter '%s' isn't enabled", psz_filter);
            vlc_object_release(p_vout);
            return;
        }
        var_SetString(p_filter, psz_name, EnsureUTF8(psz_new_value));
        vlc_object_release(p_vout);
        vlc_object_release(p_filter);

        [self restartFilterIfNeeded: psz_filter option: psz_name];
    }

    free(psz_new_value);
}

- (void)setVideoFilterProperty: (const char *)psz_name forFilter: (const char *)psz_filter boolean: (BOOL)b_value
{
    vout_thread_t *p_vout = getVout();
    vlc_object_t *p_filter;
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return;

    config_PutInt(p_intf, psz_name, b_value);

    if (p_vout) {
        p_filter = vlc_object_find_name(pl_Get(p_intf), psz_filter);

        if (!p_filter) {
            msg_Warn(p_intf, "filter '%s' isn't enabled", psz_filter);
            vlc_object_release(p_vout);
            return;
        }
        var_SetBool(p_filter, psz_name, b_value);
        vlc_object_release(p_vout);
        vlc_object_release(p_filter);
    }
}

@end
