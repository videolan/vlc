/*****************************************************************************
 * vout_qnx.c: QNX RTOS video output display method
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <photon/PtWidget.h>
#include <photon/PtWindow.h>
#include <photon/PtLabel.h>
#include <photon/PdDirect.h>

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "interface.h"

/*****************************************************************************
 * vout_sys_t: video output QNX method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the QNX specific properties of an output thread. QNX video
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *****************************************************************************/

#define MODE_NORMAL_MEM     0
#define MODE_SHARED_MEM     1
#define MODE_VIDEO_MEM      2
#define MODE_VIDEO_OVERLAY  3

typedef struct vout_sys_s
{
    /* video mode */
    int                     i_mode;

    /* internal stuff */
    PtWidget_t *            p_window;

    /* [shared] memory blit */
    PhImage_t *             p_image[2];
    int                     i_img_type;

    /* video memory blit */
    PdOffscreenContext_t *  p_ctx[2];
    char *                  p_buf[2];

    /* video overlay */
    PgVideoChannel_t *      p_channel;
    void *                  p_vc_y[2];
    void *                  p_vc_u[2];
    void *                  p_vc_v[2];
    int                     i_vc_flags;
    int                     i_vc_format;

    /* position & dimensions */
    PhPoint_t               pos;
    PhDim_t                 dim;
    PhPoint_t               old_pos;
    PhDim_t                 old_dim;
    PhDim_t                 screen_dim;
    PhRect_t                frame;
} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );

static int  QNXInitDisplay ( struct vout_thread_s * );
static int  QNXCreateWnd   ( struct vout_thread_s * );
static int  QNXDestroyWnd  ( struct vout_thread_s * );

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
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to initialize SDL and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "qnx" ) )
    {
        return( 999 );
    }

    return( 100 );
}

