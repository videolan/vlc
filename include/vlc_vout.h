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

#include <vlc_es.h>
#include <vlc_filter.h>

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
    bool            b_slow;                 /**< is picture in slow memory ? */
    /**@}*/

    /** \name Picture management properties
     * These properties can be modified using the video output thread API,
     * but should never be written directly */
    /**@{*/
    unsigned        i_refcount;                  /**< link reference counter */
    mtime_t         date;                                  /**< display date */
    bool            b_force;
    /**@}*/

    /** \name Picture dynamic properties
     * Those properties can be changed by the decoder
     * @{
     */
    bool            b_progressive;          /**< is it a progressive frame ? */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    bool            b_top_field_first;             /**< which field is first */
    uint8_t        *p_q;                           /**< quantification table */
    int             i_qstride;                    /**< quantification stride */
    int             i_qtype;                       /**< quantification style */
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
 * This function will create a new picture.
 * The picture created will implement a default release management compatible
 * with picture_Hold and picture_Release. This default management will release
 * picture_sys_t *p_sys field if non NULL.
 */
VLC_EXPORT( picture_t *, picture_New, ( vlc_fourcc_t i_chroma, int i_width, int i_height, int i_aspect ) );

/**
 * This function will force the destruction a picture.
 * The value of the picture reference count should be 0 before entering this
 * function.
 * Unless used for reimplementing pf_release, you should not use this
 * function but picture_Release.
 */
VLC_EXPORT( void, picture_Delete, ( picture_t * ) );

/**
 * This function will increase the picture reference count.
 * It will not have any effect on picture obtained from vout
 */
static inline void picture_Hold( picture_t *p_picture )
{
    if( p_picture->pf_release )
        p_picture->i_refcount++;
}
/**
 * This function will release a picture.
 * It will not have any effect on picture obtained from vout
 */
static inline void picture_Release( picture_t *p_picture )
{
    /* FIXME why do we let pf_release handle the i_refcount ? */
    if( p_picture->pf_release )
        p_picture->pf_release( p_picture );
}

/**
 * Cleanup quantization matrix data and set to 0
 */
static inline void picture_CleanupQuant( picture_t *p_pic )
{
    free( p_pic->p_q );
    p_pic->p_q = NULL;
    p_pic->i_qstride = 0;
    p_pic->i_qtype = 0;
}

/**
 * This function will copy all picture dynamic properties.
 */
static inline void picture_CopyProperties( picture_t *p_dst, const picture_t *p_src )
{
    p_dst->date = p_src->date;
    p_dst->b_force = p_src->b_force;

    p_dst->b_progressive = p_src->b_progressive;
    p_dst->i_nb_fields = p_src->i_nb_fields;
    p_dst->b_top_field_first = p_src->b_top_field_first;

    /* FIXME: copy ->p_q and ->p_qstride */
}

/**
 * This function will copy the picture pixels.
 * You can safely copy between pictures that do not have the same size,
 * only the compatible(smaller) part will be copied.
 */
VLC_EXPORT( void, picture_CopyPixels, ( picture_t *p_dst, const picture_t *p_src ) );
VLC_EXPORT( void, plane_CopyPixels, ( plane_t *p_dst, const plane_t *p_src ) );

/**
 * This function will copy both picture dynamic properties and pixels.
 * You have to notice that sometime a simple picture_Hold may do what
 * you want without the copy overhead.
 * Provided for convenience.
 *
 * \param p_dst pointer to the destination picture.
 * \param p_src pointer to the source picture.
 */
static inline void picture_Copy( picture_t *p_dst, const picture_t *p_src )
{
    picture_CopyPixels( p_dst, p_src );
    picture_CopyProperties( p_dst, p_src );
}

/**
 * This function will export a picture to an encoded bitstream.
 *
 * pp_image will contain the encoded bitstream in psz_format format.
 *
 * p_fmt can be NULL otherwise it will be set with the format used for the
 * picture before encoding.
 *
 * i_override_width/height allow to override the width and/or the height of the
 * picture to be encoded. If at most one of them is > 0 then the picture aspect
 * ratio will be kept.
 */
VLC_EXPORT( int, picture_Export, ( vlc_object_t *p_obj, block_t **pp_image, video_format_t *p_fmt, picture_t *p_picture, vlc_fourcc_t i_format, int i_override_width, int i_override_height ) );


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
    bool            b_allow_modify_pics;

    /* Stuff used for truecolor RGB planes */
    uint32_t i_rmask; int i_rrshift, i_lrshift;
    uint32_t i_gmask; int i_rgshift, i_lgshift;
    uint32_t i_bmask; int i_rbshift, i_lbshift;

    /** Stuff used for palettized RGB planes */
    void (* pf_setpalette) ( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );
};

