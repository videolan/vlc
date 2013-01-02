/*****************************************************************************
 * vlc_subpicture.h: subpicture definitions
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

#ifndef VLC_SUBPICTURE_H
#define VLC_SUBPICTURE_H 1

/**
 * \file
 * This file defines subpicture structures and functions in vlc
 */

#include <vlc_picture.h>
#include <vlc_text_style.h>

/**
 * \defgroup subpicture Video Subpictures
 * Subpictures are pictures that should be displayed on top of the video, like
 * subtitles and OSD
 * \ingroup video_output
 * @{
 */

/**
 * Video subtitle region spu core private
 */
typedef struct subpicture_region_private_t subpicture_region_private_t;

/**
 * Video subtitle region
 *
 * A subtitle region is defined by a picture (graphic) and its rendering
 * coordinates.
 * Subtitles contain a list of regions.
 */
struct subpicture_region_t
{
    video_format_t  fmt;                          /**< format of the picture */
    picture_t       *p_picture;          /**< picture comprising this region */

    int             i_x;                             /**< position of region */
    int             i_y;                             /**< position of region */
    int             i_align;                  /**< alignment within a region */
    int             i_alpha;                               /**< transparency */

    char            *psz_text;       /**< text string comprising this region */
    char            *psz_html;       /**< HTML version of subtitle (NULL = use psz_text) */
    text_style_t    *p_style;        /**< a description of the text style formatting */
    bool            b_renderbg;      /**< render black background under text */

    subpicture_region_t *p_next;                /**< next region in the list */
    subpicture_region_private_t *p_private;  /**< Private data for spu_t *only* */
};

/* Subpicture region position flags */
#define SUBPICTURE_ALIGN_LEFT 0x1
#define SUBPICTURE_ALIGN_RIGHT 0x2
#define SUBPICTURE_ALIGN_TOP 0x4
#define SUBPICTURE_ALIGN_BOTTOM 0x8
#define SUBPICTURE_ALIGN_LEAVETEXT 0x10 /**< Align the subpicture, but not the text inside */
#define SUBPICTURE_ALIGN_MASK ( SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_RIGHT| \
                                SUBPICTURE_ALIGN_TOP |SUBPICTURE_ALIGN_BOTTOM| \
                                SUBPICTURE_ALIGN_LEAVETEXT )
/**
 * This function will create a new subpicture region.
 *
 * You must use subpicture_region_Delete to destroy it.
 */
VLC_API subpicture_region_t * subpicture_region_New( const video_format_t *p_fmt );

/**
 * This function will destroy a subpicture region allocated by
 * subpicture_region_New.
 *
 * You may give it NULL.
 */
VLC_API void subpicture_region_Delete( subpicture_region_t *p_region );

/**
 * This function will destroy a list of subpicture regions allocated by
 * subpicture_region_New.
 *
 * Provided for convenience.
 */
VLC_API void subpicture_region_ChainDelete( subpicture_region_t *p_head );

/**
 *
 */
typedef struct subpicture_updater_sys_t subpicture_updater_sys_t;
typedef struct
{
    int  (*pf_validate)( subpicture_t *,
                         bool has_src_changed, const video_format_t *p_fmt_src,
                         bool has_dst_changed, const video_format_t *p_fmt_dst,
                         mtime_t);
    void (*pf_update)  ( subpicture_t *,
                         const video_format_t *p_fmt_src,
                         const video_format_t *p_fmt_dst,
                         mtime_t );
    void (*pf_destroy) ( subpicture_t * );
    subpicture_updater_sys_t *p_sys;
} subpicture_updater_t;

typedef struct subpicture_private_t subpicture_private_t;

/**
 * Video subtitle
 *
 * Any subtitle destined to be displayed by a video output thread should
 * be stored in this structure from it's creation to it's effective display.
 * Subtitle type and flags should only be modified by the output thread. Note
 * that an empty subtitle MUST have its flags set to 0.
 */
struct subpicture_t
{
    /** \name Channel ID */
    /**@{*/
    int             i_channel;                    /**< subpicture channel ID */
    /**@}*/

    /** \name Type and flags
       Should NOT be modified except by the vout thread */
    /**@{*/
    int64_t         i_order;                 /** an increasing unique number */
    subpicture_t *  p_next;               /**< next subtitle to be displayed */
    /**@}*/

    subpicture_region_t *p_region;  /**< region list composing this subtitle */

    /** \name Date properties */
    /**@{*/
    mtime_t         i_start;                  /**< beginning of display date */
    mtime_t         i_stop;                         /**< end of display date */
    bool            b_ephemer;    /**< If this flag is set to true the subtitle
                                will be displayed untill the next one appear */
    bool            b_fade;                               /**< enable fading */
    /**@}*/

    /** \name Display properties
     * These properties are only indicative and may be
     * changed by the video output thread, or simply ignored depending of the
     * subtitle type. */
    /**@{*/
    bool         b_subtitle;            /**< the picture is a movie subtitle */
    bool         b_absolute;                       /**< position is absolute */
    int          i_original_picture_width;  /**< original width of the movie */
    int          i_original_picture_height;/**< original height of the movie */
    int          i_alpha;                                  /**< transparency */
     /**@}*/

    subpicture_updater_t updater;

    subpicture_private_t *p_private;    /* Reserved to the core */
};

/**
 * This function create a new empty subpicture.
 *
 * You must use subpicture_Delete to destroy it.
 */
VLC_API subpicture_t * subpicture_New( const subpicture_updater_t * );

/**
 * This function delete a subpicture created by subpicture_New.
 * You may give it NULL.
 */
VLC_API void subpicture_Delete( subpicture_t *p_subpic );

/**
 * This function will create a subpicture having one region in the requested
 * chroma showing the given picture.
 *
 * The picture_t given is not released nor used inside the
 * returned subpicture_t.
 */
VLC_API subpicture_t * subpicture_NewFromPicture( vlc_object_t *, picture_t *, vlc_fourcc_t i_chroma );

/**
 * This function will update the content of a subpicture created with
 * a non NULL subpicture_updater_t.
 */
VLC_API void subpicture_Update( subpicture_t *, const video_format_t *src, const video_format_t *, mtime_t );

/**@}*/

#endif /* _VLC_VIDEO_H */
