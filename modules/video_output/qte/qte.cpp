/*****************************************************************************
 * qte.cpp : QT Embedded plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Gerald Hansink <gerald.hansink@ordina.nl>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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
 * Preamble
 *****************************************************************************/

/*****************************************************************************
 * notes:
 * - written for ipaq, so hardcoded assumptions specific for ipaq...
 * - runs full screen
 * - no "mouse events" handling
 * - etc.
 *****************************************************************************/

extern "C"
{
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                                /* strerror() */

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
} /* extern "C" */

#include <qapplication.h>
#include <qpainter.h>

#ifdef Q_WS_QWS
#   define USE_DIRECT_PAINTER
#   include <qdirectpainter_qws.h>
#   include <qgfxraster_qws.h>
#endif

extern "C"
{
#include "qte.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DISPLAY_TEXT N_("QT Embedded display name")
#define DISPLAY_LONGTEXT N_( \
    "Specify the Qt Embedded hardware display you want to use. " \
    "By default VLC will use the value of the DISPLAY environment variable.")

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open      ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static void Render    ( vout_thread_t *, picture_t * );
static void Display   ( vout_thread_t *, picture_t * );
static int  Manage    ( vout_thread_t * );
static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );

static int  OpenDisplay ( vout_thread_t * );
static void CloseDisplay( vout_thread_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static void ToggleFullScreen      ( vout_thread_t * );

static void RunQtThread( event_thread_t *p_event );
} /* extern "C" */

/*****************************************************************************
* Exported prototypes
*****************************************************************************/
extern "C"
{

vlc_module_begin();
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
//    add_category_hint( N_("QT Embedded"), NULL );
//    add_string( "qte-display", "landscape", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT);
    set_description( _("QT Embedded video output") );
    set_capability( "video output", 70 );
    add_shortcut( "qte" );
    set_callbacks( Open, Close);
vlc_module_end();

} /* extern "C" */

/*****************************************************************************
 * Seeking function TODO: put this in a generic location !
 *****************************************************************************/
static inline void vout_Seek( off_t i_seek )
{
}

/*****************************************************************************
 * Open: allocate video thread output method
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = (struct vout_sys_t*) malloc( sizeof( struct vout_sys_t ) );

    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    p_vout->pf_init    = Init;
    p_vout->pf_end     = End;
    p_vout->pf_manage  = Manage;
    p_vout->pf_render  = NULL; //Render;
    p_vout->pf_display = Display;

#ifdef NEED_QTE_MAIN
    p_vout->p_sys->p_qte_main =
        module_Need( p_this, "gui-helper", "qte", VLC_TRUE );
    if( p_vout->p_sys->p_qte_main == NULL )
    {
        free( p_vout->p_sys );
        return VLC_ENOMOD;
    }
#endif

    if (OpenDisplay(p_vout))
    {
        msg_Err( p_vout, "Cannot set up qte video output" );
        Close(p_this);
        return( -1 );
    }
    return( 0 );
}

/*****************************************************************************
 * CloseVideo: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by Open
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    msg_Dbg( p_vout, "close" );
    if( p_vout->p_sys->p_event )
    {
        vlc_object_detach( p_vout->p_sys->p_event );

        /* Kill RunQtThread */
        p_vout->p_sys->p_event->b_die = VLC_TRUE;
        CloseDisplay(p_vout);

        vlc_thread_join( p_vout->p_sys->p_event );
        vlc_object_destroy( p_vout->p_sys->p_event );
    }

#ifdef NEED_QTE_MAIN
    msg_Dbg( p_vout, "releasing gui-helper" );
    module_Unneed( p_vout, p_vout->p_sys->p_qte_main );