/*****************************************************************************
 * Flags used to describe the status of a picture
 *****************************************************************************/

/* Picture type
 * FIXME are the values meaningfull ? */
enum
{
    EMPTY_PICTURE = 0,                             /* empty buffer */
    MEMORY_PICTURE = 100,                 /* heap-allocated buffer */
    DIRECT_PICTURE = 200,                         /* direct buffer */
};

/* Picture status */
enum
{
    FREE_PICTURE,                              /* free and not allocated */
    RESERVED_PICTURE,                          /* allocated and reserved */
    READY_PICTURE,                                  /* ready for display */
    DISPLAYED_PICTURE,                   /* been displayed but is linked */
    DESTROYED_PICTURE,                     /* allocated but no more used */
};

/* Quantification type */
enum
{
    QTYPE_NONE,

    QTYPE_MPEG1,
    QTYPE_MPEG2,
    QTYPE_H264,
};

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

    /** \name Date properties */
    /**@{*/
    mtime_t         i_start;                  /**< beginning of display date */
    mtime_t         i_stop;                         /**< end of display date */
    bool            b_ephemer;    /**< If this flag is set to true the subtitle
                                will be displayed untill the next one appear */
    bool            b_fade;                               /**< enable fading */
    /**@}*/

    subpicture_region_t *p_region;  /**< region list composing this subtitle */

    /** \name Display properties
     * These properties are only indicative and may be
     * changed by the video output thread, or simply ignored depending of the
     * subtitle type. */
    /**@{*/
    int          i_original_picture_width;  /**< original width of the movie */
    int          i_original_picture_height;/**< original height of the movie */
    bool         b_subtitle;            /**< the picture is a movie subtitle */
    bool         b_absolute;                       /**< position is absolute */
    int          i_alpha;                                  /**< transparency */
     /**@}*/

    /** Pointer to function that renders this subtitle in a picture */
    void ( *pf_render )  ( vout_thread_t *, picture_t *, const subpicture_t * );
    /** Pointer to function that cleans up the private data of this subtitle */
    void ( *pf_destroy ) ( subpicture_t * );

    /** Pointer to functions for region management */
    void (*pf_pre_render)    ( spu_t *, subpicture_t *, const video_format_t * );
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

/* Default subpicture channel ID */
#define DEFAULT_CHAN           1

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
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

    /** \name Thread properties and locks */
    /**@{*/
    vlc_mutex_t         picture_lock;                 /**< picture heap lock */
    vlc_mutex_t         change_lock;                 /**< thread change lock */
    vout_sys_t *        p_sys;                     /**< system output method */
    /**@}*/

    /** \name Current display properties */
    /**@{*/
    uint16_t            i_changes;          /**< changes made to the thread.
                                                      \see \ref vout_changes */
    unsigned            b_fullscreen:1;       /**< toogle fullscreen display */
    unsigned            b_autoscale:1;      /**< auto scaling picture or not */
    unsigned            b_on_top:1; /**< stay always on top of other windows */
    int                 i_zoom;               /**< scaling factor if no auto */
    unsigned int        i_window_width;              /**< video window width */
    unsigned int        i_window_height;            /**< video window height */
    unsigned int        i_alignment;          /**< video alignment in window */

    /**@}*/

    /** \name Plugin used and shortcuts to access its capabilities */
    /**@{*/
    module_t *   p_module;
    int       ( *pf_init )       ( vout_thread_t * );
    void      ( *pf_end )        ( vout_thread_t * );
    int       ( *pf_manage )     ( vout_thread_t * );
    void      ( *pf_render )     ( vout_thread_t *, picture_t * );
    void      ( *pf_display )    ( vout_thread_t *, picture_t * );
    void      ( *pf_swap )       ( vout_thread_t * );         /* OpenGL only */
    int       ( *pf_lock )       ( vout_thread_t * );         /* OpenGL only */
    void      ( *pf_unlock )     ( vout_thread_t * );         /* OpenGL only */
    int       ( *pf_control )    ( vout_thread_t *, int, va_list );
    /**@}*/

    /** \name Video heap and translation tables */
    /**@{*/
    int                 i_heap_size;                          /**< heap size */
    picture_heap_t      render;                       /**< rendered pictures */
    picture_heap_t      output;                          /**< direct buffers */

    video_format_t      fmt_render;      /* render format (from the decoder) */
    video_format_t      fmt_in;            /* input (modified render) format */
    video_format_t      fmt_out;     /* output format (for the video output) */
    /**@}*/

    /* Picture heap */
    picture_t           p_picture[2*VOUT_MAX_PICTURES+1];      /**< pictures */

    /* Subpicture unit */
    spu_t          *p_spu;

    /* Video output configuration */
    config_chain_t *p_cfg;

    /* Private vout_thread data */
    vout_thread_sys_t *p;
};

