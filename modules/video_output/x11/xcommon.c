/*****************************************************************************
 * xcommon.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc_keys.h>

#ifdef HAVE_MACHINE_PARAM_H
    /* BSD */
#   include <machine/param.h>
#   include <sys/types.h>                                  /* typedef ushort */
#   include <sys/ipc.h>
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_SYS_SHM_H
#   include <sys/shm.h>                                /* shmget(), shmctl() */
#endif

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef HAVE_SYS_SHM_H
#   include <X11/extensions/XShm.h>
#endif
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#ifdef MODULE_NAME_IS_xvideo
#   include <X11/extensions/Xv.h>
#   include <X11/extensions/Xvlib.h>
#endif

#ifdef MODULE_NAME_IS_glx
#   include <GL/glx.h>
#endif

#ifdef HAVE_XINERAMA
#   include <X11/extensions/Xinerama.h>
#endif

#include "xcommon.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_(Activate)   ( vlc_object_t * );
void E_(Deactivate) ( vlc_object_t * );

static int  InitVideo      ( vout_thread_t * );
static void EndVideo       ( vout_thread_t * );
static void DisplayVideo   ( vout_thread_t *, picture_t * );
static int  ManageVideo    ( vout_thread_t * );
static int  Control        ( vout_thread_t *, int, va_list );

static int  InitDisplay    ( vout_thread_t * );

static int  CreateWindow   ( vout_thread_t *, x11_window_t * );
static void DestroyWindow  ( vout_thread_t *, x11_window_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static IMAGE_TYPE *CreateImage    ( vout_thread_t *,
                                    Display *, EXTRA_ARGS, int, int );
#ifdef HAVE_SYS_SHM_H
static IMAGE_TYPE *CreateShmImage ( vout_thread_t *,
                                    Display *, EXTRA_ARGS_SHM, int, int );
static vlc_bool_t b_shm = VLC_TRUE;
#endif

static void ToggleFullScreen      ( vout_thread_t * );

static void EnableXScreenSaver    ( vout_thread_t * );
static void DisableXScreenSaver   ( vout_thread_t * );

static void CreateCursor   ( vout_thread_t * );
static void DestroyCursor  ( vout_thread_t * );
static void ToggleCursor   ( vout_thread_t * );

#ifdef MODULE_NAME_IS_xvideo
static int  XVideoGetPort    ( vout_thread_t *, vlc_fourcc_t, vlc_fourcc_t * );
static void XVideoReleasePort( vout_thread_t *, int );
#endif

#ifdef MODULE_NAME_IS_x11
static void SetPalette     ( vout_thread_t *,
                             uint16_t *, uint16_t *, uint16_t * );
#endif

static void TestNetWMSupport( vout_thread_t * );
static int ConvertKey( int );

static int WindowOnTop( vout_thread_t *, vlc_bool_t );

static int X11ErrorHandler( Display *, XErrorEvent * );

/*****************************************************************************
 * Activate: allocate X11 video thread output method
 *****************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
int E_(Activate) ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *        psz_display;
    vlc_value_t   val;

#ifdef MODULE_NAME_IS_xvideo
    char *       psz_chroma;
    vlc_fourcc_t i_chroma = 0;
    vlc_bool_t   b_chroma = 0;
#endif

    p_vout->pf_init = InitVideo;
    p_vout->pf_end = EndVideo;
    p_vout->pf_manage = ManageVideo;
    p_vout->pf_render = NULL;
    p_vout->pf_display = DisplayVideo;
    p_vout->pf_control = Control;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    vlc_mutex_init( p_vout, &p_vout->p_sys->lock );

    /* Open display, using the "display" config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPsz( p_vout, MODULE_STRING "-display" );

    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                          /* error */
    {
        msg_Err( p_vout, "cannot open display %s",
                         XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        if( psz_display ) free( psz_display );
        return VLC_EGENERIC;
    }
    if( psz_display ) free( psz_display );

    /* Replace error handler so we can intercept some non-fatal errors */
    XSetErrorHandler( X11ErrorHandler );

    /* Get a screen ID matching the XOpenDisplay return value */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

#ifdef MODULE_NAME_IS_xvideo
    psz_chroma = config_GetPsz( p_vout, "xvideo-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) >= 4 )
        {
            /* Do not use direct assignment because we are not sure of the
             * alignment. */
            memcpy(&i_chroma, psz_chroma, 4);
            b_chroma = 1;
        }

        free( psz_chroma );
    }

    if( b_chroma )
    {
        msg_Dbg( p_vout, "forcing chroma 0x%.8x (%4.4s)",
                 i_chroma, (char*)&i_chroma );
    }
    else
    {
        i_chroma = p_vout->render.i_chroma;
    }

    /* Check that we have access to an XVideo port providing this chroma */
    p_vout->p_sys->i_xvport = XVideoGetPort( p_vout, VLC2X11_FOURCC(i_chroma),
                                             &p_vout->output.i_chroma );
    if( p_vout->p_sys->i_xvport < 0 )
    {
        /* If a specific chroma format was requested, then we don't try to
         * be cleverer than the user. He knew pretty well what he wanted. */
        if( b_chroma )
        {
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* It failed, but it's not completely lost ! We try to open an
         * XVideo port for an YUY2 picture. We'll need to do an YUV
         * conversion, but at least it has got scaling. */
        p_vout->p_sys->i_xvport =
                        XVideoGetPort( p_vout, X11_FOURCC('Y','U','Y','2'),
                                               &p_vout->output.i_chroma );
        if( p_vout->p_sys->i_xvport < 0 )
        {
            /* It failed, but it's not completely lost ! We try to open an
             * XVideo port for a simple 16bpp RGB picture. We'll need to do
             * an YUV conversion, but at least it has got scaling. */
            p_vout->p_sys->i_xvport =
                            XVideoGetPort( p_vout, X11_FOURCC('R','V','1','6'),
                                                   &p_vout->output.i_chroma );
            if( p_vout->p_sys->i_xvport < 0 )
            {
                XCloseDisplay( p_vout->p_sys->p_display );
                free( p_vout->p_sys );
                return VLC_EGENERIC;
            }
        }
    }
    p_vout->output.i_chroma = X112VLC_FOURCC(p_vout->output.i_chroma);
#endif

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Set main window's size */
    p_vout->p_sys->original_window.i_width = p_vout->i_window_width;
    p_vout->p_sys->original_window.i_height = p_vout->i_window_height;
    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout, &p_vout->p_sys->original_window ) )
    {
        msg_Err( p_vout, "cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Open and initialize device. */
    if( InitDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot initialize X11 display" );
        DestroyCursor( p_vout );
        DestroyWindow( p_vout, &p_vout->p_sys->original_window );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Disable screen saver */
    DisableXScreenSaver( p_vout );

    /* Misc init */
    p_vout->p_sys->b_altfullscreen = 0;
    p_vout->p_sys->i_time_button_last_pressed = 0;

    TestNetWMSupport( p_vout );

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_Get( p_vout, "video-on-top", &val );
    var_Set( p_vout, "video-on-top", val );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by Open
 *****************************************************************************/
void E_(Deactivate) ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* If the fullscreen window is still open, close it */
    if( p_vout->b_fullscreen )
    {
        ToggleFullScreen( p_vout );
    }

    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
    }

#ifdef MODULE_NAME_IS_x11
    /* Destroy colormap */
    if( XDefaultDepth(p_vout->p_sys->p_display, p_vout->p_sys->i_screen) == 8 )
    {
        XFreeColormap( p_vout->p_sys->p_display, p_vout->p_sys->colormap );
    }
#elif defined(MODULE_NAME_IS_xvideo)
    XVideoReleasePort( p_vout, p_vout->p_sys->i_xvport );
#endif

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout, &p_vout->p_sys->original_window );

    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    vlc_mutex_destroy( &p_vout->p_sys->lock );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * InitVideo: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int InitVideo( vout_thread_t *p_vout )
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

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('R','V','1','5'):
            p_vout->output.i_rmask = 0x001f;
            p_vout->output.i_gmask = 0x07e0;
            p_vout->output.i_bmask = 0xf800;
            break;
        case VLC_FOURCC('R','V','1','6'):
            p_vout->output.i_rmask = 0x001f;
            p_vout->output.i_gmask = 0x03e0;
            p_vout->output.i_bmask = 0x7c00;
            break;
    }

#elif defined(MODULE_NAME_IS_x11)
    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    switch( p_vout->p_sys->i_screen_depth )
    {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2'); break;
        case 15:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5'); break;
        case 16:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6'); break;
        case 24:
        case 32:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2'); break;
        default:
            msg_Err( p_vout, "unknown screen depth %i",
                     p_vout->p_sys->i_screen_depth );
            return VLC_SUCCESS;
    }

    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                       p_vout->p_sys->p_win->i_height,
                       &i_index, &i_index,
                       &p_vout->fmt_out.i_visible_width,
                       &p_vout->fmt_out.i_visible_height );

    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;

    p_vout->output.i_width = p_vout->fmt_out.i_width =
        p_vout->fmt_out.i_visible_width * p_vout->fmt_in.i_width /
        p_vout->fmt_in.i_visible_width;
    p_vout->output.i_height = p_vout->fmt_out.i_height =
        p_vout->fmt_out.i_visible_height * p_vout->fmt_in.i_height /
        p_vout->fmt_in.i_visible_height;
    p_vout->fmt_out.i_x_offset =
        p_vout->fmt_out.i_visible_width * p_vout->fmt_in.i_x_offset /
        p_vout->fmt_in.i_visible_width;
    p_vout->fmt_out.i_y_offset =
        p_vout->fmt_out.i_visible_height * p_vout->fmt_in.i_y_offset /
        p_vout->fmt_in.i_visible_height;

    p_vout->fmt_out.i_sar_num = p_vout->fmt_out.i_sar_den = 1;
    p_vout->output.i_aspect = p_vout->fmt_out.i_aspect =
        p_vout->fmt_out.i_width * VOUT_ASPECT_FACTOR /p_vout->fmt_out.i_height;

    msg_Dbg( p_vout, "x11 image size %ix%i (%i,%i,%ix%i)",
             p_vout->fmt_out.i_width, p_vout->fmt_out.i_height,
             p_vout->fmt_out.i_x_offset, p_vout->fmt_out.i_y_offset,
             p_vout->fmt_out.i_visible_width,
             p_vout->fmt_out.i_visible_height );
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

    if( p_vout->output.i_chroma == VLC_FOURCC('Y','V','1','2') )
    {
        /* U and V inverted compared to I420
         * Fixme: this should be handled by the vout core */
        p_vout->output.i_chroma = VLC_FOURCC('I','4','2','0');
        p_vout->fmt_out.i_chroma = VLC_FOURCC('I','4','2','0');
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to X11 server.
 * (The Xv extension takes care of "double-buffering".)
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                       p_vout->p_sys->p_win->i_height,
                       &i_x, &i_y, &i_width, &i_height );

    vlc_mutex_lock( &p_vout->p_sys->lock );

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        /* Display rendered image using shared memory extension */
#   ifdef MODULE_NAME_IS_xvideo
        XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                       p_vout->p_sys->p_win->video_window,
                       p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                       p_vout->fmt_out.i_x_offset,
                       p_vout->fmt_out.i_y_offset,
                       p_vout->fmt_out.i_visible_width,
                       p_vout->fmt_out.i_visible_height,
                       0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height,
                       False /* Don't put True here or you'll waste your CPU */ );