#endif

    if( p_vout->p_sys )
    {
        free( p_vout->p_sys );
        p_vout->p_sys = NULL;
    }
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************
 * This function create the buffers needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int         i_index;
    picture_t*  p_pic;
    int         dd = QPixmap::defaultDepth();

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_chroma = (dd == 16) ? VLC_FOURCC('R','V','1','6'): VLC_FOURCC('R','V','3','2');
    p_vout->output.i_rmask  = 0xf800;
    p_vout->output.i_gmask  = 0x07e0;
    p_vout->output.i_bmask  = 0x001f;

    /* All we have is an RGB image with square pixels */
    p_vout->output.i_width  = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;
    if( !p_vout->b_fullscreen )
    {
        p_vout->output.i_aspect = p_vout->output.i_width
                                   * VOUT_ASPECT_FACTOR
                                   / p_vout->output.i_height;
    }
    else
    {
        p_vout->output.i_aspect = p_vout->render.i_aspect;
    }
#if 0
    msg_Dbg( p_vout, "Init (h=%d,w=%d,aspect=%d)",p_vout->output.i_height,p_vout->output.i_width,p_vout->output.i_aspect );
#endif
    /* Try to initialize MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < QTE_MAX_DIRECTBUFFERS )
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
        if( p_pic == NULL ||  NewPicture( p_vout, p_pic ) )
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
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to screen.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    unsigned int x, y, w, h;

    vout_PlacePicture( p_vout, p_vout->output.i_width, p_vout->output.i_height,
                       &x, &y, &w, &h );
#if 0
    msg_Dbg(p_vout, "+qte::Display( p_vout, i_width=%d, i_height=%d, x=%u, y=%u, w=%u, h=%u",
        p_vout->output.i_width, p_vout->output.i_height, x, y, w, h );
#endif

    if(p_vout->p_sys->p_VideoWidget)
    {
// shameless borrowed from opie mediaplayer....
#ifndef USE_DIRECT_PAINTER
        msg_Dbg(p_vout, "not using direct painter");
        QPainter p(p_vout->p_sys->p_VideoWidget);

        /* rotate frame */
        int dd      = QPixmap::defaultDepth();
        int bytes   = ( dd == 16 ) ? 2 : 4;
        int rw = h, rh = w;

        QImage rotatedFrame( rw, rh, bytes << 3 );

        ushort* in  = (ushort*)p_pic->p_sys->pQImage->bits();
        ushort* out = (ushort*)rotatedFrame.bits();

        int spl = rotatedFrame.bytesPerLine() / bytes;
        for (int x=0; x<h; x++)
        {
            if ( bytes == 2 )
            {
                ushort* lout = out++ + (w - 1)*spl;
                for (int y=0; y<w; y++)
                {
                    *lout=*in++;
                    lout-=spl;
                }
            }
            else
            {
                ulong* lout = ((ulong *)out)++ + (w - 1)*spl;
                for (int y=0; y<w; y++)
                {
                    *lout=*((ulong*)in)++;
                    lout-=spl;
                }
            }
        }

        p.drawImage( x, y, rotatedFrame, 0, 0, rw, rh );
#else
        QDirectPainter p(p_vout->p_sys->p_VideoWidget);
        p.transformOrientation();
        // just copy the image to the frame buffer...
        memcpy(p.frameBuffer(), (p_pic->p_sys->pQImage->jumpTable())[0], h * p.lineStep());
#endif
    }
}

/*****************************************************************************
 * Manage: handle Qte events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * Qte events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
//    msg_Dbg( p_vout, "Manage" );

    /* Fullscreen change */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        p_vout->b_fullscreen = ! p_vout->b_fullscreen;

//        p_vout->p_sys->b_cursor_autohidden = 0;
//        SDL_ShowCursor( p_vout->p_sys->b_cursor &&
//                        ! p_vout->p_sys->b_cursor_autohidden );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /*
     * Size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        msg_Dbg( p_vout, "video display resized (%dx%d)",
                 p_vout->p_sys->i_width, p_vout->p_sys->i_height );

        CloseDisplay( p_vout );
        OpenDisplay( p_vout );

        /* We don't need to signal the vout thread about the size change if
         * we can handle rescaling ourselves */
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
    }

    /* Pointer change */
//    if( ! p_vout->p_sys->b_cursor_autohidden &&
//        ( mdate() - p_vout->p_sys->i_lastmoved > 2000000 ) )
//    {
//        /* Hide the mouse automatically */
//        p_vout->p_sys->b_cursor_autohidden = 1;
//        SDL_ShowCursor( 0 );
//    }
//
//    if( p_vout->p_vlc->b_die )
//        p_vout->p_sys->bRunning = FALSE;

    return 0;
}

