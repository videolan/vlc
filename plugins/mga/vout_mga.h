/*****************************************************************************
 * vout_mga.h: MGA video output display method headers
 *****************************************************************************
 * Copyright (C) 1999 Aaron Holtzman
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
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
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

#ifndef __LINUX_MGAVID_H
#define __LINUX_MGAVID_H

typedef struct mga_vid_config_s
{
    u32     card_type;
    u32     ram_size;
    u32     src_width;
    u32     src_height;
    u32     dest_width;
    u32     dest_height;
    u32     x_org;
    u32     y_org;
    u8      colkey_on;
    u8      colkey_red;
    u8      colkey_green;
    u8      colkey_blue;
} mga_vid_config_t;

#define MGA_VID_CONFIG _IOR('J', 1, mga_vid_config_t)
#define MGA_VID_ON     _IO ('J', 2)
#define MGA_VID_OFF    _IO ('J', 3)

#define MGA_G200 0x1234
#define MGA_G400 0x5678

#endif

/*****************************************************************************
 * vout_sys_t: video output X11 method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 specific properties of an output thread. X11 video
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* User settings */
    boolean_t           b_shm;               /* shared memory extension flag */

    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */
    Visual *            p_visual;                          /* visual pointer */
    int                 i_screen;                           /* screen number */
    Window              root_window;                          /* root window */
    Window              window;                   /* window instance handler */
    GC                  gc;              /* graphic context instance handler */
    Colormap            colormap;               /* colormap used (8bpp only) */

    /* Display buffers and shared memory information */
    XImage *            p_ximage[2];                       /* XImage pointer */
    XShmSegmentInfo     shm_info[2];       /* shared memory zone information */

    /* MGA specific variables */
    int                 i_fd;
    int                 i_size;
    mga_vid_config_t *  p_mga;
    byte_t *            p_mga_vid_base;
    boolean_t           b_g400;

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  X11OpenDisplay      ( vout_thread_t *p_vout, char *psz_display, Window root_window );
static void X11CloseDisplay     ( vout_thread_t *p_vout );
static int  X11CreateWindow     ( vout_thread_t *p_vout );
static void X11DestroyWindow    ( vout_thread_t *p_vout );
static int  X11CreateImage      ( vout_thread_t *p_vout, XImage **pp_ximage );
static void X11DestroyImage     ( XImage *p_ximage );
static int  X11CreateShmImage   ( vout_thread_t *p_vout, XImage **pp_ximage,
                                  XShmSegmentInfo *p_shm_info );
static void X11DestroyShmImage  ( vout_thread_t *p_vout, XImage *p_ximage,
                                  XShmSegmentInfo *p_shm_info );