#   else
        XShmPutImage( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->video_window,
                      p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                      p_vout->fmt_out.i_x_offset,
                      p_vout->fmt_out.i_y_offset,
                      0 /*dest_x*/, 0 /*dest_y*/,
                      p_vout->fmt_out.i_visible_width,
                      p_vout->fmt_out.i_visible_height,
                      False /* Don't put True here ! */ );
#   endif
    }
    else
#endif /* HAVE_SYS_SHM_H */
    {
        /* Use standard XPutImage -- this is gonna be slow ! */
#ifdef MODULE_NAME_IS_xvideo
        XvPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                    p_vout->p_sys->p_win->video_window,
                    p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                    p_vout->fmt_out.i_x_offset,
                    p_vout->fmt_out.i_y_offset,
                    p_vout->fmt_out.i_visible_width,
                    p_vout->fmt_out.i_visible_height,
                    0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height );
#else
        XPutImage( p_vout->p_sys->p_display,
                   p_vout->p_sys->p_win->video_window,
                   p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                   p_vout->fmt_out.i_x_offset,
                   p_vout->fmt_out.i_y_offset,
                   0 /*dest_x*/, 0 /*dest_y*/,
                   p_vout->fmt_out.i_visible_width,
                   p_vout->fmt_out.i_visible_height );
#endif
    }

    /* Make sure the command is sent now - do NOT use XFlush !*/
    XSync( p_vout->p_sys->p_display, False );

    vlc_mutex_unlock( &p_vout->p_sys->lock );
}

