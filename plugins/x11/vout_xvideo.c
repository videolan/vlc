/*****************************************************************************
 * vout_xvideo.c: Xvideo video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_xvideo.c,v 1.43 2001/12/29 00:39:49 massiot Exp $
 *
 * Authors: Shane Harper <shanegh@optusnet.com.au>
 *          Vincent Seguin <seguin@via.ecp.fr>
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

#define MODULE_NAME xvideo
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
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/dpms.h>

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

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"

#include "modules.h"
#include "modules_export.h"

#define XVIDEO_MAX_DIRECTBUFFERS 5
#define GUID_YUV12_PLANAR 0x32315659

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t * );
static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );

static int  XVideoNewPicture   ( vout_thread_t *, picture_t * );

static XvImage *CreateShmImage ( Display *, int, XShmSegmentInfo *, int, int );
static void     DestroyShmImage( Display *, XvImage *, XShmSegmentInfo * );

static int  CheckForXVideo     ( Display * );
static int  GetXVideoPort      ( Display * );

/*static void XVideoSetAttribute       ( vout_thread_t *, char *, float );*/

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
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This returns a score to the plugin manager so that it can select the best
 * plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    Display *p_display;                                   /* display pointer */
    char    *psz_display;

    /* Open display, unsing 'vlc_display' or DISPLAY environment variable */
    psz_display = XDisplayName( main_GetPszVariable(VOUT_DISPLAY_VAR, NULL) );
    p_display = XOpenDisplay( psz_display );
    if( p_display == NULL )                                         /* error */
    {
        intf_WarnMsg( 3, "vout: Xvideo cannot open display %s", psz_display );
        intf_WarnMsg( 3, "vout: Xvideo not supported" );
        return( 0 );
    }

    if( !CheckForXVideo( p_display ) )
    {
        intf_WarnMsg( 3, "vout: Xvideo not supported" );
        XCloseDisplay( p_display );
        return( 0 );
    }

    if( GetXVideoPort( p_display ) < 0 )
    {
        intf_WarnMsg( 3, "vout: Xvideo not supported" );
        XCloseDisplay( p_display );
        return( 0 );
    }

    /* Clean-up everyting */
    XCloseDisplay( p_display );

    if( TestMethod( VOUT_METHOD_VAR, "xvideo" ) )
    {
        return( 999 );
    }

    return( 150 );
}

