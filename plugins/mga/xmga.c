/*****************************************************************************
 * xmga.c : X11 MGA plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: xmga.c,v 1.9 2002/03/17 17:00:38 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#ifdef HAVE_MACHINE_PARAM_H
/* BSD */
#include <machine/param.h>
#include <sys/types.h>                                     /* typedef ushort */
#include <sys/ipc.h>
#endif

#ifndef WIN32
#include <netinet/in.h>                               /* BSD: struct in_addr */
#endif

#include <sys/shm.h>                                   /* shmget(), shmctl() */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/dpms.h>

#include "video.h"
#include "video_output.h"

#include "interface.h"
#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"

//#include "mga.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void vout_getfunctions( function_list_t * );

static int  vout_Create    ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );
static int  vout_Manage    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );

static int  CreateWindow   ( vout_thread_t * );
static void DestroyWindow  ( vout_thread_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static void ToggleFullScreen      ( vout_thread_t * );

static void EnableXScreenSaver    ( vout_thread_t * );
static void DisableXScreenSaver   ( vout_thread_t * );

static void CreateCursor   ( vout_thread_t * );
static void DestroyCursor  ( vout_thread_t * );
static void ToggleCursor   ( vout_thread_t * );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/

#define ALT_FS_TEXT "Alternate fullscreen method"
#define ALT_FS_LONGTEXT "There are two ways to make a fullscreen window, " \
                        "unfortunately each one has its drawbacks.\n" \
                        "1) Let the window manager handle your fullscreen " \
                        "window (default). But things like taskbars will " \
                        "likely show on top of the video\n" \
                        "2) Completly bypass the window manager, but then " \
                        "nothing will be able to show on top of the video"

MODULE_CONFIG_START
ADD_CATEGORY_HINT( "Miscellaneous", NULL )
ADD_BOOL    ( "xmga_altfullscreen", NULL, ALT_FS_TEXT, ALT_FS_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "X11 MGA module" )
    ADD_CAPABILITY( VOUT, 60 )
    ADD_SHORTCUT( "xmga" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 and XVideo specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */

    Visual *            p_visual;                          /* visual pointer */
    int                 i_screen;                           /* screen number */
    Window              window;                               /* root window */
    GC                  gc;              /* graphic context instance handler */

    boolean_t           b_shm;               /* shared memory extension flag */

#ifdef MODULE_NAME_IS_xvideo
    Window              yuv_window;   /* sub-window for displaying yuv video
                                                                        data */
    int                 i_xvport;
#else
    Colormap            colormap;               /* colormap used (8bpp only) */

    int                 i_screen_depth;
    int                 i_bytes_per_pixel;
    int                 i_bytes_per_line;
    int                 i_red_mask;
    int                 i_green_mask;
    int                 i_blue_mask;
#endif

    /* X11 generic properties */
    Atom                wm_protocols;
    Atom                wm_delete_window;

    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */
    boolean_t           b_altfullscreen;          /* which fullscreen method */

    /* Backup of window position and size before fullscreen switch */
    int                 i_width_backup;
    int                 i_height_backup;
    int                 i_xpos_backup;
    int                 i_ypos_backup;
    int                 i_width_backup_2;
    int                 i_height_backup_2;
    int                 i_xpos_backup_2;
    int                 i_ypos_backup_2;

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */
    BOOL                b_ss_dpms;                              /* DPMS mode */

    /* Mouse pointer properties */
    boolean_t           b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    Cursor              blank_cursor;                   /* the hidden cursor */
    Pixmap              cursor_pixmap;

} vout_sys_t;

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * XVideo specific properties of a direct buffer.
 *****************************************************************************/
typedef struct picture_sys_s
{
} picture_sys_t;

/*****************************************************************************
 * mwmhints_t: window manager hints
 *****************************************************************************
 * Fullscreen needs to be able to hide the wm decorations so we provide
 * this structure to make it easier.
 *****************************************************************************/
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct mwmhints_s
{
    u32 flags;
    u32 functions;
    u32 decorations;
    s32 input_mode;
    u32 status;
} mwmhints_t;

/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#ifdef MODULE_NAME_IS_xvideo
#   define MAX_DIRECTBUFFERS 5
#else
#   define MAX_DIRECTBUFFERS 2
#endif

/*****************************************************************************
 * Seeking function TODO: put this in a generic location !
 *****************************************************************************/
static __inline__ void vout_Seek( off_t i_seek )
{
    off_t i_tell;

    vlc_mutex_lock( &p_input_bank->lock );
    if( p_input_bank->pp_input[0] != NULL )
    {
#define S p_input_bank->pp_input[0]->stream
        i_tell = S.p_selected_area->i_tell + i_seek * (off_t)50 * S.i_mux_rate;

        i_tell = ( i_tell <= 0 /*S.p_selected_area->i_start*/ )
                   ? 0 /*S.p_selected_area->i_start*/
                   : ( i_tell >= S.p_selected_area->i_size )
                       ? S.p_selected_area->i_size
                       : i_tell;

        input_Seek( p_input_bank->pp_input[0], i_tell );
#undef S
    }
    vlc_mutex_unlock( &p_input_bank->lock );
}

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
}

