/*****************************************************************************
 * vlc_picture.h: picture definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2009 VLC authors and VideoLAN
 * $Id$
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

#ifndef VLC_PICTURE_H
#define VLC_PICTURE_H 1

/**
 * \file
 * This file defines picture structures and functions in vlc
 */

#include <vlc_es.h>

/** Description of a planar graphic field */
typedef struct plane_t
{
    uint8_t *p_pixels;                        /**< Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_lines;           /**< Number of lines, including margins */
    int i_pitch;           /**< Number of bytes in a line, including margins */

    /** Size of a macropixel, defaults to 1 */
    int i_pixel_pitch;

    /* Variables used for pictures with margins */
    int i_visible_lines;            /**< How many visible lines are there ? */
    int i_visible_pitch;            /**< How many visible pixels are there ? */

} plane_t;

/**
 * Maximum number of plane for a picture
 */
#define PICTURE_PLANE_MAX (VOUT_MAX_PLANES)

typedef struct picture_context_t
{
    void (*destroy)(struct picture_context_t *);
    struct picture_context_t *(*copy)(struct picture_context_t *);
} picture_context_t;

/**
 * Video picture
 */
struct picture_t
{
    /**
     * The properties of the picture
     */
    video_frame_format_t format;

    plane_t         p[PICTURE_PLANE_MAX];     /**< description of the planes */
    int             i_planes;                /**< number of allocated planes */

    /** \name Picture management properties
     * These properties can be modified using the video output thread API,
     * but should never be written directly */
    /**@{*/
    vlc_tick_t      date;                                  /**< display date */
    bool            b_force;
    /**@}*/

    /** \name Picture dynamic properties
     * Those properties can be changed by the decoder
     * @{
     */
    bool            b_progressive;          /**< is it a progressive frame ? */
    bool            b_top_field_first;             /**< which field is first */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    picture_context_t *context;      /**< video format-specific data pointer */
    /**@}*/

    /** Private data - the video output plugin might want to put stuff here to
     * keep track of the picture */
    picture_sys_t * p_sys;

    /** Next picture in a FIFO a pictures */
    struct picture_t *p_next;
};

/**
 * This function will create a new picture.
 * The picture created will implement a default release management compatible
 * with picture_Hold and picture_Release. This default management will release
 * p_sys, gc.p_sys fields if non NULL.
 */
VLC_API picture_t * picture_New( vlc_fourcc_t i_chroma, int i_width, int i_height, int i_sar_num, int i_sar_den ) VLC_USED;

/**
 * This function will create a new picture using the given format.
 *
 * When possible, it is preferred to use this function over picture_New
 * as more information about the format is kept.
 */
VLC_API picture_t * picture_NewFromFormat( const video_format_t *p_fmt ) VLC_USED;

/**
 * Resource for a picture.
 */
typedef struct
{
    picture_sys_t *p_sys;
    void (*pf_destroy)(picture_t *);

    /* Plane resources
     * XXX all fields MUST be set to the right value.
     */
    struct
    {
        uint8_t *p_pixels;  /**< Start of the plane's data */
        int i_lines;        /**< Number of lines, including margins */
        int i_pitch;        /**< Number of bytes in a line, including margins */
    } p[PICTURE_PLANE_MAX];

} picture_resource_t;

/**
 * This function will create a new picture using the provided resource.
 *
 * If the resource is NULL then a plain picture_NewFromFormat is returned.
 */
VLC_API picture_t * picture_NewFromResource( const video_format_t *, const picture_resource_t * ) VLC_USED;

/**
 * This function will increase the picture reference count.
 * It will not have any effect on picture obtained from vout
 *
 * It returns the given picture for convenience.
 */
VLC_API picture_t *picture_Hold( picture_t *p_picture );

/**
 * This function will release a picture.
 * It will not have any effect on picture obtained from vout
 */
VLC_API void picture_Release( picture_t *p_picture );

/**
 * This function will copy all picture dynamic properties.
 */
VLC_API void picture_CopyProperties( picture_t *p_dst, const picture_t *p_src );

/**
 * This function will reset a picture information (properties and quantizers).
 * It is sometimes useful for reusing pictures (like from a pool).
 */
VLC_API void picture_Reset( picture_t * );

/**
 * This function will copy the picture pixels.
 * You can safely copy between pictures that do not have the same size,
 * only the compatible(smaller) part will be copied.
 */
VLC_API void picture_CopyPixels( picture_t *p_dst, const picture_t *p_src );
VLC_API void plane_CopyPixels( plane_t *p_dst, const plane_t *p_src );

/**
 * This function will copy both picture dynamic properties and pixels.
 * You have to notice that sometime a simple picture_Hold may do what
 * you want without the copy overhead.
 * Provided for convenience.
 *
 * \param p_dst pointer to the destination picture.
 * \param p_src pointer to the source picture.
 */
VLC_API void picture_Copy( picture_t *p_dst, const picture_t *p_src );

/**
 * Perform a shallow picture copy
 *
 * This function makes a shallow copy of an existing picture. The same planes
 * and resources will be used, and the cloned picture reference count will be
 * incremented.
 *
 * \return A clone picture on success, NULL on error.
 */
VLC_API picture_t *picture_Clone(picture_t *pic);

/**
 * This function will export a picture to an encoded bitstream.
 *
 * pp_image will contain the encoded bitstream in psz_format format.
 *
 * p_fmt can be NULL otherwise it will be set with the format used for the
 * picture before encoding.
 *
 * i_override_width/height allow to override the width and/or the height of the
 * picture to be encoded:
 *  - if strictly lower than 0, the original dimension will be used.
 *  - if equal to 0, it will be deduced from the other dimension which must be
 *  different to 0.
 *  - if strictly higher than 0, it will override the dimension.
 * If at most one of them is > 0 then the picture aspect ratio will be kept.
 */
VLC_API int picture_Export( vlc_object_t *p_obj, block_t **pp_image, video_format_t *p_fmt, picture_t *p_picture, vlc_fourcc_t i_format, int i_override_width, int i_override_height );

/**
 * This function will setup all fields of a picture_t without allocating any
 * memory.
 * XXX The memory must already be initialized.
 * It does not need to be released.
 *
 * It will return VLC_EGENERIC if the core does not understand the requested
 * format.
 *
 * It can be useful to get the properties of planes.
 */
VLC_API int picture_Setup( picture_t *, const video_format_t * );


/*****************************************************************************
 * Shortcuts to access image components
 *****************************************************************************/

/* Plane indices */
enum
{
    Y_PLANE = 0,
    U_PLANE = 1,
    V_PLANE = 2,
    A_PLANE = 3,
};

/* Shortcuts */
#define Y_PIXELS     p[Y_PLANE].p_pixels
#define Y_PITCH      p[Y_PLANE].i_pitch
#define U_PIXELS     p[U_PLANE].p_pixels
#define U_PITCH      p[U_PLANE].i_pitch
#define V_PIXELS     p[V_PLANE].p_pixels
#define V_PITCH      p[V_PLANE].i_pitch
#define A_PIXELS     p[A_PLANE].p_pixels
#define A_PITCH      p[A_PLANE].i_pitch

/**@}*/

#endif /* VLC_PICTURE_H */