/*****************************************************************************
 * ManageVideo: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int ManageVideo( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    vlc_value_t val;

    vlc_mutex_lock( &p_vout->p_sys->lock );

    /* Handle events from the owner window */
    if( p_vout->p_sys->p_win->owner_window )
    {
        while( XCheckWindowEvent( p_vout->p_sys->p_display,
                                  p_vout->p_sys->p_win->owner_window,
                                  StructureNotifyMask, &xevent ) == True )
        {
            /* ConfigureNotify event: prepare  */
            if( xevent.type == ConfigureNotify )
            {
                /* Update dimensions */
                XResizeWindow( p_vout->p_sys->p_display,
                               p_vout->p_sys->p_win->base_window,
                               xevent.xconfigure.width,
                               xevent.xconfigure.height );
            }
        }
    }

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */

    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->p_win->base_window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
        {
            if( (unsigned int)xevent.xconfigure.width
                   != p_vout->p_sys->p_win->i_width
              || (unsigned int)xevent.xconfigure.height
                    != p_vout->p_sys->p_win->i_height )
            {
                /* Update dimensions */
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
                p_vout->p_sys->p_win->i_width = xevent.xconfigure.width;
                p_vout->p_sys->p_win->i_height = xevent.xconfigure.height;
            }
        }
        /* Keyboard event */
        else if( xevent.type == KeyPress )
        {
            unsigned int state = xevent.xkey.state;
            KeySym x_key_symbol;
            char i_key;                                   /* ISO Latin-1 key */

            /* We may have keys like F1 trough F12, ESC ... */
            x_key_symbol = XKeycodeToKeysym( p_vout->p_sys->p_display,
                                             xevent.xkey.keycode, 0 );
            val.i_int = ConvertKey( (int)x_key_symbol );

            xevent.xkey.state &= ~ShiftMask;
            xevent.xkey.state &= ~ControlMask;
            xevent.xkey.state &= ~Mod1Mask;

            if( !val.i_int &&
                XLookupString( &xevent.xkey, &i_key, 1, NULL, NULL ) )
            {
                /* "Normal Keys"
                 * The reason why I use this instead of XK_0 is that
                 * with XLookupString, we don't have to care about
                 * keymaps. */
                val.i_int = i_key;
            }

            if( val.i_int )
            {
                if( state & ShiftMask )
                {
                    val.i_int |= KEY_MODIFIER_SHIFT;
                }
                if( state & ControlMask )
                {
                    val.i_int |= KEY_MODIFIER_CTRL;
                }
                if( state & Mod1Mask )
                {
                    val.i_int |= KEY_MODIFIER_ALT;
                }
                var_Set( p_vout->p_vlc, "key-pressed", val );
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 1;
                    var_Set( p_vout, "mouse-button-down", val );

                    /* detect double-clicks */
                    if( ( ((XButtonEvent *)&xevent)->time -
                          p_vout->p_sys->i_time_button_last_pressed ) < 300 )
                    {
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    }

                    p_vout->p_sys->i_time_button_last_pressed =
                        ((XButtonEvent *)&xevent)->time;
                    break;
                case Button2:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 2;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button3:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 4;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~1;
                    var_Set( p_vout, "mouse-button-down", val );

                    val.b_bool = VLC_TRUE;
                    var_Set( p_vout, "mouse-clicked", val );
                    break;

                case Button2:
                    {
                        playlist_t *p_playlist;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~2;
                        var_Set( p_vout, "mouse-button-down", val );

                        p_playlist = vlc_object_find( p_vout,
                                                      VLC_OBJECT_PLAYLIST,
                                                      FIND_ANYWHERE );
                        if( p_playlist != NULL )
                        {
                            vlc_value_t val;
                            var_Get( p_playlist, "intf-show", &val );
                            val.b_bool = !val.b_bool;
                            var_Set( p_playlist, "intf-show", val );
                            vlc_object_release( p_playlist );
                        }
                    }
                    break;

                case Button3:
                    {
                        intf_thread_t *p_intf;
                        playlist_t *p_playlist;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~4;
                        var_Set( p_vout, "mouse-button-down", val );
                        p_intf = vlc_object_find( p_vout, VLC_OBJECT_INTF,
                                                          FIND_ANYWHERE );
                        if( p_intf )
                        {
                            p_intf->b_menu_change = 1;
                            vlc_object_release( p_intf );
                        }

                        p_playlist = vlc_object_find( p_vout,
                                                      VLC_OBJECT_PLAYLIST,
                                                      FIND_ANYWHERE );
                        if( p_playlist != NULL )
                        {
                            vlc_value_t val; val.b_bool = VLC_TRUE;
                            var_Set( p_playlist, "intf-popupmenu", val );
                            vlc_object_release( p_playlist );
                        }
                    }
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            int i_width, i_height, i_x, i_y;
            vlc_value_t val;

            /* somewhat different use for vout_PlacePicture:
             * here the values are needed to give to mouse coordinates
             * in the original picture space */
            vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                               p_vout->p_sys->p_win->i_height,
                               &i_x, &i_y, &i_width, &i_height );

            val.i_int = ( xevent.xmotion.x - i_x )
                         * p_vout->render.i_width / i_width;
            var_Set( p_vout, "mouse-x", val );
            val.i_int = ( xevent.xmotion.y - i_y )
                         * p_vout->render.i_height / i_height;
            var_Set( p_vout, "mouse-y", val );

            val.b_bool = VLC_TRUE;
            var_Set( p_vout, "mouse-moved", val );

            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout );
            }
        }
        else if( xevent.type == ReparentNotify /* XXX: why do we get this? */
                  || xevent.type == MapNotify
                  || xevent.type == UnmapNotify )
        {
            /* Ignore these events */
        }
        else /* Other events */
        {
            msg_Warn( p_vout, "unhandled event %d received", xevent.type );
        }
    }

    /* Handle events for video output sub-window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->p_win->video_window,
                              ExposureMask, &xevent ) == True )
    {
        /* Window exposed (only handled if stream playback is paused) */
        if( xevent.type == Expose )
        {
            if( ((XExposeEvent *)&xevent)->count == 0 )
            {
                /* (if this is the last a collection of expose events...) */
#if 0
                if( p_vout->p_vlc->p_input_bank->pp_input[0] != NULL )
                {
                    if( PAUSE_S == p_vout->p_vlc->p_input_bank->pp_input[0]
                                                   ->stream.control.i_status )
                    {
                        /* XVideoDisplay( p_vout )*/;
                    }
                }
#endif
            }
        }
    }

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_vout->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_vout->p_sys->p_win->wm_protocols)
               && ((Atom)xevent.xclient.data.l[0]
                     == p_vout->p_sys->p_win->wm_delete_window ) )
        {
            /* the user wants to close the window */
            playlist_t * p_playlist =
                (playlist_t *)vlc_object_find( p_vout, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
            if( p_playlist != NULL )
            {
                playlist_Stop( p_playlist );
                vlc_object_release( p_playlist );
            }
        }
    }

    /*
     * Fullscreen Change
     */
    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        vlc_value_t val;

        /* Update the object variable and trigger callback */
        val.b_bool = !p_vout->b_fullscreen;
        var_Set( p_vout, "fullscreen", val );

        ToggleFullScreen( p_vout );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /*
     * Size change
     *
     * (Needs to be placed after VOUT_FULLSREEN_CHANGE because we can activate
     *  the size flag inside the fullscreen routine)
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        int i_width, i_height, i_x, i_y;

        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

#ifdef MODULE_NAME_IS_x11
        /* We need to signal the vout thread about the size change because it
         * is doing the rescaling */
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
#endif

        vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                           p_vout->p_sys->p_win->i_height,
                           &i_x, &i_y, &i_width, &i_height );

        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->video_window,
                           i_x, i_y, i_width, i_height );
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

    vlc_mutex_unlock( &p_vout->p_sys->lock );

    return 0;
}