/*****************************************************************************
 * End: terminate video thread output method
 *****************************************************************************
 * Destroy the buffers created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}


/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int dd = QPixmap::defaultDepth();

    p_pic->p_sys = (picture_sys_t*) malloc( sizeof( picture_sys_t ) );
    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    /* Create the display */
    p_pic->p_sys->pQImage = new QImage(p_vout->output.i_width,
                                       p_vout->output.i_height, dd );

    if(p_pic->p_sys->pQImage == NULL)
    {
        return -1;
    }

    switch( dd )
    {
        case 8:
            p_pic->p->i_pixel_pitch = 1;
            break;
        case 15:
        case 16:
            p_pic->p->i_pixel_pitch = 2;
            break;
        case 24:
        case 32:
            p_pic->p->i_pixel_pitch = 4;
            break;
        default:
            return( -1 );
    }

    p_pic->p->p_pixels = (p_pic->p_sys->pQImage->jumpTable())[0];
    p_pic->p->i_pitch = p_pic->p_sys->pQImage->bytesPerLine();
    p_pic->p->i_lines = p_vout->output.i_height;
    p_pic->p->i_visible_lines = p_vout->output.i_height;
    p_pic->p->i_visible_pitch =
            p_pic->p->i_pixel_pitch * p_vout->output.i_width;

    p_pic->i_planes = 1;

    return 0;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    delete p_pic->p_sys->pQImage;
}

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    if ( p_vout->b_fullscreen )
       p_vout->p_sys->p_VideoWidget->showFullScreen();
    else
       p_vout->p_sys->p_VideoWidget->showNormal();

    p_vout->b_fullscreen = !p_vout->b_fullscreen;
}

/*****************************************************************************
 * OpenDisplay: create qte applicaton / window
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int OpenDisplay( vout_thread_t *p_vout )
{
    /* for displaying the vout in a qt window we need the QtApplication */
    p_vout->p_sys->p_QApplication = NULL;
    p_vout->p_sys->p_VideoWidget = NULL;

    p_vout->p_sys->p_event = (event_thread_t*) vlc_object_create( p_vout, sizeof(event_thread_t) );
    p_vout->p_sys->p_event->p_vout = p_vout;

    /* Initializations */
#if 1 /* FIXME: I need an event queue to handle video output size changes. */
    p_vout->b_fullscreen = VLC_TRUE;
#endif

    /* Set main window's size */
    QWidget *desktop = p_vout->p_sys->p_QApplication->desktop();
    p_vout->p_sys->i_width = p_vout->b_fullscreen ? desktop->height() :
                                                    p_vout->i_window_width;
    p_vout->p_sys->i_height = p_vout->b_fullscreen ? desktop->width() :
                                                     p_vout->i_window_height;

#if 0 /* FIXME: I need an event queue to handle video output size changes. */
    /* Update dimensions */
    p_vout->i_changes |= VOUT_SIZE_CHANGE;
    p_vout->i_window_width = p_vout->p_sys->i_width;
    p_vout->i_window_height = p_vout->p_sys->i_height;
#endif

    msg_Dbg( p_vout, "OpenDisplay (h=%d,w=%d)",p_vout->p_sys->i_height,p_vout->p_sys->i_width);

    /* create thread to exec the qpe application */
    if ( vlc_thread_create( p_vout->p_sys->p_event, "QT Embedded Thread",
                            RunQtThread,
                            VLC_THREAD_PRIORITY_OUTPUT, VLC_TRUE) )
    {
        msg_Err( p_vout, "cannot create QT Embedded Thread" );
        vlc_object_destroy( p_vout->p_sys->p_event );
        p_vout->p_sys->p_event = NULL;
        return -1;
    }

    if( p_vout->p_sys->p_event->b_error )
    {
        msg_Err( p_vout, "RunQtThread failed" );
        return -1;
    }

    vlc_object_attach( p_vout->p_sys->p_event, p_vout );
    msg_Dbg( p_vout, "RunQtThread running" );

    // just wait until the crew is complete...
    while(p_vout->p_sys->p_VideoWidget == NULL)
    {
        msleep(1);
    }
    return VLC_SUCCESS;
}


