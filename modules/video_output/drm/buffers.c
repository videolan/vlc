/**
 * @file buffers.c
 * @brief DRM dumb buffers
 */
/*****************************************************************************
 * Copyright © 2022 Rémi Denis-Courmont
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

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#ifndef HAVE_LIBDRM
# include <drm/drm_fourcc.h>
# include <drm/drm_mode.h>
#else
# include <drm_fourcc.h>
# include <drm_mode.h>
#endif
#include <vlc_common.h>
#include <vlc_picture.h>
#include "vlc_drm.h"

struct vlc_drm_buf {
    struct picture_buffer_t buf; /**< Common picture buffer properties */
    uint32_t handle; /**< DRM buffer handle */
    uint32_t fb_id; /**< DRM frame buffer identifier */
};

static void drmDestroyDumb(int fd, uint32_t handle)
{
    struct drm_mode_destroy_dumb dreq = {
        .handle = handle,
    };

    vlc_drm_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
}

static void vlc_drm_dumb_buf_destroy(struct vlc_drm_buf *picbuf)
{
    munmap(picbuf->buf.base, picbuf->buf.size);
    drmDestroyDumb(picbuf->buf.fd, picbuf->handle);
    free(picbuf);
}

static void vlc_drm_dumb_destroy(picture_t *pic)
{
    struct vlc_drm_buf *picbuf = pic->p_sys;

    if (picbuf->fb_id != 0)
        vlc_drm_ioctl(picbuf->buf.fd, DRM_IOCTL_MODE_RMFB, &picbuf->fb_id);

    vlc_drm_dumb_buf_destroy(picbuf);
}

picture_t *vlc_drm_dumb_alloc(struct vlc_logger *log, int fd,
                              const video_format_t *restrict fmt)
{
    picture_t template;

    if (picture_Setup(&template, fmt))
        return NULL;

    /* Create the buffer handle */
    struct drm_mode_create_dumb creq = {
        .height = template.format.i_height,
        .width = template.format.i_width,
        .bpp = (vlc_fourcc_GetChromaBPP(template.format.i_chroma) + 7) & ~7,
        .flags =  0,
    };

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        vlc_error(log, "DRM dumb buffer creation error: %s",
                  vlc_strerror(errno));
        return NULL;
    }

    /* Map (really allocate) the buffer on driver side */
    struct drm_mode_map_dumb mreq = {
        .handle = creq.handle,
    };

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        vlc_error(log, "DRM dumb buffer mapping setup error: %s",
                  vlc_strerror(errno));
error:  drmDestroyDumb(fd, creq.handle);
        return NULL;
    }

    /* Map the buffer into the application process */
    unsigned char *base = mmap(NULL, creq.size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, mreq.offset);
    if (base == MAP_FAILED) {
        vlc_error(log, "DRM dumb buffer memory-mapping error: %s",
                  vlc_strerror(errno));
        goto error;
    }

    /* Create the VLC picture representation */
    struct vlc_drm_buf *picbuf = malloc(sizeof (*picbuf));
    if (unlikely(picbuf == NULL)) {
        munmap(base, creq.size);
        goto error;
    }

    picbuf->buf.fd = fd;
    picbuf->buf.base = base;
    picbuf->buf.size = creq.size;
    picbuf->buf.offset = mreq.offset;
    picbuf->handle = creq.handle;
    picbuf->fb_id = 0;

    picture_resource_t res = {
        .p_sys = picbuf,
        .pf_destroy = vlc_drm_dumb_destroy,
    };

    for (int i = 0; i < template.i_planes; i++) {
        res.p[i].p_pixels = base;
        res.p[i].i_lines = template.p[i].i_lines;
        res.p[i].i_pitch = template.p[i].i_pitch;
        base += template.p[i].i_lines * template.p[i].i_pitch;
    }

    picture_t *pic = picture_NewFromResource(fmt, &res);
    if (unlikely(pic == NULL))
        vlc_drm_dumb_buf_destroy(picbuf);
    return pic;
}

picture_t *vlc_drm_dumb_alloc_fb(struct vlc_logger *log, int fd,
                                 const video_format_t *restrict fmt)
{
    uint32_t pixfmt = vlc_drm_fourcc(fmt->i_chroma);
    if (pixfmt == DRM_FORMAT_INVALID)
        return NULL;

    picture_t *pic = vlc_drm_dumb_alloc(log, fd, fmt);
    if (pic == NULL)
        return NULL;

    struct vlc_drm_buf *picbuf = pic->p_sys;
    struct drm_mode_fb_cmd2 cmd = {
        .width = fmt->i_width,
        .height = fmt->i_height,
        .pixel_format = pixfmt,
        .flags = 0,
    };

    for (int i = 0; i < pic->i_planes; i++)
        cmd.handles[i] = picbuf->handle;
    for (int i = 0; i < pic->i_planes; i++)
        cmd.pitches[i] = pic->p[i].i_pitch;
    for (int i = 1; i < pic->i_planes; i++)
        cmd.offsets[i] = pic->p[i].p_pixels - pic->p[0].p_pixels;

    if (vlc_drm_ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &cmd) < 0) {
        vlc_error(log, "DRM framebuffer addition error: %s",
                  vlc_strerror(errno));
        picture_Release(pic);
        pic = NULL;
    } else
        picbuf->fb_id = cmd.fb_id;

    return pic;
}

uint32_t vlc_drm_dumb_get_fb_id(const picture_t *pic)
{
    struct vlc_drm_buf *picbuf = pic->p_sys;

    assert(picbuf->fb_id != 0);
    return picbuf->fb_id;
}