/*****************************************************************************
 * EndVideo: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void EndVideo( vout_thread_t *p_vout )
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
static int CreateWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;

    vlc_bool_t              b_expose = VLC_FALSE;
    vlc_bool_t              b_configure_notify = VLC_FALSE;
    vlc_bool_t              b_map_notify = VLC_FALSE;
    vlc_value_t             val;

    /* Prepare window manager hints and properties */
    p_win->wm_protocols =
             XInternAtom( p_vout->p_sys->p_display, "WM_PROTOCOLS", True );
    p_win->wm_delete_window =
             XInternAtom( p_vout->p_sys->p_display, "WM_DELETE_WINDOW", True );

    /* Never have a 0-pixel-wide window */
    xsize_hints.min_width = 2;
    xsize_hints.min_height = 1;

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    if( !p_vout->b_fullscreen )
    {
        p_win->owner_window =
            (Window)vout_RequestWindow( p_vout, &p_win->i_x, &p_win->i_y,
                                        &p_win->i_width, &p_win->i_height );

        xsize_hints.base_width  = xsize_hints.width = p_win->i_width;
        xsize_hints.base_height = xsize_hints.height = p_win->i_height;
        xsize_hints.flags       = PSize | PMinSize;

        if( p_win->i_x >=0 || p_win->i_y >= 0 )
        {
            xsize_hints.x = p_win->i_x;
            xsize_hints.y = p_win->i_y;
            xsize_hints.flags |= PPosition;
        }
    }
    else
    {
        /* Fullscreen window size and position */
        p_win->owner_window = 0;
        p_win->i_x = p_win->i_y = 0;
        p_win->i_width =
            DisplayWidth( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );
        p_win->i_height =
            DisplayHeight( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );
    }

    if( !p_win->owner_window )
    {
        /* Create the window and set hints - the window must receive
         * ConfigureNotify events, and until it is displayed, Expose and
         * MapNotify events. */

        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           DefaultRootWindow( p_vout->p_sys->p_display ),
                           p_win->i_x, p_win->i_y,
                           p_win->i_width, p_win->i_height,
                           0,
                           0, InputOutput, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );

        if( !p_vout->b_fullscreen )
        {
            /* Set window manager hints and properties: size hints, command,
             * window's name, and accepted protocols */
            XSetWMNormalHints( p_vout->p_sys->p_display,
                               p_win->base_window, &xsize_hints );
            XSetCommand( p_vout->p_sys->p_display, p_win->base_window,
                         p_vout->p_vlc->ppsz_argv, p_vout->p_vlc->i_argc );

            if( !var_GetBool( p_vout, "video-deco") )
            {
                Atom prop;
                mwmhints_t mwmhints;

                mwmhints.flags = MWM_HINTS_DECORATIONS;
                mwmhints.decorations = False;

                prop = XInternAtom( p_vout->p_sys->p_display, "_MOTIF_WM_HINTS",
                                    False );

                XChangeProperty( p_vout->p_sys->p_display,
                                 p_win->base_window,
                                 prop, prop, 32, PropModeReplace,
                                 (unsigned char *)&mwmhints,
                                 PROP_MWM_HINTS_ELEMENTS );
            }
            else
            {
                 var_Get( p_vout, "video-title", &val );
                 if( !val.psz_string || !*val.psz_string )
                 {
                    XStoreName( p_vout->p_sys->p_display, p_win->base_window,
#ifdef MODULE_NAME_IS_x11
                                VOUT_TITLE " (X11 output)"
#elif defined(MODULE_NAME_IS_glx)
                                VOUT_TITLE " (GLX output)"
#else
                                VOUT_TITLE " (XVideo output)"
#endif
                      );
                }
                else
                {
                    XStoreName( p_vout->p_sys->p_display,
                               p_win->base_window, val.psz_string );
                }
            }
        }
    }
    else
    {
        Window dummy1;
        unsigned int dummy2, dummy3;

        /* Select events we are interested in. */
        XSelectInput( p_vout->p_sys->p_display, p_win->owner_window,
                      StructureNotifyMask );

        /* Get the parent window's geometry information */
        XGetGeometry( p_vout->p_sys->p_display, p_win->owner_window,
                      &dummy1, &dummy2, &dummy3,
                      &p_win->i_width,
                      &p_win->i_height,
                      &dummy2, &dummy3 );

        /* We are already configured */
        b_configure_notify = VLC_TRUE;

        /* From man XSelectInput: only one client at a time can select a
         * ButtonPress event, so we need to open a new window anyway. */
        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           p_win->owner_window,
                           0, 0,
                           p_win->i_width, p_win->i_height,
                           0,
                           0, CopyFromParent, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );
    }

    if( (p_win->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_win->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_sys->p_display, p_win->base_window,
                             &p_win->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        msg_Warn( p_vout, "missing or bad window manager" );
    }

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_win->gc = XCreateGC( p_vout->p_sys->p_display,
                           p_win->base_window,
                           GCGraphicsExposures, &xgcvalues );

    /* Send orders to server, and wait until window is displayed - three
     * events must be received: a MapNotify event, an Expose event allowing
     * drawing in the window, and a ConfigureNotify to get the window
     * dimensions. Once those events have been received, only
     * ConfigureNotify events need to be received. */
    XMapWindow( p_vout->p_sys->p_display, p_win->base_window );
    do
    {
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask |
                      ExposureMask, &xevent);
        if( (xevent.type == Expose)
            && (xevent.xexpose.window == p_win->base_window) )
        {
            b_expose = VLC_TRUE;
            /* ConfigureNotify isn't sent if there isn't a window manager.
             * Expose should be the last event to be received so it should
             * be fine to assume we won't receive it anymore. */
            b_configure_notify = VLC_TRUE;
        }
        else if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_map_notify = VLC_TRUE;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_win->base_window) )
        {
            b_configure_notify = VLC_TRUE;
            p_win->i_width = xevent.xconfigure.width;
            p_win->i_height = xevent.xconfigure.height;
        }
    } while( !( b_expose && b_configure_notify && b_map_notify ) );

    XSelectInput( p_vout->p_sys->p_display, p_win->base_window,
                  StructureNotifyMask | KeyPressMask |
                  ButtonPressMask | ButtonReleaseMask |
                  PointerMotionMask );

#ifdef MODULE_NAME_IS_x11
    if( XDefaultDepth(p_vout->p_sys->p_display, p_vout->p_sys->i_screen) == 8 )
    {
        /* Allocate a new palette */
        p_vout->p_sys->colormap =
            XCreateColormap( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             DefaultVisual( p_vout->p_sys->p_display,
                                            p_vout->p_sys->i_screen ),
                             AllocAll );

        xwindow_attributes.colormap = p_vout->p_sys->colormap;
        XChangeWindowAttributes( p_vout->p_sys->p_display, p_win->base_window,
                                 CWColormap, &xwindow_attributes );
    }
#endif

    /* Create video output sub-window. */
    p_win->video_window =  XCreateSimpleWindow(
                                      p_vout->p_sys->p_display,
                                      p_win->base_window, 0, 0,
                                      p_win->i_width, p_win->i_height,
                                      0,
                                      BlackPixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ),
                                      WhitePixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ) );

    XSetWindowBackground( p_vout->p_sys->p_display, p_win->video_window,
                          BlackPixel( p_vout->p_sys->p_display,
                                      p_vout->p_sys->i_screen ) );

    XMapWindow( p_vout->p_sys->p_display, p_win->video_window );
    XSelectInput( p_vout->p_sys->p_display, p_win->video_window,
                  ExposureMask );

    /* make sure the video window will be centered in the next ManageVideo() */
    p_vout->i_changes |= VOUT_SIZE_CHANGE;

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
        ToggleCursor( p_vout );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    /* At this stage, the window is open, displayed, and ready to
     * receive data */
    p_vout->p_sys->p_win = p_win;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyWindow: destroy the window
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    if( p_win->video_window != None )
        XDestroyWindow( p_vout->p_sys->p_display, p_win->video_window );

    XFreeGC( p_vout->p_sys->p_display, p_win->gc );

    XUnmapWindow( p_vout->p_sys->p_display, p_win->base_window );
    XDestroyWindow( p_vout->p_sys->p_display, p_win->base_window );

    if( p_win->owner_window )
        vout_ReleaseWindow( p_vout, (void *)p_win->owner_window );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifndef MODULE_NAME_IS_glx

#ifdef MODULE_NAME_IS_xvideo
    int i_plane;
#endif

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    /* Fill in picture_t fields */
    vout_InitPicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                      p_vout->output.i_width, p_vout->output.i_height,
                      p_vout->output.i_aspect );

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        /* Create image using XShm extension */
        p_pic->p_sys->p_image =
            CreateShmImage( p_vout, p_vout->p_sys->p_display,
#   ifdef MODULE_NAME_IS_xvideo
                            p_vout->p_sys->i_xvport, 
                            VLC2X11_FOURCC(p_vout->output.i_chroma),
#   else
                            p_vout->p_sys->p_visual,
                            p_vout->p_sys->i_screen_depth,
#   endif
                            &p_pic->p_sys->shminfo,
                            p_vout->output.i_width, p_vout->output.i_height );
    }

    if( !p_vout->p_sys->b_shm || !p_pic->p_sys->p_image )
