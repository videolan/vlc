/*****************************************************************************
 * vlc_video.h: common video definitions
 * This header is required by all modules which have to handle pictures. It
 * includes all common video types and constants.
 *****************************************************************************
 * Copyright (C) 1999 - 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
#ifndef _VLC_VIDEO_H
#define _VLC_VIDEO_H 1

#include "vlc_es.h"

/**
 * Description of a planar graphic field
 */
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
 * Video picture
 *
 * Any picture destined to be displayed by a video output thread should be
 * stored in this structure from it's creation to it's effective display.
 * Picture type and flags should only be modified by the output thread. Note
 * that an empty picture MUST have its flags set to 0.
 */
struct picture_t
{
    /**
     * The properties of the picture
     */
    video_frame_format_t format;

    /** Picture data - data can always be freely modified, but p_data may
     * NEVER be modified. A direct buffer can be handled as the plugin
     * wishes, it can even swap p_pixels buffers. */
    uint8_t        *p_data;
    void           *p_data_orig;                /**< pointer before memalign */
    plane_t         p[ VOUT_MAX_PLANES ];     /**< description of the planes */
    int             i_planes;                /**< number of allocated planes */

    /** \name Type and flags
     * Should NOT be modified except by the vout thread
     * @{*/
    int             i_status;                             /**< picture flags */
    int             i_type;                /**< is picture a direct buffer ? */
    vlc_bool_t      b_slow;                 /**< is picture in slow memory ? */
    int             i_matrix_coefficients;   /**< in YUV type, encoding type */
    /**@}*/

    /** \name Picture management properties
     * These properties can be modified using the video output thread API,
     * but should never be written directly */
    /**@{*/
    int             i_refcount;                  /**< link reference counter */
    mtime_t         date;                                  /**< display date */
    vlc_bool_t      b_force;
    /**@}*/

    /** \name Picture dynamic properties
     * Those properties can be changed by the decoder
     * @{
     */
    vlc_bool_t      b_progressive;          /**< is it a progressive frame ? */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    vlc_bool_t      b_top_field_first;             /**< which field is first */
    /**@}*/

    /** The picture heap we are attached to */
    picture_heap_t* p_heap;

    /* Some vouts require the picture to be locked before it can be modified */
    int (* pf_lock) ( vout_thread_t *, picture_t * );
    int (* pf_unlock) ( vout_thread_t *, picture_t * );

    /** Private data - the video output plugin might want to put stuff here to
     * keep track of the picture */
    picture_sys_t * p_sys;

    /** This way the picture_Release can be overloaded */
    void (*pf_release)( picture_t * );

    /** Next picture in a FIFO a pictures */
    struct picture_t *p_next;
};

/**
 * Video picture heap, either render (to store pictures used
 * by the decoder) or output (to store pictures displayed by the vout plugin)
 */
struct picture_heap_t
{
    int i_pictures;                                   /**< current heap size */

    /* \name Picture static properties
     * Those properties are fixed at initialization and should NOT be modified
     * @{
     */
    unsigned int i_width;                                 /**< picture width */
    unsigned int i_height;                               /**< picture height */
    vlc_fourcc_t i_chroma;                               /**< picture chroma */
    unsigned int i_aspect;                                 /**< aspect ratio */
    /**@}*/

    /* Real pictures */
    picture_t*      pp_picture[VOUT_MAX_PICTURES];             /**< pictures */
    int             i_last_used_pic;              /**< last used pic in heap */
    vlc_bool_t      b_allow_modify_pics;

    /* Stuff used for truecolor RGB planes */
    int i_rmask, i_rrshift, i_lrshift;
    int i_gmask, i_rgshift, i_lgshift;
    int i_bmask, i_rbshift, i_lbshift;

    /** Stuff used for palettized RGB planes */
    void (* pf_setpalette) ( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );
};

/*****************************************************************************
 * Flags used to describe the status of a picture
 *****************************************************************************/

/* Picture type */
#define EMPTY_PICTURE           0                            /* empty buffer */
#define MEMORY_PICTURE          100                 /* heap-allocated buffer */
#define DIRECT_PICTURE          200                         /* direct buffer */

/* Picture status */
#define FREE_PICTURE            0                  /* free and not allocated */
#define RESERVED_PICTURE        1                  /* allocated and reserved */
#define RESERVED_DATED_PICTURE  2              /* waiting for DisplayPicture */
#define RESERVED_DISP_PICTURE   3               /* waiting for a DatePicture */
#define READY_PICTURE           4                       /* ready for display */
#define DISPLAYED_PICTURE       5            /* been displayed but is linked */
#define DESTROYED_PICTURE       6              /* allocated but no more used */

