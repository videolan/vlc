/*****************************************************************************
 * vlc_subpicture.h: subpicture definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2009 VLC authors and VideoLAN
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
 */

#include <vlc_picture.h>
#include <vlc_text_style.h>
#include <vlc_list.h>
#include <vlc_vector.h>
#include <vlc_vout_display.h>

/**
 * \defgroup subpicture Video sub-pictures
 * \ingroup video_output
 * Subpictures are pictures that should be displayed on top of the video, like
 * subtitles and OSD
 * @{
 * \file
 * Subpictures functions
 */

/**
 * Video subtitle region spu core private
 */
typedef struct subpicture_region_private_t subpicture_region_private_t;
typedef struct vlc_spu_highlight_t vlc_spu_highlight_t;
typedef struct filter_t vlc_blender_t;

/**< render background under text only */
#define VLC_SUBPIC_TEXT_FLAG_NO_REGION_BG      (1 << 4)
/** if the decoder sends row/cols based output */
#define VLC_SUBPIC_TEXT_FLAG_GRID_MODE         (1 << 5)
/** don't try to balance wrapped text lines */
#define VLC_SUBPIC_TEXT_FLAG_TEXT_NOT_BALANCED (1 << 6)
/** mark the subpicture region as a text flag */
#define VLC_SUBPIC_TEXT_FLAG_IS_TEXT           (1 << 7)

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

    bool            b_absolute;       /**< position is absolute in the movie */
    int             i_x;      /**< position of region, relative to alignment */
    int             i_y;      /**< position of region, relative to alignment */
    int             i_align;                  /**< alignment flags of region */
    int             i_alpha;                               /**< transparency */

    /* Parameters for text regions (p_picture to be rendered) */
    text_segment_t  *p_text;         /**< subtitle text, made of a list of segments */
    int             text_flags;      /**< VLC_SUBPIC_TEXT_FLAG_xxx and SUBPICTURE_ALIGN_xxx */
    int             i_max_width;     /** horizontal rendering/cropping target/limit */
    int             i_max_height;    /** vertical rendering/cropping target/limit */

    struct vlc_list node;             /**< for inclusion in a vlc_spu_regions */
    subpicture_region_private_t *p_private;  /**< Private data for spu_t *only* */
};

typedef struct vlc_list vlc_spu_regions;

#define vlc_spu_regions_init(p_rs) \
    vlc_list_init((p_rs))
#define vlc_spu_regions_push(p_rs,reg) \
    vlc_list_append(&(reg)->node, (p_rs))
#define vlc_spu_regions_foreach(reg,p_rs) \
    vlc_list_foreach(reg, (p_rs), node)
#define vlc_spu_regions_foreach_const(reg,p_rs) \
    vlc_list_foreach_const(reg, (p_rs), node)
#define vlc_spu_regions_is_empty(p_rs) \
    vlc_list_is_empty((p_rs))
#define vlc_spu_regions_first_or_null(p_rs) \
    vlc_list_first_entry_or_null((p_rs), subpicture_region_t, node)
#define vlc_spu_regions_remove(p_rs, reg) \
    vlc_list_remove(&(reg)->node)

struct vlc_spu_highlight_t
{
    int x_start;
    int x_end;
    int y_start;
    int y_end;
    video_palette_t palette;
};

/* Subpicture region position flags */
#define SUBPICTURE_ALIGN_LEFT       0x1
#define SUBPICTURE_ALIGN_RIGHT      0x2
#define SUBPICTURE_ALIGN_TOP        0x4
#define SUBPICTURE_ALIGN_BOTTOM     0x8
#define SUBPICTURE_ALIGN_MASK ( SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_RIGHT| \
                                SUBPICTURE_ALIGN_TOP |SUBPICTURE_ALIGN_BOTTOM )
/**
 * This function will create a new subpicture region.
 *
 * You must use subpicture_region_Delete to destroy it.
 *
 * \note use subpicture_region_NewText() to create a text region
 */
VLC_API subpicture_region_t * subpicture_region_New( const video_format_t *p_fmt );

/**
 * This function will create a new text subpicture region.
 *
 * You must use subpicture_region_Delete to destroy it.
 */
VLC_API subpicture_region_t * subpicture_region_NewText( void );

/**
 * Create a subpicture region containing the picture.
 *
 * A reference will be added to the picture on success.
 *
 * You must use subpicture_region_Delete to destroy it.
 *
 * The chroma of the format must match the one of the picture.
 * The dimensions of the format should not exceed the ones of the picture. This
 * is not checked explicitly in the function.
 *
 * \param p_fmt format for the subpicture cropping/SAR (may be NULL)
 *
 * \note if p_fmt is NULL, the format of the picture will be used.
 */
