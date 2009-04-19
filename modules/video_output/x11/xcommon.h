/*****************************************************************************
 * xcommon.h: Defines common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

/*****************************************************************************
 * Defines
 *****************************************************************************/
#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
#   define IMAGE_TYPE     XvImage
#   define EXTRA_ARGS     int i_xvport, int i_chroma, int i_bits_per_pixel
#   define EXTRA_ARGS_SHM int i_xvport, int i_chroma, XShmSegmentInfo *p_shm
#   define DATA_SIZE(p)   (p)->data_size
#   define IMAGE_FREE     XFree      /* There is nothing like XvDestroyImage */
#else
#   define IMAGE_TYPE     XImage
#   define EXTRA_ARGS     Visual *p_visual, int i_depth, int i_bytes_per_pixel
#   define EXTRA_ARGS_SHM Visual *p_visual, int i_depth, XShmSegmentInfo *p_shm
#   define DATA_SIZE(p)   ((p)->bytes_per_line * (p)->height)
#   define IMAGE_FREE     XDestroyImage
#endif

#define X11_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )
#define VLC2X11_FOURCC( i ) \
        X11_FOURCC( ((char *)&i)[0], ((char *)&i)[1], ((char *)&i)[2], \
                    ((char *)&i)[3] )
#define X112VLC_FOURCC( i ) \
        VLC_FOURCC( i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, \
                    (i >> 24) & 0xff )

#ifdef HAVE_OSSO
#include <libosso.h>
#endif

struct vout_window_t;

/*****************************************************************************
 * x11_window_t: X11 window descriptor
 *****************************************************************************
 * This structure contains all the data necessary to describe an X11 window.
 *****************************************************************************/
typedef struct x11_window_t
{
    struct vout_window_t*owner_window;               /* owner window (if any) */
    Window              base_window;                          /* base window */
    Window              video_window;     /* sub-window for displaying video */
    GC                  gc;              /* graphic context instance handler */

    unsigned int        i_width;                             /* window width */
    unsigned int        i_height;                           /* window height */
    int                 i_x;                          /* window x coordinate */
    int                 i_y;                          /* window y coordinate */

    Atom                wm_protocols;
    Atom                wm_delete_window;

#ifdef HAVE_XINERAMA
    int                 i_screen;
#endif

} x11_window_t;

/*****************************************************************************
 * Xxmc defines
 *****************************************************************************/

#ifdef MODULE_NAME_IS_xvmc

typedef struct
{         /* CLUT == Color LookUp Table */
    uint8_t cb;
    uint8_t cr;
    uint8_t y;
    uint8_t foo;
} clut_t;

#define XX44_PALETTE_SIZE 32
#define OVL_PALETTE_SIZE 256
#define XVMC_MAX_SURFACES 16
#define XVMC_MAX_SUBPICTURES 4
#define FOURCC_IA44 0x34344149
#define FOURCC_AI44 0x34344941

typedef struct
{
    unsigned size;
    unsigned max_used;
    uint32_t cluts[XX44_PALETTE_SIZE];
    /* cache palette entries for both colors and clip_colors */
    int lookup_cache[OVL_PALETTE_SIZE*2];
} xx44_palette_t;

/*
 * Functions to handle the vlc-specific palette.
 */

void clear_xx44_palette( xx44_palette_t *p );

/*
 * Convert the xine-specific palette to something useful.
 */

void xx44_to_xvmc_palette( const xx44_palette_t *p,unsigned char *xvmc_palette,
             unsigned first_xx44_entry, unsigned num_xx44_entries,
             unsigned num_xvmc_components, char *xvmc_components );

typedef struct
{
    vlc_macroblocks_t   vlc_mc;
    XvMCBlockArray      blocks;            /* pointer to memory for dct block array  */
    int                 num_blocks;
    XvMCMacroBlock      *macroblockptr;     /* pointer to current macro block         */
    XvMCMacroBlock      *macroblockbaseptr; /* pointer to base MacroBlock in MB array */
    XvMCMacroBlockArray macro_blocks;      /* pointer to memory for macroblock array */
    int                 slices;
} xvmc_macroblocks_t;

