/*****************************************************************************
 * xcommon.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: xcommon.c,v 1.1 2002/08/04 17:23:44 sam Exp $
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

#include "netutils.h"                                 /* network_ChannelJoin */

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
static void SetPalette     ( vout_thread_t *, u16 *, u16 *, u16 * );
#endif

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
    char *       psz_display;
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

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    /* Open display, unsing the "display" config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPsz( p_vout, MODULE_STRING "-display" );

    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                          /* error */
    {
        msg_Err( p_vout, "cannot open display %s",
                         XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        if( psz_display ) free( psz_display );
        return( 1 );
    }
    if( psz_display ) free( psz_display );

    /* Get a screen ID matching the XOpenDisplay return value */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

#ifdef MODULE_NAME_IS_xvideo
    psz_chroma = config_GetPsz( p_vout, "xvideo-chroma" );
    if( psz_chroma )
    {
        if( strlen( psz_chroma ) >= 4 )
        {
            i_chroma  = (unsigned char)psz_chroma[0] <<  0;
            i_chroma |= (unsigned char)psz_chroma[1] <<  8;
            i_chroma |= (unsigned char)psz_chroma[2] << 16;
            i_chroma |= (unsigned char)psz_chroma[3] << 24;

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
    p_vout->p_sys->i_xvport = XVideoGetPort( p_vout, i_chroma,
                                             &p_vout->output.i_chroma );
    if( p_vout->p_sys->i_xvport < 0 )
    {
        /* If a specific chroma format was requested, then we don't try to
         * be cleverer than the user. He knows pretty well what he wants. */
        if( b_chroma )
        {
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return 1;
        }

        /* It failed, but it's not completely lost ! We try to open an
         * XVideo port for an YUY2 picture. We'll need to do an YUV
         * conversion, but at least it has got scaling. */
        p_vout->p_sys->i_xvport =
                        XVideoGetPort( p_vout, VLC_FOURCC('Y','U','Y','2'),
                                               &p_vout->output.i_chroma );
        if( p_vout->p_sys->i_xvport < 0 )
        {
            /* It failed, but it's not completely lost ! We try to open an
             * XVideo port for a simple 16bpp RGB picture. We'll need to do
             * an YUV conversion, but at least it has got scaling. */
            p_vout->p_sys->i_xvport =
                            XVideoGetPort( p_vout, VLC_FOURCC('R','V','1','6'),
                                                   &p_vout->output.i_chroma );
            if( p_vout->p_sys->i_xvport < 0 )
            {
                XCloseDisplay( p_vout->p_sys->p_display );
                free( p_vout->p_sys );
                return 1;
            }
        }
    }
#endif

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Set main window's size */
    p_vout->p_sys->original_window.i_width = p_vout->i_window_width;
    p_vout->p_sys->original_window.i_height = p_vout->i_window_height;

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout, &p_vout->p_sys->original_window ) )
    {
        msg_Err( p_vout, "cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Open and initialize device. */
    if( InitDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot initialize X11 display" );
        DestroyCursor( p_vout );
        DestroyWindow( p_vout, &p_vout->p_sys->original_window );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Disable screen saver */
    DisableXScreenSaver( p_vout );

    /* Misc init */
    p_vout->p_sys->b_altfullscreen = 0;
    p_vout->p_sys->i_time_button_last_pressed = 0;

    return( 0 );
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
#else
    XVideoReleasePort( p_vout, p_vout->p_sys->i_xvport );
#endif

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout, &p_vout->p_sys->original_window );

    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
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

#else
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
            p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4'); break;
        case 32:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4'); break;
        default:
            msg_Err( p_vout, "unknown screen depth %i",
                     p_vout->p_sys->i_screen_depth );
            return( 0 );
    }

    vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                       p_vout->p_sys->p_win->i_height,
                       &i_index, &i_index,
                       &p_vout->output.i_width, &p_vout->output.i_height );

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->output.i_width
                               * VOUT_ASPECT_FACTOR / p_vout->output.i_height;
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

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        /* Display rendered image using shared memory extension */
#   ifdef MODULE_NAME_IS_xvideo
        XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                       p_vout->p_sys->p_win->video_window,
                       p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                       0 /*src_x*/, 0 /*src_y*/,
                       p_vout->output.i_width, p_vout->output.i_height,
                       0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height,
                       False /* Don't put True here or you'll waste your CPU */ );