/*****************************************************************************
 * Shortcuts to access image components
 *****************************************************************************/

/* Plane indices */
#define Y_PLANE      0
#define U_PLANE      1
#define V_PLANE      2
#define A_PLANE      3

/* Shortcuts */
#define Y_PIXELS     p[Y_PLANE].p_pixels
#define Y_PITCH      p[Y_PLANE].i_pitch
#define U_PIXELS     p[U_PLANE].p_pixels
#define U_PITCH      p[U_PLANE].i_pitch
#define V_PIXELS     p[V_PLANE].p_pixels
#define V_PITCH      p[V_PLANE].i_pitch
#define A_PIXELS     p[A_PLANE].p_pixels
#define A_PITCH      p[A_PLANE].i_pitch

/**
 * \defgroup subpicture Video Subpictures
 * Subpictures are pictures that should be displayed on top of the video, like
 * subtitles and OSD
 * \ingroup video_output
 * @{
 */

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
    picture_t       picture;             /**< picture comprising this region */

    int             i_x;                             /**< position of region */
    int             i_y;                             /**< position of region */

    char            *psz_text;       /**< text string comprising this region */
    text_style_t    *p_style;  /* a description of the text style formatting */

    subpicture_region_t *p_next;                /**< next region in the list */
    subpicture_region_t *p_cache;       /**< modified version of this region */
};

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
    int             i_type;                                        /**< type */
    int             i_status;                                     /**< flags */
    subpicture_t *  p_next;               /**< next subtitle to be displayed */
    /**@}*/

    /** \name Date properties */
    /**@{*/
    mtime_t         i_start;                  /**< beginning of display date */
    mtime_t         i_stop;                         /**< end of display date */
    vlc_bool_t      b_ephemer;    /**< If this flag is set to true the subtitle
                                will be displayed untill the next one appear */
    vlc_bool_t      b_fade;                               /**< enable fading */
    /**@}*/

    subpicture_region_t *p_region;  /**< region list composing this subtitle */

    /** \name Display properties
     * These properties are only indicative and may be
     * changed by the video output thread, or simply ignored depending of the
     * subtitle type. */
    /**@{*/
    int          i_x;                    /**< offset from alignment position */
    int          i_y;                    /**< offset from alignment position */
    int          i_width;                                 /**< picture width */
    int          i_height;                               /**< picture height */
    int          i_alpha;                                  /**< transparency */
    int          i_original_picture_width;  /**< original width of the movie */
    int          i_original_picture_height;/**< original height of the movie */
    vlc_bool_t   b_absolute;                       /**< position is absolute */
    int          i_flags;                                /**< position flags */
     /**@}*/

    /** Pointer to function that renders this subtitle in a picture */
    void ( *pf_render )  ( vout_thread_t *, picture_t *, const subpicture_t * );
    /** Pointer to function that cleans up the private data of this subtitle */
    void ( *pf_destroy ) ( subpicture_t * );

    /** Pointer to functions for region management */
    subpicture_region_t * ( *pf_create_region ) ( vlc_object_t *,
                                                  video_format_t * );
    subpicture_region_t * ( *pf_make_region ) ( vlc_object_t *,
                                                video_format_t *, picture_t * );
    void ( *pf_destroy_region ) ( vlc_object_t *, subpicture_region_t * );

    /** Private data - the subtitle plugin might want to put stuff here to
     * keep track of the subpicture */
    subpicture_sys_t *p_sys;                              /* subpicture data */
};

/* Subpicture type */
#define EMPTY_SUBPICTURE       0     /* subtitle slot is empty and available */
#define MEMORY_SUBPICTURE      100            /* subpicture stored in memory */

/* Default subpicture channel ID */
#define DEFAULT_CHAN           1

/* Subpicture status */
#define FREE_SUBPICTURE        0                   /* free and not allocated */
#define RESERVED_SUBPICTURE    1                   /* allocated and reserved */
#define READY_SUBPICTURE       2                        /* ready for display */