/*****************************************************************************
 * vout_Create: allocate X11 video thread output method
 *****************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    char *psz_display;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open display, unsing the "display" config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPszVariable( "display" );
    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                          /* error */
    {
        intf_ErrMsg( "vout error: cannot open display %s",
                     XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        if( psz_display ) free( psz_display );
        return( 1 );
    }
    if( psz_display ) free( psz_display );

    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Disable screen saver */
    DisableXScreenSaver( p_vout );

    /* Misc init */
    p_vout->p_sys->b_altfullscreen = 0;

    return( 0 );
}

/*****************************************************************************
 * vout_Destroy: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
    }

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout );

    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Init: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

#ifdef MODULE_NAME_IS_xvideo
    /* Initialize the output structure; we already found an XVideo port,
     * and the corresponding chroma we will be using. Since we can
     * arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

#else
    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    switch( p_vout->p_sys->i_screen_depth )
    {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = FOURCC_RGB2; break;
        case 15:
            p_vout->output.i_chroma = FOURCC_RV15; break;
        case 16:
            p_vout->output.i_chroma = FOURCC_RV16; break;
        case 24:
            p_vout->output.i_chroma = FOURCC_RV24; break;
        case 32:
            p_vout->output.i_chroma = FOURCC_RV32; break;
        default:
            intf_ErrMsg( "vout error: unknown screen depth" );
            return( 0 );
    }

    p_vout->output.i_width = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;
#endif

    /* Try to initialize up to MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Render: render previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

 /*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to X11 server.
 * (The Xv extension takes care of "double-buffering".)
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                       &i_x, &i_y, &i_width, &i_height );
}

/*****************************************************************************
 * vout_Manage: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    boolean_t   b_resized;                        /* window has been resized */
    char        i_key;                                    /* ISO Latin-1 key */
    KeySym      x_key_symbol;

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */

    b_resized = 0;
    while( XCheckWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask | 
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( (xevent.type == ConfigureNotify)
          && ((xevent.xconfigure.width != p_vout->p_sys->i_width)
             || (xevent.xconfigure.height != p_vout->p_sys->i_height)) )
        {
            /* Update dimensions */
            b_resized = 1;
            p_vout->i_changes |= VOUT_SIZE_CHANGE;
            p_vout->p_sys->i_width = xevent.xconfigure.width;
            p_vout->p_sys->i_height = xevent.xconfigure.height;
        }
        /* MapNotify event: change window status and disable screen saver */
        else if( xevent.type == MapNotify)
        {
            if( (p_vout != NULL) && !p_vout->b_active )
            {
                DisableXScreenSaver( p_vout );
                p_vout->b_active = 1;
            }
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( xevent.type == UnmapNotify )
        {
            if( (p_vout != NULL) && p_vout->b_active )
            {
                EnableXScreenSaver( p_vout );
                p_vout->b_active = 0;
            }
        }
        /* Keyboard event */
        else if( xevent.type == KeyPress )
        {
            /* We may have keys like F1 trough F12, ESC ... */
            x_key_symbol = XKeycodeToKeysym( p_vout->p_sys->p_display,
                                             xevent.xkey.keycode, 0 );
            switch( x_key_symbol )
            {
                 case XK_Escape:
                     p_main->p_intf->b_die = 1;
                     break;
                 case XK_Menu:
                     p_main->p_intf->b_menu_change = 1;
                     break;
                 case XK_Left:
                     vout_Seek( -5 );
                     break;
                 case XK_Right:
                     vout_Seek( 5 );
                     break;
                 case XK_Up:
                     vout_Seek( 60 );
                     break;
                 case XK_Down:
                     vout_Seek( -60 );
                     break;
                 case XK_Home:
                     input_Seek( p_input_bank->pp_input[0],
                     p_input_bank->pp_input[0]->stream.p_selected_area->i_start );
                     break;
                 case XK_End:
                     input_Seek( p_input_bank->pp_input[0],
                     p_input_bank->pp_input[0]->stream.p_selected_area->i_size );
                     break;
                 case XK_Page_Up:
                     vout_Seek( 900 );
                     break;
                 case XK_Page_Down:
                     vout_Seek( -900 );
                     break;
                 case XK_space:
                     input_SetStatus( p_input_bank->pp_input[0],
                                      INPUT_STATUS_PAUSE );
                     break;

                 default:
                     /* "Normal Keys"
                      * The reason why I use this instead of XK_0 is that 
                      * with XLookupString, we don't have to care about
                      * keymaps. */

                    if( XLookupString( &xevent.xkey, &i_key, 1, NULL, NULL ) )
                    {
                        /* FIXME: handle stuff here */
                        switch( i_key )
                        {
                        case 'q':
                        case 'Q':
                            p_main->p_intf->b_die = 1;
                            break;
                        case 'f':
                        case 'F':
                            p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                            break;

                        case '0': network_ChannelJoin( 0 ); break;
                        case '1': network_ChannelJoin( 1 ); break;
                        case '2': network_ChannelJoin( 2 ); break;
                        case '3': network_ChannelJoin( 3 ); break;
                        case '4': network_ChannelJoin( 4 ); break;
                        case '5': network_ChannelJoin( 5 ); break;
                        case '6': network_ChannelJoin( 6 ); break;
                        case '7': network_ChannelJoin( 7 ); break;
                        case '8': network_ChannelJoin( 8 ); break;
                        case '9': network_ChannelJoin( 9 ); break;

                        default:
                            break;
                        }
                    }
                break;
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    /* In this part we will eventually manage
                     * clicks for DVD navigation for instance. For the
                     * moment just pause the stream. */
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_PAUSE );
                    break;

                case Button4:
                    vout_Seek( 15 );
                    break;

                case Button5:
                    vout_Seek( -15 );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button3:
                    /* FIXME: need locking ! */
                    p_main->p_intf->b_menu_change = 1;
                    break;
            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout ); 
            }
        }
        /* Other event */
        else
        {
            intf_WarnMsg( 3, "vout: unhandled event %d received", xevent.type );
        }
    }

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_vout->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_vout->p_sys->wm_protocols)
            && (xevent.xclient.data.l[0] == p_vout->p_sys->wm_delete_window ) )
        {
            p_main->p_intf->b_die = 1;
        }
    }

    /*
     * Fullscreen Change
     */
    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        ToggleFullScreen( p_vout );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;

    }

    /*
     * Size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        int i_width, i_height, i_x, i_y;

        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        intf_WarnMsg( 3, "vout: video display resized (%dx%d)",
                      p_vout->p_sys->i_width,
                      p_vout->p_sys->i_height );
 
        vout_PlacePicture( p_vout, p_vout->p_sys->i_width,
                           p_vout->p_sys->i_height,
                           &i_x, &i_y, &i_width, &i_height );
    }

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_time_mouse_last_moved > 2000000 )
    {
        /* Hide the mouse automatically */
        if( p_vout->p_sys->b_mouse_pointer_visible )
        {
            ToggleCursor( p_vout ); 
        }
    }

    return 0;
}

