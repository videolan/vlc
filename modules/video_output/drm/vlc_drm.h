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
#include <stdbool.h>
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
 * Converts a DRM pixel format to a VLC video format.
 *
 * \param [in,out] fmt VLC video format
 * \param drm_fourcc DRM pixel format identifier
 * \retval true the conversion succeeded (i.e. DRM format is recognised)
 * \retval false the conversion failed (i.e. DRM format is unknown)
 */
bool vlc_video_format_drm(video_format_t *restrict fmt,
                          uint_fast32_t drm_fourcc);

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

/**
 * Finds the index of a CRTC.
 *
 * The DRM API represents sets of CRTCs as 32-bit bit masks.
 * This function determines the bit index of a given CRTC.
 *
 * \param fd DRM device file descriptor
 * \param crtc_id CRTC object ID
 * \return On success, the index (between 0 and 31) of object is returned,
 * On error, -1 is returned and @c errno is set.
 */
int vlc_drm_get_crtc_index(int fd, uint_fast32_t crtc_id);

/**
 * Finds the primary plane of a CRTC.
 *
 * \param fd DRM device file descriptor
 * \param idx CRTC object index (as returned by vlc_drm_get_crtc_index())
 * }param[out] nfmts storage space for the plane's count of pixel formats
 * \return the primary plane object ID or zero on error
 */
uint_fast32_t vlc_drm_get_crtc_primary_plane(int fd, unsigned int idx,
                                             size_t *nfmts);

/**
 * Finds the best matching DRM format.
 *
 * This determines the DRM format of a plane given by ID, which best matches
 * a given VLC pixel format. If there is an exact match, it will be returned.
 *
 * \param fd DRM device file descriptor
 * \param plane_id DRM plane object ID
 * \param nfmt number of DRM pixel formats for the plane
 * \param chroma VLC pixel format to match
 * \return the matched DRM format on success, zero on failure
 */
uint_fast32_t vlc_drm_find_best_format(int fd, uint_fast32_t plane_id,
                                       size_t nfmt, vlc_fourcc_t chroma);

static inline int vlc_drm_ioctl(int fd, unsigned long cmd, void *argp)
{
    int ret;

    do
        ret = ioctl(fd, cmd, argp);
    while (ret < 0 && (errno == EINTR || errno == EAGAIN));

    return ret;
}
