/*****************************************************************************
 * video_output.h : video output thread
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppenned video output thread.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_output.h,v 1.64 2001/12/09 17:01:35 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * vout_bank_t, p_vout_bank (global variable)
 *****************************************************************************
 * This global variable is accessed by any function using the video output.
 *****************************************************************************/
typedef struct vout_bank_s
{
    /* Array to all the video outputs */
    struct vout_thread_s *pp_vout[ VOUT_MAX_THREADS ];

    int                   i_count;
    vlc_mutex_t           lock;                               /* Global lock */

} vout_bank_t;

extern vout_bank_t *p_vout_bank;

/*****************************************************************************
 * vout_chroma_t: Chroma conversion function
 *****************************************************************************
 * This is the prototype common to all conversion functions.
 * Parameters:
 *      p_source                        source picture
 *      p_dest                          destination picture
 * Picture width and source dimensions must be multiples of 16.
 *****************************************************************************/
typedef void (vout_chroma_convert_t)( picture_t *p_source, picture_t *p_dest );

typedef struct vout_chroma_s
{
    /* conversion functions */
    vout_chroma_convert_t *pf_convert;

    /* Private module-dependant data */
    p_chroma_sys_t      p_sys;                               /* private data */

    /* Plugin used and shortcuts to access its capabilities */
    struct module_s *   p_module;
    int  ( * pf_init )  ( struct vout_thread_s * );
    int  ( * pf_reset ) ( struct vout_thread_s * );
    void ( * pf_end )   ( struct vout_thread_s * );

} vout_chroma_t;

/*****************************************************************************
 * vout_buffer_t: rendering buffer
 *****************************************************************************
 * This structure stores information about a buffer. Buffers are not completely
 * cleared between displays, and modified areas need to be stored.
 *****************************************************************************/
typedef struct vout_buffer_s
{
    /* Picture area */
    int         i_pic_x, i_pic_y;                       /* picture position  */
    int         i_pic_width, i_pic_height;                   /* picture size */

    /* Other areas - only vertical extensions of areas are stored */
    int         i_areas;                                  /* number of areas */
    int         pi_area_begin[VOUT_MAX_AREAS];          /* beginning of area */
    int         pi_area_end[VOUT_MAX_AREAS];                  /* end of area */

    /* Picture data */
    byte_t *    p_data;                                    /* memory address */
} vout_buffer_t;

/*****************************************************************************
 * vout_fifo_t
 *****************************************************************************/
typedef struct vout_fifo_s
{
    /* See the fifo types below */
    int                 i_type;
    boolean_t           b_die;
    int                 i_fifo;      /* Just to keep track of the fifo index */

    vlc_mutex_t         data_lock;
    vlc_cond_t          data_wait;

} vout_fifo_t;

#define VOUT_EMPTY_FIFO         0
#define VOUT_YUV_FIFO           1
#define VOUT_SPU_FIFO           2

/*****************************************************************************
 * vout_thread_t: video output thread descriptor
 *****************************************************************************
 * Any independant video output device, such as an X11 window or a GGI device,
 * is represented by a video output thread, and described using the following
 * structure.
 *****************************************************************************/
typedef struct vout_thread_s
{
    /* Thread properties and lock */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_error;                             /* `error' flag */
    boolean_t           b_active;                           /* `active' flag */
    vlc_thread_t        thread_id;               /* id for pthread functions */
    vlc_mutex_t         picture_lock;                   /* picture heap lock */
    vlc_mutex_t         subpicture_lock;             /* subpicture heap lock */
    vlc_mutex_t         change_lock;                   /* thread change lock */
    int *               pi_status;                  /* temporary status flag */
    p_vout_sys_t        p_sys;                       /* system output method */
                                                                   
    /* Current input properties */
    int                 i_width;                      /* current input width */
    int                 i_height;                    /* current input height */
    int                 i_chroma;                    /* current input chroma */
    int                 i_aspect_ratio;        /* current input aspect ratio */

    /* Current display properties */
    u16                 i_changes;             /* changes made to the thread */
    float               f_gamma;                                    /* gamma */

    /* Color masks and shifts in RGB mode - masks are set by system
     * initialization, shifts are calculated. A pixel color value can be
     * obtained using the formula ((value >> rshift) << lshift) */
    u32                 i_red_mask;                              /* red mask */
    u32                 i_green_mask;                          /* green mask */
    u32                 i_blue_mask;                            /* blue mask */
    int                 i_red_lshift, i_red_rshift;            /* red shifts */
    int                 i_green_lshift, i_green_rshift;      /* green shifts */
    int                 i_blue_lshift, i_blue_rshift;         /* blue shifts */

    /* Useful pre-calculated pixel values - these are not supposed to be
     * accurate values, but rather values looking nice, given their usage. */
    u32                 i_white_pixel;                              /* white */
    u32                 i_black_pixel;                              /* black */
    u32                 i_gray_pixel;                                /* gray */
    u32                 i_blue_pixel;                                /* blue */

    /* Plugin used and shortcuts to access its capabilities */
    struct module_s *   p_module;
    int              ( *pf_create )     ( struct vout_thread_s * );
    int              ( *pf_init )       ( struct vout_thread_s * );
    void             ( *pf_end )        ( struct vout_thread_s * );
    void             ( *pf_destroy )    ( struct vout_thread_s * );
    int              ( *pf_manage )     ( struct vout_thread_s * );
    void             ( *pf_display )    ( struct vout_thread_s *,
                                          struct picture_s * );
    void             ( *pf_setpalette ) ( struct vout_thread_s *,
                                          u16 *, u16 *, u16 * );

    /* Pictures and rendering properties */
    boolean_t           b_grayscale;           /* color or grayscale display */
    boolean_t           b_info;              /* print additional information */
    boolean_t           b_interface;                     /* render interface */
    boolean_t           b_scale;                    /* allow picture scaling */
    boolean_t           b_fullscreen;           /* toogle fullscreen display */
    mtime_t             render_time;             /* last picture render time */

    /* Statistics - these numbers are not supposed to be accurate, but are a
     * good indication of the thread status */
    count_t             c_fps_samples;                     /* picture counts */
    mtime_t             p_fps_sample[VOUT_FPS_SAMPLES]; /* FPS samples dates */

    /* Video heap and translation tables */
    picture_t           p_picture[VOUT_MAX_PICTURES];            /* pictures */
    subpicture_t        p_subpicture[VOUT_MAX_PICTURES];      /* subpictures */
    int                 i_directbuffers;       /* number of pictures in VRAM */
    int                 i_pictures;                     /* current heap size */

    vout_chroma_t       chroma;                        /* translation tables */

    /* Bitmap fonts */
    p_vout_font_t       p_default_font;                      /* default font */
    p_vout_font_t       p_large_font;                          /* large font */

    /* Statistics */
    count_t             c_loops;
    count_t             c_pictures, c_late_pictures;
    mtime_t             display_jitter;    /* average deviation from the PTS */
    count_t             c_jitter_samples;  /* number of samples used for the *
                                            * calculation of the jitter      */
} vout_thread_t;