/*****************************************************************************
 * vout_End: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}

/* following functions are local */

/*****************************************************************************
 * CreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int CreateWindow( vout_thread_t *p_vout )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;

    boolean_t               b_expose;
    boolean_t               b_configure_notify;
    boolean_t               b_map_notify;

    /* Set main window's size */
    if( p_vout->render.i_height * p_vout->render.i_aspect
        >= p_vout->render.i_width * VOUT_ASPECT_FACTOR )
    {
        p_vout->p_sys->i_width = p_vout->render.i_height
          * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
        p_vout->p_sys->i_height = p_vout->render.i_height;
    }
    else
    {
        p_vout->p_sys->i_width = p_vout->render.i_width;
        p_vout->p_sys->i_height = p_vout->render.i_width
          * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }

#if 0
    if( p_vout->p_sys->i_width <= 300 && p_vout->p_sys->i_height <= 300 )
    {
        p_vout->p_sys->i_width <<= 1;
        p_vout->p_sys->i_height <<= 1;
    }
    else if( p_vout->p_sys->i_width <= 400
             && p_vout->p_sys->i_height <= 400 )
    {
        p_vout->p_sys->i_width += p_vout->p_sys->i_width >> 1;
        p_vout->p_sys->i_height += p_vout->p_sys->i_height >> 1;
    }
