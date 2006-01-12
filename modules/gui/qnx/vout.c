/*****************************************************************************
 * vout.c: QNX RTOS video output display method
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Pascal Levesque <Pascal.Levesque@mindready.com>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <photon/PtWidget.h>
#include <photon/PtWindow.h>
#include <photon/PtLabel.h>
#include <photon/PdDirect.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

/*****************************************************************************
 * vout_sys_t: video output QNX method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the QNX specific properties of an output thread. QNX video
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *****************************************************************************/
#define MAX_DIRECTBUFFERS 2

#define MODE_NORMAL_MEM     0
#define MODE_SHARED_MEM     1
#define MODE_VIDEO_MEM      2
#define MODE_VIDEO_OVERLAY  3

struct vout_sys_t
{
    /* video mode */
    int                     i_mode;

    /* internal stuff */
    PtWidget_t *            p_window;

    /* Color palette for 8bpp */
    PgColor_t p_colors[255];

    /* [shared] memory blit */
    int                     i_img_type;

    /* video memory blit */

    /* video overlay */
    PgVideoChannel_t *      p_channel;
    int                     i_vc_flags;
    int                     i_vc_format;

    int                 i_screen_depth;
    int                 i_bytes_per_pixel;
    int                 i_bytes_per_line;

    /* position & dimensions */
    PhPoint_t               pos;
    PhDim_t                 dim;
    PhPoint_t               old_pos;
    PhDim_t                 old_dim;
    PhDim_t                 screen_dim;
    PhRect_t                frame;
};


/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * XVideo specific properties of a direct buffer.
 *****************************************************************************/
struct picture_sys_t
{
    /* [shared] memory blit */
    PhImage_t *             p_image;

    /* video memory blit and video overlay */
    PdOffscreenContext_t *  p_ctx[3];   /* 0: y, 1: u, 2: v */
    char *                  p_buf[3];
};


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  QNXInit      ( vout_thread_t * );
static void QNXEnd       ( vout_thread_t * );
static int  QNXManage    ( vout_thread_t * );
static void QNXDisplay   ( vout_thread_t *, picture_t * );

static int  QNXInitDisplay ( vout_thread_t * );
static int  QNXCreateWnd   ( vout_thread_t * );
static int  QNXDestroyWnd  ( vout_thread_t * );

static int  NewPicture     ( vout_thread_t *, picture_t *, int );
static void FreePicture    ( vout_thread_t *, picture_t * );
static int  ResizeOverlayOutput ( vout_thread_t * );
static void SetPalette     ( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

/*****************************************************************************
 * OpenVideo: allocate QNX video thread output method
 *****************************************************************************
 * This function allocate and initialize a QNX vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
int E_(OpenVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    /* init connection to photon */
    if( PtInit( "/dev/photon" ) != 0 )
    {
        msg_Err( p_vout, "unable to connect to photon" );
        return( 1 );
    }

    /* allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    p_vout->b_fullscreen = config_GetInt( p_vout, "fullscreen" );
    p_vout->p_sys->i_mode = config_GetInt( p_vout, "overlay" ) ?
                                MODE_VIDEO_OVERLAY : MODE_VIDEO_MEM;
    p_vout->p_sys->dim.w = p_vout->i_window_width;
    p_vout->p_sys->dim.h = p_vout->i_window_height;

    /* init display and create window */
    if( QNXInitDisplay( p_vout ) || QNXCreateWnd( p_vout ) )
    {
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->pf_init = QNXInit;
    p_vout->pf_end = QNXEnd;
    p_vout->pf_manage = QNXManage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = QNXDisplay;

    return( 0 );
}

/*****************************************************************************
 * QNXInit: initialize QNX video thread output method
 *****************************************************************************
 * This function create the buffers needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int QNXInit( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    switch( p_vout->p_sys->i_mode )
    {
    case MODE_NORMAL_MEM:
    case MODE_SHARED_MEM:
        p_vout->output.i_width = p_vout->p_sys->dim.w;
        p_vout->output.i_height = p_vout->p_sys->dim.h;

        /* Assume we have square pixels */
        p_vout->output.i_aspect = p_vout->p_sys->dim.w
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->dim.h;
        break;

    case MODE_VIDEO_MEM:
        p_vout->output.i_width = p_vout->p_sys->dim.w;
        p_vout->output.i_height = p_vout->p_sys->dim.h;

        /* Assume we have square pixels */
        p_vout->output.i_aspect = p_vout->p_sys->dim.w
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->dim.h;
        break;

    case MODE_VIDEO_OVERLAY:
        p_vout->output.i_width  = p_vout->render.i_width;
        p_vout->output.i_height = p_vout->render.i_height;
        p_vout->output.i_aspect = p_vout->render.i_aspect;

        if (ResizeOverlayOutput(p_vout))
        {
            return (1);
        }
        break;

    default:
        /* This shouldn't happen ! */
        break;
    }

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
        if( p_pic == NULL || NewPicture( p_vout, p_pic, I_OUTPUTPICTURES ) )
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
 * QNXEnd: terminate QNX video thread output method
 *****************************************************************************
 * Destroy the buffers created by QNXInit. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void QNXEnd( vout_thread_t *p_vout )
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
 * CloseVideo: destroy QNX video thread output method
 *****************************************************************************
 * Terminate an output method created by QNXCreate
 *****************************************************************************/