/*****************************************************************************
 * vout_Create: allocate QNX video thread output method
 *****************************************************************************
 * This function allocate and initialize a QNX vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    /* init connection to photon */
    if( PtInit( "/dev/photon" ) != 0 )
    {
        intf_ErrMsg( "vout error: unable to connect to photon" );
        return( 1 );
    }

    /* allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror( ENOMEM ) );
        return( 1 );
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    p_vout->b_fullscreen = 
        main_GetIntVariable( VOUT_FULLSCREEN_VAR, VOUT_FULLSCREEN_DEFAULT );
    p_vout->p_sys->i_mode = 
        main_GetIntVariable( VOUT_NOOVERLAY_VAR, VOUT_NOOVERLAY_DEFAULT ) ?
        MODE_NORMAL_MEM : MODE_VIDEO_OVERLAY;
    p_vout->p_sys->dim.w =
        main_GetIntVariable( VOUT_WIDTH_VAR, VOUT_WIDTH_DEFAULT );
    p_vout->p_sys->dim.h =
        main_GetIntVariable( VOUT_HEIGHT_VAR, VOUT_HEIGHT_DEFAULT );

    /* init display and create window */
    if( QNXInitDisplay( p_vout ) || QNXCreateWnd( p_vout ) )
    {
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize QNX video thread output method
 *****************************************************************************
 * This function create the buffers needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->i_mode == MODE_NORMAL_MEM ||
        p_vout->p_sys->i_mode == MODE_SHARED_MEM )
    {
        /* create images for [shared] memory blit */

        if( !( p_vout->p_sys->p_image[0] = PhCreateImage( NULL,
                    p_vout->p_sys->dim.w, p_vout->p_sys->dim.h,
                    p_vout->p_sys->i_img_type, NULL, 0,
                    p_vout->p_sys->i_mode == MODE_SHARED_MEM ) ) ) {
            intf_ErrMsg( "vout error: cannot create image" );
            return( 1 );
        }

        if( !( p_vout->p_sys->p_image[1] = PhCreateImage( NULL,
                    p_vout->p_sys->dim.w, p_vout->p_sys->dim.h,
                    p_vout->p_sys->i_img_type, NULL, 0,
                    p_vout->p_sys->i_mode == MODE_SHARED_MEM ) ) ) {
            intf_ErrMsg( "vout error: cannot create image" );
            PhReleaseImage( p_vout->p_sys->p_image[0] );
            free( p_vout->p_sys->p_image[0] );
            p_vout->p_sys->p_image[0] = NULL;
            return( 1 );
        }
        
        /* set bytes per line, set buffers */
        p_vout->i_bytes_per_line = p_vout->p_sys->p_image[0]->bpl;
        p_vout->pf_setbuffers( p_vout, p_vout->p_sys->p_image[0]->image,
                               p_vout->p_sys->p_image[1]->image );
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_MEM )
    {
        /* create offscreen contexts for video memory blit */

        if( ( p_vout->p_sys->p_ctx[0] = PdCreateOffscreenContext( 0,
                        p_vout->p_sys->dim.w, p_vout->p_sys->dim.h,
                        Pg_OSC_MEM_PAGE_ALIGN ) ) == NULL )
        {
            intf_ErrMsg( "vout error: unable to create offscreen context" );
            return( 1 );
        }

        if( ( p_vout->p_sys->p_ctx[1] = PdCreateOffscreenContext( 0,
                        p_vout->p_sys->dim.w, p_vout->p_sys->dim.h,
                        Pg_OSC_MEM_PAGE_ALIGN ) ) == NULL )
        {
            intf_ErrMsg( "vout error: unable to create offscreen context" );
            PhDCRelease ( p_vout->p_sys->p_ctx[0] );
            p_vout->p_sys->p_ctx[0] = NULL;
            return( 1 );
        }

        /* get context pointers */
        if( ( ( p_vout->p_sys->p_buf[0] =
            PdGetOffscreenContextPtr ( p_vout->p_sys->p_ctx[0] ) ) == NULL ) ||
            ( p_vout->p_sys->p_buf[1] =
            PdGetOffscreenContextPtr ( p_vout->p_sys->p_ctx[1] ) ) == NULL )
        {
            intf_ErrMsg( "vout error: unable to get offscreen context ptr" );
            PhDCRelease ( p_vout->p_sys->p_ctx[0] );
            PhDCRelease ( p_vout->p_sys->p_ctx[1] );
            p_vout->p_sys->p_ctx[0] = NULL;
            p_vout->p_sys->p_ctx[1] = NULL;
            return( 1 );
        }

        /* set bytes per line, clear buffers, set buffers */
        p_vout->i_bytes_per_line = p_vout->p_sys->p_ctx[0]->pitch; 
        memset( p_vout->p_sys->p_buf[0], 0,
            p_vout->i_bytes_per_line * p_vout->p_sys->dim.h );
        memset( p_vout->p_sys->p_buf[1], 0,
            p_vout->i_bytes_per_line * p_vout->p_sys->dim.h );
        p_vout->pf_setbuffers( p_vout, p_vout->p_sys->p_buf[0],
                               p_vout->p_sys->p_buf[1] );
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
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
        props.viewport.lr.x = p_vout->p_sys->dim.w + props.viewport.ul.x;
        props.viewport.lr.y = p_vout->p_sys->dim.h + props.viewport.ul.y;

        /* set source dimension */
        props.src_dim.w = p_vout->i_width;
        props.src_dim.h = p_vout->i_height;

        /* configure scaler channel */
        i_ret = PgConfigScalerChannel( p_vout->p_sys->p_channel, &props );

        if( i_ret == -1 )
        {
            intf_ErrMsg( "vout error: unable to configure video channel" );
            return( 1 );
        }
        else if( i_ret == 1 )
        {
            p_vout->p_sys->p_vc_y[0] =
                PdGetOffscreenContextPtr( p_vout->p_sys->p_channel->yplane1 );
            p_vout->p_sys->p_vc_y[1] =
                PdGetOffscreenContextPtr( p_vout->p_sys->p_channel->yplane2 );

            if( p_vout->p_sys->p_vc_y[0] == NULL ||
                p_vout->p_sys->p_vc_y[1] == NULL )
            {
                intf_ErrMsg( "vout error: unable to get video channel ctx ptr" );
                return( 1 );
            }
        }

        if( p_vout->p_sys->i_vc_format == Pg_VIDEO_FORMAT_YV12 && i_ret == 1 )
        {
            p_vout->b_need_render = 0;

            p_vout->p_sys->p_vc_u[0] =
                PdGetOffscreenContextPtr( p_vout->p_sys->p_channel->uplane1 );
            p_vout->p_sys->p_vc_u[1] =
                PdGetOffscreenContextPtr( p_vout->p_sys->p_channel->uplane2 );
            p_vout->p_sys->p_vc_v[0] =
                PdGetOffscreenContextPtr( p_vout->p_sys->p_channel->vplane1 );
            p_vout->p_sys->p_vc_v[1] =
                PdGetOffscreenContextPtr( p_vout->p_sys->p_channel->vplane2 );

            if( p_vout->p_sys->p_vc_u[0] == NULL ||
                p_vout->p_sys->p_vc_u[1] == NULL ||
                p_vout->p_sys->p_vc_v[0] == NULL ||
                p_vout->p_sys->p_vc_v[1] == NULL )
            {
                intf_ErrMsg( "vout error: unable to get video channel ctx ptr" );
                return( 1 );
            }
        }
        else if( p_vout->p_sys->i_vc_format == Pg_VIDEO_FORMAT_RGB8888 )
        {
            /* set bytes per line, clear buffers, set buffers */
            p_vout->i_bytes_per_line =
                p_vout->p_sys->p_channel->yplane1->pitch;
            memset( p_vout->p_sys->p_vc_y[0], 0,
                p_vout->i_bytes_per_line * p_vout->i_height );
            memset( p_vout->p_sys->p_vc_y[1], 0,
                p_vout->i_bytes_per_line * p_vout->i_height );
            p_vout->pf_setbuffers( p_vout,
                p_vout->p_sys->p_vc_y[0], p_vout->p_sys->p_vc_y[1] );
        }
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate QNX video thread output method
 *****************************************************************************
 * Destroy the buffers created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    if( ( p_vout->p_sys->i_mode == MODE_NORMAL_MEM ||
        p_vout->p_sys->i_mode == MODE_SHARED_MEM ) && 
        p_vout->p_sys->p_image[0] )
    {
        PhReleaseImage( p_vout->p_sys->p_image[0] );
        PhReleaseImage( p_vout->p_sys->p_image[1] );
        free( p_vout->p_sys->p_image[0] );
        free( p_vout->p_sys->p_image[1] );
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_MEM &&
             p_vout->p_sys->p_ctx[0] )
    {
        PhDCRelease( p_vout->p_sys->p_ctx[0] );
        PhDCRelease( p_vout->p_sys->p_ctx[1] );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy QNX video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* destroy the window */
    QNXDestroyWnd( p_vout );

    /* destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle QNX events
 *****************************************************************************
 * This function should be called regularly by video output thread. It allows 
 * window resizing. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    int i_ev, i_buflen;
    PhEvent_t *p_event;
    boolean_t b_repos = 0;

    /* allocate buffer for event */
    i_buflen = sizeof( PhEvent_t ) * 4;
    if( ( p_event = malloc( i_buflen ) ) == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror( ENOMEM ) );
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
                intf_ErrMsg( "vout error: %s", strerror( ENOMEM ) );
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
                    p_main->p_intf->b_die = 1;
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
                        p_main->p_intf->b_die = 1;
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
                        if( i_key >= Pk_0 && i_key <= Pk_9 )
                        {
                            network_ChannelJoin( i_key );
                        }
                        else if( intf_ProcessKey( p_main->p_intf,
                                                    (char) i_key ) )
                        {
                            intf_DbgMsg( "vout: unhandled key '%c' (%i)",
                                         (char) i_key, i_key );
                        }
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

        intf_DbgMsg( "vout: changing full-screen status" );

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
        intf_DbgMsg( "vout: resizing window" );
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        if( p_vout->p_sys->i_mode != MODE_VIDEO_OVERLAY )
        {
            p_vout->i_width = p_vout->p_sys->dim.w;
            p_vout->i_height = p_vout->p_sys->dim.h;
            p_vout->i_changes |= VOUT_YUV_CHANGE;
        }

        vout_End( p_vout );
        if( vout_Init( p_vout ) )
        {
            intf_ErrMsg( "vout error: cannot resize display" );
            return( 1 );
        }

        intf_Msg( "vout: video display resized (%dx%d)",
                  p_vout->p_sys->dim.w, p_vout->p_sys->dim.h );
    }

    /*
     * position change, move video channel
     */
    if( b_repos && p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        intf_DbgMsg( "vout: moving video channel" );

        vout_End( p_vout );
        if( vout_Init( p_vout ) )
        {
            intf_ErrMsg( "vout error: unable to move video channel" );
            return( 1 );
        }
    }

    return( i_ev == -1 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to QNX server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->i_mode == MODE_NORMAL_MEM ||
        p_vout->p_sys->i_mode == MODE_SHARED_MEM )
    {
        PhPoint_t pos = { 0, 0 };

        PgSetRegion( PtWidgetRid( p_vout->p_sys->p_window ) );
        PgDrawPhImagemx( &pos, p_vout->p_sys->p_image[p_vout->i_buffer_index], 0 );
        PgFlush();
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_MEM )
    {
        PhRect_t rc = { { 0, 0 }, { 
                p_vout->p_sys->dim.w,
                p_vout->p_sys->dim.h
        } };

        PgSetRegion( PtWidgetRid ( p_vout->p_sys->p_window ) );
        PgContextBlit( p_vout->p_sys->p_ctx[p_vout->i_buffer_index], &rc, NULL, &rc );
        PgFlush();
    }
    else if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY &&
             p_vout->p_sys->i_vc_format == Pg_VIDEO_FORMAT_YV12 )
    {
        int i_size, i_index;

        /* this code has NOT been tested */

        i_size = p_vout->p_rendered_pic->i_width *
                p_vout->p_rendered_pic->i_height;
        i_index = PgNextVideoFrame( p_vout->p_sys->p_channel );
    
        memcpy( p_vout->p_sys->p_vc_y[i_index],
            p_vout->p_rendered_pic->p_y, i_size );
        memcpy( p_vout->p_sys->p_vc_v[i_index],
            p_vout->p_rendered_pic->p_v, i_size / 4 );
        memcpy( p_vout->p_sys->p_vc_u[i_index],
            p_vout->p_rendered_pic->p_u, i_size / 4 );
    }
}