#endif

    /* Prepare window manager hints and properties */
    xsize_hints.base_width          = p_vout->p_sys->i_width;
    xsize_hints.base_height         = p_vout->p_sys->i_height;
    xsize_hints.flags               = PSize;
    p_vout->p_sys->wm_protocols     = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_PROTOCOLS", True );
    p_vout->p_sys->wm_delete_window = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_DELETE_WINDOW", True );

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;
    

    /* Create the window and set hints - the window must receive
     * ConfigureNotify events, and until it is displayed, Expose and
     * MapNotify events. */

    p_vout->p_sys->window =
        XCreateWindow( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       0, 0,
                       p_vout->p_sys->i_width,
                       p_vout->p_sys->i_height,
                       0,
                       0, InputOutput, 0,
                       CWBackingStore | CWBackPixel | CWEventMask,
                       &xwindow_attributes );

    /* Set window manager hints and properties: size hints, command,
     * window's name, and accepted protocols */
    XSetWMNormalHints( p_vout->p_sys->p_display, p_vout->p_sys->window,
                       &xsize_hints );
    XSetCommand( p_vout->p_sys->p_display, p_vout->p_sys->window,
                 p_main->ppsz_argv, p_main->i_argc );
    XStoreName( p_vout->p_sys->p_display, p_vout->p_sys->window,
                VOUT_TITLE " (XMGA output)"
              );

    if( (p_vout->p_sys->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_vout->p_sys->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_sys->p_display, p_vout->p_sys->window,
                             &p_vout->p_sys->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        intf_Msg( "vout error: missing or bad window manager" );
    } 

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_vout->p_sys->gc = XCreateGC( p_vout->p_sys->p_display,
                                   p_vout->p_sys->window,
                                   GCGraphicsExposures, &xgcvalues);

    /* Send orders to server, and wait until window is displayed - three
     * events must be received: a MapNotify event, an Expose event allowing
     * drawing in the window, and a ConfigureNotify to get the window
     * dimensions. Once those events have been received, only ConfigureNotify
     * events need to be received. */
    b_expose = 0;
    b_configure_notify = 0;
    b_map_notify = 0;
    XMapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window);
    do
    {
        XNextEvent( p_vout->p_sys->p_display, &xevent);
        if( (xevent.type == Expose)
            && (xevent.xexpose.window == p_vout->p_sys->window) )
        {
            b_expose = 1;
        }
        else if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_vout->p_sys->window) )
        {
            b_map_notify = 1;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_vout->p_sys->window) )
        {
            b_configure_notify = 1;
            p_vout->p_sys->i_width = xevent.xconfigure.width;
            p_vout->p_sys->i_height = xevent.xconfigure.height;
        }
    } while( !( b_expose && b_configure_notify && b_map_notify ) );

    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->window,
                  StructureNotifyMask | KeyPressMask |
                  ButtonPressMask | ButtonReleaseMask | 
                  PointerMotionMask );

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
        ToggleCursor( p_vout );
    }

    XSync( p_vout->p_sys->p_display, False );

    /* At this stage, the window is open, displayed, and ready to
     * receive data */

    return( 0 );
}

