/*****************************************************************************
 * CoreInteraction.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2013 Felix Paul Kühne
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

@interface VLCMainWindow (Internal)
- (void)jumpWithValue:(char *)p_value forward:(BOOL)b_value;
@end

@implementation VLCCoreInteraction
static VLCCoreInteraction *_o_sharedInstance = nil;

+ (VLCCoreInteraction *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

#pragma mark -
#pragma mark Initialization

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

- (void)awakeFromNib
{
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(applicationWillFinishLaunching:)
                                                 name: NSApplicationWillFinishLaunchingNotification
                                               object: nil];
}

#pragma mark -
#pragma mark Playback Controls

- (void)playOrPause
{
    input_thread_t * p_input;
    p_input = pl_CurrentInput(VLCIntf);
    playlist_t * p_playlist = pl_Get(VLCIntf);

    if (p_input) {
        playlist_Pause(p_playlist);
        vlc_object_release(p_input);
    } else {
        bool empty;

        PL_LOCK;
        empty = playlist_IsEmpty(p_playlist);
        PL_UNLOCK;

        if ([[[VLCMain sharedInstance] playlist] isSelectionEmpty] && ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot] == p_playlist->p_local_category || [[[VLCMain sharedInstance] playlist] currentPlaylistRoot] == p_playlist->p_ml_category))
            [[[VLCMain sharedInstance] open] openFileGeneric];
        else
            [[[VLCMain sharedInstance] playlist] playItem:nil];
    }
}

- (void)pause
{
    playlist_t *p_playlist = pl_Get(VLCIntf);

    PL_LOCK;
    bool b_playlist_playing = playlist_Status(p_playlist) == PLAYLIST_RUNNING;
    PL_UNLOCK;

    if (b_playlist_playing)
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

    NSString *o_name;
    char *format = var_InheritString(VLCIntf, "input-title-format");
    char *formated = str_format_meta(pl_Get(VLCIntf), format);
    free(format);
    o_name = [NSString stringWithUTF8String:formated];
    free(formated);

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

#pragma mark -
#pragma mark drag and drop support for VLCVoutView, VLBrushedMetalImageView and VLCThreePartDropView
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *o_paste = [sender draggingPasteboard];
    NSArray *o_types = [NSArray arrayWithObject:NSFilenamesPboardType];
    NSString *o_desired_type = [o_paste availableTypeFromArray:o_types];
    NSData *o_carried_data = [o_paste dataForType:o_desired_type];
    BOOL b_autoplay = config_GetInt(VLCIntf, "macosx-autoplay");

    if (o_carried_data) {
        if ([o_desired_type isEqualToString:NSFilenamesPboardType]) {
            NSArray *o_array = [NSArray array];
            NSArray *o_values = [[o_paste propertyListForType: NSFilenamesPboardType] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
            NSUInteger count = [o_values count];

            input_thread_t * p_input = pl_CurrentInput(VLCIntf);
            BOOL b_returned = NO;

            if (count == 1 && p_input) {
                b_returned = input_AddSubtitle(p_input, [[o_values objectAtIndex:0] UTF8String], true);
                vlc_object_release(p_input);
                if (!b_returned)
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
            if (b_autoplay)
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:NO];
            else
                [[[VLCMain sharedInstance] playlist] appendArray: o_array atPos: -1 enqueue:YES];

            return YES;
        }
    }
    return NO;
}

#pragma mark -
#pragma mark video output stuff

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
        [[[VLCMain sharedInstance] voutController] setFullscreen:b_fs forWindow:nil];
    }
}

#pragma mark -
#pragma mark uncommon stuff

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

@end