typedef struct
{
    unsigned int        mpeg_flags;
    unsigned int        accel_flags;
    unsigned int        max_width;
    unsigned int        max_height;
    unsigned int        sub_max_width;
    unsigned int        sub_max_height;
    int                 type_id;
    XvImageFormatValues subPicType;
    int                 flags;
} xvmc_capabilities_t;

typedef struct xvmc_surface_handler_s
{
    XvMCSurface         surfaces[XVMC_MAX_SURFACES];
    int                 surfInUse[XVMC_MAX_SURFACES];
    int                 surfValid[XVMC_MAX_SURFACES];
    XvMCSubpicture      subpictures[XVMC_MAX_SUBPICTURES];
    int                 subInUse[XVMC_MAX_SUBPICTURES];
    int                 subValid[XVMC_MAX_SUBPICTURES];
    pthread_mutex_t     mutex;
} xvmc_surface_handler_t;

typedef struct context_lock_s
{
    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
    int                 num_readers;
} context_lock_t;

#define XVMCLOCKDISPLAY(display) XLockDisplay(display);
#define XVMCUNLOCKDISPLAY(display) XUnlockDisplay(display);

void xvmc_context_reader_unlock( context_lock_t *c );
void xvmc_context_reader_lock( context_lock_t *c );
void xvmc_context_writer_lock( context_lock_t *c );
void xvmc_context_writer_unlock( context_lock_t *c );
void free_context_lock( context_lock_t *c );
void xxmc_dispose_context( vout_thread_t *p_vout );

int xxmc_xvmc_surface_valid( vout_thread_t *p_vout, XvMCSurface *surf );
void xxmc_xvmc_free_surface( vout_thread_t *p_vout, XvMCSurface *surf );

void xvmc_vld_slice( picture_t *picture );
void xvmc_vld_frame( picture_t *picture );

void xxmc_do_update_frame( picture_t *picture, uint32_t width, uint32_t height,
        double ratio, int format, int flags);

int checkXvMCCap( vout_thread_t *p_vout);

XvMCSubpicture *xxmc_xvmc_alloc_subpicture( vout_thread_t *p_vout,
        XvMCContext *context, unsigned short width, unsigned short height,
        int xvimage_id );

void xxmc_xvmc_free_subpicture( vout_thread_t *p_vout, XvMCSubpicture *sub );
void blend_xx44( uint8_t *dst_img, subpicture_t *sub_img, int dst_width,
        int dst_height, int dst_pitch, xx44_palette_t *palette,int ia44);

#endif /* XvMC defines */

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 and XVideo specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */

    Visual *            p_visual;                          /* visual pointer */
    int                 i_screen;                           /* screen number */

    /* Our current window */
    x11_window_t *      p_win;

    /* Our two windows */
    x11_window_t        original_window;
    x11_window_t        fullscreen_window;

    /* key and mouse event handling */
    int                 i_vout_event;  /* 1(Fullsupport), 2(FullscreenOnly), 3(none) */

    /* X11 generic properties */
    bool          b_altfullscreen;          /* which fullscreen method */
#ifdef HAVE_SYS_SHM_H
    int                 i_shm_opcode;      /* shared memory extension opcode */
#endif

#if defined(MODULE_NAME_IS_xvideo) || defined(MODULE_NAME_IS_xvmc)
    int                 i_xvport;
    bool          b_paint_colourkey;
    int                 i_colourkey;
#else
    Colormap            colormap;               /* colormap used (8bpp only) */

    unsigned int        i_screen_depth;
    unsigned int        i_bytes_per_pixel;
    unsigned int        i_bytes_per_line;
#endif

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */
#ifdef DPMSINFO_IN_DPMS_H
    BOOL                b_ss_dpms;                              /* DPMS mode */
#endif

    /* Mouse pointer properties */
    bool          b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    mtime_t             i_mouse_hide_timeout;      /* after time hide cursor */
    Cursor              blank_cursor;                   /* the hidden cursor */
    mtime_t             i_time_button_last_pressed;   /* to track dbl-clicks */
    Pixmap              cursor_pixmap;

    /* Window manager properties */
    Atom                net_wm_state;
    Atom                net_wm_state_fullscreen;
    bool          b_net_wm_state_fullscreen;
    Atom                net_wm_state_above;
    bool          b_net_wm_state_above;
    Atom                net_wm_state_stays_on_top;
    bool          b_net_wm_state_stays_on_top;
    Atom                net_wm_state_below;
    bool          b_net_wm_state_below;