/*****************************************************************************
 * vout_Create: allocate XVideo video thread output method
 *****************************************************************************
 * This function allocates and initialize a XVideo vout method. It uses some of
 * the vout properties to choose the window size, and change them according to
 * the actual properties of the display.
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

    if( !CheckForXVideo( p_vout->p_sys->p_display ) )
    {
        intf_ErrMsg( "vout error: no XVideo extension" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Check we have access to a video port */
    if( (p_vout->p_sys->i_xvport = GetXVideoPort(p_vout->p_sys->p_display)) <0 )
    {
        intf_ErrMsg( "vout error: cannot get XVideo port" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return 1;
    }
    intf_DbgMsg( "Using xv port %d" , p_vout->p_sys->i_xvport );

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

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( _M( XCommonCreateWindow ) ( p_vout ) )
    {
        intf_ErrMsg( "vout error: no suitable Xvideo image input port" );
        _M( XCommonDestroyWindow ) ( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

#if 0
    /* XXX The brightness and contrast values should be read from environment
     * XXX variables... */
    XVideoSetAttribute( p_vout, "XV_BRIGHTNESS", 0.5 );
    XVideoSetAttribute( p_vout, "XV_CONTRAST",   0.5 );
#endif

    /* Disable screen saver and return */
    _M( XCommonDisableScreenSaver ) ( p_vout );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize XVideo video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    switch( p_vout->render.i_chroma )
    {
        case YUV_420_PICTURE:
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            return( 0 );
    }

    /* Try to initialize up to XVIDEO_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < XVIDEO_MAX_DIRECTBUFFERS )
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
        if( XVideoNewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status        = DESTROYED_PICTURE;
        p_pic->i_type          = DIRECT_PICTURE;

        p_pic->i_left_margin   =
        p_pic->i_right_margin  =
        p_pic->i_top_margin    =
        p_pic->i_bottom_margin = 0;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate XVideo video thread output method
 *****************************************************************************
 * Destroy the XvImage. It is called at the end of the thread, but also each
 * time the image is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        DestroyShmImage( p_vout->p_sys->p_display,
                         PP_OUTPUTPICTURE[ i_index ]->p_sys->p_xvimage,
                         &PP_OUTPUTPICTURE[ i_index ]->p_sys->shminfo );
        free( PP_OUTPUTPICTURE[ i_index ]->p_sys );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy XVideo video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_Create
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        _M( XCommonToggleMousePointer ) ( p_vout );
    }

    /* Destroy blank cursor pixmap */
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );

    _M( XCommonEnableScreenSaver ) ( p_vout );
    _M( XCommonDestroyWindow ) ( p_vout );
    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    free( p_vout->p_sys );
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

    XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                   p_vout->p_sys->yuv_window, p_vout->p_sys->gc,
                   p_pic->p_sys->p_xvimage, 0 /*src_x*/, 0 /*src_y*/,
                   p_vout->output.i_width, p_vout->output.i_height,
                   0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height,
                   False /* Don't put True here or you'll waste your CPU */ );

    XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                   i_width, i_height );

    XMoveWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                 i_x, i_y );

    /* Force synchronization */
    XSync( p_vout->p_sys->p_display, False );
}

/* following functions are local */

/*****************************************************************************
 * CheckForXVideo: check for the XVideo extension
 *****************************************************************************/
static int CheckForXVideo( Display *p_display )
{
    unsigned int i;

    switch( XvQueryExtension( p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            return( 1 );

        case XvBadExtension:
            intf_WarnMsg( 3, "vout error: XvBadExtension" );
            return( 0 );

        case XvBadAlloc:
            intf_WarnMsg( 3, "vout error: XvBadAlloc" );
            return( 0 );

        default:
            intf_WarnMsg( 3, "vout error: XvQueryExtension failed" );
            return( 0 );
    }
}

/*****************************************************************************
 * XVideoNewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int XVideoNewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width  = p_vout->output.i_width;
    int i_height = p_vout->output.i_height;

    switch( p_vout->output.i_chroma )
    {
        case YUV_420_PICTURE:
            /* We know this chroma, allocate a buffer which will be used
             * directly by the decoder */
            p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

            if( p_pic->p_sys == NULL )
            {
                return -1;
            }

            /* Create XvImage using XShm extension */
            p_pic->p_sys->p_xvimage =
                CreateShmImage( p_vout->p_sys->p_display,
                                p_vout->p_sys->i_xvport,
                                &p_pic->p_sys->shminfo,
                                p_vout->output.i_width,
                                p_vout->output.i_height );
            if( p_pic->p_sys->p_xvimage == NULL )
            {
                free( p_pic->p_sys );
                return -1;
            }


            /* Precalculate some values */
            p_pic->i_size         = i_width * i_height;
            p_pic->i_chroma_width = i_width / 2;
            p_pic->i_chroma_size  = i_height * ( i_width / 2 );

            /* FIXME: try to get the right i_bytes value from p_xvimage */
            p_pic->planes[Y_PLANE].p_data  = p_pic->p_sys->p_xvimage->data;
            p_pic->planes[Y_PLANE].i_bytes = p_pic->i_size * sizeof(u8);
            p_pic->planes[Y_PLANE].i_line_bytes = i_width * sizeof(u8);

            p_pic->planes[U_PLANE].p_data  = (u8*)p_pic->p_sys->p_xvimage->data
                                               + p_pic->i_size * 5 / 4;
            p_pic->planes[U_PLANE].i_bytes = p_pic->i_size * sizeof(u8) / 4;
            p_pic->planes[U_PLANE].i_line_bytes = p_pic->i_chroma_width
                                                   * sizeof(u8);

            p_pic->planes[V_PLANE].p_data  = (u8*)p_pic->p_sys->p_xvimage->data
                                               + p_pic->i_size;
            p_pic->planes[V_PLANE].i_bytes = p_pic->i_size * sizeof(u8) / 4;
            p_pic->planes[V_PLANE].i_line_bytes = p_pic->i_chroma_width
                                                   * sizeof(u8);

            p_pic->i_planes = 3;

            return 0;

        default:
            /* Unknown chroma, tell the guy to get lost */
            p_pic->i_planes = 0;

            return -1;
    }
}