#endif /* HAVE_SYS_SHM_H */
    {
        /* Create image without XShm extension */
        p_pic->p_sys->p_image =
            CreateImage( p_vout, p_vout->p_sys->p_display,
#ifdef MODULE_NAME_IS_xvideo
                         p_vout->p_sys->i_xvport, 
                         VLC2X11_FOURCC(p_vout->output.i_chroma),
                         p_pic->format.i_bits_per_pixel,
#else
                         p_vout->p_sys->p_visual,
                         p_vout->p_sys->i_screen_depth,
                         p_vout->p_sys->i_bytes_per_pixel,
#endif
                         p_vout->output.i_width, p_vout->output.i_height );

#ifdef HAVE_SYS_SHM_H
        if( p_pic->p_sys->p_image && p_vout->p_sys->b_shm )
        {
            msg_Warn( p_vout, "couldn't create SHM image, disabling SHM." );
            p_vout->p_sys->b_shm = VLC_FALSE;
        }
#endif /* HAVE_SYS_SHM_H */
    }

    if( p_pic->p_sys->p_image == NULL )
    {
        free( p_pic->p_sys );
        return -1;
    }

    switch( p_vout->output.i_chroma )
    {
#ifdef MODULE_NAME_IS_xvideo
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('Y','2','1','1'):
        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('U','Y','V','Y'):
        case VLC_FOURCC('R','V','1','5'):
        case VLC_FOURCC('R','V','1','6'):
        case VLC_FOURCC('R','V','2','4'): /* Fixme: pixel pitch == 4 ? */
        case VLC_FOURCC('R','V','3','2'):

            for( i_plane = 0; i_plane < p_pic->p_sys->p_image->num_planes;
                 i_plane++ )
            {
                p_pic->p[i_plane].p_pixels = p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[i_plane];
                p_pic->p[i_plane].i_pitch =
                    p_pic->p_sys->p_image->pitches[i_plane];
            }
            if( p_vout->output.i_chroma == VLC_FOURCC('Y','V','1','2') )
            {
                /* U and V inverted compared to I420
                 * Fixme: this should be handled by the vout core */
                p_pic->U_PIXELS = p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[2];
                p_pic->V_PIXELS = p_pic->p_sys->p_image->data
                    + p_pic->p_sys->p_image->offsets[1];
            }
            break;

#else
        case VLC_FOURCC('R','G','B','2'):
        case VLC_FOURCC('R','V','1','6'):
        case VLC_FOURCC('R','V','1','5'):
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):

            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_visible_lines = p_pic->p_sys->p_image->height;
            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;

            /* p_pic->p->i_pixel_pitch = 4 for RV24 but this should be set
             * properly by vout_InitPicture() */
            p_pic->p->i_visible_pitch = p_pic->p->i_pixel_pitch
                                         * p_pic->p_sys->p_image->width;
            break;
#endif

        default:
            /* Unknown chroma, tell the guy to get lost */
            IMAGE_FREE( p_pic->p_sys->p_image );
            free( p_pic->p_sys );
            msg_Err( p_vout, "never heard of chroma 0x%.8x (%4.4s)",
                     p_vout->output.i_chroma, (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return -1;
    }

#endif /* !MODULE_NAME_IS_glx */

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
    /* The order of operations is correct */
#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        XShmDetach( p_vout->p_sys->p_display, &p_pic->p_sys->shminfo );
        IMAGE_FREE( p_pic->p_sys->p_image );

        shmctl( p_pic->p_sys->shminfo.shmid, IPC_RMID, 0 );
        if( shmdt( p_pic->p_sys->shminfo.shmaddr ) )
        {
            msg_Err( p_vout, "cannot detach shared memory (%s)",
                             strerror(errno) );
        }
    }
    else
#endif
    {
        IMAGE_FREE( p_pic->p_sys->p_image );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    free( p_pic->p_sys );
}

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    Atom prop;
    XEvent xevent;
    mwmhints_t mwmhints;
    XSetWindowAttributes attributes;

#ifdef HAVE_XINERAMA
    int i_d1, i_d2;
#endif

    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "entering fullscreen mode" );

        p_vout->p_sys->b_altfullscreen =
            config_GetInt( p_vout, MODULE_STRING "-altfullscreen" );

        XUnmapWindow( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->base_window );

        p_vout->p_sys->p_win = &p_vout->p_sys->fullscreen_window;

        CreateWindow( p_vout, p_vout->p_sys->p_win );
        XDestroyWindow( p_vout->p_sys->p_display,
                        p_vout->p_sys->fullscreen_window.video_window );
        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->original_window.video_window,
                         p_vout->p_sys->fullscreen_window.base_window, 0, 0 );
        p_vout->p_sys->fullscreen_window.video_window =
            p_vout->p_sys->original_window.video_window;

        /* To my knowledge there are two ways to create a borderless window.
         * There's the generic way which is to tell x to bypass the window
         * manager, but this creates problems with the focus of other
         * applications.
         * The other way is to use the motif property "_MOTIF_WM_HINTS" which
         * luckily seems to be supported by most window managers. */
        if( !p_vout->p_sys->b_altfullscreen )
        {
            mwmhints.flags = MWM_HINTS_DECORATIONS;
            mwmhints.decorations = False;

            prop = XInternAtom( p_vout->p_sys->p_display, "_MOTIF_WM_HINTS",
                                False );
            XChangeProperty( p_vout->p_sys->p_display,
                             p_vout->p_sys->p_win->base_window,
                             prop, prop, 32, PropModeReplace,
                             (unsigned char *)&mwmhints,
                             PROP_MWM_HINTS_ELEMENTS );
        }
        else
        {
            /* brute force way to remove decorations */
            attributes.override_redirect = True;
            XChangeWindowAttributes( p_vout->p_sys->p_display,
                                     p_vout->p_sys->p_win->base_window,
                                     CWOverrideRedirect,
                                     &attributes);

            /* Make sure the change is effective */
            XReparentWindow( p_vout->p_sys->p_display,
                             p_vout->p_sys->p_win->base_window,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             0, 0 );
        }

        if( p_vout->p_sys->b_net_wm_state_fullscreen )
        {
            XClientMessageEvent event;

            memset( &event, 0, sizeof( XClientMessageEvent ) );

            event.type = ClientMessage;
            event.message_type = p_vout->p_sys->net_wm_state;
            event.display = p_vout->p_sys->p_display;
            event.window = p_vout->p_sys->p_win->base_window;
            event.format = 32;
            event.data.l[ 0 ] = 1; /* set property */
            event.data.l[ 1 ] = p_vout->p_sys->net_wm_state_fullscreen;

            XSendEvent( p_vout->p_sys->p_display,
                        DefaultRootWindow( p_vout->p_sys->p_display ),
                        False, SubstructureRedirectMask,
                        (XEvent*)&event );
        }

        /* Make sure the change is effective */
        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->p_win->base_window,
                         DefaultRootWindow( p_vout->p_sys->p_display ),
                         0, 0 );

#ifdef HAVE_XINERAMA
        if( XineramaQueryExtension( p_vout->p_sys->p_display, &i_d1, &i_d2 ) &&
            XineramaIsActive( p_vout->p_sys->p_display ) )
        {
            XineramaScreenInfo *screens;   /* infos for xinerama */
            int i_num_screens;

            msg_Dbg( p_vout, "using XFree Xinerama extension");

#define SCREEN p_vout->p_sys->p_win->i_screen

            /* Get Information about Xinerama (num of screens) */
            screens = XineramaQueryScreens( p_vout->p_sys->p_display,
                                            &i_num_screens );

            if( !SCREEN )
                SCREEN = config_GetInt( p_vout,
                                        MODULE_STRING "-xineramascreen" );

            /* just check that user has entered a good value */
            if( SCREEN >= i_num_screens || SCREEN < 0 )
            {
                msg_Dbg( p_vout, "requested screen number invalid" );
                SCREEN = 0;
            }

            /* Get the X/Y upper left corner coordinate of the above screen */
            p_vout->p_sys->p_win->i_x = screens[SCREEN].x_org;
            p_vout->p_sys->p_win->i_y = screens[SCREEN].y_org;

            /* Set the Height/width to the screen resolution */
            p_vout->p_sys->p_win->i_width = screens[SCREEN].width;
            p_vout->p_sys->p_win->i_height = screens[SCREEN].height;

            XFree(screens);

#undef SCREEN

        }
        else
