/*****************************************************************************
 * vout_x11.c: X11 video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_x11.c,v 1.34 2001/12/09 17:01:37 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
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

#define MODULE_NAME x11
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

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

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"
#include "vout_common.h"

#include "interface.h"
#include "netutils.h"                                 /* network_ChannelJoin */

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );
static void vout_SetPalette( struct vout_thread_s *, u16*, u16*, u16*, u16* );

static int  X11InitDisplay      ( vout_thread_t *p_vout, char *psz_display );

static int  X11CreateImage      ( vout_thread_t *p_vout, XImage **pp_ximage );
static void X11DestroyImage     ( XImage *p_ximage );
static int  X11CreateShmImage   ( vout_thread_t *p_vout, XImage **pp_ximage,
                                  XShmSegmentInfo *p_shm_info );
static void X11DestroyShmImage  ( vout_thread_t *p_vout, XImage *p_ximage,
                                  XShmSegmentInfo *p_shm_info );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = _M( vout_Manage );
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = vout_SetPalette;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to initialize SDL and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "x11" ) )
    {
        return( 999 );
    }

    return( 50 );
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
    XColor cursor_color;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open display, unsing 'vlc_display' or DISPLAY environment variable */
    psz_display = XDisplayName( main_GetPszVariable( VOUT_DISPLAY_VAR, NULL ) );
    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                          /* error */
    {
        intf_ErrMsg( "vout error: cannot open display %s", psz_display );
        free( p_vout->p_sys );
        return( 1 );
    }
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */

    if( _M( XCommonCreateWindow ) ( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot create X11 window" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Open and initialize device. This function issues its own error messages.
     * Since XLib is usually not thread-safe, we can't use the same display
     * pointer than the interface or another thread. However, the root window
     * id is still valid. */
    if( X11InitDisplay( p_vout, psz_display ) )
    {
        intf_ErrMsg( "vout error: cannot initialize X11 display" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    p_vout->p_sys->cursor_pixmap = XCreatePixmap( p_vout->p_sys->p_display,
                                                  DefaultRootWindow(
                                                     p_vout->p_sys->p_display),
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
    
    p_vout->p_sys->blank_cursor = XCreatePixmapCursor(
                                      p_vout->p_sys->p_display,
                                      p_vout->p_sys->cursor_pixmap,
                                      p_vout->p_sys->cursor_pixmap,
                                      &cursor_color,
                                      &cursor_color, 1, 1 );    

    /* Disable screen saver and return */
    _M( XCommonDisableScreenSaver ) ( p_vout );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_err;

#ifdef SYS_DARWIN
    /* FIXME : As of 2001-03-16, XFree4 for MacOS X does not support Xshm. */
    p_vout->p_sys->b_shm = 0;
#endif

    /* Create XImages using XShm extension - on failure, fall back to regular
     * way (and destroy the first image if it was created successfully) */
    if( p_vout->p_sys->b_shm )
    {
        /* Create first image */
        i_err = X11CreateShmImage( p_vout, &p_vout->p_sys->p_ximage[0],
                                   &p_vout->p_sys->shm_info[0] );
        if( !i_err )                         /* first image has been created */
        {
            /* Create second image */
            if( X11CreateShmImage( p_vout, &p_vout->p_sys->p_ximage[1],
                                   &p_vout->p_sys->shm_info[1] ) )
            {                             /* error creating the second image */
                X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0],
                                    &p_vout->p_sys->shm_info[0] );
                i_err = 1;
            }
        }
        if( i_err )                                      /* an error occured */
        {
            intf_Msg( "vout: XShm video extension unavailable" );
            p_vout->p_sys->b_shm = 0;
        }
    }

    /* Create XImages without XShm extension */
    if( !p_vout->p_sys->b_shm )
    {
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[0] ) )
        {
            intf_ErrMsg( "vout error: cannot create images" );
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( 1 );
        }
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[1] ) )
        {
            intf_ErrMsg( "vout error: cannot create images" );
            X11DestroyImage( p_vout->p_sys->p_ximage[0] );
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( 1 );
        }
    }

    /* Set bytes per line and initialize buffers */
    p_vout->i_bytes_per_line = p_vout->p_sys->p_ximage[0]->bytes_per_line;
    p_vout->pf_setbuffers( p_vout, p_vout->p_sys->p_ximage[ 0 ]->data,
                                   p_vout->p_sys->p_ximage[ 1 ]->data );

    /* Set date for autohiding cursor */
    p_vout->p_sys->i_lastmoved = mdate();
    
    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm )                             /* Shm XImages... */
    {
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0],
                            &p_vout->p_sys->shm_info[0] );
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[1],
                            &p_vout->p_sys->shm_info[1] );
    }
    else                                          /* ...or regular XImages */
    {
        X11DestroyImage( p_vout->p_sys->p_ximage[0] );
        X11DestroyImage( p_vout->p_sys->p_ximage[1] );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Enable screen saver */
    _M( XCommonEnableScreenSaver ) ( p_vout );

    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        _M( XCommonToggleMousePointer ) ( p_vout );
    }

    /* Destroy blank cursor pixmap */
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );

    /* Destroy colormap */
    if( p_vout->i_screen_depth == 8 )
    {
        XFreeColormap( p_vout->p_sys->p_display, p_vout->p_sys->colormap );
    }

    /* Destroy window */
    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );

    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to X11 server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm)                                /* XShm is used */
    {
        /* Display rendered image using shared memory extension */
        XShmPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc,
                     p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ],
                     0, 0, 0, 0,
                     p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->width,
                     p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->height, False);

        /* Send the order to the X server */
        XSync(p_vout->p_sys->p_display, False);
    }
    else                                /* regular X11 capabilities are used */
    {
        XPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc,
                  p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ],
                  0, 0, 0, 0,
                  p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->width,
                  p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->height);

        /* Send the order to the X server */
        XSync(p_vout->p_sys->p_display, False);
    }
}