/* Flags for changes - these flags are set in the i_changes field when another
 * thread changed a variable */
#define VOUT_INFO_CHANGE        0x0001                     /* b_info changed */
#define VOUT_GRAYSCALE_CHANGE   0x0002                /* b_grayscale changed */
#define VOUT_INTF_CHANGE        0x0004                /* b_interface changed */
#define VOUT_SCALE_CHANGE       0x0008                    /* b_scale changed */
#define VOUT_GAMMA_CHANGE       0x0010                      /* gamma changed */
#define VOUT_CURSOR_CHANGE      0x0020                   /* b_cursor changed */
#define VOUT_FULLSCREEN_CHANGE  0x0040               /* b_fullscreen changed */
#define VOUT_SIZE_CHANGE        0x0200                       /* size changed */
#define VOUT_DEPTH_CHANGE       0x0400                      /* depth changed */
#define VOUT_CHROMA_CHANGE      0x0800               /* change chroma tables */

/* Disabled for thread deadlocks issues --Meuuh */
//#define VOUT_NODISPLAY_CHANGE   0xff00    /* changes which forbidden display */

#define MAX_JITTER_SAMPLES      20

/*****************************************************************************
 * Macros
 *****************************************************************************/

/* RGB2PIXEL: assemble RGB components to a pixel value, returns a u32 */
#define RGB2PIXEL( p_vout, i_red, i_green, i_blue )                           \
    (((((u32)i_red)   >> p_vout->i_red_rshift)   << p_vout->i_red_lshift)   | \
     ((((u32)i_green) >> p_vout->i_green_rshift) << p_vout->i_green_lshift) | \
     ((((u32)i_blue)  >> p_vout->i_blue_rshift)  << p_vout->i_blue_lshift))


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void            vout_InitBank       ( void );
void            vout_EndBank        ( void );

vout_thread_t * vout_CreateThread   ( int *pi_status, int, int, int, int );
void            vout_DestroyThread  ( vout_thread_t *, int *pi_status );

vout_fifo_t *   vout_CreateFifo     ( void );
void            vout_DestroyFifo    ( vout_fifo_t * );
void            vout_FreeFifo       ( vout_fifo_t * );

picture_t *     vout_CreatePicture  ( vout_thread_t *, int, int, int, int );
void            vout_DestroyPicture ( vout_thread_t *, picture_t * );
void            vout_DisplayPicture ( vout_thread_t *, picture_t * );
void            vout_DatePicture    ( vout_thread_t *, picture_t *, mtime_t );
void            vout_LinkPicture    ( vout_thread_t *, picture_t * );
void            vout_UnlinkPicture  ( vout_thread_t *, picture_t * );
picture_t *     vout_RenderPicture  ( vout_thread_t *, picture_t *,
                                                       subpicture_t * );

subpicture_t *  vout_CreateSubPicture   ( vout_thread_t *, int, int );
void            vout_DestroySubPicture  ( vout_thread_t *, subpicture_t * );
void            vout_DisplaySubPicture  ( vout_thread_t *, subpicture_t * );
subpicture_t *  vout_SortSubPictures    ( vout_thread_t *, mtime_t );
void            vout_RenderSubPictures  ( picture_t *, subpicture_t * );