/*****************************************************************************
 * QNXInitDisplay: check screen resolution, depth, amount of video ram, etc
 *****************************************************************************/
static int QNXInitDisplay( p_vout_thread_t p_vout )
{
    PgHWCaps_t hwcaps;
    PgDisplaySettings_t cfg;
    PgVideoModeInfo_t minfo;

    /* get graphics card hw capabilities */
    if( PgGetGraphicsHWCaps( &hwcaps ) != 0 )
    {
        intf_ErrMsg( "vout error: unable to get gfx card capabilities" );
        return( 1 );
    }

    /* get current video mode */
    if( PgGetVideoMode( &cfg ) != 0 )
    {
        intf_ErrMsg( "vout error: unable to get current video mode" );
        return( 1 );
    }

    /* get video mode info */
    if( PgGetVideoModeInfo( cfg.mode, &minfo ) != 0 )
    {
        intf_ErrMsg( "vout error: unable to get info for video mode" );
        return( 1 );
    }

    /* switch to normal mode if no overlay support */
    if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY &&
        !( minfo.mode_capabilities1 & PgVM_MODE_CAP1_VIDEO_OVERLAY ) )
    {
        intf_ErrMsg( "vout error: no overlay support detected" );
        p_vout->p_sys->i_mode = MODE_NORMAL_MEM;
    }

    /* use video ram if we have enough available */
    if( p_vout->p_sys->i_mode == MODE_NORMAL_MEM &&
        hwcaps.currently_available_video_ram >= 
        ( ( minfo.width * minfo.height * minfo.bits_per_pixel ) / 8 ) )
    {
        intf_DbgMsg( "vout: using video ram" );
        p_vout->p_sys->i_mode = MODE_VIDEO_MEM;
    }

    p_vout->p_sys->i_img_type = minfo.type;
    p_vout->p_sys->screen_dim.w = minfo.width;
    p_vout->p_sys->screen_dim.h = minfo.height;
    p_vout->i_screen_depth = minfo.bits_per_pixel;

    switch( minfo.type )
    {
        case Pg_IMAGE_PALETTE_BYTE:
            p_vout->i_bytes_per_pixel = 1;
            break;

        case Pg_IMAGE_DIRECT_555:
        case Pg_IMAGE_DIRECT_565:
            p_vout->i_bytes_per_pixel = 2;
            break;
    
        case Pg_IMAGE_DIRECT_8888:
            p_vout->i_bytes_per_pixel = 4;
            break;
    }

    switch( p_vout->i_screen_depth )
    {
        case 15:
            p_vout->i_red_mask   = 0x7c00;
            p_vout->i_green_mask = 0x03e0;
            p_vout->i_blue_mask  = 0x001f;
            break;

        case 16:
            p_vout->i_red_mask   = 0xf800;
            p_vout->i_green_mask = 0x07e0;
            p_vout->i_blue_mask  = 0x001f;
            break;

        case 24:
        case 32:
        default:
            p_vout->i_red_mask   = 0xff0000;
            p_vout->i_green_mask = 0x00ff00;
            p_vout->i_blue_mask  = 0x0000ff;
            break;
    }

    return( 0 );
}

