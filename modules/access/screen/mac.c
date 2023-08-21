/*****************************************************************************
 * mac.c: Screen capture module for the Mac.
 *****************************************************************************
 * Copyright (C) 2004 - 2013 VLC authors and VideoLAN
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

static void screen_CloseCapture(void *);
static block_t *screen_Capture(demux_t *);


typedef struct
{
    block_t *p_block;

    int width;
    int height;

    int screen_top;
    int screen_left;
    int screen_width;
    int screen_height;

    float rate;

    CGDirectDisplayID display_id;

    CGContextRef offscreen_context;
    CGRect offscreen_rect;
    void *offscreen_bitmap;
    size_t offscreen_bitmap_size;
} screen_data_t;

int screen_InitCapture(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    CGLError returnedError;
    unsigned int i_display_id;
    unsigned int i_screen_index;

    p_sys->p_data = p_data = calloc(1, sizeof(screen_data_t));
    if (!p_data)
        return VLC_ENOMEM;

    i_display_id = var_CreateGetInteger( p_demux, "screen-display-id" );
    i_screen_index = var_CreateGetInteger( p_demux, "screen-index" );

    /* fetch the screen we should capture */
    p_data->display_id = kCGDirectMainDisplay;
    p_data->rate = var_InheritFloat(p_demux, "screen-fps");

    unsigned int displayCount = 0;
    returnedError = CGGetOnlineDisplayList(0, NULL, &displayCount);
    if (!returnedError) {
        CGDirectDisplayID *ids;
        ids = vlc_alloc(displayCount, sizeof(CGDirectDisplayID));
        returnedError = CGGetOnlineDisplayList(displayCount, ids, &displayCount);
        if (!returnedError) {
            if (i_display_id > 0) {
                for (unsigned int i = 0; i < displayCount; i++) {
                    if (i_display_id == ids[i]) {
                        p_data->display_id = ids[i];
                        break;
                    }
                }
            } else if (i_screen_index > 0 && i_screen_index <= displayCount)
                p_data->display_id = ids[i_screen_index - 1];
        }
        free(ids);
    }

    /* Get the device context for the whole screen */
    CGRect rect = CGDisplayBounds(p_data->display_id);
    p_data->screen_left = rect.origin.x;
    p_data->screen_top = rect.origin.y;
    p_data->screen_width = rect.size.width;
    p_data->screen_height = rect.size.height;

#ifdef SCREEN_SUBSCREEN
    p_data->width = p_sys->i_width;
    p_data->height = p_sys->i_height;
    if (p_data->width <= 0 || p_data->height <= 0)
#endif
    {
        p_data->width = p_data->screen_width;
        p_data->height = p_data->screen_height;
    }

    /* setup format */
    es_format_Init(&p_sys->fmt, VIDEO_ES, VLC_CODEC_XRGB);
    p_sys->fmt.video.i_visible_width   =
    p_sys->fmt.video.i_width           = rect.size.width;
    p_sys->fmt.video.i_visible_height  =
    p_sys->fmt.video.i_height          = rect.size.height;
    p_sys->fmt.video.i_chroma          = VLC_CODEC_XRGB;
    p_sys->fmt.video.i_sar_num         =
    p_sys->fmt.video.i_sar_den         = 1;
    p_sys->fmt.video.i_frame_rate      = 1000 * p_data->rate;
    p_sys->fmt.video.i_frame_rate_base = 1000;

    static const struct screen_capture_operations ops = {
        screen_Capture, screen_CloseCapture,
    };
    p_sys->ops = &ops;

    return VLC_SUCCESS;
}

static void screen_CloseCapture(void *opaque)
{
    screen_data_t *p_data = opaque;
    if (p_data->offscreen_context)
        CFRelease(p_data->offscreen_context);

    if (p_data->offscreen_bitmap)
        free(p_data->offscreen_bitmap);

    if (p_data->p_block)
        block_Release(p_data->p_block);

    free(p_data);
}