/*****************************************************************************
 * CloseDisplay: destroy the window
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    // quit qt application loop
    msg_Dbg( p_vout, "destroying Qt Window" );
#ifdef NEED_QTE_MAIN
    if(p_vout->p_sys->p_QApplication)
    {
        p_vout->p_sys->bRunning = FALSE;
        while(p_vout->p_sys->p_VideoWidget)
        {
            msleep(1);
        }
    }
#else
    if (p_vout->p_sys->p_QApplication)
       p_vout->p_sys->p_QApplication->quit();
#endif
}

/*****************************************************************************
 * main loop of qtapplication
 *****************************************************************************/
static void RunQtThread(event_thread_t *p_event)
{
    msg_Dbg( p_event->p_vout, "RunQtThread Starting" );

#ifdef NEED_QTE_MAIN
    if (qApp)
    {
        p_event->p_vout->p_sys->p_QApplication = qApp;
        p_event->p_vout->p_sys->bOwnsQApp = FALSE;
        p_event->p_vout->p_sys->p_VideoWidget = qApp->mainWidget();
        msg_Dbg( p_event->p_vout, "RunQtThread applicaton attached" );
    }
#else
    if (qApp==NULL)
    {
        int argc = 0;
        QApplication* pApp = new QApplication(argc, NULL);
        if(pApp)
        {
            p_event->p_vout->p_sys->p_QApplication = pApp;
            p_event->p_vout->p_sys->bOwnsQApp = TRUE;
        }
        QWidget* pWidget = new QWidget();
        if (pWidget)
            {
            p_event->p_vout->p_sys->p_VideoWidget = pWidget;
        }
    }
#endif
    /* signal the creation of the window */
    vlc_thread_ready( p_event );
    msg_Dbg( p_event->p_vout, "RunQtThread ready" );

    if (p_event->p_vout->p_sys->p_QApplication)
    {
        /* Set default window width and heigh to exactly preferred size. */
            QWidget *desktop = p_event->p_vout->p_sys->p_QApplication->desktop();
            p_event->p_vout->p_sys->p_VideoWidget->setMinimumWidth( 10 );
             p_event->p_vout->p_sys->p_VideoWidget->setMinimumHeight( 10 );
            p_event->p_vout->p_sys->p_VideoWidget->setBaseSize( p_event->p_vout->p_sys->i_width,
            p_event->p_vout->p_sys->i_height );
        p_event->p_vout->p_sys->p_VideoWidget->setMaximumWidth( desktop->width() );
        p_event->p_vout->p_sys->p_VideoWidget->setMaximumHeight( desktop->height() );
        /* Check on fullscreen */
        if (p_event->p_vout->b_fullscreen)
                  p_event->p_vout->p_sys->p_VideoWidget->showFullScreen();
        else
                p_event->p_vout->p_sys->p_VideoWidget->showNormal();

        p_event->p_vout->p_sys->p_VideoWidget->show();
        p_event->p_vout->p_sys->bRunning = TRUE;

#ifdef NEED_QTE_MAIN
        while(!p_event->b_die && p_event->p_vout->p_sys->bRunning)
              {
               /* Check if we are asked to exit */
           if( p_event->b_die )
               break;

               msleep(100);
            }
#else
        // run the main loop of qtapplication until someone says: 'quit'
        p_event->p_vout->p_sys->pcQApplication->exec();
#endif
    }

#ifndef NEED_QTE_MAIN
    if(p_event->p_vout->p_sys->p_QApplication)
    {
        delete p_event->p_vout->p_sys->p_VideoWidget;
        p_event->p_vout->p_sys->p_VideoWidget = NULL;
        delete p_event->p_vout->p_sys->p_QApplication;
        p_event->p_vout->p_sys->p_QApplication = NULL;
    }
#else
    p_event->p_vout->p_sys->p_VideoWidget = NULL;
#endif

    msg_Dbg( p_event->p_vout, "RunQtThread terminating" );
}