/*****************************************************************************
 * QNXCreateWnd: create and realize the main window
 *****************************************************************************/
static int QNXCreateWnd( p_vout_thread_t p_vout )
{
    PtArg_t args[8];
    PhPoint_t pos = { 0, 0 };
    PgColor_t color = Pg_BLACK;

    if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        int i = 0;
        PgScalerCaps_t vcaps;

        if( ( p_vout->p_sys->p_channel = 
            PgCreateVideoChannel( Pg_VIDEO_CHANNEL_SCALER, 0 ) ) == NULL )
        {
            intf_ErrMsg( "vout error: unable to create video channel" );
            return( 1 );
        }

        vcaps.size = sizeof( vcaps );
        while( PgGetScalerCapabilities( p_vout->p_sys->p_channel, 
                                        i++, &vcaps ) == 0 )
        {    
            if( vcaps.format == Pg_VIDEO_FORMAT_YV12 ||
                vcaps.format == Pg_VIDEO_FORMAT_RGB8888 )
            {
                p_vout->p_sys->i_vc_flags  = vcaps.flags;
                p_vout->p_sys->i_vc_format = vcaps.format;
            }
                
            vcaps.size = sizeof( vcaps );
        }

        if( p_vout->p_sys->i_vc_format == 0 )
        {
            intf_ErrMsg( "vout error: need YV12 or RGB8888 overlay" );
            
            return( 1 );
        }
        
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
        p_vout->i_width = p_vout->p_sys->dim.w = p_vout->p_sys->screen_dim.w;
        p_vout->i_height = p_vout->p_sys->dim.h = p_vout->p_sys->screen_dim.h;
    }

    /* set window parameters */
    PtSetArg( &args[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &args[1], Pt_ARG_DIM, &p_vout->p_sys->dim, 0 );
    PtSetArg( &args[2], Pt_ARG_FILL_COLOR, color, 0 );
    PtSetArg( &args[3], Pt_ARG_WINDOW_TITLE, "VideoLan Client", 0 );
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
        intf_ErrMsg( "vout error: unable to create window" );
        return( 1 );
    }

    /* realize the window widget */
    if( PtRealizeWidget( p_vout->p_sys->p_window ) != 0 )
    {
        intf_ErrMsg( "vout error: unable to realize window widget" );
        PtDestroyWidget( p_vout->p_sys->p_window );
        return( 1 );
    }

    /* get window frame size */
    if( PtWindowFrameSize( NULL, p_vout->p_sys->p_window, 
                           &p_vout->p_sys->frame ) != 0 )
    {
        intf_ErrMsg( "vout error: unable to get window frame size" );
        PtDestroyWidget( p_vout->p_sys->p_window );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * QNXDestroyWnd: unrealize and destroy the main window
 *****************************************************************************/
static int QNXDestroyWnd( p_vout_thread_t p_vout )
{
    /* destroy the window widget */
    PtUnrealizeWidget( p_vout->p_sys->p_window );
    PtDestroyWidget( p_vout->p_sys->p_window );

    /* destroy video channel */
    if( p_vout->p_sys->i_mode == MODE_VIDEO_OVERLAY )
    {
        PgDestroyVideoChannel( p_vout->p_sys->p_channel );
    }

    return( 0 );
}