#endif
        {
            /* The window wasn't necessarily created at the requested size */
            p_vout->p_sys->p_win->i_x = p_vout->p_sys->p_win->i_y = 0;
            p_vout->p_sys->p_win->i_width =
                DisplayWidth( p_vout->p_sys->p_display,
                              p_vout->p_sys->i_screen );
            p_vout->p_sys->p_win->i_height =
                DisplayHeight( p_vout->p_sys->p_display,
                               p_vout->p_sys->i_screen );
        }

        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->base_window,
                           p_vout->p_sys->p_win->i_x,
                           p_vout->p_sys->p_win->i_y,
                           p_vout->p_sys->p_win->i_width,
                           p_vout->p_sys->p_win->i_height );
    }
    else
    {
        msg_Dbg( p_vout, "leaving fullscreen mode" );

        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->original_window.video_window,
                         p_vout->p_sys->original_window.base_window, 0, 0 );

        p_vout->p_sys->fullscreen_window.video_window = None;
        DestroyWindow( p_vout, &p_vout->p_sys->fullscreen_window );
        p_vout->p_sys->p_win = &p_vout->p_sys->original_window;

        XMapWindow( p_vout->p_sys->p_display,
                    p_vout->p_sys->p_win->base_window );
    }

    /* Unfortunately, using XSync() here is not enough to ensure the
     * window has already been mapped because the XMapWindow() request
     * has not necessarily been sent directly to our window (remember,
     * the call is first redirected to the window manager) */
    do
    {
        XWindowEvent( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->base_window,
                      StructureNotifyMask, &xevent );
    } while( xevent.type != MapNotify );

    /* Be careful, this can generate a BadMatch error if the window is not
     * already mapped by the server (see above) */
    XSetInputFocus(p_vout->p_sys->p_display,
                   p_vout->p_sys->p_win->base_window,
                   RevertToParent,
                   CurrentTime);

    /* signal that the size needs to be updated */
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
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Restore DPMS settings */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        if( p_vout->p_sys->b_ss_dpms )
        {
            DPMSEnable( p_vout->p_sys->p_display );
        }
    }
#endif
}

/*****************************************************************************
 * DisableXScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableXScreenSaver
 *****************************************************************************/
static void DisableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    /* Save screen saver information */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, 0,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Disable DPMS */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        CARD16 unused;
        /* Save DPMS current state */
        DPMSInfo( p_vout->p_sys->p_display, &unused,
                  &p_vout->p_sys->b_ss_dpms );
        DPMSDisable( p_vout->p_sys->p_display );
   }
#endif
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
                       p_vout->p_sys->p_win->base_window,
                       p_vout->p_sys->blank_cursor );
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else
    {
        XUndefineCursor( p_vout->p_sys->p_display,
                         p_vout->p_sys->p_win->base_window );
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
}

#ifdef MODULE_NAME_IS_xvideo
/*****************************************************************************
 * XVideoGetPort: get YUV12 port
 *****************************************************************************/
static int XVideoGetPort( vout_thread_t *p_vout,
                          vlc_fourcc_t i_chroma, vlc_fourcc_t *pi_newchroma )
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    int i_adaptor, i_num_adaptors, i_requested_adaptor;
    int i_selected_port;

    switch( XvQueryExtension( p_vout->p_sys->p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension" );
            return -1;

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc" );
            return -1;

        default:
            msg_Warn( p_vout, "XvQueryExtension failed" );
            return -1;
    }

    switch( XvQueryAdaptors( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension for XvQueryAdaptors" );
            return -1;

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc for XvQueryAdaptors" );
            return -1;

        default:
            msg_Warn( p_vout, "XvQueryAdaptors failed" );
            return -1;
    }

    i_selected_port = -1;
    i_requested_adaptor = config_GetInt( p_vout, "xvideo-adaptor" );

    for( i_adaptor = 0; i_adaptor < i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && i_adaptor != i_requested_adaptor )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( p_vout->p_sys->p_display,
                                        p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;

            /* If this is not the format we want, or at least a
             * similar one, forget it */
            if( !vout_ChromaCmp( p_formats[ i_format ].id, i_chroma ) )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < (int)(p_adaptor[i_adaptor].base_id
                                   + p_adaptor[i_adaptor].num_ports) )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( p_vout->p_sys->p_display, i_port, CurrentTime )
                     == Success )
                {
                    i_selected_port = i_port;
                    *pi_newchroma = p_formats[ i_format ].id;
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            msg_Dbg( p_vout, "adaptor %i, port %i, format 0x%x (%4.4s) %s",
                     i_adaptor, i_selected_port, p_formats[ i_format ].id,
                     (char *)&p_formats[ i_format ].id,
                     ( p_formats[ i_format ].format == XvPacked ) ?
                         "packed" : "planar" );

            /* Make sure XV_AUTOPAINT_COLORKEY is set */
            p_attr = XvQueryPortAttributes( p_vout->p_sys->p_display,
                                            i_selected_port,
                                            &i_num_attributes );

            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                if( !strcmp( p_attr[i_attr].name, "XV_AUTOPAINT_COLORKEY" ) )
                {
                    const Atom autopaint =
                        XInternAtom( p_vout->p_sys->p_display,
                                     "XV_AUTOPAINT_COLORKEY", False );
                    XvSetPortAttribute( p_vout->p_sys->p_display,
                                        i_selected_port, autopaint, 1 );
                    break;
                }
            }

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        int i_chroma_tmp = X112VLC_FOURCC( i_chroma );
        if( i_requested_adaptor == -1 )
        {
            msg_Warn( p_vout, "no free XVideo port found for format "
                      "0x%.8x (%4.4s)", i_chroma_tmp, (char*)&i_chroma_tmp );
        }
        else
        {
            msg_Warn( p_vout, "XVideo adaptor %i does not have a free "
                      "XVideo port for format 0x%.8x (%4.4s)",
                      i_requested_adaptor, i_chroma_tmp, (char*)&i_chroma_tmp );
        }
    }

    return i_selected_port;
}

/*****************************************************************************
 * XVideoReleasePort: release YUV12 port
 *****************************************************************************/
static void XVideoReleasePort( vout_thread_t *p_vout, int i_port )
{
    XvUngrabPort( p_vout->p_sys->p_display, i_port, CurrentTime );
}
#endif

/*****************************************************************************
 * InitDisplay: open and initialize X11 device
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int InitDisplay( vout_thread_t *p_vout )
{
#ifdef MODULE_NAME_IS_x11
    XPixmapFormatValues *       p_formats;                 /* pixmap formats */
    XVisualInfo *               p_xvisual;            /* visuals information */
    XVisualInfo                 xvisual_template;         /* visual template */
    int                         i_count;                       /* array size */
#endif

#ifdef HAVE_SYS_SHM_H
    p_vout->p_sys->b_shm = 0;

    if( config_GetInt( p_vout, MODULE_STRING "-shm" ) )
    {
#   ifdef SYS_DARWIN
        /* FIXME: As of 2001-03-16, XFree4 for MacOS X does not support Xshm */
#   else
        p_vout->p_sys->b_shm =
                  ( XShmQueryExtension( p_vout->p_sys->p_display ) == True );
#   endif

        if( !p_vout->p_sys->b_shm )
        {
            msg_Warn( p_vout, "XShm video extension is unavailable" );
        }
    }
    else
    {
        msg_Dbg( p_vout, "disabling XShm video extension" );
    }

#else
    msg_Warn( p_vout, "XShm video extension is unavailable" );

#endif

#ifdef MODULE_NAME_IS_xvideo
    /* XXX The brightness and contrast values should be read from environment
     * XXX variables... */
#if 0
    XVideoSetAttribute( p_vout, "XV_BRIGHTNESS", 0.5 );
    XVideoSetAttribute( p_vout, "XV_CONTRAST",   0.5 );
#endif
#endif