void E_(CloseVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    /* destroy the window */
    QNXDestroyWnd( p_vout );

    /* destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * QNXManage: handle QNX events
 *****************************************************************************
 * This function should be called regularly by video output thread. It allows
 * window resizing. It returns a non null value on error.
 *****************************************************************************/
static int QNXManage( vout_thread_t *p_vout )
{
    int i_ev,  i_buflen;
    PhEvent_t *p_event;
    vlc_bool_t b_repos = 0;

    if (p_vout->b_die == 1)
    {
        return ( 0 );
    }

    /* allocate buffer for event */
    i_buflen = sizeof( PhEvent_t ) * 4;
    if( ( p_event = malloc( i_buflen ) ) == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    /* event loop */
    do
    {
        memset( p_event, 0, i_buflen );
        i_ev = PhEventPeek( p_event, i_buflen );

        if( i_ev == Ph_RESIZE_MSG )
        {
            i_buflen = PhGetMsgSize( p_event );
            if( ( p_event = realloc( p_event, i_buflen ) ) == NULL )
            {
                msg_Err( p_vout, "out of memory" );
                return( 1 );
            }
        }
        else if( i_ev == Ph_EVENT_MSG )
        {
            PtEventHandler( p_event );

            if( p_event->type == Ph_EV_WM )
            {
                PhWindowEvent_t *p_ev = PhGetData( p_event );

                switch( p_ev->event_f )
                {
                case Ph_WM_CLOSE:
                    p_vout->p_vlc->b_die = 1;
                    break;

                case Ph_WM_MOVE:
                    p_vout->p_sys->pos.x = p_ev->pos.x;
                    p_vout->p_sys->pos.y = p_ev->pos.y;
                    b_repos = 1;
                    break;

                case Ph_WM_RESIZE:
                    p_vout->p_sys->old_dim.w = p_vout->p_sys->dim.w;
                    p_vout->p_sys->old_dim.h = p_vout->p_sys->dim.h;
                    p_vout->p_sys->dim.w = p_ev->size.w;
                    p_vout->p_sys->dim.h = p_ev->size.h;
                    p_vout->i_changes |= VOUT_SIZE_CHANGE;
                    break;
                }
            }
            else if( p_event->type == Ph_EV_KEY )
            {
                PhKeyEvent_t *p_ev = PhGetData( p_event );
                long i_key = p_ev->key_sym;

                if( ( p_ev->key_flags & Pk_KF_Key_Down ) &&
                    ( p_ev->key_flags & Pk_KF_Sym_Valid ) )
                {
                    switch( i_key )
                    {
                    case Pk_q:
                    case Pk_Q:
                        p_vout->p_vlc->b_die = 1;
                        break;

                    case Pk_f:
                    case Pk_F:
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                        break;

                    case Pk_c:
                    case Pk_C:
                        p_vout->b_grayscale = ! p_vout->b_grayscale;
                        p_vout->i_changes |= VOUT_GRAYSCALE_CHANGE;
                        break;

                    default:
                        break;
                    }
                }
            }
        }
    } while( i_ev != -1 && i_ev != 0 );

    free( p_event );

    /*
     * fullscreen
     */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        PhDim_t dim;

        p_vout->b_fullscreen = !p_vout->b_fullscreen;
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;

        if( p_vout->b_fullscreen )
        {
            p_vout->p_sys->old_pos.x = p_vout->p_sys->pos.x;
            p_vout->p_sys->old_pos.y = p_vout->p_sys->pos.y;
            p_vout->p_sys->pos.x = p_vout->p_sys->pos.y = 0;
            dim.w = p_vout->p_sys->screen_dim.w + 1;
            dim.h = p_vout->p_sys->screen_dim.h + 1;
        }
        else
        {
            p_vout->p_sys->pos.x = p_vout->p_sys->old_pos.x;
            p_vout->p_sys->pos.y = p_vout->p_sys->old_pos.y;
            dim.w = p_vout->p_sys->old_dim.w + 1;
            dim.h = p_vout->p_sys->old_dim.h + 1;
        }

        /* modify render flags, border */
        PtSetResource( p_vout->p_sys->p_window,
            Pt_ARG_WINDOW_RENDER_FLAGS,
            p_vout->b_fullscreen ? Pt_FALSE : Pt_TRUE,
            Ph_WM_RENDER_BORDER | Ph_WM_RENDER_TITLE );

        /* set position and dimension */
        PtSetResource( p_vout->p_sys->p_window,
                       Pt_ARG_POS, &p_vout->p_sys->pos, 0 );
        PtSetResource( p_vout->p_sys->p_window,
                       Pt_ARG_DIM, &dim, 0 );

        /* mark as damaged to force redraw */
        PtDamageWidget( p_vout->p_sys->p_window );
    }

    /*
     * size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
        {
            ResizeOverlayOutput(p_vout);
        }
#if 0
        else
        {
            p_vout->output.i_width = p_vout->p_sys->dim.w;
            p_vout->output.i_height = p_vout->p_sys->dim.h;
            p_vout->i_changes |= VOUT_YUV_CHANGE;

            QNXEnd( p_vout );
            if( QNXInit( p_vout ) )
            {
                msg_Err( p_vout, "cannot resize display" );
                return( 1 );
            }
        }
#endif

        msg_Dbg( p_vout, "video display resized (%dx%d)",
                         p_vout->p_sys->dim.w, p_vout->p_sys->dim.h );
    }

    /*
     * position change, move video channel
     */
    if( b_repos && p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        ResizeOverlayOutput(p_vout);
    }

    return( i_ev == -1 );
}

/*****************************************************************************
 * QNXDisplay: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to QNX server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
static void QNXDisplay( vout_thread_t *p_vout, picture_t *p_pic )
{
    if( p_vout->p_sys->i_mode == MODE_NORMAL_MEM ||
        p_vout->p_sys->i_mode == MODE_SHARED_MEM )
    {
        PhPoint_t pos = { 0, 0 };

        PgSetRegion( PtWidgetRid( p_vout->p_sys->p_window ) );
        if (p_vout->p_sys->i_screen_depth == 8)
        {
            PgSetPalette( p_vout->p_sys->p_colors, 0, 0, 255, Pg_PALSET_SOFT, 0);
        }
        PgDrawPhImagemx( &pos, p_pic->p_sys->p_image, 0 );
        PgFlush();
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_MEM )
    {
        PhRect_t rc = { { 0, 0 }, { p_vout->output.i_width, p_vout->output.i_height } };

//        PgSetRegion( PtWidgetRid ( p_vout->p_sys->p_window ) );
        PgContextBlit( p_pic->p_sys->p_ctx[0], &rc, NULL, &rc );
        PgFlush();
    }
}

/*****************************************************************************
 * QNXInitDisplay: check screen resolution, depth, amount of video ram, etc
 *****************************************************************************/
static int QNXInitDisplay( vout_thread_t * p_vout )
{
    PgHWCaps_t hwcaps;
    PgDisplaySettings_t cfg;
    PgVideoModeInfo_t minfo;

    /* get graphics card hw capabilities */
    if( PgGetGraphicsHWCaps( &hwcaps ) != 0 )
    {
        msg_Err( p_vout, "unable to get gfx card capabilities" );
        return( 1 );
    }

    /* get current video mode */
    if( PgGetVideoMode( &cfg ) != 0 )
    {
        msg_Err( p_vout, "unable to get current video mode" );
        return( 1 );
    }

    /* get video mode info */
    if( PgGetVideoModeInfo( cfg.mode, &minfo ) != 0 )
    {
        msg_Err( p_vout, "unable to get info for video mode" );
        return( 1 );
    }

    if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        int i = 0;
        PgScalerCaps_t vcaps;

        if( ( p_vout->p_sys->p_channel =
            PgCreateVideoChannel( Pg_VIDEO_CHANNEL_SCALER, 0 ) ) == NULL )
        {
            msg_Err( p_vout, "unable to create video channel" );
            printf("errno = %d\n", errno);
            p_vout->p_sys->i_mode = MODE_NORMAL_MEM;
        }
        else
        {
            vcaps.size = sizeof( vcaps );
            while( PgGetScalerCapabilities( p_vout->p_sys->p_channel,
                                            i++, &vcaps ) == 0 )
            {
                printf("vcaps.format = 0x%x\n", vcaps.format);
                if( vcaps.format == Pg_VIDEO_FORMAT_YV12 ||
                    vcaps.format == Pg_VIDEO_FORMAT_YUV420 ||
                    vcaps.format == Pg_VIDEO_FORMAT_YUY2 ||
                    vcaps.format == Pg_VIDEO_FORMAT_UYVY ||
                    vcaps.format == Pg_VIDEO_FORMAT_RGB555 ||
                    vcaps.format == Pg_VIDEO_FORMAT_RGB565 ||
                    vcaps.format == Pg_VIDEO_FORMAT_RGB8888 )
                {
                    p_vout->p_sys->i_vc_flags  = vcaps.flags;
                    p_vout->p_sys->i_vc_format = vcaps.format;
                }

                vcaps.size = sizeof( vcaps );
            }

            if( p_vout->p_sys->i_vc_format == 0 )
            {
                msg_Warn( p_vout, "need YV12, YUY2 or RGB8888 overlay" );

                p_vout->p_sys->i_mode = MODE_NORMAL_MEM;
            }
        }
    }

    /* use video ram if we have enough available */
    if( p_vout->p_sys->i_mode == MODE_NORMAL_MEM &&
        (minfo.bits_per_pixel != 8) &&
        hwcaps.currently_available_video_ram >=
        ( ( minfo.width * minfo.height * minfo.bits_per_pixel * MAX_DIRECTBUFFERS) / 8 ) )
    {
        p_vout->p_sys->i_mode = MODE_VIDEO_MEM;
        printf("Using video memory...\n");
    }

    p_vout->p_sys->i_img_type = minfo.type;
    p_vout->p_sys->screen_dim.w = minfo.width;
    p_vout->p_sys->screen_dim.h = minfo.height;
    p_vout->p_sys->i_screen_depth = minfo.bits_per_pixel;

    switch( p_vout->p_sys->i_screen_depth )
    {
        case 8:
            p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
            p_vout->p_sys->i_bytes_per_pixel = 1;
            p_vout->output.pf_setpalette = SetPalette;
            break;

        case 15:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5');
            p_vout->p_sys->i_bytes_per_pixel = 2;
            p_vout->output.i_rmask = 0x7c00;
            p_vout->output.i_gmask = 0x03e0;
            p_vout->output.i_bmask = 0x001f;
            break;

        case 16:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
            p_vout->p_sys->i_bytes_per_pixel = 2;
            p_vout->output.i_rmask = 0xf800;
            p_vout->output.i_gmask = 0x07e0;
            p_vout->output.i_bmask = 0x001f;
            break;

        case 24:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4');
            p_vout->p_sys->i_bytes_per_pixel = 3;
            p_vout->output.i_rmask = 0xff0000;
            p_vout->output.i_gmask = 0x00ff00;
            p_vout->output.i_bmask = 0x0000ff;
            break;

        case 32:
        default:
            p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
            p_vout->p_sys->i_bytes_per_pixel = 4;
            p_vout->output.i_rmask = 0xff0000;
            p_vout->output.i_gmask = 0x00ff00;
            p_vout->output.i_bmask = 0x0000ff;
            break;
    }

    return( 0 );
}