/*****************************************************************************
 * DestroyWindow: destroy the window
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyWindow( vout_thread_t *p_vout )
{
    XSync( p_vout->p_sys->p_display, False );

    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    /* XXX */

    switch( p_vout->output.i_chroma )
    {
        /* XXX ?? */

        default:
            /* Unknown chroma, tell the guy to get lost */
            free( p_pic->p_sys );
            intf_ErrMsg( "vout error: never heard of chroma 0x%.8x (%4.4s)",
                         p_vout->output.i_chroma,
                         (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return -1;
    }

    return 0;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************
 * Destroy XImage AND associated data. If using Shm, detach shared memory
 * segment from server and process, then free it. The XDestroyImage manpage
 * says that both the image structure _and_ the data pointed to by the
 * image structure are freed, so no need to free p_image->data.
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{

    XSync( p_vout->p_sys->p_display, False );

    free( p_pic->p_sys );
}

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    Atom prop;
    mwmhints_t mwmhints;
    int i_xpos, i_ypos, i_width, i_height;
    XEvent xevent;
    XSetWindowAttributes attributes;

    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    if( p_vout->b_fullscreen )
    {
        Window next_parent, parent, *p_dummy, dummy1;
        unsigned int dummy2, dummy3;

        intf_WarnMsg( 3, "vout: entering fullscreen mode" );

        /* Only check the fullscreen method when we actually go fullscreen,
         * because to go back to window mode we need to know in which
         * fullscreen mode we where */
        p_vout->p_sys->b_altfullscreen =
            config_GetIntVariable( "xmga_altfullscreen" );

        /* Save current window coordinates so they can be restored when
         * we exit from fullscreen mode. This is the tricky part because
         * this heavily depends on the behaviour of the window manager.
         * When you use XMoveWindow some window managers will adjust the top
         * of the window to the coordinates you gave, but others will instead
         * adjust the top of the client area to the coordinates
         * (don't forget windows have decorations). */

        /* First, get the position and size of the client area */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_vout->p_sys->window,
                      &dummy1,
                      &dummy2,
                      &dummy3,
                      &p_vout->p_sys->i_width_backup_2,
                      &p_vout->p_sys->i_height_backup_2,
                      &dummy2, &dummy3 );
        XTranslateCoordinates( p_vout->p_sys->p_display,
                               p_vout->p_sys->window,
                               DefaultRootWindow( p_vout->p_sys->p_display ),
                               0,
                               0,
                               &p_vout->p_sys->i_xpos_backup_2,
                               &p_vout->p_sys->i_ypos_backup_2,
                               &dummy1 );

        /* Then try to get the position and size of the whole window */

        /* find the real parent of our window (created by the window manager),
         * the one which is a direct child of the root window */
        next_parent = parent = p_vout->p_sys->window;
        while( next_parent != DefaultRootWindow( p_vout->p_sys->p_display ) )
        {
            parent = next_parent;
            XQueryTree( p_vout->p_sys->p_display,
                        parent,
                        &dummy1,
                        &next_parent,
                        &p_dummy,
                        &dummy2 );
            XFree((void *)p_dummy);
        }

        XGetGeometry( p_vout->p_sys->p_display,
                      p_vout->p_sys->window,
                      &dummy1,
                      &dummy2,
                      &dummy3,
                      &p_vout->p_sys->i_width_backup,
                      &p_vout->p_sys->i_height_backup,
                      &dummy2, &dummy3 );

        XTranslateCoordinates( p_vout->p_sys->p_display,
                               parent,
                               DefaultRootWindow( p_vout->p_sys->p_display ),
                               0,
                               0,
                               &p_vout->p_sys->i_xpos_backup,
                               &p_vout->p_sys->i_ypos_backup,
                               &dummy1 );

        /* fullscreen window size and position */
        i_xpos = 0;
        i_ypos = 0;
        i_width = DisplayWidth( p_vout->p_sys->p_display,
                                p_vout->p_sys->i_screen );
        i_height = DisplayHeight( p_vout->p_sys->p_display,
                                  p_vout->p_sys->i_screen );

    }
    else
    {
        intf_WarnMsg( 3, "vout: leaving fullscreen mode" );

        i_xpos = p_vout->p_sys->i_xpos_backup;
        i_ypos = p_vout->p_sys->i_ypos_backup;
        i_width = p_vout->p_sys->i_width_backup;
        i_height = p_vout->p_sys->i_height_backup;
    }

    /* To my knowledge there are two ways to create a borderless window.
     * There's the generic way which is to tell x to bypass the window manager,
     * but this creates problems with the focus of other applications.
     * The other way is to use the motif property "_MOTIF_WM_HINTS" which
     * luckily seems to be supported by most window managers.
     */
    if( !p_vout->p_sys->b_altfullscreen )
    {
        mwmhints.flags = MWM_HINTS_DECORATIONS;
        mwmhints.decorations = !p_vout->b_fullscreen;

        prop = XInternAtom( p_vout->p_sys->p_display, "_MOTIF_WM_HINTS",
                            False );
        XChangeProperty( p_vout->p_sys->p_display, p_vout->p_sys->window,
                         prop, prop, 32, PropModeReplace,
                         (unsigned char *)&mwmhints,
                         PROP_MWM_HINTS_ELEMENTS );
    }
    else
    {
        /* brute force way to remove decorations */
        attributes.override_redirect = p_vout->b_fullscreen;
        XChangeWindowAttributes( p_vout->p_sys->p_display,
                                 p_vout->p_sys->window,
                                 CWOverrideRedirect,
                                 &attributes);
    }

    /* We need to unmap and remap the window if we want the window 
     * manager to take our changes into effect */
    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window);

    XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                  StructureNotifyMask, &xevent );
    while( xevent.type != UnmapNotify )
        XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      StructureNotifyMask, &xevent );

    XMapRaised( p_vout->p_sys->p_display, p_vout->p_sys->window);

    while( xevent.type != MapNotify )
        XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      StructureNotifyMask, &xevent );

    XMoveResizeWindow( p_vout->p_sys->p_display,
                       p_vout->p_sys->window,
                       i_xpos,
                       i_ypos,
                       i_width,
                       i_height );

    /* Purge all ConfigureNotify events, this is needed to fix a bug where we
     * would lose the original size of the window */
    while( xevent.type != ConfigureNotify )
        XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      StructureNotifyMask, &xevent );
    while( XCheckWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask, &xevent ) );


    /* We need to check that the window was really restored where we wanted */
    if( !p_vout->b_fullscreen )
    {
        Window dummy1;
        unsigned int dummy2, dummy3, dummy4, dummy5;

        /* Check the position */
        XTranslateCoordinates( p_vout->p_sys->p_display,
                               p_vout->p_sys->window,
                               DefaultRootWindow( p_vout->p_sys->p_display ),
                               0,
                               0,
                               &dummy2,
                               &dummy3,
                               &dummy1 );
        if( dummy2 != p_vout->p_sys->i_xpos_backup_2 ||
            dummy3 != p_vout->p_sys->i_ypos_backup_2 )
        {
            /* Ok it didn't work... second try */

            XMoveWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->window,
                         p_vout->p_sys->i_xpos_backup_2,
                         p_vout->p_sys->i_ypos_backup_2 );
            
            /* Purge all ConfigureNotify events, this is needed to fix a bug
             * where we would lose the original size of the window */
            XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                          StructureNotifyMask, &xevent );
            while( xevent.type != ConfigureNotify )
                XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask, &xevent );
            while( XCheckWindowEvent( p_vout->p_sys->p_display,
                                      p_vout->p_sys->window,
                                      StructureNotifyMask, &xevent ) );
        }

        /* Check the size */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_vout->p_sys->window,
                      &dummy1,
                      &dummy2,
                      &dummy3,
                      &dummy4,
                      &dummy5,
                      &dummy2, &dummy3 );

        if( dummy4 != p_vout->p_sys->i_width_backup_2 ||
            dummy5 != p_vout->p_sys->i_height_backup_2 )
        {
            /* Ok it didn't work... third try */

            XResizeWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->window,
                         p_vout->p_sys->i_width_backup_2,
                         p_vout->p_sys->i_height_backup_2 );
            
            /* Purge all ConfigureNotify events, this is needed to fix a bug
             * where we would lose the original size of the window */
            XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                          StructureNotifyMask, &xevent );
            while( xevent.type != ConfigureNotify )
                XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask, &xevent );
            while( XCheckWindowEvent( p_vout->p_sys->p_display,
                                      p_vout->p_sys->window,
                                      StructureNotifyMask, &xevent ) );
        }
    }

    if( p_vout->p_sys->b_altfullscreen )
        XSetInputFocus(p_vout->p_sys->p_display,
                       p_vout->p_sys->window,
                       RevertToParent,
                       CurrentTime);

    /* signal that the size needs to be updated */
    p_vout->p_sys->i_width = i_width;
    p_vout->p_sys->i_height = i_height;
    p_vout->i_changes |= VOUT_SIZE_CHANGE;

}