#define I_OUTPUTPICTURES p_vout->output.i_pictures
#define PP_OUTPUTPICTURE p_vout->output.pp_picture
#define I_RENDERPICTURES p_vout->render.i_pictures
#define PP_RENDERPICTURE p_vout->render.pp_picture

/** \defgroup vout_changes Flags for changes
 * These flags are set in the vout_thread_t::i_changes field when another
 * thread changed a variable
 * @{
 */
/** b_info changed */
#define VOUT_INFO_CHANGE        0x0001
/** b_interface changed */
#define VOUT_INTF_CHANGE        0x0004
/** b_autoscale changed */
#define VOUT_SCALE_CHANGE       0x0008
/** b_on_top changed */
#define VOUT_ON_TOP_CHANGE	0x0010
/** b_cursor changed */
#define VOUT_CURSOR_CHANGE      0x0020
/** b_fullscreen changed */
#define VOUT_FULLSCREEN_CHANGE  0x0040
/** i_zoom changed */
#define VOUT_ZOOM_CHANGE        0x0080
/** size changed */
#define VOUT_SIZE_CHANGE        0x0200
/** depth changed */
#define VOUT_DEPTH_CHANGE       0x0400
/** change chroma tables */
#define VOUT_CHROMA_CHANGE      0x0800
/** cropping parameters changed */
#define VOUT_CROP_CHANGE        0x1000
/** aspect ratio changed */
#define VOUT_ASPECT_CHANGE      0x2000
/** change/recreate picture buffers */
#define VOUT_PICTURE_BUFFERS_CHANGE 0x4000
/**@}*/

/* Alignment flags */
#define VOUT_ALIGN_LEFT         0x0001
#define VOUT_ALIGN_RIGHT        0x0002
#define VOUT_ALIGN_HMASK        0x0003
#define VOUT_ALIGN_TOP          0x0004
#define VOUT_ALIGN_BOTTOM       0x0008
#define VOUT_ALIGN_VMASK        0x000C

#define MAX_JITTER_SAMPLES      20

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
#define vout_Request(a,b,c) __vout_Request(VLC_OBJECT(a),b,c)
VLC_EXPORT( vout_thread_t *, __vout_Request,    ( vlc_object_t *p_this, vout_thread_t *p_vout, video_format_t *p_fmt ) );

/**
 * This function will create a suitable vout for a given p_fmt. It will never
 * reuse an already existing unused vout.
 *
 * You have to call either vout_Close or vout_Request on the returned value
 * \param p_this a vlc object to which the returned vout will be attached
 * \param p_fmt the video format requested
 * \return a vout if the request is successfull, NULL otherwise
 */
#define vout_Create(a,b) __vout_Create(VLC_OBJECT(a),b)
VLC_EXPORT( vout_thread_t *, __vout_Create,       ( vlc_object_t *p_this, video_format_t *p_fmt ) );

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
VLC_EXPORT( int,             vout_ChromaCmp,      ( uint32_t, uint32_t ) );

VLC_EXPORT( picture_t *,     vout_CreatePicture,  ( vout_thread_t *, bool, bool, unsigned int ) );
VLC_EXPORT( void,            vout_InitFormat,     ( video_frame_format_t *, uint32_t, int, int, int ) );
VLC_EXPORT( void,            vout_DestroyPicture, ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_DisplayPicture, ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_LinkPicture,    ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_UnlinkPicture,  ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_PlacePicture,   ( const vout_thread_t *, unsigned int, unsigned int, unsigned int *, unsigned int *, unsigned int *, unsigned int * ) );

void vout_IntfInit( vout_thread_t * );
VLC_EXPORT( void, vout_EnableFilter, ( vout_thread_t *, char *,bool , bool  ) );


static inline int vout_vaControl( vout_thread_t *p_vout, int i_query,
                                  va_list args )
{
    if( p_vout->pf_control )
        return p_vout->pf_control( p_vout, i_query, args );
    else
        return VLC_EGENERIC;
}

static inline int vout_Control( vout_thread_t *p_vout, int i_query, ... )
{
    va_list args;
    int i_result;

    va_start( args, i_query );
    i_result = vout_vaControl( p_vout, i_query, args );
    va_end( args );
    return i_result;
}

enum output_query_e
{
    VOUT_SET_SIZE,         /* arg1= unsigned int, arg2= unsigned int, res= */
    VOUT_SET_STAY_ON_TOP,  /* arg1= bool       res=    */
    VOUT_SET_VIEWPORT,      /* arg1= view rect, arg2=clip rect, res= */
    VOUT_REDRAW_RECT,       /* arg1= area rect, res= */
};

/**@}*/

#endif /* _VLC_VIDEO_H */