/*****************************************************************************
 * QNXCreateWnd: create and realize the main window
 *****************************************************************************/
static int QNXCreateWnd( vout_thread_t * p_vout )
{
    PtArg_t args[8];
    PhPoint_t pos = { 0, 0 };
    PgColor_t color = Pg_BLACK;

    if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        if( p_vout->p_sys->i_vc_flags & Pg_SCALER_CAP_DST_CHROMA_KEY )
        {
            color = PgGetOverlayChromaColor();
        }
    }

    /* fullscreen, set dimension */
    if( p_vout->b_fullscreen )
    {
        p_vout->p_sys->old_dim.w = p_vout->p_sys->dim.w;
        p_vout->p_sys->old_dim.h = p_vout->p_sys->dim.h;
        p_vout->output.i_width = p_vout->p_sys->dim.w = p_vout->p_sys->screen_dim.w;
        p_vout->output.i_height = p_vout->p_sys->dim.h = p_vout->p_sys->screen_dim.h;
    }

    /* set window parameters */
    PtSetArg( &args[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &args[1], Pt_ARG_DIM, &p_vout->p_sys->dim, 0 );
    PtSetArg( &args[2], Pt_ARG_FILL_COLOR, color, 0 );
    PtSetArg( &args[3], Pt_ARG_WINDOW_TITLE, "VLC media player", 0 );
    PtSetArg( &args[4], Pt_ARG_WINDOW_MANAGED_FLAGS, Pt_FALSE, Ph_WM_CLOSE );
    PtSetArg( &args[5], Pt_ARG_WINDOW_NOTIFY_FLAGS, Pt_TRUE,
              Ph_WM_MOVE | Ph_WM_RESIZE | Ph_WM_CLOSE );
    PtSetArg( &args[6], Pt_ARG_WINDOW_RENDER_FLAGS,
              p_vout->b_fullscreen ? Pt_FALSE : Pt_TRUE,
              Ph_WM_RENDER_BORDER | Ph_WM_RENDER_TITLE );

    /* create window */
    p_vout->p_sys->p_window = PtCreateWidget( PtWindow, Pt_NO_PARENT, 7, args);
    if( p_vout->p_sys->p_window == NULL )
    {
        msg_Err( p_vout, "unable to create window" );
        return( 1 );
    }

    /* realize the window widget */
    if( PtRealizeWidget( p_vout->p_sys->p_window ) != 0 )
    {
        msg_Err( p_vout, "unable to realize window widget" );
        PtDestroyWidget( p_vout->p_sys->p_window );
        return( 1 );
    }

    /* get window frame size */
    if( PtWindowFrameSize( NULL, p_vout->p_sys->p_window,
                           &p_vout->p_sys->frame ) != 0 )
    {
        msg_Err( p_vout, "unable to get window frame size" );
        PtDestroyWidget( p_vout->p_sys->p_window );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * QNXDestroyWnd: unrealize and destroy the main window
 *****************************************************************************/
static int QNXDestroyWnd( vout_thread_t * p_vout )
{
    /* destroy the window widget */
    PtUnrealizeWidget( p_vout->p_sys->p_window );
//    PtDestroyWidget( p_vout->p_sys->p_window );

    /* destroy video channel */
    if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        PgDestroyVideoChannel( p_vout->p_sys->p_channel );
    }

    return( 0 );
}


/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic, int index )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    switch( p_vout->p_sys->i_mode )
    {
    case MODE_NORMAL_MEM:
    case MODE_SHARED_MEM:
        /* create images for [shared] memory blit */
        if( !( p_pic->p_sys->p_image = PhCreateImage( NULL,
                    p_vout->p_sys->dim.w, p_vout->p_sys->dim.h,
                    p_vout->p_sys->i_img_type, NULL, 0,
                    p_vout->p_sys->i_mode == MODE_SHARED_MEM ) ) ) {
            msg_Err( p_vout, "cannot create image" );
            free( p_pic->p_sys );
            return( -1 );
        }

        p_pic->p->p_pixels = p_pic->p_sys->p_image->image;
        p_pic->p->i_lines = p_pic->p_sys->p_image->size.h;
        p_pic->p->i_visible_lines = p_pic->p_sys->p_image->size.h;
        p_pic->p->i_pitch = p_pic->p_sys->p_image->bpl;
        p_pic->p->i_pixel_pitch = p_vout->p_sys->i_bytes_per_pixel;
        p_pic->p->i_visible_pitch = p_vout->p_sys->i_bytes_per_pixel
                                     * p_pic->p_sys->p_image->size.w;
        p_pic->i_planes = 1;
        break;

    case MODE_VIDEO_MEM:
        /* create offscreen contexts for video memory blit */
        if( ( p_pic->p_sys->p_ctx[0] = PdCreateOffscreenContext( 0,
                        p_vout->p_sys->dim.w, p_vout->p_sys->dim.h,
                       Pg_OSC_MEM_PAGE_ALIGN) ) == NULL )
        {
            msg_Err( p_vout, "unable to create offscreen context" );
            free( p_pic->p_sys );
            return( -1 );
        }

        /* get context pointers */
        if( (  p_pic->p_sys->p_buf[0] =
            PdGetOffscreenContextPtr ( p_pic->p_sys->p_ctx[0] ) ) == NULL )
        {
            msg_Err( p_vout, "unable to get offscreen context ptr" );
            PhDCRelease ( p_pic->p_sys->p_ctx[0] );
            p_pic->p_sys->p_ctx[0] = NULL;
            free( p_pic->p_sys );
            return( -1 );
        }

        p_vout->p_sys->i_bytes_per_line = p_pic->p_sys->p_ctx[0]->pitch;
        memset( p_pic->p_sys->p_buf[0], 0,
            p_vout->p_sys->i_bytes_per_line * p_vout->p_sys->dim.h );

        p_pic->p->p_pixels = p_pic->p_sys->p_buf[0];
        p_pic->p->i_lines = p_pic->p_sys->p_ctx[0]->dim.h;
        p_pic->p->i_visible_lines = p_pic->p_sys->p_ctx[0]->dim.h;
        p_pic->p->i_pitch = p_pic->p_sys->p_ctx[0]->pitch;
        p_pic->p->i_pixel_pitch = p_vout->p_sys->i_bytes_per_pixel;
        p_pic->p->i_visible_pitch = p_vout->p_sys->i_bytes_per_pixel
                                     * p_pic->p_sys->p_ctx[0]->dim.w;
        p_pic->i_planes = 1;
        break;

    case MODE_VIDEO_OVERLAY:
        if (index == 0)
        {
            p_pic->p_sys->p_ctx[Y_PLANE] = p_vout->p_sys->p_channel->yplane1;
            p_pic->p_sys->p_ctx[U_PLANE] = p_vout->p_sys->p_channel->uplane1;
            p_pic->p_sys->p_ctx[V_PLANE] = p_vout->p_sys->p_channel->vplane1;
        }
        else
        {
            p_pic->p_sys->p_ctx[Y_PLANE] = p_vout->p_sys->p_channel->yplane2;
            p_pic->p_sys->p_ctx[U_PLANE] = p_vout->p_sys->p_channel->uplane2;
            p_pic->p_sys->p_ctx[V_PLANE] = p_vout->p_sys->p_channel->vplane2;
        }

        p_pic->p_sys->p_buf[Y_PLANE] = PdGetOffscreenContextPtr( p_pic->p_sys->p_ctx[Y_PLANE] );
        if( p_pic->p_sys->p_buf[Y_PLANE] == NULL )
        {
            msg_Err( p_vout, "unable to get video channel ctx ptr" );
            return( 1 );
        }

        switch (p_vout->p_sys->i_vc_format)
        {
            case Pg_VIDEO_FORMAT_YUV420:
                p_vout->output.i_chroma = VLC_FOURCC('I','4','2','0');

                p_pic->p_sys->p_buf[U_PLANE] = PdGetOffscreenContextPtr( p_pic->p_sys->p_ctx[U_PLANE] );
                p_pic->p_sys->p_buf[V_PLANE] = PdGetOffscreenContextPtr( p_pic->p_sys->p_ctx[V_PLANE] );

                if( p_pic->p_sys->p_buf[U_PLANE] == NULL ||
                    p_pic->p_sys->p_buf[V_PLANE] == NULL )
                {
                    msg_Err( p_vout, "unable to get video channel ctx ptr" );
                    return( 1 );
                }

                p_pic->Y_PIXELS = p_pic->p_sys->p_buf[Y_PLANE];
                p_pic->p[Y_PLANE].i_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p[Y_PLANE].i_visible_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_ctx[Y_PLANE]->pitch;
                p_pic->p[Y_PLANE].i_pixel_pitch = 1;
                p_pic->p[Y_PLANE].i_visible_pitch = p_pic->p[Y_PLANE].i_pitch;

                p_pic->U_PIXELS = p_pic->p_sys->p_buf[U_PLANE];
                p_pic->p[U_PLANE].i_lines = p_pic->p_sys->p_ctx[U_PLANE]->dim.h;
                p_pic->p[U_PLANE].i_visible_lines = p_pic->p_sys->p_ctx[U_PLANE]->dim.h;
                p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_ctx[U_PLANE]->pitch;
                p_pic->p[U_PLANE].i_pixel_pitch = 1;
                p_pic->p[U_PLANE].i_visible_pitch = p_pic->p[U_PLANE].i_pitch;

                p_pic->V_PIXELS = p_pic->p_sys->p_buf[V_PLANE];
                p_pic->p[V_PLANE].i_lines = p_pic->p_sys->p_ctx[V_PLANE]->dim.h;
                p_pic->p[V_PLANE].i_visible_lines = p_pic->p_sys->p_ctx[V_PLANE]->dim.h;
                p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_ctx[V_PLANE]->pitch;
                p_pic->p[V_PLANE].i_pixel_pitch = 1;
                p_pic->p[V_PLANE].i_visible_pitch = p_pic->p[V_PLANE].i_pitch;

                p_pic->i_planes = 3;
                break;

            case Pg_VIDEO_FORMAT_YV12:
                p_vout->output.i_chroma = VLC_FOURCC('Y','V','1','2');

                p_pic->p_sys->p_buf[U_PLANE] = PdGetOffscreenContextPtr( p_pic->p_sys->p_ctx[U_PLANE] );
                p_pic->p_sys->p_buf[V_PLANE] = PdGetOffscreenContextPtr( p_pic->p_sys->p_ctx[V_PLANE] );

                if( p_pic->p_sys->p_buf[U_PLANE] == NULL ||
                    p_pic->p_sys->p_buf[V_PLANE] == NULL )
                {
                    msg_Err( p_vout, "unable to get video channel ctx ptr" );
                    return( 1 );
                }

                p_pic->Y_PIXELS = p_pic->p_sys->p_buf[Y_PLANE];
                p_pic->p[Y_PLANE].i_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p[Y_PLANE].i_visible_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_ctx[Y_PLANE]->pitch;
                p_pic->p[Y_PLANE].i_pixel_pitch = 1;
                p_pic->p[Y_PLANE].i_visible_pitch = p_pic->p[Y_PLANE].i_pitch;

                p_pic->U_PIXELS = p_pic->p_sys->p_buf[U_PLANE];
                p_pic->p[U_PLANE].i_lines = p_pic->p_sys->p_ctx[U_PLANE]->dim.h;
                p_pic->p[U_PLANE].i_visible_lines = p_pic->p_sys->p_ctx[U_PLANE]->dim.h;
                p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_ctx[U_PLANE]->pitch;
                p_pic->p[U_PLANE].i_pixel_pitch = 1;
                p_pic->p[U_PLANE].i_visible_pitch = p_pic->p[U_PLANE].i_pitch;

                p_pic->V_PIXELS = p_pic->p_sys->p_buf[V_PLANE];
                p_pic->p[V_PLANE].i_lines = p_pic->p_sys->p_ctx[V_PLANE]->dim.h;
                p_pic->p[V_PLANE].i_visible_lines = p_pic->p_sys->p_ctx[V_PLANE]->dim.h;
                p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_ctx[V_PLANE]->pitch;
                p_pic->p[V_PLANE].i_pixel_pitch = 1;
                p_pic->p[V_PLANE].i_visible_pitch = p_pic->p[V_PLANE].i_pitch;

                p_pic->i_planes = 3;
                break;

            case Pg_VIDEO_FORMAT_UYVY:
            case Pg_VIDEO_FORMAT_YUY2:
                if (p_vout->p_sys->i_vc_format == Pg_VIDEO_FORMAT_UYVY)
                {
                    p_vout->output.i_chroma = VLC_FOURCC('U','Y','V','Y');
                }
                else
                {
                    p_vout->output.i_chroma = VLC_FOURCC('Y','U','Y','2');
                }

                p_pic->p->p_pixels = p_pic->p_sys->p_buf[Y_PLANE];
                p_pic->p->i_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_visible_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_pitch = p_pic->p_sys->p_ctx[Y_PLANE]->pitch;
                p_pic->p->i_pixel_pitch = 4;
                p_pic->p->i_visible_pitch = p_pic->p->i_pitch;

                p_pic->i_planes = 1;
                break;

            case Pg_VIDEO_FORMAT_RGB555:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','1','5');
                p_vout->output.i_rmask = 0x001f;
                p_vout->output.i_gmask = 0x03e0;
                p_vout->output.i_bmask = 0x7c00;

                p_pic->p->p_pixels = p_pic->p_sys->p_buf[Y_PLANE];
                p_pic->p->i_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_visible_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_pitch = p_pic->p_sys->p_ctx[Y_PLANE]->pitch;
                p_pic->p->i_pixel_pitch = 2;
                p_pic->p->i_visible_pitch = 2 * p_pic->p_sys->p_ctx[Y_PLANE]->dim.w;

                p_pic->i_planes = 1;
                break;

            case Pg_VIDEO_FORMAT_RGB565:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
                p_vout->output.i_rmask = 0x001f;
                p_vout->output.i_gmask = 0x07e0;
                p_vout->output.i_bmask = 0xf800;

                p_pic->p->p_pixels = p_pic->p_sys->p_buf[Y_PLANE];
                p_pic->p->i_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_visible_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_pitch = p_pic->p_sys->p_ctx[Y_PLANE]->pitch;
                p_pic->p->i_pixel_pitch = 4;
                p_pic->p->i_visible_pitch = 4 * p_pic->p_sys->p_ctx[Y_PLANE]->dim.w;

                p_pic->i_planes = 1;
                break;

            case Pg_VIDEO_FORMAT_RGB8888:
                p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
                p_vout->output.i_rmask = 0x000000ff;
                p_vout->output.i_gmask = 0x0000ff00;
                p_vout->output.i_bmask = 0x00ff0000;

                p_pic->p->p_pixels = p_pic->p_sys->p_buf[Y_PLANE];
                p_pic->p->i_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_visible_lines = p_pic->p_sys->p_ctx[Y_PLANE]->dim.h;
                p_pic->p->i_pitch = p_pic->p_sys->p_ctx[Y_PLANE]->pitch;
                p_pic->p->i_pixel_pitch = 4;
                p_pic->p->i_visible_pitch = 4 * p_pic->p_sys->p_ctx[Y_PLANE]->dim.w;

                p_pic->i_planes = 1;
                break;
        }

#if 0
    switch( p_vout->output.i_chroma )
    {
#ifdef MODULE_NAME_IS_xvideo
        case VLC_FOURCC('Y','2','1','1'):

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_visible_lines = p_vout->output.i_height;
            /* XXX: this just looks so plain wrong... check it out ! */
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0] / 4;
            p_pic->p->i_pixel_pitch = 4;
            p_pic->p->i_visible_pitch = p_pic->p->i_pitch;

            p_pic->i_planes = 1;
            break;
#endif

#endif

    default:
        /* This shouldn't happen ! */
        break;
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
    if( ( p_vout->p_sys->i_mode == MODE_NORMAL_MEM ||
        p_vout->p_sys->i_mode == MODE_SHARED_MEM ) &&
        p_pic->p_sys->p_image )
    {
        PhReleaseImage( p_pic->p_sys->p_image );
        free( p_pic->p_sys->p_image );
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_MEM &&
             p_pic->p_sys->p_ctx[0] )
    {
        PhDCRelease( p_pic->p_sys->p_ctx[0] );
    }

    free( p_pic->p_sys );
}


