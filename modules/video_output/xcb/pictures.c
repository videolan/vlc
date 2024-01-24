/**
 * @file pictures.c
 * @brief Pictures management code for XCB video output plugins
 */
/*****************************************************************************
 * Copyright © 2009-2013 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdbit.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>

#include <vlc_common.h>

#include "pictures.h"

const xcb_format_t *vlc_xcb_DepthToPixmapFormat(const xcb_setup_t *setup,
                                                uint_fast8_t depth)
{
    const xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);

    for (int i = xcb_setup_pixmap_formats_length(setup); i > 0; i--, fmt++)
        if (fmt->depth == depth)
        {   /* Sanity check: unusable format is as good as none. */
            if (unlikely(fmt->bits_per_pixel < depth
                      || (fmt->scanline_pad % fmt->bits_per_pixel)))
                return NULL;
            return fmt;
        }
    return NULL; /* should never happen, buggy server */
}

/** Convert X11 visual to VLC video format */
bool vlc_xcb_VisualToFormat(const xcb_setup_t *setup, uint_fast8_t depth,
                            const xcb_visualtype_t *vt,
                            video_format_t *restrict f)
{
    if (vt->_class == XCB_VISUAL_CLASS_TRUE_COLOR)
    {
        /* Check that VLC supports the TrueColor visual. */
        switch (depth)
        {
            /* TODO: 30 bits HDR RGB */
            case 32:
            case 24:
                if (stdc_count_ones(vt->red_mask) == 8
                 && stdc_count_ones(vt->green_mask) == 8
                 && stdc_count_ones(vt->red_mask) == 8)
                    break; /* 32-bits ARGB or 24-bits RGB */
                return false;
            case 16:
            case 15:
                if (stdc_count_ones(vt->red_mask) == 5
                 && stdc_count_ones(vt->green_mask) == (depth - 10u)
                 && stdc_count_ones(vt->red_mask) == 5)
                    break; /* 16-bits or 15-bits RGB */
                return false;
            case 8:
                /* XXX: VLC does not use masks for 8-bits. Untested. */
                break;
            default:
                return false;
        }
    }
    else
    if (vt->_class == XCB_VISUAL_CLASS_STATIC_GRAY)
    {
        if (depth != 8)
            return false;
    }
    else
        return false; /* unsupported visual class */

    const xcb_format_t *fmt = vlc_xcb_DepthToPixmapFormat(setup, depth);
    if (unlikely(fmt == NULL))
        return false;

    /* Check that VLC supports the pixel format. */
    switch (fmt->depth)
    {
        case 32:
            if (fmt->bits_per_pixel != 32)
                return false;
            /* fallthrough */
        case 24:
            if (fmt->bits_per_pixel == 32)
            {
                if (vt->red_mask   == 0x00ff0000 &&
                    vt->green_mask == 0x0000ff00 &&
                    vt->blue_mask  == 0x000000ff)
                {
                    f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                                  VLC_CODEC_XRGB : VLC_CODEC_BGRX;
                }
                else
                if (vt->red_mask   == 0x000000ff &&
                    vt->green_mask == 0x0000ff00 &&
                    vt->blue_mask  == 0x00ff0000)
                {
                    f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                                  VLC_CODEC_XBGR : VLC_CODEC_RGBX;
                }
                else
                if (vt->red_mask   == 0xff000000 &&
                    vt->green_mask == 0x00ff0000 &&
                    vt->blue_mask  == 0x0000ff00)
                {
                    f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                                  VLC_CODEC_RGBX : VLC_CODEC_XBGR;
                }
                else
                if (vt->red_mask   == 0x0000ff00 &&
                    vt->green_mask == 0x00ff0000 &&
                    vt->blue_mask  == 0xff000000)
                {
                    f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                                  VLC_CODEC_BGRX : VLC_CODEC_XRGB;
                }
                else
                    return false;
            }
            else if (fmt->bits_per_pixel == 24)
            {
                if (vt->red_mask   == 0xff0000 &&
                    vt->green_mask == 0x00ff00 &&
                    vt->blue_mask  == 0x0000ff)
                {
                    f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                                  VLC_CODEC_RGB24 : VLC_CODEC_BGR24;
                }
                else
                if (vt->red_mask   == 0x0000ff &&
                    vt->green_mask == 0x00ff00 &&
                    vt->blue_mask  == 0xff0000)
                {
                    f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                                  VLC_CODEC_BGR24 : VLC_CODEC_RGB24;
                }
                else
                    return false;
            }
            else
                return false;
            break;
        case 16:
            if (fmt->bits_per_pixel != 16)
                return false;
            if (vt->red_mask   == 0xf800 &&
                vt->green_mask == 0x07e0 &&
                vt->blue_mask  == 0x001f)
            {
                f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                              VLC_CODEC_RGB565BE : VLC_CODEC_RGB565LE;
            }
            else
            if (vt->red_mask   == 0x001f &&
                vt->green_mask == 0x07e0 &&
                vt->blue_mask  == 0xf800)
            {
                f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                              VLC_CODEC_BGR565BE : VLC_CODEC_BGR565LE;
            }
            else
                return false;
            break;
        case 15:
            if (fmt->bits_per_pixel != 16)
                return false;
            if (vt->red_mask   == 0x7c00 &&
                vt->green_mask == 0x03e0 &&
                vt->blue_mask  == 0x001f)
            {
                f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                              VLC_CODEC_RGB555BE : VLC_CODEC_RGB555LE;
            }
            else
            if (vt->red_mask   == 0x001f &&
                vt->green_mask == 0x03e0 &&
                vt->blue_mask  == 0x7c00)
            {
                f->i_chroma = setup->image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST ?
                              VLC_CODEC_BGR555BE : VLC_CODEC_BGR555LE;
            }
            else
                return false;
            break;
        case 8:
            if (fmt->bits_per_pixel != 8)
                return false;
            if (vt->_class == XCB_VISUAL_CLASS_TRUE_COLOR)
                f->i_chroma = VLC_CODEC_RGB233;
            else
                f->i_chroma = VLC_CODEC_GREY;
            break;
        default:
            vlc_assert_unreachable();
    }

    return true;
}

/** Check MIT-SHM shared memory support */
bool XCB_shm_Check (vlc_object_t *obj, xcb_connection_t *conn)
{
    xcb_shm_query_version_cookie_t ck;
    xcb_shm_query_version_reply_t *r;

    ck = xcb_shm_query_version (conn);
    r = xcb_shm_query_version_reply (conn, ck, NULL);
    if (r == NULL)
    {
        msg_Err(obj, "MIT-SHM extension not available");
        goto fail;
    }

    msg_Dbg(obj, "MIT-SHM extension version %"PRIu16".%"PRIu16,
            r->major_version, r->minor_version);

    if (r->major_version == 1 && r->minor_version < 2)
    {
        msg_Err(obj, "MIT-SHM extension too old");
        free(r);
        goto fail;
    }

    free (r);
    return true;
fail:
    msg_Warn(obj, "display will be slow");
    return false;
}