/*****************************************************************************
 * vout_SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    int i, j;
    XColor p_colors[255];

    intf_DbgMsg( "vout: Palette change called" );

    /* allocate palette */
    for( i = 0, j = 255; i < 255; i++, j-- )
    {
        /* kludge: colors are indexed reversely because color 255 seems
         * to be reserved for black even if we try to set it to white */
        p_colors[ i ].pixel = j;
        p_colors[ i ].pad   = 0;
        p_colors[ i ].flags = DoRed | DoGreen | DoBlue;
        p_colors[ i ].red   = red[ j ];
        p_colors[ i ].blue  = blue[ j ];
        p_colors[ i ].green = green[ j ];
    }

    XStoreColors( p_vout->p_sys->p_display,
                  p_vout->p_sys->colormap, p_colors, 256 );
}

/* following functions are local */

/*****************************************************************************
 * X11InitDisplay: open and initialize X11 device
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int X11InitDisplay( vout_thread_t *p_vout, char *psz_display )
{
    XPixmapFormatValues *       p_formats;                 /* pixmap formats */
    XVisualInfo *               p_xvisual;           /* visuals informations */
    XVisualInfo                 xvisual_template;         /* visual template */
    int                         i_count;                       /* array size */


    /* Initialize structure */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );
    p_vout->p_sys->b_shm    = ( XShmQueryExtension( p_vout->p_sys->p_display )
                                 == True );
    if( !p_vout->p_sys->b_shm )
    {
        intf_Msg( "vout: XShm video extension is not available" );
    }

    /* Get screen depth */
    p_vout->i_screen_depth = XDefaultDepth( p_vout->p_sys->p_display,
                                            p_vout->p_sys->i_screen );
    switch( p_vout->i_screen_depth )
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
            intf_ErrMsg( "vout error: no PseudoColor visual available" );
            return( 1 );
        }
        p_vout->i_bytes_per_pixel = 1;
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
            intf_ErrMsg( "vout error: no TrueColor visual available" );
            return( 1 );
        }
        p_vout->i_red_mask =        p_xvisual->red_mask;
        p_vout->i_green_mask =      p_xvisual->green_mask;
        p_vout->i_blue_mask =       p_xvisual->blue_mask;

        /* There is no difference yet between 3 and 4 Bpp. The only way
         * to find the actual number of bytes per pixel is to list supported
         * pixmap formats. */
        p_formats = XListPixmapFormats( p_vout->p_sys->p_display, &i_count );
        p_vout->i_bytes_per_pixel = 0;

        for( ; i_count-- ; p_formats++ )
        {
            /* Under XFree4.0, the list contains pixmap formats available
             * through all video depths ; so we have to check against current
             * depth. */
            if( p_formats->depth == p_vout->i_screen_depth )
            {
                if( p_formats->bits_per_pixel / 8
                        > p_vout->i_bytes_per_pixel )
                {
                    p_vout->i_bytes_per_pixel = p_formats->bits_per_pixel / 8;
                }
            }
        }
        break;
    }
    p_vout->p_sys->p_visual = p_xvisual->visual;
    XFree( p_xvisual );

    return( 0 );
}

/*****************************************************************************
 * X11CreateImage: create an XImage
 *****************************************************************************
 * Create a simple XImage used as a buffer.
 *****************************************************************************/