#ifdef MODULE_NAME_IS_x11
    /* Initialize structure */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

    /* Get screen depth */
    p_vout->p_sys->i_screen_depth = XDefaultDepth( p_vout->p_sys->p_display,
                                                   p_vout->p_sys->i_screen );
    switch( p_vout->p_sys->i_screen_depth )
    {
    case 8:
        /*
         * Screen depth is 8bpp. Use PseudoColor visual with private colormap.
         */
        xvisual_template.screen =   p_vout->p_sys->i_screen;
        xvisual_template.class =    DirectColor;
        p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask,
                                    &xvisual_template, &i_count );
        if( p_xvisual == NULL )
        {
            msg_Err( p_vout, "no PseudoColor visual available" );
            return VLC_EGENERIC;
        }
        p_vout->p_sys->i_bytes_per_pixel = 1;
        p_vout->output.pf_setpalette = SetPalette;
        break;
    case 15:
    case 16:
    case 24:
    default:
        /*
         * Screen depth is higher than 8bpp. TrueColor visual is used.
         */
        xvisual_template.screen =   p_vout->p_sys->i_screen;
        xvisual_template.class =    TrueColor;
        p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask,
                                    &xvisual_template, &i_count );
        if( p_xvisual == NULL )
        {
            msg_Err( p_vout, "no TrueColor visual available" );
            return VLC_EGENERIC;
        }

        p_vout->output.i_rmask = p_xvisual->red_mask;
        p_vout->output.i_gmask = p_xvisual->green_mask;
        p_vout->output.i_bmask = p_xvisual->blue_mask;

        /* There is no difference yet between 3 and 4 Bpp. The only way
         * to find the actual number of bytes per pixel is to list supported
         * pixmap formats. */
        p_formats = XListPixmapFormats( p_vout->p_sys->p_display, &i_count );
        p_vout->p_sys->i_bytes_per_pixel = 0;

        for( ; i_count-- ; p_formats++ )
        {
            /* Under XFree4.0, the list contains pixmap formats available
             * through all video depths ; so we have to check against current
             * depth. */
            if( p_formats->depth == (int)p_vout->p_sys->i_screen_depth )
            {
                if( p_formats->bits_per_pixel / 8
                        > (int)p_vout->p_sys->i_bytes_per_pixel )
                {
                    p_vout->p_sys->i_bytes_per_pixel =
                                               p_formats->bits_per_pixel / 8;
                }
            }
        }
        break;
    }
    p_vout->p_sys->p_visual = p_xvisual->visual;
    XFree( p_xvisual );
#endif

    return VLC_SUCCESS;
}

#ifdef HAVE_SYS_SHM_H
/*****************************************************************************
 * CreateShmImage: create an XImage or XvImage using shared memory extension
 *****************************************************************************
 * Prepare an XImage or XvImage for display function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there. See http://ftp.xfree86.org/pub/XFree86/4.0/doc/mit-shm.TXT
 *****************************************************************************/
static IMAGE_TYPE * CreateShmImage( vout_thread_t *p_vout,
                                    Display* p_display, EXTRA_ARGS_SHM,
                                    int i_width, int i_height )
{
    IMAGE_TYPE *p_image;
    Status result;

    /* Create XImage / XvImage */
#ifdef MODULE_NAME_IS_xvideo

    /* Make sure the buffer is aligned to multiple of 16 */
    i_height = ( i_height + 15 ) >> 4 << 4;
    i_width = ( i_width + 15 ) >> 4 << 4;

    p_image = XvShmCreateImage( p_display, i_xvport, i_chroma, 0,
                                i_width, i_height, p_shm );
#else
    p_image = XShmCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                               p_shm, i_width, i_height );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "image creation failed" );
        return NULL;
    }

    /* Allocate shared memory segment - 0776 set the access permission
     * rights (like umask), they are not yet supported by all X servers */
    p_shm->shmid = shmget( IPC_PRIVATE, DATA_SIZE(p_image), IPC_CREAT | 0776 );
    if( p_shm->shmid < 0 )
    {
        msg_Err( p_vout, "cannot allocate shared image data (%s)",
                         strerror( errno ) );
        IMAGE_FREE( p_image );
        return NULL;
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm->shmaddr = p_image->data = shmat( p_shm->shmid, 0, 0 );
    if(! p_shm->shmaddr )
    {
        msg_Err( p_vout, "cannot attach shared memory (%s)",
                         strerror(errno));
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        return NULL;
    }

    /* Read-only data. We won't be using XShmGetImage */
    p_shm->readOnly = True;

    /* Attach shared memory segment to X server */
    XSynchronize( p_display, True );
    b_shm = VLC_TRUE;
    result = XShmAttach( p_display, p_shm );
    if( result == False || !b_shm )
    {
        msg_Err( p_vout, "cannot attach shared memory to X server" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        shmdt( p_shm->shmaddr );
        return NULL;
    }
    XSynchronize( p_display, False );

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay,
     * and remember NOT to use XFlush ! */
    XSync( p_display, False );

#if 0
    /* Mark the shm segment to be removed when there are no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm->shmid, IPC_RMID, 0 );
#endif

    return p_image;
}
#endif

/*****************************************************************************
 * CreateImage: create an XImage or XvImage
 *****************************************************************************
 * Create a simple image used as a buffer.
 *****************************************************************************/
static IMAGE_TYPE * CreateImage( vout_thread_t *p_vout,
                                 Display *p_display, EXTRA_ARGS,
                                 int i_width, int i_height )
{
    byte_t *    p_data;                           /* image data storage zone */
    IMAGE_TYPE *p_image;
#ifdef MODULE_NAME_IS_x11
    int         i_quantum;                     /* XImage quantum (see below) */
    int         i_bytes_per_line;
#endif

    /* Allocate memory for image */
#ifdef MODULE_NAME_IS_xvideo

    /* Make sure the buffer is aligned to multiple of 16 */
    i_height = ( i_height + 15 ) >> 4 << 4;
    i_width = ( i_width + 15 ) >> 4 << 4;

    p_data = (byte_t *) malloc( i_width * i_height * i_bits_per_pixel / 8 );
#elif defined(MODULE_NAME_IS_x11)
    i_bytes_per_line = i_width * i_bytes_per_pixel;
    p_data = (byte_t *) malloc( i_bytes_per_line * i_height );
#endif
    if( !p_data )
    {
        msg_Err( p_vout, "out of memory" );
        return NULL;
    }

#ifdef MODULE_NAME_IS_x11
    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( i_bytes_per_line & 0xf )
    {
        i_quantum = 0x8;
    }
    else if( i_bytes_per_line & 0x10 )
    {
        i_quantum = 0x10;
    }
    else
    {
        i_quantum = 0x20;
    }
#endif

    /* Create XImage. p_data will be automatically freed */
#ifdef MODULE_NAME_IS_xvideo
    p_image = XvCreateImage( p_display, i_xvport, i_chroma,
                             p_data, i_width, i_height );
#elif defined(MODULE_NAME_IS_x11)
    p_image = XCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                            p_data, i_width, i_height, i_quantum, 0 );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "XCreateImage() failed" );
        free( p_data );
        return NULL;
    }

    return p_image;
}

/*****************************************************************************
 * X11ErrorHandler: replace error handler so we can intercept some of them
 *****************************************************************************/
static int X11ErrorHandler( Display * display, XErrorEvent * event )
{
    /* Ingnore errors on XSetInputFocus()
     * (they happen when a window is not yet mapped) */
    if( event->request_code == X_SetInputFocus )
    {
        fprintf(stderr, "XSetInputFocus failed\n");
        return 0;
    }

    if( event->request_code == 150 /* MIT-SHM */ &&
        event->minor_code == X_ShmAttach )
    {
        fprintf(stderr, "XShmAttach failed\n");
        b_shm = VLC_FALSE;
        return 0;
    }

    XSetErrorHandler(NULL);
    return (XSetErrorHandler(X11ErrorHandler))( display, event );
}

