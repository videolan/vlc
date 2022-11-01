/*****************************************************************************
 * vlc_vout.h: common video definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2008 VLC authors and VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Olivier Aubert <oaubert 47 videolan d07 org>
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

#ifndef VLC_VOUT_H_
#define VLC_VOUT_H_ 1

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_subpicture.h>

/**
 * \defgroup output Output
 * \ingroup vlc
 * \defgroup video_output Video output
 * \ingroup output
 * Video rendering, output and window management
 *
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously opened video output thread.
 * @{
 * \file
 * Video output thread interface
 */

/**
 * Video output thread descriptor
 *
 * Any independent video output device, such as an X11 window or a GGI device,
 * is represented by a video output thread, and described using the following
 * structure.
 */
struct vout_thread_t {
    struct vlc_object_t obj;
};

/* Alignment flags */
#define VOUT_ALIGN_LEFT         0x0001
#define VOUT_ALIGN_RIGHT        0x0002
#define VOUT_ALIGN_HMASK        0x0003
#define VOUT_ALIGN_TOP          0x0004
#define VOUT_ALIGN_BOTTOM       0x0008
#define VOUT_ALIGN_VMASK        0x000C

/**
 * vout or spu_channel order
 */
enum vlc_vout_order
{
    VLC_VOUT_ORDER_NONE,
    /**
     * There is only one primary vout/spu_channel
     * For vouts: this is the first vout, probably embedded in the UI.
     * For spu channels: main and first SPU channel.
     */
    VLC_VOUT_ORDER_PRIMARY,
    /**
     * There can be several secondary vouts or spu_channels
     * For vouts: a secondary vout using its own window.
     * For spu channels: a secondary spu channel that is placed in function of
     * the primary one. See "secondary-sub-margin" and
     * "secondary-sub-alignment".
     */
    VLC_VOUT_ORDER_SECONDARY,
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * Destroys a vout.
 *
 * This function closes and releases a vout created by vout_Create().
 *
 * \param vout the vout to close
 */
VLC_API void vout_Close(vout_thread_t *vout);

/**
 * This function will handle a snapshot request.
 *
 * pp_image, pp_picture and p_fmt can be NULL otherwise they will be
 * set with returned value in case of success.
 *
 * pp_image will hold an encoded picture in psz_format format.
 *
 * p_fmt can be NULL otherwise it will be set with the format used for the
 * picture before encoding.
 *
 * i_timeout specifies the time the function will wait for a snapshot to be
 * available.
 *
 */
VLC_API int vout_GetSnapshot( vout_thread_t *p_vout,
                              block_t **pp_image, picture_t **pp_picture,
                              video_format_t *p_fmt,
                              const char *psz_format, vlc_tick_t i_timeout );

/* */
VLC_API void vout_PutPicture( vout_thread_t *, picture_t * );

/* Subpictures channels ID */
#define VOUT_SPU_CHANNEL_INVALID      (-1) /* Always fails in comparison */
#define VOUT_SPU_CHANNEL_OSD            0 /* OSD channel is automatically cleared */
#define VOUT_SPU_CHANNEL_OSD_HSLIDER    1
#define VOUT_SPU_CHANNEL_OSD_VSLIDER    2
#define VOUT_SPU_CHANNEL_OSD_COUNT      3

/* */
VLC_API void vout_PutSubpicture( vout_thread_t *, subpicture_t * );
VLC_API ssize_t vout_RegisterSubpictureChannel( vout_thread_t * );
VLC_API void vout_UnregisterSubpictureChannel( vout_thread_t *, size_t );
VLC_API void vout_FlushSubpictureChannel( vout_thread_t *, size_t );
/**
 * This function will ensure that all ready/displayed pictures have at most
 * the provided date.
 */
VLC_API void vout_Flush( vout_thread_t *p_vout, vlc_tick_t i_date );

/**
 * Empty all the pending pictures in the vout
 */
#define vout_FlushAll( vout )  vout_Flush( vout, VLC_TICK_INVALID )

/**@}*/

#endif /* _VLC_VOUT_H */
