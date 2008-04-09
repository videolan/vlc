/*****************************************************************************
 * vlc_video.h: common video definitions
 *****************************************************************************
 * Copyright (C) 1999 - 2005 the VideoLAN team
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_VOUT_H_
#define _VLC_VOUT_H_ 1

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
    unsigned        i_refcount;                  /**< link reference counter */
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
    uint32_t i_rmask; int i_rrshift, i_lrshift;
    uint32_t i_gmask; int i_rgshift, i_lgshift;
    uint32_t i_bmask; int i_rbshift, i_lbshift;

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
    int             i_align;                  /**< alignment within a region */
    int             i_alpha;                               /**< transparency */

    char            *psz_text;       /**< text string comprising this region */
    char            *psz_html;       /**< HTML version of subtitle (NULL = use psz_text) */
    text_style_t    *p_style;        /**< a description of the text style formatting */

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
    vlc_bool_t      b_pausable;               /**< subpicture will be paused if
                                                            stream is paused */
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

    void ( *pf_pre_render ) ( video_format_t *, spu_t *, subpicture_t *, mtime_t );
    subpicture_region_t * ( *pf_update_regions ) ( video_format_t *, spu_t *,
                                                   subpicture_t *, mtime_t );

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
#define SUBPICTURE_ALIGN_MASK ( SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_RIGHT| \
                                SUBPICTURE_ALIGN_TOP |SUBPICTURE_ALIGN_BOTTOM )

/* Subpicture rendered flag - should only be used by vout_subpictures */
#define SUBPICTURE_RENDERED  0x10

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * Copy the source picture onto the destination picture.
 * \param p_this a vlc object
 * \param p_dst pointer to the destination picture.
 * \param p_src pointer to the source picture.
 */
#define vout_CopyPicture(a,b,c) __vout_CopyPicture(VLC_OBJECT(a),b,c)
VLC_EXPORT( void, __vout_CopyPicture, ( vlc_object_t *p_this, picture_t *p_dst, picture_t *p_src ) );

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
 * Chroma conversion function
 *
 * This is the prototype common to all conversion functions.
 * \param p_vout video output thread
 * \param p_source source picture
 * \param p_dest destination picture
 * Picture width and source dimensions must be multiples of 16.
 */
typedef void (vout_chroma_convert_t)( vout_thread_t *,
                                      picture_t *, picture_t * );

typedef struct vout_chroma_t
{
    /** conversion functions */
    vout_chroma_convert_t *pf_convert;

    /** Private module-dependant data */
    chroma_sys_t *      p_sys;                               /* private data */

    /** Plugin used and shortcuts to access its capabilities */
    module_t * p_module;

} vout_chroma_t;

/** Maximum numbers of video filters2 that can be attached to a vout */
#define MAX_VFILTERS 10

/**
 * Video output thread descriptor
 *
 * Any independant video output device, such as an X11 window or a GGI device,
 * is represented by a video output thread, and described using the following
 * structure.
 */
struct vout_thread_t
{
    VLC_COMMON_MEMBERS

    /** \name Thread properties and locks */
    /**@{*/
    vlc_mutex_t         picture_lock;                 /**< picture heap lock */
    vlc_mutex_t         subpicture_lock;           /**< subpicture heap lock */
    vlc_mutex_t         change_lock;                 /**< thread change lock */
    vlc_mutex_t         vfilter_lock;         /**< video filter2 change lock */
    vout_sys_t *        p_sys;                     /**< system output method */
    /**@}*/

    /** \name Current display properties */
    /**@{*/
    uint16_t            i_changes;          /**< changes made to the thread.
                                                      \see \ref vout_changes */
    float               f_gamma;                                  /**< gamma */
    vlc_bool_t          b_grayscale;         /**< color or grayscale display */
    vlc_bool_t          b_info;            /**< print additional information */
    vlc_bool_t          b_interface;                   /**< render interface */
    vlc_bool_t          b_scale;                  /**< allow picture scaling */
    vlc_bool_t          b_fullscreen;         /**< toogle fullscreen display */
    uint32_t            render_time;           /**< last picture render time */
    unsigned int        i_window_width;              /**< video window width */
    unsigned int        i_window_height;            /**< video window height */
    unsigned int        i_alignment;          /**< video alignment in window */
    unsigned int        i_par_num;           /**< monitor pixel aspect-ratio */
    unsigned int        i_par_den;           /**< monitor pixel aspect-ratio */

    intf_thread_t       *p_parent_intf;   /**< parent interface for embedded
                                                               vout (if any) */
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

    /** \name Statistics
     * These numbers are not supposed to be accurate, but are a
     * good indication of the thread status */
    /**@{*/
    count_t       c_fps_samples;                         /**< picture counts */
    mtime_t       p_fps_sample[VOUT_FPS_SAMPLES];     /**< FPS samples dates */
    /**@}*/

    /** \name Video heap and translation tables */
    /**@{*/
    int                 i_heap_size;                          /**< heap size */
    picture_heap_t      render;                       /**< rendered pictures */
    picture_heap_t      output;                          /**< direct buffers */
    vlc_bool_t          b_direct;            /**< rendered are like direct ? */
    vout_chroma_t       chroma;                      /**< translation tables */

    video_format_t      fmt_render;      /* render format (from the decoder) */
    video_format_t      fmt_in;            /* input (modified render) format */
    video_format_t      fmt_out;     /* output format (for the video output) */
    /**@}*/