#ifdef MODULE_NAME_IS_glx
    /* GLX properties */
    int                 b_glx13;
    GLXContext          gwctx;
    GLXWindow           gwnd;
#endif

#ifdef MODULE_NAME_IS_xvmc
    /* XvMC related stuff here */
    xvmc_macroblocks_t  macroblocks;
    xvmc_capabilities_t *xvmc_cap;
    unsigned int        xvmc_num_cap;
    unsigned int        xvmc_max_subpic_x;
    unsigned int        xvmc_max_subpic_y;
    int                 xvmc_eventbase;
    int                 xvmc_errbase;
    int                 hwSubpictures;
    XvMCSubpicture      *old_subpic;
    XvMCSubpicture      *new_subpic;
    xx44_palette_t      palette;
    int                 first_overlay;
    float               cpu_saver;
    int                 cpu_save_enabled;
    int                 reverse_nvidia_palette;
    int                 context_flags;

    /*
     * These variables are protected by the context lock:
     */
    unsigned            xvmc_cur_cap;
    int                 xvmc_backend_subpic;
    XvMCContext         context;
    int                 contextActive;
    xvmc_surface_handler_t xvmc_surf_handler;
    unsigned            xvmc_mpeg;
    unsigned            xvmc_accel;
    unsigned            last_accel_request;
    unsigned            xvmc_width;
    unsigned            xvmc_height;
    int                 have_xvmc_autopaint;
    int                 xvmc_xoverlay_type;
    int                 unsigned_intra;

    /*
     * Only creation and destruction of the below.
     */
    char                *xvmc_palette;
    XvImage             *subImage;
    XShmSegmentInfo     subShmInfo;

    /*
     * The mutex below is needed since XlockDisplay wasn't really enough
     * to protect the XvMC Calls.
     */
    context_lock_t      xvmc_lock;
    subpicture_t *      p_last_subtitle_save;
    int                 xvmc_deinterlace_method;
    int                 xvmc_crop_style;
    mtime_t             last_date;

    //alphablend_t       alphablend_extra_data;
#endif

#ifdef HAVE_XSP
    int                 i_hw_scale;
#endif

#ifdef HAVE_OSSO
    osso_context_t      *p_octx;
    int                 i_backlight_on_counter;
#endif
};

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * XVideo specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_t
{
    IMAGE_TYPE *        p_image;

#ifdef HAVE_SYS_SHM_H
    XShmSegmentInfo     shminfo;       /* shared memory zone information */
#endif

#ifdef MODULE_NAME_IS_xvmc
    XvMCSurface         *xvmc_surf;
    vlc_xxmc_t           xxmc_data;
    int                  last_sw_format;
    vout_thread_t        *p_vout;
    int                  nb_display;
#endif
};

/*****************************************************************************
 * mwmhints_t: window manager hints
 *****************************************************************************
 * Fullscreen needs to be able to hide the wm decorations so we provide
 * this structure to make it easier.
 *****************************************************************************/
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5

typedef struct mwmhints_t
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    signed   long input_mode;
    unsigned long status;
} mwmhints_t;

/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#ifdef MODULE_NAME_IS_xvideo
#   define MAX_DIRECTBUFFERS (VOUT_MAX_PICTURES)
#elif defined(MODULE_NAME_IS_xvmc)
#   define MAX_DIRECTBUFFERS (VOUT_MAX_PICTURES+2)
#else
#   define MAX_DIRECTBUFFERS 2
#endif

#ifndef MODULE_NAME_IS_glx
static IMAGE_TYPE *CreateImage    ( vout_thread_t *,
                                    Display *, EXTRA_ARGS, int, int );
#ifdef HAVE_SYS_SHM_H
IMAGE_TYPE *CreateShmImage ( vout_thread_t *,
                                    Display *, EXTRA_ARGS_SHM, int, int );
#endif
#endif