#ifdef MODULE_NAME_IS_x11
/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    int i;
    XColor p_colors[255];

    /* allocate palette */
    for( i = 0; i < 255; i++ )
    {
        /* kludge: colors are indexed reversely because color 255 seems
         * to be reserved for black even if we try to set it to white */
        p_colors[ i ].pixel = 255 - i;
        p_colors[ i ].pad   = 0;
        p_colors[ i ].flags = DoRed | DoGreen | DoBlue;
        p_colors[ i ].red   = red[ 255 - i ];
        p_colors[ i ].blue  = blue[ 255 - i ];
        p_colors[ i ].green = green[ 255 - i ];
    }

    XStoreColors( p_vout->p_sys->p_display,
                  p_vout->p_sys->colormap, p_colors, 255 );
}
#endif

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    double f_arg;
    vlc_bool_t b_arg;

    switch( i_query )
    {
        case VOUT_SET_ZOOM:
            if( p_vout->p_sys->p_win->owner_window )
                return vout_ControlWindow( p_vout,
                    (void *)p_vout->p_sys->p_win->owner_window, i_query, args);

            f_arg = va_arg( args, double );

            vlc_mutex_lock( &p_vout->p_sys->lock );

            /* Update dimensions */
            /* FIXME: export InitWindowSize() from vout core */
            XResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->base_window,
                           p_vout->i_window_width * f_arg,
                           p_vout->i_window_height * f_arg );

            vlc_mutex_unlock( &p_vout->p_sys->lock );
            return VLC_SUCCESS;

       case VOUT_CLOSE:
            vlc_mutex_lock( &p_vout->p_sys->lock );
            XUnmapWindow( p_vout->p_sys->p_display,
                          p_vout->p_sys->original_window.base_window );
            vlc_mutex_unlock( &p_vout->p_sys->lock );
            /* Fall through */

       case VOUT_REPARENT:
            vlc_mutex_lock( &p_vout->p_sys->lock );
            XReparentWindow( p_vout->p_sys->p_display,
                             p_vout->p_sys->original_window.base_window,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             0, 0 );
            XSync( p_vout->p_sys->p_display, False );
            p_vout->p_sys->original_window.owner_window = 0;
            vlc_mutex_unlock( &p_vout->p_sys->lock );
            return vout_vaControlDefault( p_vout, i_query, args );

        case VOUT_SET_STAY_ON_TOP:
            if( p_vout->p_sys->p_win->owner_window )
                return vout_ControlWindow( p_vout,
                    (void *)p_vout->p_sys->p_win->owner_window, i_query, args);

            b_arg = va_arg( args, vlc_bool_t );
            vlc_mutex_lock( &p_vout->p_sys->lock );
            WindowOnTop( p_vout, b_arg );
            vlc_mutex_unlock( &p_vout->p_sys->lock );
            return VLC_SUCCESS;

       default:
            return vout_vaControlDefault( p_vout, i_query, args );
    }
}

/*****************************************************************************
 * TestNetWMSupport: tests for Extended Window Manager Hints support
 *****************************************************************************/
static void TestNetWMSupport( vout_thread_t *p_vout )
{
    int i_ret, i_format;
    unsigned long i, i_items, i_bytesafter;
    Atom net_wm_supported;
    union { Atom *p_atom; unsigned char *p_char; } p_args;

    p_args.p_atom = NULL;

    p_vout->p_sys->b_net_wm_state_fullscreen = VLC_FALSE;
    p_vout->p_sys->b_net_wm_state_above = VLC_FALSE;
    p_vout->p_sys->b_net_wm_state_below = VLC_FALSE;
    p_vout->p_sys->b_net_wm_state_stays_on_top = VLC_FALSE;

    net_wm_supported =
        XInternAtom( p_vout->p_sys->p_display, "_NET_SUPPORTED", False );

    i_ret = XGetWindowProperty( p_vout->p_sys->p_display,
                                DefaultRootWindow( p_vout->p_sys->p_display ),
                                net_wm_supported,
                                0, 16384, False, AnyPropertyType,
                                &net_wm_supported,
                                &i_format, &i_items, &i_bytesafter,
                                (unsigned char **)&p_args );

    if( i_ret != Success || i_items == 0 ) return;

    msg_Dbg( p_vout, "Window manager supports NetWM" );

    p_vout->p_sys->net_wm_state =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE", False );
    p_vout->p_sys->net_wm_state_fullscreen =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_FULLSCREEN",
                     False );
    p_vout->p_sys->net_wm_state_above =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_ABOVE", False );
    p_vout->p_sys->net_wm_state_below =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_BELOW", False );
    p_vout->p_sys->net_wm_state_stays_on_top =
        XInternAtom( p_vout->p_sys->p_display, "_NET_WM_STATE_STAYS_ON_TOP",
                     False );

    for( i = 0; i < i_items; i++ )
    {
        if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_fullscreen )
        {
            msg_Dbg( p_vout,
                     "Window manager supports _NET_WM_STATE_FULLSCREEN" );
            p_vout->p_sys->b_net_wm_state_fullscreen = VLC_TRUE;
        }
        else if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_above )
        {
            msg_Dbg( p_vout, "Window manager supports _NET_WM_STATE_ABOVE" );
            p_vout->p_sys->b_net_wm_state_above = VLC_TRUE;
        }
        else if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_below )
        {
            msg_Dbg( p_vout, "Window manager supports _NET_WM_STATE_BELOW" );
            p_vout->p_sys->b_net_wm_state_below = VLC_TRUE;
        }
        else if( p_args.p_atom[i] == p_vout->p_sys->net_wm_state_stays_on_top )
        {
            msg_Dbg( p_vout,
                     "Window manager supports _NET_WM_STATE_STAYS_ON_TOP" );
            p_vout->p_sys->b_net_wm_state_stays_on_top = VLC_TRUE;
        }
    }

    XFree( p_args.p_atom );
}

/*****************************************************************************
 * Key events handling
 *****************************************************************************/
static struct
{
    int i_x11key;
    int i_vlckey;

} x11keys_to_vlckeys[] =
{
    { XK_F1, KEY_F1 }, { XK_F2, KEY_F2 }, { XK_F3, KEY_F3 }, { XK_F4, KEY_F4 },
    { XK_F5, KEY_F5 }, { XK_F6, KEY_F6 }, { XK_F7, KEY_F7 }, { XK_F8, KEY_F8 },
    { XK_F9, KEY_F9 }, { XK_F10, KEY_F10 }, { XK_F11, KEY_F11 },
    { XK_F12, KEY_F12 },

    { XK_Return, KEY_ENTER },
    { XK_KP_Enter, KEY_ENTER },
    { XK_space, KEY_SPACE },
    { XK_Escape, KEY_ESC },

    { XK_Menu, KEY_MENU },
    { XK_Left, KEY_LEFT },
    { XK_Right, KEY_RIGHT },
    { XK_Up, KEY_UP },
    { XK_Down, KEY_DOWN },

    { XK_Home, KEY_HOME },
    { XK_End, KEY_END },
    { XK_Page_Up, KEY_PAGEUP },
    { XK_Page_Down, KEY_PAGEDOWN },

    { XK_Insert, KEY_INSERT },
    { XK_Delete, KEY_DELETE },

    { 0, 0 }
};

static int ConvertKey( int i_key )
{
    int i;

    for( i = 0; x11keys_to_vlckeys[i].i_x11key != 0; i++ )
    {
        if( x11keys_to_vlckeys[i].i_x11key == i_key )
        {
            return x11keys_to_vlckeys[i].i_vlckey;
        }
    }

    return 0;
}

/*****************************************************************************
 * WindowOnTop: Switches the "always on top" state of the video window.
 *****************************************************************************/
static int WindowOnTop( vout_thread_t *p_vout, vlc_bool_t b_on_top )
{
    if( p_vout->p_sys->b_net_wm_state_stays_on_top )
    {
        XClientMessageEvent event;

        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = p_vout->p_sys->net_wm_state;
        event.display = p_vout->p_sys->p_display;
        event.window = p_vout->p_sys->p_win->base_window;
        event.format = 32;
        event.data.l[ 0 ] = b_on_top; /* set property */
        event.data.l[ 1 ] = p_vout->p_sys->net_wm_state_stays_on_top;

        XSendEvent( p_vout->p_sys->p_display,
                    DefaultRootWindow( p_vout->p_sys->p_display ),
                    False, SubstructureRedirectMask,
                    (XEvent*)&event );
    }

    return VLC_SUCCESS;
}