/*****************************************************************************
 * EnableXScreenSaver: enable screen saver
 *****************************************************************************
 * This function enables the screen saver on a display after it has been
 * disabled by XDisableScreenSaver.
 * FIXME: what happens if multiple vlc sessions are running at the same
 *        time ???
 *****************************************************************************/
static void EnableXScreenSaver( vout_thread_t *p_vout )
{
    int dummy;

    XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );

    /* Restore DPMS settings */
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        if( p_vout->p_sys->b_ss_dpms )
        {
            DPMSEnable( p_vout->p_sys->p_display );
        }
    }
}

/*****************************************************************************
 * DisableXScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableXScreenSaver
 *****************************************************************************/
static void DisableXScreenSaver( vout_thread_t *p_vout )
{
    int dummy;

    /* Save screen saver informations */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    XSetScreenSaver( p_vout->p_sys->p_display, 0,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );

    /* Disable DPMS */
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        CARD16 dummy;
        /* Save DPMS current state */
        DPMSInfo( p_vout->p_sys->p_display, &dummy,
                  &p_vout->p_sys->b_ss_dpms );
        DPMSDisable( p_vout->p_sys->p_display );
   }
}

/*****************************************************************************
 * CreateCursor: create a blank mouse pointer
 *****************************************************************************/