/* Subpicture position flags */
#define SUBPICTURE_ALIGN_LEFT 0x1
#define SUBPICTURE_ALIGN_RIGHT 0x2
#define SUBPICTURE_ALIGN_TOP 0x4
#define SUBPICTURE_ALIGN_BOTTOM 0x8

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
/**
 * vout_CopyPicture
 *
 * Copy the source picture onto the destination picture.
 * \param p_this a vlc object
 * \param p_dst pointer to the destination picture.
 * \param p_src pointer to the source picture.
 */
#define vout_CopyPicture(a,b,c) __vout_CopyPicture(VLC_OBJECT(a),b,c)
VLC_EXPORT( void, __vout_CopyPicture, ( vlc_object_t *p_this, picture_t *p_dst, picture_t *p_src ) );

/**
 * vout_InitPicture
 *
 * Initialise different fields of a picture_t (but does not allocate memory).
 * \param p_this a vlc object
 * \param p_pic pointer to the picture structure.
 * \param i_chroma the wanted chroma for the picture.
 * \param i_width the wanted width for the picture.
 * \param i_height the wanted height for the picture.
 * \param i_aspect the wanted aspect ratio for the picture.
 */
#define vout_InitPicture(a,b,c,d,e,f) \
        __vout_InitPicture(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( int, __vout_InitPicture, ( vlc_object_t *p_this, picture_t *p_pic, uint32_t i_chroma, int i_width, int i_height, int i_aspect ) );

/**
 * vout_AllocatePicture
 *
 * Initialise different fields of a picture_t and allocates the picture buffer.
 * \param p_this a vlc object
 * \param p_pic pointer to the picture structure.
 * \param i_chroma the wanted chroma for the picture.
 * \param i_width the wanted width for the picture.
 * \param i_height the wanted height for the picture.
 * \param i_aspect the wanted aspect ratio for the picture.
 */
#define vout_AllocatePicture(a,b,c,d,e,f) \
        __vout_AllocatePicture(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( int, __vout_AllocatePicture,( vlc_object_t *p_this, picture_t *p_pic, uint32_t i_chroma, int i_width, int i_height, int i_aspect ) );

/**
 * vout_ShowTextRelative
 *
 * Show text on the video for some time
 * \param p_vout pointer to the vout the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_duration Amount of time the text is to be shown.
 */
VLC_EXPORT( int, vout_ShowTextRelative, ( vout_thread_t *, int, char *, text_style_t *, int, int, int, mtime_t ) );

/**
 * vout_ShowTextAbsolute
 *
 * Show text on the video from a given start date to a given end date
 * \param p_vout pointer to the vout the text is to be showed on
 * \param i_channel Subpicture channel
 * \param psz_string The text to be shown
 * \param p_style Pointer to a struct with text style info
 * \param i_flags flags for alignment and such
 * \param i_hmargin horizontal margin in pixels
 * \param i_vmargin vertical margin in pixels
 * \param i_start the time when this string is to appear on the video
 * \param i_stop the time when this string should stop to be displayed
 *               if this is 0 the string will be shown untill the next string
 *               is about to be shown
 */
VLC_EXPORT( int, vout_ShowTextAbsolute, ( vout_thread_t *, int, char *, text_style_t *, int, int, int, mtime_t, mtime_t ) );

/**
 * vout_OSDMessage
 *
 * Write an informative message at the default location,
 * for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param i_channel Subpicture channel
 * \param psz_format printf style formatting
 **/
VLC_EXPORT( void,  __vout_OSDMessage, ( vlc_object_t *, int, char *, ... ) );

/**
 * Same as __vlc_OSDMessage() but with automatic casting
 */
#if defined(HAVE_VARIADIC_MACROS)
#    define vout_OSDMessage( obj, chan, fmt, args...) __vout_OSDMessage( VLC_OBJECT(obj), chan, fmt, ## args )
#else
#    define vout_OSDMessage __vout_OSDMessage
#endif

/**
 * vout_OSDSlider
 *
 * Display a slider on the video output.
 * \param p_this    The object that called the function.
 * \param i_channel Subpicture channel
 * \param i_postion Current position in the slider
 * \param i_type    Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 * @see vlc_osd.h
 */
VLC_EXPORT( void, vout_OSDSlider, ( vlc_object_t *, int, int , short ) );

/**
 * vout_OSDIcon
 *
 * Display an Icon on the video output.
 * \param p_this    The object that called the function.
 * \param i_channel Subpicture channel
 * \param i_type    Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 * @see vlc_osd.h
 */
VLC_EXPORT( void, vout_OSDIcon, ( vlc_object_t *, int, short ) );

/**@}*/

#endif /* _VLC_VIDEO_H */