static int ResizeOverlayOutput(vout_thread_t *p_vout)
{
    int i_width, i_height, i_x, i_y;
    int i_ret;
    PgScalerProps_t props;

    props.size   = sizeof( props );
    props.format = p_vout->p_sys->i_vc_format;
    props.flags  = Pg_SCALER_PROP_SCALER_ENABLE |
                          Pg_SCALER_PROP_DOUBLE_BUFFER;

    /* enable chroma keying if available */
    if( p_vout->p_sys->i_vc_flags & Pg_SCALER_CAP_DST_CHROMA_KEY )
    {
        props.flags |= Pg_SCALER_PROP_CHROMA_ENABLE;
    }

    /* set viewport position */
    props.viewport.ul.x = p_vout->p_sys->pos.x;
    props.viewport.ul.y = p_vout->p_sys->pos.y;
    if( !p_vout->b_fullscreen )
    {
        props.viewport.ul.x += p_vout->p_sys->frame.ul.x;
        props.viewport.ul.y += p_vout->p_sys->frame.ul.y;
    }

    /* set viewport dimension */
    vout_PlacePicture( p_vout, p_vout->p_sys->dim.w,
                           p_vout->p_sys->dim.h,
                           &i_x, &i_y, &i_width, &i_height );

    props.viewport.ul.x += i_x;
    props.viewport.ul.y += i_y;
    props.viewport.lr.x = i_width + props.viewport.ul.x;
    props.viewport.lr.y = i_height + props.viewport.ul.y;

    /* set source dimension */
    props.src_dim.w = p_vout->output.i_width;
    props.src_dim.h = p_vout->output.i_height;

    /* configure scaler channel */
    i_ret = PgConfigScalerChannel( p_vout->p_sys->p_channel, &props );

    if( i_ret == -1 )
    {
        msg_Err( p_vout, "unable to configure video channel" );
        return( 1 );
    }

    return ( 0 );
}


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

    /* allocate palette */
    for( i = 0; i < 255; i++ )
    {
        /* kludge: colors are indexed reversely because color 255 seems
         * to be reserved for black even if we try to set it to white */
        p_vout->p_sys->p_colors[ i ] = PgRGB( red[ i ] >> 8, green[ i ] >> 8, blue[ i ] >> 8 );
    }
}