static int X11CreateImage( vout_thread_t *p_vout, XImage **pp_ximage )
{
    byte_t *    pb_data;                          /* image data storage zone */
    int         i_quantum;                     /* XImage quantum (see below) */

    /* Allocate memory for image */
    p_vout->i_bytes_per_line = p_vout->i_width * p_vout->i_bytes_per_pixel;
    pb_data = (byte_t *) malloc( p_vout->i_bytes_per_line * p_vout->i_height );
    if( !pb_data )                                                  /* error */
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM));
        return( 1 );
    }

    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( !(( p_vout->i_bytes_per_line ) % 32) )
    {
        i_quantum = 32;
    }
    else
    {
        if( !(( p_vout->i_bytes_per_line ) % 16) )
        {
            i_quantum = 16;
        }
        else
        {
            i_quantum = 8;
        }
    }

    /* Create XImage */
    *pp_ximage = XCreateImage( p_vout->p_sys->p_display,
                               p_vout->p_sys->p_visual, p_vout->i_screen_depth,
                               ZPixmap, 0, pb_data,
                               p_vout->i_width, p_vout->i_height, i_quantum, 0);
    if(! *pp_ximage )                                               /* error */
    {
        intf_ErrMsg( "vout error: XCreateImage() failed" );
        free( pb_data );
        return( 1 );
    }

    return 0;
}

/*****************************************************************************
 * X11CreateShmImage: create an XImage using shared memory extension
 *****************************************************************************
 * Prepare an XImage for DisplayX11ShmImage function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there.
 *****************************************************************************/
static int X11CreateShmImage( vout_thread_t *p_vout, XImage **pp_ximage,
                              XShmSegmentInfo *p_shm_info)
{
    /* Create XImage */
    *pp_ximage =
        XShmCreateImage( p_vout->p_sys->p_display, p_vout->p_sys->p_visual,
                         p_vout->i_screen_depth, ZPixmap, 0,
                         p_shm_info, p_vout->i_width, p_vout->i_height );
    if(! *pp_ximage )                                               /* error */
    {
        intf_ErrMsg( "vout error: XShmCreateImage() failed" );
        return( 1 );
    }

    /* Allocate shared memory segment - 0777 set the access permission
     * rights (like umask), they are not yet supported by X servers */
    p_shm_info->shmid =
        shmget( IPC_PRIVATE, (*pp_ximage)->bytes_per_line
                                 * (*pp_ximage)->height, IPC_CREAT | 0777);
    if( p_shm_info->shmid < 0)                                      /* error */
    {
        intf_ErrMsg( "vout error: cannot allocate shared image data (%s)",
                    strerror(errno));
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm_info->shmaddr = (*pp_ximage)->data = shmat(p_shm_info->shmid, 0, 0);
    if(! p_shm_info->shmaddr )
    {                                                               /* error */
        intf_ErrMsg( "vout error: cannot attach shared memory (%s)",
                    strerror(errno));
        shmctl( p_shm_info->shmid, IPC_RMID, 0 );      /* free shared memory */
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

#if 0
    /* Mark the shm segment to be removed when there will be no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm_info->shmid, IPC_RMID, 0 );
#endif

    /* Attach shared memory segment to X server (read only) */
    p_shm_info->readOnly = True;
    if( XShmAttach( p_vout->p_sys->p_display, p_shm_info )
         == False )                                                 /* error */
    {
        intf_ErrMsg( "vout error: cannot attach shared memory to X11 server" );
        shmctl( p_shm_info->shmid, IPC_RMID, 0 );      /* free shared memory */
        shmdt( p_shm_info->shmaddr );   /* detach shared memory from process
                                         * and automatic free */
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay */
    XFlush( p_vout->p_sys->p_display );
    return( 0 );
}

/*****************************************************************************
 * X11DestroyImage: destroy an XImage
 *****************************************************************************
 * Destroy XImage AND associated data. If pointer is NULL, the image won't be
 * destroyed (see vout_ManageOutputMethod())
 *****************************************************************************/
static void X11DestroyImage( XImage *p_ximage )
{
    if( p_ximage != NULL )
    {
        XDestroyImage( p_ximage );                     /* no free() required */
    }
}

/*****************************************************************************
 * X11DestroyShmImage
 *****************************************************************************
 * Destroy XImage AND associated data. Detach shared memory segment from
 * server and process, then free it. If pointer is NULL, the image won't be
 * destroyed (see vout_ManageOutputMethod())
 *****************************************************************************/
static void X11DestroyShmImage( vout_thread_t *p_vout, XImage *p_ximage,
                                XShmSegmentInfo *p_shm_info )
{
    /* If pointer is NULL, do nothing */
    if( p_ximage == NULL )
    {
        return;
    }

    XShmDetach( p_vout->p_sys->p_display, p_shm_info );/* detach from server */
    XDestroyImage( p_ximage );

    shmctl( p_shm_info->shmid, IPC_RMID, 0 );          /* free shared memory */

    if( shmdt( p_shm_info->shmaddr ) )  /* detach shared memory from process */
    {
        intf_ErrMsg( "vout error: cannot detach shared memory (%s)",
                     strerror(errno) );
    }
}

