/**
 * @file vlc_drm.h
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

#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <vlc_common.h>

struct vlc_logger;
struct video_format_t;

/**
 * Converts a VLC pixel format to DRM.
 *
 * \param vlc_fourcc VLC video format FourCC
 * \return the corresponding DRM pixel format FourCC or
 *         DRM_FORMAT_INVALID if not found.
 * \warning This function cannot handle RGB formats. Use vlc_drm_format().
 */
uint_fast32_t vlc_drm_fourcc(vlc_fourcc_t vlc_fourcc);

/**
 * Converts a VLC video format to DRM.
 *
 * This returns the DRM pixel format FourCC for the supplied VLC video format.
 * Unlike vlc_drm_fourcc(), this function can handle RGB formats, but it
 * requires a complete VLC format structure.
 *
 * \param fmt VLC video format
 * \return the corresponding DRM pixel format FourCC or
 *         DRM_FORMAT_INVALID if not found.
 */
uint_fast32_t vlc_drm_format(const struct video_format_t *fmt);

/**
 * Converts a DRM pixel format to VLC.
 *
 * \param drm_fourcc DRM pixel format identifier
 * \return the corresponding VLC pixel format, or 0 if not found.
 */
vlc_fourcc_t vlc_fourcc_drm(uint_fast32_t drm_fourcc);

/**
 * Allocates a DRM dumb buffer.
 *
 * \param fd DRM device file descriptor
 * \param fmt picture format
 * \return a DRM dumb frame buffer as picture, or NULL on error.
 */
picture_t *vlc_drm_dumb_alloc(struct vlc_logger *, int fd,
                              const video_format_t *restrict fmt);

/**
 * Allocates a DRM dumb frame buffer.
 *
 * \param fd DRM device file descriptor
 * \param fmt picture format
 * \return a DRM dumb frame buffer as picture, or NULL on error.
 */
picture_t *vlc_drm_dumb_alloc_fb(struct vlc_logger *, int fd,
                                 const video_format_t *restrict fmt);

uint32_t vlc_drm_dumb_get_fb_id(const picture_t *pic);

static inline int vlc_drm_ioctl(int fd, unsigned long cmd, void *argp)
{
    int ret;

    do
        ret = ioctl(fd, cmd, argp);
    while (ret < 0 && (errno == EINTR || errno == EAGAIN));

    return ret;
}