    /* Picture heap */
    picture_t           p_picture[2*VOUT_MAX_PICTURES+1];      /**< pictures */

    /* Subpicture unit */
    spu_t            *p_spu;

    /* Statistics */
    count_t          c_loops;
    count_t          c_pictures, c_late_pictures;
    mtime_t          display_jitter;    /**< average deviation from the PTS */
    count_t          c_jitter_samples;  /**< number of samples used
                                           for the calculation of the
                                           jitter  */
    /** delay created by internal caching */
    int                 i_pts_delay;

    /* Filter chain */
    char *psz_filter_chain;
    vlc_bool_t b_filter_change;

    /* Video filter2 chain
     * these are handled like in transcode.c
     * XXX: we might need to merge the two chains (v1 and v2 filters) */
    char       *psz_vfilters[MAX_VFILTERS];
    config_chain_t *p_vfilters_cfg[MAX_VFILTERS];
    int         i_vfilters_cfg;

    filter_t   *pp_vfilters[MAX_VFILTERS];
    int         i_vfilters;

    vlc_bool_t b_vfilter_change;

    /* Misc */
    vlc_bool_t       b_snapshot;     /**< take one snapshot on the next loop */

    /* Video output configuration */
    config_chain_t *p_cfg;

    /* Show media title on videoutput */
    vlc_bool_t      b_title_show;
    mtime_t         i_title_timeout;
    int             i_title_position;
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
/** b_grayscale changed */
#define VOUT_GRAYSCALE_CHANGE   0x0002
/** b_interface changed */
#define VOUT_INTF_CHANGE        0x0004
/** b_scale changed */
#define VOUT_SCALE_CHANGE       0x0008
/** gamma changed */
#define VOUT_GAMMA_CHANGE       0x0010
/** b_cursor changed */
#define VOUT_CURSOR_CHANGE      0x0020
/** b_fullscreen changed */
#define VOUT_FULLSCREEN_CHANGE  0x0040
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

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define vout_Request(a,b,c) __vout_Request(VLC_OBJECT(a),b,c)
VLC_EXPORT( vout_thread_t *, __vout_Request,    ( vlc_object_t *, vout_thread_t *, video_format_t * ) );
#define vout_Create(a,b) __vout_Create(VLC_OBJECT(a),b)
VLC_EXPORT( vout_thread_t *, __vout_Create,       ( vlc_object_t *, video_format_t * ) );
VLC_EXPORT( void,            vout_Destroy,        ( vout_thread_t * ) );
VLC_EXPORT( int, vout_VarCallback, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );

VLC_EXPORT( int,             vout_ChromaCmp,      ( uint32_t, uint32_t ) );

VLC_EXPORT( picture_t *,     vout_CreatePicture,  ( vout_thread_t *, vlc_bool_t, vlc_bool_t, unsigned int ) );
VLC_EXPORT( void,            vout_InitFormat,     ( video_frame_format_t *, uint32_t, int, int, int ) );
VLC_EXPORT( void,            vout_DestroyPicture, ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_DisplayPicture, ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_DatePicture,    ( vout_thread_t *, picture_t *, mtime_t ) );
VLC_EXPORT( void,            vout_LinkPicture,    ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_UnlinkPicture,  ( vout_thread_t *, picture_t * ) );
VLC_EXPORT( void,            vout_PlacePicture,   ( vout_thread_t *, unsigned int, unsigned int, unsigned int *, unsigned int *, unsigned int *, unsigned int * ) );
picture_t *     vout_RenderPicture  ( vout_thread_t *, picture_t *,
                                                       subpicture_t * );

VLC_EXPORT( int, vout_vaControlDefault, ( vout_thread_t *, int, va_list ) );
VLC_EXPORT( void *, vout_RequestWindow, ( vout_thread_t *, int *, int *, unsigned int *, unsigned int * ) );
VLC_EXPORT( void,   vout_ReleaseWindow, ( vout_thread_t *, void * ) );
VLC_EXPORT( int, vout_ControlWindow, ( vout_thread_t *, void *, int, va_list ) );
void vout_IntfInit( vout_thread_t * );
VLC_EXPORT( int, vout_Snapshot, ( vout_thread_t *p_vout, picture_t *p_pic ) );
VLC_EXPORT( void, vout_EnableFilter, ( vout_thread_t *, char *,vlc_bool_t , vlc_bool_t  ) );


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
    VOUT_GET_SIZE,         /* arg1= unsigned int*, arg2= unsigned int*, res= */
    VOUT_SET_SIZE,         /* arg1= unsigned int, arg2= unsigned int, res= */
    VOUT_SET_STAY_ON_TOP,  /* arg1= vlc_bool_t       res=    */
    VOUT_REPARENT,
    VOUT_SNAPSHOT,
    VOUT_CLOSE,
    VOUT_SET_FOCUS,         /* arg1= vlc_bool_t       res=    */
    VOUT_SET_VIEWPORT,      /* arg1= view rect, arg2=clip rect, res= */
    VOUT_REDRAW_RECT,       /* arg1= area rect, res= */
};

typedef struct snapshot_t {
  char *p_data;  /* Data area */

  int i_width;       /* In pixels */
  int i_height;      /* In pixels */
  int i_datasize;    /* In bytes */
  mtime_t date;      /* Presentation time */
} snapshot_t;

/**@}*/

#endif /* _VLC_VIDEO_H */
