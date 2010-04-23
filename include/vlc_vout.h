/*****************************************************************************
 * vlc_video.h: common video definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Olivier Aubert <oaubert 47 videolan d07 org>
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

#ifndef VLC_VOUT_H_
#define VLC_VOUT_H_ 1

/**
 * \file
 * This file defines common video output structures and functions in vlc
 */

#include <vlc_picture.h>
#include <vlc_filter.h>
#include <vlc_subpicture.h>

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * \defgroup video_output Video Output
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously opened video output thread.
 * @{
 */

/**
 * Video ouput thread private structure
 */
typedef struct vout_thread_sys_t vout_thread_sys_t;

/**
 * Video output thread descriptor
 *
 * Any independent video output device, such as an X11 window or a GGI device,
 * is represented by a video output thread, and described using the following
 * structure.
 */
struct vout_thread_t
{
    VLC_COMMON_MEMBERS

    video_format_t      fmt_render;      /* render format (from the decoder) */
    video_format_t      fmt_in;            /* input (modified render) format */
    video_format_t      fmt_out;     /* output format (for the video output) */

    /* Private vout_thread data */
    vout_thread_sys_t *p;
};

/* Alignment flags */
#define VOUT_ALIGN_LEFT         0x0001
#define VOUT_ALIGN_RIGHT        0x0002
#define VOUT_ALIGN_HMASK        0x0003
#define VOUT_ALIGN_TOP          0x0004
#define VOUT_ALIGN_BOTTOM       0x0008
#define VOUT_ALIGN_VMASK        0x000C

/* scaling factor (applied to i_zoom in vout_thread_t) */
#define ZOOM_FP_FACTOR          1000

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * This function will
 *  - returns a suitable vout (if requested by a non NULL p_fmt)
 *  - recycles an old vout (if given) by either destroying it or by saving it
 *  for latter usage.
 *
 * The purpose of this function is to avoid unnecessary creation/destruction of
 * vout (and to allow optional vout reusing).
 *
 * You can call vout_Request on a vout created by vout_Create or by a previous
 * call to vout_Request.
 * You can release the returned value either by vout_Request or vout_Close()
 * followed by a vlc_object_release() or shorter vout_CloseAndRelease()
 *
 * \param p_this a vlc object
 * \param p_vout a vout candidate
 * \param p_fmt the video format requested or NULL
 * \return a vout if p_fmt is non NULL and the request is successfull, NULL
 * otherwise
 */
VLC_EXPORT( vout_thread_t *, vout_Request, ( vlc_object_t *p_this, vout_thread_t *p_vout, video_format_t *p_fmt ) );
#define vout_Request(a,b,c) vout_Request(VLC_OBJECT(a),b,c)

/**
 * This function will create a suitable vout for a given p_fmt. It will never
 * reuse an already existing unused vout.
 *
 * You have to call either vout_Close or vout_Request on the returned value
 * \param p_this a vlc object to which the returned vout will be attached
 * \param p_fmt the video format requested
 * \return a vout if the request is successfull, NULL otherwise
 */
VLC_EXPORT( vout_thread_t *, vout_Create, ( vlc_object_t *p_this, video_format_t *p_fmt ) );
#define vout_Create(a,b) vout_Create(VLC_OBJECT(a),b)

/**
 * This function will close a vout created by vout_Create or vout_Request.
 * The associated vout module is closed.
 * Note: It is not released yet, you'll have to call vlc_object_release()
 * or use the convenient vout_CloseAndRelease().
 *
 * \param p_vout the vout to close
 */
VLC_EXPORT( void,            vout_Close,        ( vout_thread_t *p_vout ) );

/**
 * This function will close a vout created by vout_Create
 * and then release it.
 *
 * \param p_vout the vout to close and release
 */
static inline void vout_CloseAndRelease( vout_thread_t *p_vout )
{
    vout_Close( p_vout );
    vlc_object_release( p_vout );
}

/**
 * This function will handle a snapshot request.
 *
 * pp_image, pp_picture and p_fmt can be NULL otherwise they will be
 * set with returned value in case of success.
 *
 * pp_image will hold an encoded picture in psz_format format.
 *
 * i_timeout specifies the time the function will wait for a snapshot to be
 * available.
 *
 */
VLC_EXPORT( int, vout_GetSnapshot, ( vout_thread_t *p_vout,
                                     block_t **pp_image, picture_t **pp_picture,
                                     video_format_t *p_fmt,
                                     const char *psz_format, mtime_t i_timeout ) );

/* */
VLC_EXPORT( picture_t *,     vout_GetPicture,     ( vout_thread_t * ) );
VLC_EXPORT( void,            vout_PutPicture,     ( vout_thread_t *, picture_t * ) );

VLC_EXPORT( void,            vout_HoldPicture,    ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_ReleasePicture, ( vout_thread_t *, picture_t * ) );

/**
 * Return the spu_t object associated to a vout_thread_t.
 *
 * The return object is valid only as long as the vout is. You must not
 * release the spu_t object returned.
 * It cannot return NULL so no need to check.
 */
VLC_EXPORT( spu_t *, vout_GetSpu, ( vout_thread_t * ) );

VLC_EXPORT( void, vout_EnableFilter, ( vout_thread_t *, const char *,bool , bool  ) );

/**@}*/

#endif /* _VLC_VIDEO_H */