/*****************************************************************************
 * CreateShmImage: create an XvImage using shared memory extension
 *****************************************************************************
 * Prepare an XvImage for display function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there.
 *****************************************************************************/
static XvImage *CreateShmImage( Display* p_display, int i_xvport,
                                XShmSegmentInfo *p_shminfo,
                                int i_width, int i_height )
{
    XvImage *p_xvimage;

    p_xvimage = XvShmCreateImage( p_display, i_xvport,
                                  GUID_YUV12_PLANAR, 0,
                                  i_width, i_height,
                                  p_shminfo );
    if( p_xvimage == NULL )
    {
        intf_ErrMsg( "vout error: XvShmCreateImage failed." );
        return( NULL );
    }

    p_shminfo->shmid = shmget( IPC_PRIVATE, p_xvimage->data_size,
                               IPC_CREAT | 0776 );
    if( p_shminfo->shmid < 0 ) /* error */
    {
        intf_ErrMsg( "vout error: cannot allocate shared image data (%s)",
                     strerror( errno ) );
        return( NULL );
    }

    p_shminfo->shmaddr = p_xvimage->data = shmat( p_shminfo->shmid, 0, 0 );
    p_shminfo->readOnly = False;

    if( !XShmAttach( p_display, p_shminfo ) )
    {
        intf_ErrMsg( "vout error: XShmAttach failed" );
        shmctl( p_shminfo->shmid, IPC_RMID, 0 );
        shmdt( p_shminfo->shmaddr );
        return( NULL );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay */
    XSync( p_display, False );

#if 1
    /* Mark the shm segment to be removed when there are no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shminfo->shmid, IPC_RMID, 0 );
#endif

    return( p_xvimage );
}

/*****************************************************************************
 * DestroyShmImage
 *****************************************************************************
 * Destroy XImage AND associated data. Detach shared memory segment from
 * server and process, then free it. If pointer is NULL, the image won't be
 * destroyed (see vout_Manage())
 *****************************************************************************/
static void DestroyShmImage( Display *p_display, XvImage *p_xvimage,
                             XShmSegmentInfo *p_shminfo )
{
    /* Detach from server */
    XShmDetach( p_display, p_shminfo );
    XSync( p_display, False );

#if 0
    XDestroyImage( p_xvimage ); /* XXX */
#endif

    XFree( p_xvimage );

    if( shmdt( p_shminfo->shmaddr ) )   /* detach shared memory from process */
    {
        intf_ErrMsg( "vout error: cannot detach shared memory (%s)",
                     strerror(errno) );
    }
}

/*****************************************************************************
 * GetXVideoPort: get YUV12 port
 *****************************************************************************
 * 
 *****************************************************************************/
static int GetXVideoPort( Display *dpy )
{
    XvAdaptorInfo *p_adaptor;
    int i_adaptor, i_num_adaptors, i_requested_adaptor;
    int i_selected_port;

    switch( XvQueryAdaptors( dpy, DefaultRootWindow( dpy ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            intf_WarnMsg( 3, "vout error: XvBadExtension for XvQueryAdaptors" );
            return( -1 );

        case XvBadAlloc:
            intf_WarnMsg( 3, "vout error: XvBadAlloc for XvQueryAdaptors" );
            return( -1 );

        default:
            intf_WarnMsg( 3, "vout error: XvQueryAdaptors failed" );
            return( -1 );
    }

    i_selected_port = -1;
    i_requested_adaptor = main_GetIntVariable( VOUT_XVADAPTOR_VAR, -1 );

    /* No special xv port has been requested so try all of them */
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

        /* Check that port supports YUV12 planar format... */
        p_formats = XvListImageFormats( dpy, p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0; i_format < i_num_formats; i_format++ )
        {
            XvEncodingInfo  *p_enc;
            int             i_enc, i_num_encodings;
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;

            /* If this is not the format we want, forget it */
            if( p_formats[ i_format ].id != GUID_YUV12_PLANAR )
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
                if( XvGrabPort( dpy, i_port, CurrentTime ) == Success )
                {
                    i_selected_port = i_port;
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            intf_WarnMsg( 3, "vout: GetXVideoPort found adaptor %i, port %i",
                             i_adaptor, i_selected_port );
            intf_WarnMsg( 3, "  image format 0x%x (%4.4s) %s supported",
                             p_formats[ i_format ].id,
                             (char *)&p_formats[ i_format ].id,
                             ( p_formats[ i_format ].format
                                == XvPacked ) ? "packed" : "planar" );

            intf_WarnMsg( 4, " encoding list:" );

            if( XvQueryEncodings( dpy, i_selected_port,
                                  &i_num_encodings, &p_enc )
                 != Success )
            {
                intf_WarnMsg( 4, "  XvQueryEncodings failed" );
                continue;
            }

            for( i_enc = 0; i_enc < i_num_encodings; i_enc++ )
            {
                intf_WarnMsg( 4, "  id=%ld, name=%s, size=%ldx%ld,"
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

            intf_WarnMsg( 4, " attribute list:" );
            p_attr = XvQueryPortAttributes( dpy, i_selected_port,
                                            &i_num_attributes );
            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                intf_WarnMsg( 4,
                      "  name=%s, flags=[%s%s ], min=%i, max=%i",
                      p_attr[i_attr].name,
                      (p_attr[i_attr].flags & XvGettable) ? " get" : "",
                      (p_attr[i_attr].flags & XvSettable) ? " set" : "",
                      p_attr[i_attr].min_value, p_attr[i_attr].max_value );
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
        if( i_requested_adaptor == -1 )
        {
            intf_WarnMsg( 3, "vout: no free XVideo port found for YV12" );
        }
        else
        {
            intf_WarnMsg( 3, "vout: XVideo adaptor %i does not have a free "
                             "XVideo port for YV12", i_requested_adaptor );
        }
    }

    return( i_selected_port );
}

#if 0
/*****************************************************************************
 * XVideoSetAttribute
 *****************************************************************************
 * This function can be used to set attributes, e.g. XV_BRIGHTNESS and
 * XV_CONTRAST. "f_value" should be in the range of 0 to 1.
 *****************************************************************************/
static void XVideoSetAttribute( vout_thread_t *p_vout,
                                char *attr_name, float f_value )
{
    int             i_attrib;
    XvAttribute    *p_attrib;
    Display        *p_display = p_vout->p_sys->p_display;
    int             i_xvport  = p_vout->p_sys->i_xvport;

    p_attrib = XvQueryPortAttributes( p_display, i_xvport, &i_attrib );

    do
    {
        i_attrib--;

        if( i_attrib >= 0 && !strcmp( p_attrib[ i_attrib ].name, attr_name ) )
        {
            int i_sv = f_value * ( p_attrib[ i_attrib ].max_value
                                    - p_attrib[ i_attrib ].min_value + 1 )
                        + p_attrib[ i_attrib ].min_value;

            XvSetPortAttribute( p_display, i_xvport,
                            XInternAtom( p_display, attr_name, False ), i_sv );
            break;
        }

    } while( i_attrib > 0 );

    if( p_attrib )
        XFree( p_attrib );
}
#endif

