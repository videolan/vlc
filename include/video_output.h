/*****************************************************************************
 * video_output.h : video output thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

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
    vlc_bool_t          b_override_aspect;       /**< aspect ratio overriden */
    uint32_t            render_time;           /**< last picture render time */
    unsigned int        i_window_width;              /**< video window width */
    unsigned int        i_window_height;            /**< video window height */
    unsigned int        i_alignment;          /**< video alignment in window */

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
    /**@}*/

    /* Picture heap */
    picture_t           p_picture[2*VOUT_MAX_PICTURES+1];      /**< pictures */

    /* Subpicture properties */
    subpicture_t        p_subpicture[VOUT_MAX_PICTURES];    /**< subpictures */
    subpicture_t        *p_default_channel;   /**< subpicture in the default
                                                   channel */
    int                 i_channel_count;       /**< index of last subpicture
                                                    channel registered */

    filter_t *p_blend;                            /**< alpha blending module */
    filter_t *p_text;                              /**< text renderer module */

    vlc_bool_t b_force_crop;               /**< force cropping of subpicture */
    int i_crop_x, i_crop_y, i_crop_width, i_crop_height;       /**< cropping */

    vlc_bool_t b_force_alpha;         /**< force alpha palette of subpicture */
    uint8_t pi_alpha[4];                           /**< forced alpha palette */


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
/** change/recreate picture buffers */
#define VOUT_PICTURE_BUFFERS_CHANGE 0x1000
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
#define vout_Request(a,b,c,d,e,f) __vout_Request(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( vout_thread_t *, __vout_Request,      ( vlc_object_t *, vout_thread_t *, unsigned int, unsigned int, uint32_t, unsigned int ) );
#define vout_Create(a,b,c,d,e) __vout_Create(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( vout_thread_t *, __vout_Create,       ( vlc_object_t *, unsigned int, unsigned int, uint32_t, unsigned int ) );
VLC_EXPORT( void,            vout_Destroy,        ( vout_thread_t * ) );
VLC_EXPORT( int, vout_VarCallback, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );

VLC_EXPORT( int,             vout_ChromaCmp,      ( uint32_t, uint32_t ) );

VLC_EXPORT( picture_t *,     vout_CreatePicture,  ( vout_thread_t *, vlc_bool_t, vlc_bool_t, unsigned int ) );
VLC_EXPORT( void,            vout_InitFormat,     ( video_frame_format_t *, uint32_t, int, int, int ) );
VLC_EXPORT( void,            vout_InitPicture,    ( vlc_object_t *, picture_t *, uint32_t, int, int, int ) );
VLC_EXPORT( void,            vout_AllocatePicture,( vlc_object_t *, picture_t *, uint32_t, int, int, int ) );
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
    VOUT_SET_ZOOM,         /* arg1= double           res=    */
    VOUT_SET_STAY_ON_TOP,  /* arg1= vlc_bool_t       res=    */
    VOUT_REPARENT,
    VOUT_CLOSE
};

/**
 * \addtogroup subpicture
 * @{
 */
VLC_EXPORT( subpicture_t *,  vout_CreateSubPicture,   ( vout_thread_t *, int, int ) );
VLC_EXPORT( void,            vout_DestroySubPicture,  ( vout_thread_t *, subpicture_t * ) );
VLC_EXPORT( void,            vout_DisplaySubPicture,  ( vout_thread_t *, subpicture_t * ) );
VLC_EXPORT( int,             vout_RegisterOSDChannel, ( vout_thread_t * ) );
VLC_EXPORT( void,            vout_ClearOSDChannel,    ( vout_thread_t *, int ) );
#define spu_CreateRegion(a,b) __spu_CreateRegion(VLC_OBJECT(a),b)
VLC_EXPORT( subpicture_region_t *,__spu_CreateRegion, ( vlc_object_t *, video_format_t * ) );
#define spu_DestroyRegion(a,b) __spu_DestroyRegion(VLC_OBJECT(a),b)
VLC_EXPORT( void, __spu_DestroyRegion, ( vlc_object_t *, subpicture_region_t * ) );

void           vout_InitSPU( vout_thread_t * );
void           vout_DestroySPU( vout_thread_t * );
subpicture_t * vout_SortSubPictures  ( vout_thread_t *, mtime_t );
void           vout_RenderSubPictures( vout_thread_t *, picture_t *,
                                       subpicture_t * );
/** @}*/
/**
 * @}
 */
