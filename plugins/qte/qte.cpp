/*****************************************************************************
 * qte.cpp : Qt Embedded video output plugin implementation
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: qte.cpp,v 1.1.2.1 2002/09/30 20:32:46 jpsaman Exp $
 *
 * Authors: Gerald Hansink <gerald.hansink@ordina.nl>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
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
 * notes:
 * - written for ipaq, so hardcoded assumptions specific for ipaq...
 * - runs full screen
 * - no "mouse events" handling
 * - etc.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <qapplication.h>
#include <qpainter.h>

#ifdef Q_WS_QWS
# define USE_DIRECT_PAINTER
# include <qdirectpainter_qws.h>
# include <qgfxraster_qws.h>
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                                /* strerror() */

#include <videolan/vlc.h>

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


#include "video.h"
#include "video_output.h"

#include "interface.h"
#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Create    ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );
static int  vout_Manage    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );


static int  CreateQtWindow ( vout_thread_t * );
static void DestroyQtWindow( vout_thread_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static void ToggleFullScreen      ( vout_thread_t * );

static void* vout_run_qtapp_exec (void* pVoid);

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the specific properties of an video output plugin
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* Internal settings and properties */
    int                 i_width;
    int                 i_height;

    bool                bRunning;
    bool                bOwnsQApp;

    QApplication*       pcQApplication;
    QWidget*            pcVoutWidget;
} vout_sys_t;


/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************/
typedef struct picture_sys_s
{
    QImage*             pQImage;
} picture_sys_t;


/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#define QTE_MAX_DIRECTBUFFERS    2


/*****************************************************************************
 * Seeking function TODO: put this in a generic location !
 *****************************************************************************/
static inline void vout_Seek( off_t i_seek )
{
}

extern "C"
{

void _M( vout_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
}

}


/*****************************************************************************
 * vout_Create: allocate video thread output method
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "+vout_Create::qte" );

    /* Allocate structure */
    p_vout->p_sys = (vout_sys_s*) malloc( sizeof( vout_sys_t ) );

    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    memset(p_vout->p_sys, 0, sizeof( vout_sys_t ));

    CreateQtWindow(p_vout);

    //intf_ErrMsg( "-vout_Create::qte\n" );
    return( 0 );
}

/*****************************************************************************
 * vout_Destroy: destroy video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_Create
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "+vout_Destroy::qte\n" );
    DestroyQtWindow(p_vout);
    free(p_vout->p_sys);
}

/*****************************************************************************
 * vout_Init: initialize video thread output method
 *****************************************************************************
 * This function create the buffers needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int         i_index;
    picture_t*  p_pic;

    int         dd = QPixmap::defaultDepth();

    //intf_ErrMsg( "+vout_Init::qte\n" );

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_chroma = (dd == 16) ? FOURCC_RV16 : FOURCC_RV32;
    p_vout->output.i_rmask  = 0xf800;
    p_vout->output.i_gmask  = 0x07e0;
    p_vout->output.i_bmask  = 0x001f;
    //p_vout->output.i_width  = p_vout->render.i_width;
    //p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_width  = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

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

    //intf_ErrMsg( "-vout_Init::qte %d output pictures\n", I_OUTPUTPICTURES);

    return( 0 );
}


/*****************************************************************************
 * vout_Render: render previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    //intf_ErrMsg( "+vout_Render::qte\n" );
    ;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to screen.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    int x, y, w, h;

    vout_PlacePicture( p_vout, p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                       &x, &y, &w, &h );

    if(p_vout->p_sys->pcVoutWidget)
    {
// shameless borrowed from opie mediaplayer....
#ifndef USE_DIRECT_PAINTER
        QPainter p(p_vout->p_sys->pcVoutWidget);

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
        QDirectPainter p(p_vout->p_sys->pcVoutWidget);

        // just copy the image to the frame buffer...
        memcpy(p.frameBuffer(), (p_pic->p_sys->pQImage->jumpTable())[0], h * p.lineStep());
#endif
    }
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
    //intf_ErrMsg( "+vout_Manage::qte\n" );
    return 0;
}

/*****************************************************************************
 * vout_End: terminate video thread output method
 *****************************************************************************
 * Destroy the buffers created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    //intf_ErrMsg( "+vout_End::qte\n" );

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

    //intf_ErrMsg( "+NewPicture::dd = %d\n",dd );

    p_pic->p_sys = (picture_sys_t*) malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    switch(p_vout->output.i_chroma)
    {
    case FOURCC_RV16:
        if(dd == 16)
        {
            p_pic->p_sys->pQImage = new QImage(p_vout->output.i_width,
                                               p_vout->output.i_height,
                                               dd );

            if(p_pic->p_sys->pQImage == NULL)
            {
                return -1;
            }

            p_pic->p->p_pixels = (p_pic->p_sys->pQImage->jumpTable())[0];

            p_pic->p->i_pitch = p_pic->p_sys->pQImage->bytesPerLine();

            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pixel_bytes = 2;
            p_pic->p->b_margin      = 0;
            p_pic->i_planes         = 1;
        }
        else
        {
            return -1;
        }
        break;
    case FOURCC_RV32:
        if(dd == 32)
        {
            p_pic->p_sys->pQImage = new QImage(p_vout->output.i_width,
                                               p_vout->output.i_height,
                                               dd );

            if(p_pic->p_sys->pQImage == NULL)
            {
                return -1;
            }

            p_pic->p->p_pixels = (p_pic->p_sys->pQImage->jumpTable())[0];

            p_pic->p->i_pitch = p_pic->p_sys->pQImage->bytesPerLine();

            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pixel_bytes = 4;
            p_pic->p->b_margin      = 0;
            p_pic->i_planes         = 1;
        }
        else
        {
            return -1;
        }
        break;
    default:
        return -1;
        break;
    }

/*
    intf_ErrMsg( "NewPicture: %d %d %d\n",p_vout->output.i_width,
                                 p_vout->output.i_height,
                                 p_vout->output.i_chroma );
*/
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
}


