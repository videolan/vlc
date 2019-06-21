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
            /* TODO: 32 bits RGBA */
            /* TODO: 30 bits HDR RGB */
            case 24:
                if (vlc_popcount(vt->red_mask) == 8
                 && vlc_popcount(vt->green_mask) == 8
                 && vlc_popcount(vt->red_mask) == 8)
                    break; /* 32-bits ARGB or 24-bits RGB */
                return false;
            case 16:
            case 15:
                if (vlc_popcount(vt->red_mask) == 5
                 && vlc_popcount(vt->green_mask) == (depth - 10)
                 && vlc_popcount(vt->red_mask) == 5)
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

    const xcb_format_t *fmt = vlc_xcb_DepthToPixmapFormat(setup, depth);
    if (unlikely(fmt == NULL))
        return false;

    /* Byte sex is a non-issue for 8-bits. It can be worked around with
     * RGB masks for 24-bits. Too bad for 15-bits and 16-bits. */
    if (fmt->bits_per_pixel == 16 && setup->image_byte_order != ORDER)
        return false;

    /* Check that VLC supports the pixel format. */
    switch (fmt->depth)
    {
        case 24:
            if (fmt->bits_per_pixel == 32)
                f->i_chroma = VLC_CODEC_RGB32;
            else if (fmt->bits_per_pixel == 24)
                f->i_chroma = VLC_CODEC_RGB24;
            else
                return false;
            break;
        case 16:
            if (fmt->bits_per_pixel != 16)
                return false;
            f->i_chroma = VLC_CODEC_RGB16;
            break;
        case 15:
            if (fmt->bits_per_pixel != 16)
                return false;
            f->i_chroma = VLC_CODEC_RGB15;
            break;
        case 8:
            if (fmt->bits_per_pixel != 8)
                return false;
            if (vt->_class == XCB_VISUAL_CLASS_TRUE_COLOR)
                f->i_chroma = VLC_CODEC_RGB8;
            else
                f->i_chroma = VLC_CODEC_GREY;
            break;
        default:
            vlc_assert_unreachable();
    }

    f->i_rmask = vt->red_mask;
    f->i_gmask = vt->green_mask;
    f->i_bmask = vt->blue_mask;
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