static void CreateCursor( vout_thread_t *p_vout )
{
    XColor cursor_color;

    p_vout->p_sys->cursor_pixmap =
        XCreatePixmap( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       1, 1, 1 );

    XParseColor( p_vout->p_sys->p_display,
                 XCreateColormap( p_vout->p_sys->p_display,
                                  DefaultRootWindow(
                                                    p_vout->p_sys->p_display ),
                                  DefaultVisual(
                                                p_vout->p_sys->p_display,
                                                p_vout->p_sys->i_screen ),
                                  AllocNone ),
                 "black", &cursor_color );

    p_vout->p_sys->blank_cursor =
        XCreatePixmapCursor( p_vout->p_sys->p_display,
                             p_vout->p_sys->cursor_pixmap,
                             p_vout->p_sys->cursor_pixmap,
                             &cursor_color, &cursor_color, 1, 1 );
}

/*****************************************************************************
 * DestroyCursor: destroy the blank mouse pointer
 *****************************************************************************/
static void DestroyCursor( vout_thread_t *p_vout )
{
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );
}

/*****************************************************************************
 * ToggleCursor: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by setting the pointer
 * sprite to a blank one. To show it again, we disable the sprite.
 *****************************************************************************/
static void ToggleCursor( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_mouse_pointer_visible )
    {
        XDefineCursor( p_vout->p_sys->p_display,
                       p_vout->p_sys->window,
                       p_vout->p_sys->blank_cursor );
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else
    {
        XUndefineCursor( p_vout->p_sys->p_display, p_vout->p_sys->window );
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
}