#   else
        XShmPutImage( p_vout->p_sys->p_display,
                      p_vout->p_sys->p_win->video_window,
                      p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                      0 /*src_x*/, 0 /*src_y*/, 0 /*dest_x*/, 0 /*dest_y*/,
                      p_vout->output.i_width, p_vout->output.i_height,
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
                    0 /*src_x*/, 0 /*src_y*/,
                    p_vout->output.i_width, p_vout->output.i_height,
                    0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height );
#else
        XPutImage( p_vout->p_sys->p_display,
                   p_vout->p_sys->p_win->video_window,
                   p_vout->p_sys->p_win->gc, p_pic->p_sys->p_image,
                   0 /*src_x*/, 0 /*src_y*/, 0 /*dest_x*/, 0 /*dest_y*/,
                   p_vout->output.i_width, p_vout->output.i_height );
#endif
    }

    /* Make sure the command is sent now - do NOT use XFlush !*/
    XSync( p_vout->p_sys->p_display, False );
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
    char        i_key;                                    /* ISO Latin-1 key */
    KeySym      x_key_symbol;

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
            if( (xevent.xconfigure.width != p_vout->p_sys->p_win->i_width)
              || (xevent.xconfigure.height != p_vout->p_sys->p_win->i_height) )
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
            /* We may have keys like F1 trough F12, ESC ... */
            x_key_symbol = XKeycodeToKeysym( p_vout->p_sys->p_display,
                                             xevent.xkey.keycode, 0 );
            switch( x_key_symbol )
            {
            case XK_Escape:
                if( p_vout->b_fullscreen )
                {
                    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                }
                else
                {
                    p_vout->p_vlc->b_die = 1;
                }
                break;
            case XK_Menu:
                {
                    intf_thread_t *p_intf;
                    p_intf = vlc_object_find( p_vout, VLC_OBJECT_INTF,
                                                      FIND_ANYWHERE );
                    if( p_intf )
                    {
                        p_intf->b_menu_change = 1;
                        vlc_object_release( p_intf );
                    }
                }
                break;
            case XK_Left:
                input_Seek( p_vout, -5, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                break;
            case XK_Right:
                input_Seek( p_vout, 5, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                break;
            case XK_Up:
                input_Seek( p_vout, 60, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                break;
            case XK_Down:
                input_Seek( p_vout, -60, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                break;
            case XK_Home:
                input_Seek( p_vout, 0, INPUT_SEEK_BYTES | INPUT_SEEK_SET );
                break;
            case XK_End:
                input_Seek( p_vout, 0, INPUT_SEEK_BYTES | INPUT_SEEK_END );
                break;
            case XK_Page_Up:
                input_Seek( p_vout, 900, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                break;
            case XK_Page_Down:
                input_Seek( p_vout, -900, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                break;
            case XK_space:
                input_SetStatus( p_vout, INPUT_STATUS_PAUSE );
                break;

            case XK_F1: network_ChannelJoin( p_vout, 1 ); break;
            case XK_F2: network_ChannelJoin( p_vout, 2 ); break;
            case XK_F3: network_ChannelJoin( p_vout, 3 ); break;
            case XK_F4: network_ChannelJoin( p_vout, 4 ); break;
            case XK_F5: network_ChannelJoin( p_vout, 5 ); break;
            case XK_F6: network_ChannelJoin( p_vout, 6 ); break;
            case XK_F7: network_ChannelJoin( p_vout, 7 ); break;
            case XK_F8: network_ChannelJoin( p_vout, 8 ); break;
            case XK_F9: network_ChannelJoin( p_vout, 9 ); break;
            case XK_F10: network_ChannelJoin( p_vout, 10 ); break;
            case XK_F11: network_ChannelJoin( p_vout, 11 ); break;
            case XK_F12: network_ChannelJoin( p_vout, 12 ); break;

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
                        p_vout->p_vlc->b_die = 1;
                        break;
                    case 'f':
                    case 'F':
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                        break;

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
            int i_width, i_height, i_x, i_y;

            vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                               p_vout->p_sys->p_win->i_height,
                               &i_x, &i_y, &i_width, &i_height );

            p_vout->i_mouse_x = ( xevent.xmotion.x - i_x )
                * p_vout->render.i_width / i_width;
            p_vout->i_mouse_y = ( xevent.xmotion.y - i_y )
                * p_vout->render.i_height / i_height;
            p_vout->i_mouse_button = 1;

            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    /* In this part we will eventually manage
                     * clicks for DVD navigation for instance. */

                    /* detect double-clicks */
                    if( ( ((XButtonEvent *)&xevent)->time -
                          p_vout->p_sys->i_time_button_last_pressed ) < 300 )
                    {
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    }

                    p_vout->p_sys->i_time_button_last_pressed =
                        ((XButtonEvent *)&xevent)->time;
                    break;

                case Button4:
                    input_Seek( p_vout, 15, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                    break;

                case Button5:
                    input_Seek( p_vout, -15, INPUT_SEEK_SECONDS | INPUT_SEEK_CUR );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button3:
                    {
                        intf_thread_t *p_intf;
                        p_intf = vlc_object_find( p_vout, VLC_OBJECT_INTF,
                                                          FIND_ANYWHERE );
                        if( p_intf )
                        {
                            p_intf->b_menu_change = 1;
                            vlc_object_release( p_intf );
                        }
                    }
                    break;
            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            int i_width, i_height, i_x, i_y;

            /* somewhat different use for vout_PlacePicture:
             * here the values are needed to give to mouse coordinates
             * in the original picture space */
            vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                               p_vout->p_sys->p_win->i_height,
                               &i_x, &i_y, &i_width, &i_height );

            p_vout->i_mouse_x = ( xevent.xmotion.x - i_x )
                * p_vout->render.i_width / i_width;
            p_vout->i_mouse_y = ( xevent.xmotion.y - i_y )
                * p_vout->render.i_height / i_height;
 
            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout ); 
            }
        }
        /* Reparent move -- XXX: why are we getting this ? */
        else if( xevent.type == ReparentNotify )
        {
            ;
        }
        /* Other event */
        else
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
               && (xevent.xclient.data.l[0]
                     == p_vout->p_sys->p_win->wm_delete_window ) )
        {
            p_vout->p_vlc->b_die = 1;
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
     *
     * (Needs to be placed after VOUT_FULLSREEN_CHANGE because we can activate
     *  the size flag inside the fullscreen routine)
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        int i_width, i_height, i_x, i_y;

        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        msg_Dbg( p_vout, "video display resized (%dx%d)",
                         p_vout->p_sys->p_win->i_width,
                         p_vout->p_sys->p_win->i_height );
 
#ifdef MODULE_NAME_IS_x11
        /* We need to signal the vout thread about the size change because it
         * is doing the rescaling */
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
#endif

        vout_PlacePicture( p_vout, p_vout->p_sys->p_win->i_width,
                           p_vout->p_sys->p_win->i_height,
                           &i_x, &i_y, &i_width, &i_height );

        XResizeWindow( p_vout->p_sys->p_display,
                       p_vout->p_sys->p_win->video_window, i_width, i_height );
        
        XMoveWindow( p_vout->p_sys->p_display,
                     p_vout->p_sys->p_win->video_window, i_x, i_y );
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

    vlc_bool_t              b_expose;
    vlc_bool_t              b_configure_notify;
    vlc_bool_t              b_map_notify;

    long long int           i_drawable;

    /* Prepare window manager hints and properties */
    xsize_hints.base_width          = p_win->i_width;
    xsize_hints.base_height         = p_win->i_height;
    xsize_hints.flags               = PSize;
    p_win->wm_protocols =
             XInternAtom( p_vout->p_sys->p_display, "WM_PROTOCOLS", True );
    p_win->wm_delete_window =
             XInternAtom( p_vout->p_sys->p_display, "WM_DELETE_WINDOW", True );

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    /* Check whether someone provided us with a window ID */
    i_drawable = p_vout->b_fullscreen ?
                    -1 : config_GetInt( p_vout, MODULE_STRING "-drawable");

    if( i_drawable == -1 )
    {
        p_vout->p_sys->b_createwindow = 1;

        /* Create the window and set hints - the window must receive
         * ConfigureNotify events, and until it is displayed, Expose and
         * MapNotify events. */

        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           DefaultRootWindow( p_vout->p_sys->p_display ),
                           0, 0,
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

            XStoreName( p_vout->p_sys->p_display, p_win->base_window,
#ifdef MODULE_NAME_IS_x11
                        VOUT_TITLE " (X11 output)"
#else
                        VOUT_TITLE " (XVideo output)"
#endif
                      );
        }
    }
    else
    {
        p_vout->p_sys->b_createwindow = 0;
        p_win->base_window = i_drawable;

        XChangeWindowAttributes( p_vout->p_sys->p_display,
                                 p_win->base_window,
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

    if( p_vout->p_sys->b_createwindow )
    {
        /* Send orders to server, and wait until window is displayed - three
         * events must be received: a MapNotify event, an Expose event allowing
         * drawing in the window, and a ConfigureNotify to get the window
         * dimensions. Once those events have been received, only
         * ConfigureNotify events need to be received. */
        b_expose = 0;
        b_configure_notify = 0;
        b_map_notify = 0;
        XMapWindow( p_vout->p_sys->p_display, p_win->base_window );
        do
        {
            XNextEvent( p_vout->p_sys->p_display, &xevent);
            if( (xevent.type == Expose)
                && (xevent.xexpose.window == p_win->base_window) )
            {
                b_expose = 1;
            }
            else if( (xevent.type == MapNotify)
                     && (xevent.xmap.window == p_win->base_window) )
            {
                b_map_notify = 1;
            }
            else if( (xevent.type == ConfigureNotify)
                     && (xevent.xconfigure.window == p_win->base_window) )
            {
                b_configure_notify = 1;
                p_win->i_width = xevent.xconfigure.width;
                p_win->i_height = xevent.xconfigure.height;
            }
        } while( !( b_expose && b_configure_notify && b_map_notify ) );
    }
    else
    {
        /* Get the window's geometry information */
        Window dummy1;
        unsigned int dummy2, dummy3;
        XGetGeometry( p_vout->p_sys->p_display, p_win->base_window,
                      &dummy1, &dummy2, &dummy3,
                      &p_win->i_width,
                      &p_win->i_height,
                      &dummy2, &dummy3 );
    }

    XSelectInput( p_vout->p_sys->p_display, p_win->base_window,
                  StructureNotifyMask | KeyPressMask |
                  ButtonPressMask | ButtonReleaseMask | 
                  PointerMotionMask );

#ifdef MODULE_NAME_IS_x11
    if( p_vout->p_sys->b_createwindow &&
         XDefaultDepth(p_vout->p_sys->p_display, p_vout->p_sys->i_screen) == 8 )
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

    return( 0 );
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

    XDestroyWindow( p_vout->p_sys->p_display, p_win->video_window );
    XUnmapWindow( p_vout->p_sys->p_display, p_win->base_window );
    XFreeGC( p_vout->p_sys->p_display, p_win->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_win->base_window );
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

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        /* Create image using XShm extension */
        p_pic->p_sys->p_image =
            CreateShmImage( p_vout, p_vout->p_sys->p_display,
#   ifdef MODULE_NAME_IS_xvideo
                            p_vout->p_sys->i_xvport, p_vout->output.i_chroma,
#   else
                            p_vout->p_sys->p_visual,
                            p_vout->p_sys->i_screen_depth,
#   endif
                            &p_pic->p_sys->shminfo,
                            p_vout->output.i_width, p_vout->output.i_height );
    }
    else
#endif /* HAVE_SYS_SHM_H */
    {
        /* Create image without XShm extension */
        p_pic->p_sys->p_image =
            CreateImage( p_vout, p_vout->p_sys->p_display,
#ifdef MODULE_NAME_IS_xvideo
                         p_vout->p_sys->i_xvport, p_vout->output.i_chroma,
#else
                         p_vout->p_sys->p_visual,
                         p_vout->p_sys->i_screen_depth, 
                         p_vout->p_sys->i_bytes_per_pixel,
#endif
                         p_vout->output.i_width, p_vout->output.i_height );
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

            p_pic->Y_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[0];
            p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = p_pic->p[Y_PLANE].i_pitch;

            p_pic->U_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[1];
            p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[1];
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = p_pic->p[U_PLANE].i_pitch;

            p_pic->V_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[2];
            p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[2];
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = p_pic->p[V_PLANE].i_pitch;

            p_pic->i_planes = 3;
            break;

        case VLC_FOURCC('Y','V','1','2'):

            p_pic->Y_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[0];
            p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = p_pic->p[Y_PLANE].i_pitch;

            p_pic->U_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[2];
            p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[2];
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = p_pic->p[U_PLANE].i_pitch;

            p_pic->V_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[1];
            p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[1];
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = p_pic->p[V_PLANE].i_pitch;

            p_pic->i_planes = 3;
            break;

        case VLC_FOURCC('Y','2','1','1'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            /* XXX: this just looks so plain wrong... check it out ! */
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0] / 4;
            p_pic->p->i_pixel_pitch = 4;
            p_pic->p->i_visible_pitch = p_pic->p->i_pitch;

            p_pic->i_planes = 1;
            break;

        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('U','Y','V','Y'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p->i_pixel_pitch = 4;
            p_pic->p->i_visible_pitch = p_pic->p->i_pitch;

            p_pic->i_planes = 1;
            break;

        case VLC_FOURCC('R','V','1','5'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p->i_pixel_pitch = 2;
            p_pic->p->i_visible_pitch = p_pic->p->i_pitch;

            p_pic->i_planes = 1;
            break;

        case VLC_FOURCC('R','V','1','6'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p->i_pixel_pitch = 2;
            p_pic->p->i_visible_pitch = p_pic->p->i_pitch;

            p_pic->i_planes = 1;
            break;

#else
        case VLC_FOURCC('R','G','B','2'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;
            p_pic->p->i_pixel_pitch = p_pic->p_sys->p_image->depth;
            p_pic->p->i_visible_pitch = p_pic->p_sys->p_image->width;

            p_pic->i_planes = 1;

            break;

        case VLC_FOURCC('R','V','1','6'):
        case VLC_FOURCC('R','V','1','5'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;
            p_pic->p->i_pixel_pitch = p_pic->p_sys->p_image->depth;
            p_pic->p->i_visible_pitch = 2 * p_pic->p_sys->p_image->width;

            p_pic->i_planes = 1;

            break;

        case VLC_FOURCC('R','V','3','2'):
        case VLC_FOURCC('R','V','2','4'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;
            p_pic->p->i_pixel_pitch = p_pic->p_sys->p_image->depth;
            p_pic->p->i_visible_pitch = 4 * p_pic->p_sys->p_image->width;

            p_pic->i_planes = 1;

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
    mwmhints_t mwmhints;
    XSetWindowAttributes attributes;

    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    if( p_vout->b_fullscreen )
    {
        msg_Dbg( p_vout, "entering fullscreen mode" );
        p_vout->p_sys->p_win = &p_vout->p_sys->fullscreen_window;

        /* Only check the fullscreen method when we actually go fullscreen,
         * because to go back to window mode we need to know in which
         * fullscreen mode we were */
        p_vout->p_sys->b_altfullscreen =
            config_GetInt( p_vout, MODULE_STRING "-altfullscreen" );

        /* fullscreen window size and position */
        p_vout->p_sys->p_win->i_width =
            DisplayWidth( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );
        p_vout->p_sys->p_win->i_height =
            DisplayHeight( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );

        CreateWindow( p_vout, p_vout->p_sys->p_win );

        /* To my knowledge there are two ways to create a borderless window.
         * There's the generic way which is to tell x to bypass the window
         * manager, but this creates problems with the focus of other
         * applications.
         * The other way is to use the motif property "_MOTIF_WM_HINTS" which
         * luckily seems to be supported by most window managers. */
        if( !p_vout->p_sys->b_altfullscreen )
        {
            mwmhints.flags = MWM_HINTS_DECORATIONS;
            mwmhints.decorations = !p_vout->b_fullscreen;

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
            attributes.override_redirect = p_vout->b_fullscreen;
            XChangeWindowAttributes( p_vout->p_sys->p_display,
                                     p_vout->p_sys->p_win->base_window,
                                     CWOverrideRedirect,
                                     &attributes);
        }

        XReparentWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->p_win->base_window,
                         DefaultRootWindow( p_vout->p_sys->p_display ),
                         0, 0 );
        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->p_win->base_window,
                           0, 0,
                           p_vout->p_sys->p_win->i_width,
                           p_vout->p_sys->p_win->i_height );
    }
    else
    {
        msg_Dbg( p_vout, "leaving fullscreen mode" );
        DestroyWindow( p_vout, &p_vout->p_sys->fullscreen_window );
        p_vout->p_sys->p_win = &p_vout->p_sys->original_window;
    }

    XSync( p_vout->p_sys->p_display, True );

    if( !p_vout->b_fullscreen || p_vout->p_sys->b_altfullscreen )
    {
        XSetInputFocus(p_vout->p_sys->p_display,
                       p_vout->p_sys->p_win->base_window,
                       RevertToParent,
                       CurrentTime);
    }

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

    XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );

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
            return( -1 );

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc" );
            return( -1 );

        default:
            msg_Warn( p_vout, "XvQueryExtension failed" );
            return( -1 );
    }

    switch( XvQueryAdaptors( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            msg_Warn( p_vout, "XvBadExtension for XvQueryAdaptors" );
            return( -1 );

        case XvBadAlloc:
            msg_Warn( p_vout, "XvBadAlloc for XvQueryAdaptors" );
            return( -1 );

        default:
            msg_Warn( p_vout, "XvQueryAdaptors failed" );
            return( -1 );
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
            /* Code removed, we can get this through xvinfo anyway */
#if 0
            XvEncodingInfo  *p_enc;
            int             i_enc, i_num_encodings;
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;
#endif

            /* If this is not the format we want, or at least a
             * similar one, forget it */
            if( !vout_ChromaCmp( p_formats[ i_format ].id, i_chroma ) )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < p_adaptor[i_adaptor].base_id
                             + p_adaptor[i_adaptor].num_ports )
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

#if 0
            msg_Dbg( p_vout, " encoding list:" );

            if( XvQueryEncodings( p_vout->p_sys->p_display, i_selected_port,
                                  &i_num_encodings, &p_enc )
                 != Success )
            {
                msg_Dbg( p_vout, "  XvQueryEncodings failed" );
                continue;
            }

            for( i_enc = 0; i_enc < i_num_encodings; i_enc++ )
            {
                msg_Dbg( p_vout, "  id=%ld, name=%s, size=%ldx%ld,"
                                      " numerator=%d, denominator=%d",
                             p_enc[i_enc].encoding_id, p_enc[i_enc].name,
                             p_enc[i_enc].width, p_enc[i_enc].height,
                             p_enc[i_enc].rate.numerator,
                             p_enc[i_enc].rate.denominator );
            }

            if( p_enc != NULL )
            {
                XvFreeEncodingInfo( p_enc );
            }

            msg_Dbg( p_vout, " attribute list:" );
            p_attr = XvQueryPortAttributes( p_vout->p_sys->p_display,
                                            i_selected_port,
                                            &i_num_attributes );
            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                msg_Dbg( p_vout, "  name=%s, flags=[%s%s ], min=%i, max=%i",
                      p_attr[i_attr].name,
                      (p_attr[i_attr].flags & XvGettable) ? " get" : "",
                      (p_attr[i_attr].flags & XvSettable) ? " set" : "",
                      p_attr[i_attr].min_value, p_attr[i_attr].max_value );
            }

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
#endif
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
        if( i_requested_adaptor == -1 )
        {
            msg_Warn( p_vout, "no free XVideo port found for format "
                       "0x%.8x (%4.4s)", i_chroma, (char*)&i_chroma );
        }
        else
        {
            msg_Warn( p_vout, "XVideo adaptor %i does not have a free "
                       "XVideo port for format 0x%.8x (%4.4s)",
                       i_requested_adaptor, i_chroma, (char*)&i_chroma );
        }
    }

    return( i_selected_port );
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
    XVisualInfo *               p_xvisual;           /* visuals informations */
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
            return( 1 );
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
            return( 1 );
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
            if( p_formats->depth == p_vout->p_sys->i_screen_depth )
            {
                if( p_formats->bits_per_pixel / 8
                        > p_vout->p_sys->i_bytes_per_pixel )
                {
                    p_vout->p_sys->i_bytes_per_pixel = p_formats->bits_per_pixel / 8;
                }
            }
        }
        break;
    }
    p_vout->p_sys->p_visual = p_xvisual->visual;
    XFree( p_xvisual );
#endif

    return( 0 );
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

    /* Create XImage / XvImage */
#ifdef MODULE_NAME_IS_xvideo
    p_image = XvShmCreateImage( p_display, i_xvport, i_chroma, 0,
                                i_width, i_height, p_shm );
#else
    p_image = XShmCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                               p_shm, i_width, i_height );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "image creation failed" );
        return( NULL );
    }

    /* Allocate shared memory segment - 0776 set the access permission
     * rights (like umask), they are not yet supported by all X servers */
    p_shm->shmid = shmget( IPC_PRIVATE, DATA_SIZE(p_image), IPC_CREAT | 0776 );
    if( p_shm->shmid < 0 )
    {
        msg_Err( p_vout, "cannot allocate shared image data (%s)",
                         strerror( errno ) );
        IMAGE_FREE( p_image );
        return( NULL );
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm->shmaddr = p_image->data = shmat( p_shm->shmid, 0, 0 );
    if(! p_shm->shmaddr )
    {
        msg_Err( p_vout, "cannot attach shared memory (%s)",
                         strerror(errno));
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        return( NULL );
    }

    /* Read-only data. We won't be using XShmGetImage */
    p_shm->readOnly = True;

    /* Attach shared memory segment to X server */
    if( XShmAttach( p_display, p_shm ) == False )
    {
        msg_Err( p_vout, "cannot attach shared memory to X server" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        shmdt( p_shm->shmaddr );
        return( NULL );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay,
     * and remember NOT to use XFlush ! */
    XSync( p_display, False );

#if 0
    /* Mark the shm segment to be removed when there are no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm->shmid, IPC_RMID, 0 );
#endif

    return( p_image );
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
    p_data = (byte_t *) malloc( i_width * i_height * 2 ); /* XXX */
#else
    i_bytes_per_line = i_width * i_bytes_per_pixel;
    p_data = (byte_t *) malloc( i_bytes_per_line * i_height );
#endif
    if( !p_data )
    {
        msg_Err( p_vout, "out of memory" );
        return( NULL );
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
#else
    p_image = XCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                            p_data, i_width, i_height, i_quantum, 0 );
#endif
    if( p_image == NULL )
    {
        msg_Err( p_vout, "XCreateImage() failed" );
        free( p_data );
        return( NULL );
    }

    return p_image;
}

#ifdef MODULE_NAME_IS_x11
/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout, u16 *red, u16 *green, u16 *blue )
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
