/*****************************************************************************
 * mac.c: Screen capture module for the Mac.
 *****************************************************************************
 * Copyright (C) 2004 - 2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: FUJISAWA Tooru <arai_a@mac.com>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          Felix Paul Kühne <fkuehne # videolan org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <vlc_common.h>
#import <vlc_block.h>

#import "screen.h"

#import <ApplicationServices/ApplicationServices.h>
#import <QuartzCore/QuartzCore.h>

extern int CGSMainConnectionID();
extern CGImageRef CGSCreateRegisteredCursorImage(int, char*, CGPoint*);

struct screen_data_t
{
    block_t *p_block;

    int width;
    int height;

    int screen_top;
    int screen_left;
    int screen_width;
    int screen_height;

    CGDirectDisplayID display_id;
};

int screen_InitCapture(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    CGLError returnedError;
    int i_bits_per_pixel, i_chroma = 0;

    p_sys->p_data = p_data = calloc(1, sizeof(screen_data_t));
    if (!p_data)
        return VLC_ENOMEM;

    /* fetch the screen we should capture */
    p_data->display_id = kCGDirectMainDisplay;

    unsigned int displayCount = 0;
    returnedError = CGGetOnlineDisplayList(0, NULL, &displayCount);
    if (!returnedError) {
        CGDirectDisplayID *ids;
        ids = (CGDirectDisplayID *)malloc(displayCount * sizeof(CGDirectDisplayID));
        returnedError = CGGetOnlineDisplayList(displayCount, ids, &displayCount);
        if (!returnedError) {
            if (p_sys->i_display_id > 0) {
                for (unsigned int i = 0; i < displayCount; i++) {
                    if (p_sys->i_display_id == ids[i]) {
                        p_data->display_id = ids[i];
                        break;
                    }
                }
            } else if (p_sys->i_screen_index > 0 && p_sys->i_screen_index <= displayCount)
                p_data->display_id = ids[p_sys->i_screen_index - 1];
        }
        free(ids);
    }

    /* Get the device context for the whole screen */
    CGRect rect = CGDisplayBounds(p_data->display_id);
    p_data->screen_left = rect.origin.x;
    p_data->screen_top = rect.origin.y;
    p_data->screen_width = rect.size.width;
    p_data->screen_height = rect.size.height;

    p_data->width = p_sys->i_width;
    p_data->height = p_sys->i_height;
    if (p_data->width <= 0 || p_data->height <= 0) {
        p_data->width = p_data->screen_width;
        p_data->height = p_data->screen_height;
    }

    CFStringRef pixelEncoding = CGDisplayModeCopyPixelEncoding(CGDisplayCopyDisplayMode(p_data->display_id));
    int length = CFStringGetLength(pixelEncoding);
    length++;
    char *psz_name = (char *)malloc(length);
    CFStringGetCString(pixelEncoding, psz_name, length, kCFStringEncodingUTF8);
    msg_Dbg(p_demux, "pixel encoding is '%s'", psz_name);
    CFRelease(pixelEncoding);

    if (!strcmp(psz_name, IO32BitDirectPixels)) {
        i_chroma = VLC_CODEC_RGB32;
        i_bits_per_pixel = 32;
    } else if (!strcmp(psz_name, IO16BitDirectPixels)) {
        i_chroma = VLC_CODEC_RGB16;
        i_bits_per_pixel = 16;
    } else if (!strcmp(psz_name, IO8BitIndexedPixels)) {
        i_chroma = VLC_CODEC_RGB8;
        i_bits_per_pixel = 8;
    } else {
        msg_Err(p_demux, "unsupported pixel encoding");
        free(p_data);
        return VLC_EGENERIC;
    }
    free(psz_name);

    /* setup format */
    es_format_Init(&p_sys->fmt, VIDEO_ES, i_chroma);
    p_sys->fmt.video.i_visible_width  =
    p_sys->fmt.video.i_width          = rect.size.width;
    p_sys->fmt.video.i_visible_height =
    p_sys->fmt.video.i_height         = rect.size.height;
    p_sys->fmt.video.i_bits_per_pixel = i_bits_per_pixel;
    p_sys->fmt.video.i_chroma         = i_chroma;

    switch (i_chroma) {
        case VLC_CODEC_RGB15:
            p_sys->fmt.video.i_rmask = 0x7c00;
            p_sys->fmt.video.i_gmask = 0x03e0;
            p_sys->fmt.video.i_bmask = 0x001f;
            break;
        case VLC_CODEC_RGB24:
            p_sys->fmt.video.i_rmask = 0x00ff0000;
            p_sys->fmt.video.i_gmask = 0x0000ff00;
            p_sys->fmt.video.i_bmask = 0x000000ff;
            break;
        case VLC_CODEC_RGB32:
            p_sys->fmt.video.i_rmask = 0x00ff0000;
            p_sys->fmt.video.i_gmask = 0x0000ff00;
            p_sys->fmt.video.i_bmask = 0x000000ff;
            break;
        default:
            msg_Warn( p_demux, "Unknown RGB masks" );
            break;
    }

    return VLC_SUCCESS;
}

int screen_CloseCapture(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = p_sys->p_data;

    if (p_data->p_block)
        block_Release(p_data->p_block);

    free(p_data);

    return VLC_SUCCESS;
}

block_t *screen_Capture(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = (screen_data_t *)p_sys->p_data;
    block_t *p_block;
    CGRect capture_rect;
    CGImageRef image;
    CGDataProviderRef dataProvider;
    CFDataRef data;

    /* forward cursor location */
    CGPoint cursor_pos;

    CGEventRef event = CGEventCreate(NULL);
    cursor_pos = CGEventGetLocation(event);
    CFRelease(event);

    cursor_pos.x -= p_data->screen_left;
    cursor_pos.y -= p_data->screen_top;

    if (p_sys->b_follow_mouse)
        FollowMouse(p_sys, cursor_pos.x, cursor_pos.y);

    capture_rect.origin.x = p_sys->i_left;
    capture_rect.origin.y = p_sys->i_top;
    capture_rect.size.width = p_data->width;
    capture_rect.size.height = p_data->height;

#if 0
    // FIXME: actually plot cursor image into snapshot
    /* fetch cursor image */
    CGImageRef cursor_image;
    int cid = CGSMainConnectionID();
    CGPoint outHotSpot;
    cursor_image = CGSCreateRegisteredCursorImage(cid, (char *)"com.apple.coregraphics.GlobalCurrent", &outHotSpot);
#endif

    /* fetch image data */
    image = CGDisplayCreateImageForRect(p_data->display_id, capture_rect);
    if (image) {
        /* build block */
        int i_buffer = (p_sys->fmt.video.i_bits_per_pixel + 7) / 8 * p_sys->fmt.video.i_width * p_sys->fmt.video.i_height;
        p_block = block_Alloc(i_buffer);
        if (!p_block) {
            msg_Warn(p_demux, "can't get block");
            return NULL;
        }

        dataProvider = CGImageGetDataProvider(image);
        data = CGDataProviderCopyData(dataProvider);
        CFDataGetBytes(data, CFRangeMake(0,CFDataGetLength(data)), p_block->p_buffer);

        CFRelease(data);
        CFRelease(dataProvider);
        CFRelease(image);

        return p_block;
    }

    msg_Warn(p_demux, "no image!");
    return NULL;
}