block_t *screen_Capture(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = (screen_data_t *)p_sys->p_data;
    block_t *p_block;
    CGRect capture_rect;
    CGImageRef image;

    /* forward cursor location */
    CGPoint cursor_pos;

    CGEventRef event = CGEventCreate(NULL);
    cursor_pos = CGEventGetLocation(event);
    CFRelease(event);

    cursor_pos.x -= p_data->screen_left;
    cursor_pos.y -= p_data->screen_top;

#ifdef SCREEN_SUBSCREEN
    if (p_sys->b_follow_mouse)
        FollowMouse(p_sys, cursor_pos.x, cursor_pos.y);

    capture_rect.origin.x = p_sys->i_left;
    capture_rect.origin.y = p_sys->i_top;
#else // !SCREEN_SUBSCREEN
    capture_rect.origin.x = 0;
    capture_rect.origin.y = 0;
#endif // !SCREEN_SUBSCREEN
    capture_rect.size.width = p_data->width;
    capture_rect.size.height = p_data->height;

    /* fetch image data */
    image = CGDisplayCreateImageForRect(p_data->display_id, capture_rect);
    if (!image) {
        msg_Warn(p_demux, "no image!");
        return NULL;
    }

    /* create offscreen context */
    if (!p_data->offscreen_context) {
        CGColorSpaceRef colorspace;

        colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);

        p_data->offscreen_bitmap_size = p_sys->fmt.video.i_width * p_sys->fmt.video.i_height * 4;
        p_data->offscreen_bitmap = calloc(1, p_data->offscreen_bitmap_size);
        if (p_data->offscreen_bitmap == NULL) {
            msg_Warn(p_demux, "can't allocate offscreen bitmap");
            CFRelease(image);
            return NULL;
        }

        p_data->offscreen_context = CGBitmapContextCreate(p_data->offscreen_bitmap, p_sys->fmt.video.i_width, p_sys->fmt.video.i_height, 8, p_sys->fmt.video.i_width * 4, colorspace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        if (!p_data->offscreen_context) {
            msg_Warn(p_demux, "can't create offscreen bitmap context");
            CFRelease(image);
            return NULL;
        }

        CGColorSpaceRelease(colorspace);

        p_data->offscreen_rect = CGRectMake(0, 0, p_sys->fmt.video.i_width, p_sys->fmt.video.i_height);
    }

    /* fetch cursor image */
    CGImageRef cursor_image;
    int cid = CGSMainConnectionID();
    CGPoint outHotSpot;
    cursor_image = CGSCreateRegisteredCursorImage(cid, (char *)"com.apple.coregraphics.GlobalCurrent", &outHotSpot);

    /* draw screen image and cursor image */
    CGRect cursor_rect;
    cursor_rect.size.width = CGImageGetWidth(cursor_image);
    cursor_rect.size.height = CGImageGetHeight(cursor_image);
#ifdef SCREEN_SUBSCREEN
    cursor_rect.origin.x = cursor_pos.x - p_sys->i_left - outHotSpot.x;
    cursor_rect.origin.y = p_data->offscreen_rect.size.height
        - (cursor_pos.y + cursor_rect.size.height - p_sys->i_top - outHotSpot.y);
#else // !SCREEN_SUBSCREEN
    cursor_rect.origin.x = cursor_pos.x - outHotSpot.x;
    cursor_rect.origin.y = p_data->offscreen_rect.size.height
        - (cursor_pos.y + cursor_rect.size.height - outHotSpot.y);
#endif // !SCREEN_SUBSCREEN

    CGContextDrawImage(p_data->offscreen_context, p_data->offscreen_rect, image);
    CGContextDrawImage(p_data->offscreen_context, cursor_rect, cursor_image);

    /* build block */
    p_block = block_Alloc(p_data->offscreen_bitmap_size);
    if (!p_block) {
        msg_Warn(p_demux, "can't get block");
        CFRelease(image);
        return NULL;
    }

    memmove(p_block->p_buffer, p_data->offscreen_bitmap, p_data->offscreen_bitmap_size);

    CFRelease(image);

    return p_block;
}
