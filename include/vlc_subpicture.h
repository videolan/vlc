/*****************************************************************************
 * vlc_subpicture.h: subpicture definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2009 the VideoLAN team
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

#ifndef VLC_SUBPICTURE_H
#define VLC_SUBPICTURE_H 1

/**
 * \file
 * This file defines subpicture structures and functions in vlc
 */

#include <vlc_picture.h>

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

    subpicture_region_t *p_next;                /**< next region in the list */
    subpicture_region_private_t *p_private;  /**< Private data for spu_t *only* */
};

/* Subpicture region position flags */
#define SUBPICTURE_ALIGN_LEFT 0x1
#define SUBPICTURE_ALIGN_RIGHT 0x2
#define SUBPICTURE_ALIGN_TOP 0x4
#define SUBPICTURE_ALIGN_BOTTOM 0x8
#define SUBPICTURE_ALIGN_MASK ( SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_RIGHT| \
                                SUBPICTURE_ALIGN_TOP |SUBPICTURE_ALIGN_BOTTOM )

/**
 * This function will create a new subpicture region.
 *
 * You must use subpicture_region_Delete to destroy it.
 */
VLC_EXPORT( subpicture_region_t *, subpicture_region_New, ( const video_format_t *p_fmt ) );

/**
 * This function will destroy a subpicture region allocated by
 * subpicture_region_New.
 *
 * You may give it NULL.
 */
VLC_EXPORT( void, subpicture_region_Delete, ( subpicture_region_t *p_region ) );

/**
 * This function will destroy a list of subpicture regions allocated by
 * subpicture_region_New.
 *
 * Provided for convenience.
 */
VLC_EXPORT( void, subpicture_region_ChainDelete, ( subpicture_region_t *p_head ) );

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

    /** Pointer to function that cleans up the private data of this subtitle */
    void ( *pf_destroy ) ( subpicture_t * );

    /** Pointer to function that update the regions before rendering (optionnal) */
    void (*pf_update_regions)( spu_t *,
                               subpicture_t *, const video_format_t *, mtime_t );

    /** Private data - the subtitle plugin might want to put stuff here to
     * keep track of the subpicture */
    subpicture_sys_t *p_sys;                              /* subpicture data */
};


/**
 * This function create a new empty subpicture.
 *
 * You must use subpicture_Delete to destroy it.
 */
VLC_EXPORT( subpicture_t *, subpicture_New, ( void ) );

/**
 * This function delete a subpicture created by subpicture_New.
 * You may give it NULL.
 */
VLC_EXPORT( void,  subpicture_Delete, ( subpicture_t *p_subpic ) );

/**
 * This function will create a subpicture having one region in the requested
 * chroma showing the given picture.
 *
 * The picture_t given is not released nor used inside the
 * returned subpicture_t.
 */
VLC_EXPORT( subpicture_t *, subpicture_NewFromPicture, ( vlc_object_t *, picture_t *, vlc_fourcc_t i_chroma ) );

/**@}*/

#endif /* _VLC_VIDEO_H */