/*****************************************************************************
 * CreateQtWindow: create qte applicaton / window
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int CreateQtWindow( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "vout_qt: +init qt window");

    /* for displaying the vout in a qt window we need the QtApplication */
    vlc_thread_t    thread_id;
    //intf_ErrMsg( "vout_qt: +init qt window, creating qpe application");

    p_vout->p_sys->pcVoutWidget = NULL;

    /* create thread to exec the qpe application */
    if ( vlc_thread_create( &thread_id, "vout qte",
                            (vlc_thread_func_t)vout_run_qtapp_exec,
                            (void *)p_vout) )
    {
        intf_ErrMsg( "input error: can't spawn vout thread");
        return( -1 );
    }

    p_vout->p_sys->i_width  = 320;
    p_vout->p_sys->i_height = 240;

    // just wait until the crew is complete...
    while(p_vout->p_sys->pcVoutWidget == NULL)
    {
        msleep(1);
    }

    //intf_ErrMsg( "vout_qt: -init qt window");

    return( 0 );
}


/*****************************************************************************
 * DestroyQtWindow: destroy the window
 *****************************************************************************/
static void DestroyQtWindow( vout_thread_t *p_vout )
{
    // quit qt application loop
    if(p_vout->p_sys->pcQApplication)
    {
        if(p_vout->p_sys->bOwnsQApp)
        {
            p_vout->p_sys->pcQApplication->quit();
        }
        else
        {
            p_vout->p_sys->bRunning = FALSE;
        }

        while(p_vout->p_sys->pcVoutWidget)
        {
            msleep(1);
        }
    }
}

/*****************************************************************************
 * main loop of qtapplication
 *****************************************************************************/
static void*
vout_run_qtapp_exec(void* pVoid)
{
    int     argc    = 0;
    char    arg0[]  = "vout qte";

    vout_thread_t* p_vout = (vout_thread_t*) pVoid;

    if(qApp == NULL)
    {
        QApplication* pApp = new QApplication(argc, NULL);
        if(pApp)
        {
            p_vout->p_sys->pcQApplication = pApp;
            p_vout->p_sys->bOwnsQApp = TRUE;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        p_vout->p_sys->pcQApplication = qApp;
    }

    {
        QWidget vo(0, "vout");
        vo.showFullScreen();
        vo.show();
        p_vout->p_sys->pcVoutWidget = &vo;

        p_vout->p_sys->bRunning = TRUE;

        if(p_vout->p_sys->bOwnsQApp)
        {
            // run the main loop of qtapplication until someone says: 'quit'
            p_vout->p_sys->pcQApplication->exec();
        }
        else
        {
            while(p_vout->p_sys->bRunning) msleep(100);
        }
    }

    p_vout->p_sys->pcVoutWidget = NULL;

    if(p_vout->p_sys->bOwnsQApp)
    {
        delete p_vout->p_sys->pcQApplication;
        p_vout->p_sys->pcQApplication = NULL;
    }

    return 0;
}

