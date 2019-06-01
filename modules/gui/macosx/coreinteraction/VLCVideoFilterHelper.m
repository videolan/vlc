/*****************************************************************************
 * VLCVideoFilterHelper.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2006-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCVideoFilterHelper.h"

#import <vlc_modules.h>
#import <vlc_charset.h>

#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

@implementation VLCVideoFilterHelper

+ (const char *)getFilterType:(const char *)psz_name
{
    module_t *p_obj = module_find(psz_name);
    if (!p_obj) {
        return NULL;
    }

    if (module_provides(p_obj, "video splitter")) {
        return "video-splitter";
    } else if (module_provides(p_obj, "video filter")) {
        return "video-filter";
    } else if (module_provides(p_obj, "sub source")) {
        return "sub-source";
    } else if (module_provides(p_obj, "sub filter")) {
        return "sub-filter";
    } else {
        msg_Err(getIntf(), "Unknown video filter type.");
        return NULL;
    }
}

+ (void)setVideoFilter: (const char *)psz_name on:(BOOL)b_on
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return;
    char *psz_string, *psz_parser;

    const char *psz_filter_type = [self getFilterType:psz_name];
    if (!psz_filter_type) {
        msg_Err(p_intf, "Unable to find filter module \"%s\".", psz_name);
        return;
    }

    VLCPlayerController *playerController = [[[VLCMain sharedInstance] playlistController] playerController];
    vout_thread_t *vout = [playerController mainVideoOutputThread];
    if (!vout)
        return;

    msg_Dbg(p_intf, "will turn filter '%s' %s", psz_name, b_on ? "on" : "off");

    psz_string = var_InheritString(vout, psz_filter_type);

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
        {
            vout_Release(vout);
            return;
        }

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
            vout_Release(vout);
            return;
        }
    }
    var_SetString(vout, psz_filter_type, psz_string);
    vout_Release(vout);
    free(psz_string);
}

+ (void)setVideoFilterProperty: (char const *)psz_property
                     forFilter: (char const *)psz_filter
                     withValue: (vlc_value_t)value
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf)
        return;

    int i_type = 0;
    char const *psz_filter_type = [self getFilterType: psz_filter];
    if (!psz_filter_type) {
        msg_Err(p_intf, "Unable to find filter module \"%s\".", psz_filter);
        return;
    }

    VLCPlayerController *playerController = [[[VLCMain sharedInstance] playlistController] playerController];
    vout_thread_t *vout = [playerController mainVideoOutputThread];
    if (!vout)
        return;

    i_type = var_Type(vout, psz_property);
    if (!i_type)
        i_type = config_GetType(psz_property);

    i_type &= VLC_VAR_CLASS;
    var_SetChecked(vout, psz_property, i_type, value);
    vout_Release(vout);
}


@end