VLC_API subpicture_region_t * subpicture_region_ForPicture( const video_format_t *p_fmt, picture_t *pic );

/**
 * This function will destroy a subpicture region allocated by
 * subpicture_region_New.
 *
 * You may give it NULL.
 */
VLC_API void subpicture_region_Delete( subpicture_region_t *p_region );

/**
 * This function will clear a list of subpicture regions allocated by
 * subpicture_region_New.
 *
 * Provided for convenience.
 */
VLC_API void vlc_spu_regions_Clear( vlc_spu_regions * );

/**
 * Subpicture updater operation virtual table.
 *
 * This structure gathers the operations that are implemented by a
 * subpicture_updater_t instance. */
struct vlc_spu_updater_ops
{
    /** Mandatory callback called after pf_validate and doing
      * the main job of creating the subpicture regions for the
      * current video_format */
    void (*update)(subpicture_t *,
                   const video_format_t *prev_src, const video_format_t *p_fmt_src,
                   const video_format_t *prev_dst, const video_format_t *p_fmt_dst,
                   vlc_tick_t);

    /** Optional callback for subpicture private data cleanup */
    void (*destroy)(subpicture_t *);
};

/**
 * Tells if the region is a text-based region.
 */
#define subpicture_region_IsText(r)  \
    (((r)->text_flags & VLC_SUBPIC_TEXT_FLAG_IS_TEXT) != 0)

/**
 *
 */
typedef struct
{
    void *sys;
    const struct vlc_spu_updater_ops *ops;
} subpicture_updater_t;

typedef struct subpicture_private_t subpicture_private_t;

struct subpicture_region_rendered
{
    picture_t       *p_picture;          /**< picture comprising this region */
    vout_display_place_t place;     /**< visible area in display coordinates */
    int             i_alpha;                               /**< transparency */
};

struct vlc_render_subpicture
{
    struct VLC_VECTOR(struct subpicture_region_rendered *) regions; /**< list of regions to render */
    int64_t      i_order;                    /** an increasing unique number */
};

typedef struct vlc_render_subpicture vlc_render_subpicture;

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
    ssize_t         i_channel;                    /**< subpicture channel ID */
    /**@}*/

    /** \name Type and flags
       Should NOT be modified except by the vout thread */
    /**@{*/
    int64_t         i_order;                 /** an increasing unique number */
    subpicture_t *  p_next;               /**< next subtitle to be displayed */
    /**@}*/

    vlc_spu_regions regions;        /**< region list composing this subtitle */

    /** \name Date properties */
    /**@{*/
    vlc_tick_t      i_start;                  /**< beginning of display date */
    vlc_tick_t      i_stop;                   /**< end of display date. Will be
                                      considered invalid if set to TICK_INVALID
                                      or less than i_start. See b_ephemer */
    bool            b_ephemer;    /**< If this flag is set to true the subtitle
                                   will be displayed until the next one appears
                                   or if i_stop is reached when it is valid */
    bool            b_fade;                               /**< enable fading */
    bool            b_subtitle;      /**< subtitle with timestamps relative to
                                                                  the video */
    /**@}*/

    /** \name Display properties
     * These properties are only indicative and may be
     * changed by the video output thread, or simply ignored depending of the
     * subtitle type. */
    /**@{*/
    unsigned     i_original_picture_width;  /**< original width of the movie */
    unsigned     i_original_picture_height;/**< original height of the movie */
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
VLC_API void subpicture_Update( subpicture_t *, const video_format_t *src, const video_format_t *, vlc_tick_t );

/**
 * This function will blend a given subpicture onto a picture.
 *
 * The subpicture and all its region must:
 *  - be absolute.
 *  - not be ephemere.
 *  - not have the fade flag.
 *  - contains only picture (no text rendering).
 * \return the number of region(s) successfully blent
 */
VLC_API unsigned picture_BlendSubpicture( picture_t *, vlc_blender_t *, vlc_render_subpicture * );

/**
 * Create a vlc_render_subpicture.
 *
 * It should be released with \ref vlc_render_subpicture_Delete.
 */
VLC_API vlc_render_subpicture *vlc_render_subpicture_New( void );

/**
 * Destroy a vlc_render_subpicture.
 */
VLC_API void vlc_render_subpicture_Delete(vlc_render_subpicture *);

/**@}*/

#endif /* _VLC_SUBPICTURE_H */
