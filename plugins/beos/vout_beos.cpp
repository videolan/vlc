/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
 * Jean-Marc Dressler
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>
#include <string.h>                                            /* strerror() */
#include <kernel/OS.h>
#include <View.h>
#include <Application.h>
#include <DirectWindow.h>
#include <Locker.h>
#include <malloc.h>
#include <string.h>

extern "C"
{
#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h" /* XXX maybe to remove if beos_window.h is splitted */

#include "main.h"
}

#include "beos_window.h"

#define WIDTH 128
#define HEIGHT 64
#define BITS_PER_PLANE 16
#define BYTES_PER_PIXEL 2

/*****************************************************************************
 * vout_sys_t: dummy video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the dummy specific properties of an output thread.
 *****************************************************************************/
 
typedef struct vout_sys_s
{
    VideoWindow *         p_window;
    
    byte_t *              pp_buffer[2];
    s32                   i_width;
    s32                   i_height;
} vout_sys_t;


/*****************************************************************************
 * beos_GetAppWindow : retrieve a BWindow pointer from the window name
 *****************************************************************************/

BWindow *beos_GetAppWindow(char *name)
{
    int32       index;
    BWindow     *window;
    
    for (index = 0 ; ; index++)
    {
        window = be_app->WindowAt(index);
        if (window == NULL)
            break;
        if (window->LockWithTimeout(200000) == B_OK)
        {
            if (strcmp(window->Name(), name) == 0)
            {
                window->Unlock();
                break;
            }
            window->Unlock();
        }
    }
    return window; 
}

/*****************************************************************************
 * DrawingThread : thread that really does the drawing
 *****************************************************************************/

int32 DrawingThread(void *data)
{
    uint32 i, j, y;
    uint64 *pp, *qq;
    uint8 *p, *q;
    uint32 byte_width;
    uint32 height, bytes_per_line;
    clipping_rect *clip;

    VideoWindow *w;
    w = (VideoWindow*) data;
    
    while(!w->fConnectionDisabled)
    {
        w->locker->Lock();
        if( w->fConnected )
        {
            if( w->fDirty && (!w->fReady || w->i_screen_depth != w->p_vout->i_screen_depth) )
            {
                bytes_per_line = w->fRowBytes;
                for( i=0 ; i < w->fNumClipRects ; i++ )
                {
                    clip = &(w->fClipList[i]);
                    height = clip->bottom - clip->top +1;
                    byte_width = w->i_bytes_per_pixel * ((clip->right - clip->left)+1);
                    p = w->fBits + clip->top*w->fRowBytes + clip->left * w->i_bytes_per_pixel;
                    for( y=0 ; y < height ; )
                    {
                        pp = (uint64*) p;
                        for( j=0 ; j < byte_width/64 ; j++ )
                        {
                            *pp++ = 0;
                            *pp++ = 0; 
                            *pp++ = 0;
                            *pp++ = 0; 
                            *pp++ = 0;
                            *pp++ = 0; 
                            *pp++ = 0;
                            *pp++ = 0; 
                        }
                        memset( pp , 0, byte_width & 63 );
                        y++;
                        p += bytes_per_line;
                    }
                }
            }
            else if( w->fDirty )
            {
                bytes_per_line = w->fRowBytes;
                for( i=0 ; i < w->fNumClipRects ; i++ )
                {
                    clip = &(w->fClipList[i]);
                    height = clip->bottom - clip->top +1;
                    byte_width = w->i_bytes_per_pixel * ((clip->right - clip->left)+1);
                    p = w->fBits + clip->top * bytes_per_line + clip->left * w->i_bytes_per_pixel;
                    q = w->p_vout->p_sys->pp_buffer[ !w->p_vout->i_buffer_index ] +
                        clip->top * w->p_vout->i_bytes_per_line + clip->left *
                        w->p_vout->i_bytes_per_pixel;
                    for( y=0 ; y < height ; )
                    {
                        pp = (uint64*) p;
                        qq = (uint64*) q;
                        for( j=0 ; j < byte_width/64 ; j++ )
                        {
                            *pp++ = *qq++;
                            *pp++ = *qq++; 
                            *pp++ = *qq++;
                            *pp++ = *qq++; 
                            *pp++ = *qq++;
                            *pp++ = *qq++; 
                            *pp++ = *qq++;
                            *pp++ = *qq++; 
                        }
                        memcpy( pp , qq, byte_width & 63 );
                        y++;
                        p += bytes_per_line;
                        q += w->p_vout->p_sys->i_width * w->p_vout->i_bytes_per_pixel;
                    }
                }
            }
            w->fDirty = false;
        }
        w->locker->Unlock();
        snooze( 20000 );
    }
    return B_OK;
}

/*****************************************************************************
 * VideoWindow constructor and destructor
 *****************************************************************************/

VideoWindow::VideoWindow(BRect frame, const char *name, vout_thread_t *p_video_output )
        : BDirectWindow(frame, name, B_TITLED_WINDOW, B_NOT_RESIZABLE|B_NOT_ZOOMABLE)
{
    BView * view;

    fReady = false;
    fConnected = false;
    fConnectionDisabled = false;
    locker = new BLocker();
    fClipList = NULL;
    fNumClipRects = 0;
    p_vout = p_video_output;

    view = new BView(Bounds(), "", B_FOLLOW_ALL, B_WILL_DRAW);
    view->SetViewColor(B_TRANSPARENT_32_BIT);
    AddChild(view);
/*
    if(!SupportsWindowMode())
    {
        SetFullScreen(true);
    }
*/
    fDirty = false;
    fDrawThreadID = spawn_thread(DrawingThread, "drawing_thread",
                    B_DISPLAY_PRIORITY, (void*) this);
    resume_thread(fDrawThreadID);
    Show();
}

VideoWindow::~VideoWindow()
{
    int32 result;

    fConnectionDisabled = true;
    Hide();
    Sync();
    wait_for_thread(fDrawThreadID, &result);
    free(fClipList);
    delete locker;
}

/*****************************************************************************
 * VideoWindow::DirectConnected
 *****************************************************************************/

void VideoWindow::DirectConnected(direct_buffer_info *info)
{
    unsigned int i;

    if(!fConnected && fConnectionDisabled)
    {
        return;
    }
    locker->Lock();

    switch(info->buffer_state & B_DIRECT_MODE_MASK)
    {
    case B_DIRECT_START:
        fConnected = true;
    case B_DIRECT_MODIFY:
        fBits = (uint8*)((char*)info->bits +
        (info->window_bounds.top) * info->bytes_per_row +
        (info->window_bounds.left) * (info->bits_per_pixel>>3));;
        
        i_bytes_per_pixel = info->bits_per_pixel >> 3;
        i_screen_depth = info->bits_per_pixel;
        
        fRowBytes = info->bytes_per_row;
        fFormat = info->pixel_format;
        fBounds = info->window_bounds;
        fDirty = true;

        if(fClipList)
        {
            free(fClipList);
            fClipList = NULL;
        }
        fNumClipRects = info->clip_list_count;
        fClipList = (clipping_rect*) malloc(fNumClipRects*sizeof(clipping_rect));
        for( i=0 ; i<info->clip_list_count ; i++ )
        {
            fClipList[i].top = info->clip_list[i].top - info->window_bounds.top;
            fClipList[i].left = info->clip_list[i].left - info->window_bounds.left;
            fClipList[i].bottom = info->clip_list[i].bottom - info->window_bounds.top;
            fClipList[i].right = info->clip_list[i].right - info->window_bounds.left;
        }
        break;
    case B_DIRECT_STOP:
        fConnected = false;
        break;
    }
    locker->Unlock();
}

/*****************************************************************************
 * VideoWindow::MessageReceived
 *****************************************************************************/

void VideoWindow::MessageReceived( BMessage * p_message )
{
    BWindow * p_win;
    
    switch( p_message->what )
    {
    case B_KEY_DOWN:
        // post the message to the interface window which will handle it
        p_win = beos_GetAppWindow( "interface" );
        if( p_win != NULL )
        {
            p_win->PostMessage( p_message );
        }
        break;
    
    default:
        BWindow::MessageReceived( p_message );
        break;
    }
}

/*****************************************************************************
 * VideoWindow::QuitRequested
 *****************************************************************************/

bool VideoWindow::QuitRequested()
{
    return( true );
}

extern "C"
{

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     BeosOpenDisplay   ( vout_thread_t *p_vout );
static void    BeosCloseDisplay  ( vout_thread_t *p_vout );

/*****************************************************************************
 * vout_BeCreate: allocates dummy video thread output method
 *****************************************************************************
 * This function allocates and initializes a dummy vout method.
 *****************************************************************************/
int vout_BeCreate( vout_thread_t *p_vout, char *psz_display,
                    int i_root_window, void *p_data )
{
    /* Allocate structure */
    p_vout->p_sys = (vout_sys_t*) malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }
    
    /* Set video window's size */
    p_vout->i_width =  main_GetIntVariable( VOUT_WIDTH_VAR, VOUT_WIDTH_DEFAULT );
    p_vout->i_height = main_GetIntVariable( VOUT_HEIGHT_VAR, VOUT_HEIGHT_DEFAULT );

    /* Open and initialize device */
    if( BeosOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display\n");
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_BeInit: initialize dummy video thread output method
 *****************************************************************************/
int vout_BeInit( vout_thread_t *p_vout )
{
    VideoWindow * p_win = p_vout->p_sys->p_window;
    u32 i_page_size;

    p_win->locker->Lock();

    i_page_size =   p_vout->i_width * p_vout->i_height * p_vout->i_bytes_per_pixel;
    
    p_vout->p_sys->i_width =         p_vout->i_width;
    p_vout->p_sys->i_height =        p_vout->i_height;    

    /* Allocate memory for the 2 display buffers */
    p_vout->p_sys->pp_buffer[0] = (byte_t*) malloc( i_page_size );
    p_vout->p_sys->pp_buffer[1] = (byte_t*) malloc( i_page_size );
    if( p_vout->p_sys->pp_buffer[0] == NULL  || p_vout->p_sys->pp_buffer[0] == NULL )
    {
        intf_ErrMsg("vout error: can't allocate video memory (%s)\n", strerror(errno) );
        if( p_vout->p_sys->pp_buffer[0] != NULL ) free( p_vout->p_sys->pp_buffer[0] );
        if( p_vout->p_sys->pp_buffer[1] != NULL ) free( p_vout->p_sys->pp_buffer[1] );
        p_win->locker->Unlock();
        return( 1 );
    }

    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_vout->p_sys->pp_buffer[0],
                     p_vout->p_sys->pp_buffer[1] );

    p_win->locker->Unlock();
    return( 0 );
}

/*****************************************************************************
 * vout_BeEnd: terminate dummy video thread output method
 *****************************************************************************/
void vout_BeEnd( vout_thread_t *p_vout )
{
   VideoWindow * p_win = p_vout->p_sys->p_window;
   
   p_win->Lock();
   
   free( p_vout->p_sys->pp_buffer[0] );
   free( p_vout->p_sys->pp_buffer[1] );

   p_win->fReady = false;
   p_win->Unlock();   
}

/*****************************************************************************
 * vout_BeDestroy: destroy dummy video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
void vout_BeDestroy( vout_thread_t *p_vout )
{
    BeosCloseDisplay( p_vout );
    
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_BeManage: handle dummy events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
int vout_BeManage( vout_thread_t *p_vout )
{
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        intf_DbgMsg("resizing window\n");
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        /* Resize window */
        p_vout->p_sys->p_window->ResizeTo( p_vout->i_width, p_vout->i_height );

        /* Destroy XImages to change their size */
        vout_SysEnd( p_vout );

        /* Recreate XImages. If SysInit failed, the thread can't go on. */
        if( vout_SysInit( p_vout ) )
        {
            intf_ErrMsg("error: can't resize display\n");
            return( 1 );
        }

        /* Tell the video output thread that it will need to rebuild YUV
         * tables. This is needed since convertion buffer size may have changed */
        p_vout->i_changes |= VOUT_YUV_CHANGE;
        intf_Msg("Video display resized (%dx%d)\n", p_vout->i_width, p_vout->i_height);
    }
    return( 0 );
}

/*****************************************************************************
 * vout_BeDisplay: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to dummy image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_BeDisplay( vout_thread_t *p_vout )
{
    VideoWindow * p_win = p_vout->p_sys->p_window;
    
    p_win->locker->Lock();
    p_vout->i_buffer_index = ++p_vout->i_buffer_index & 1;
    p_win->fReady = true;
    p_win->fDirty = true;
    p_win->locker->Unlock();
}

/* following functions are local */

/*****************************************************************************
 * BeosOpenDisplay: open and initialize dummy device
 *****************************************************************************
 * XXX?? The framebuffer mode is only provided as a fast and efficient way to
 * display video, providing the card is configured and the mode ok. It is
 * not portable, and is not supposed to work with many cards. Use at your
 * own risk !
 *****************************************************************************/

static int BeosOpenDisplay( vout_thread_t *p_vout )
{ 
    /* Create the DirectDraw video window */
    p_vout->p_sys->p_window =
        new VideoWindow(  BRect( 100, 100, 100+p_vout->i_width, 100+p_vout->i_height ), "VideoLAN", p_vout );
    if( p_vout->p_sys->p_window == 0 )
    {
        free( p_vout->p_sys );
        intf_ErrMsg( "error: cannot allocate memory for VideoWindow\n" );
        return( 1 );
    }   
    VideoWindow * p_win = p_vout->p_sys->p_window;
    
    /* Wait until DirectConnected has been called */
    while( !p_win->fConnected )
        snooze( 50000 );

    p_vout->i_screen_depth =         p_win->i_screen_depth;
    p_vout->i_bytes_per_pixel =      p_win->i_bytes_per_pixel;
    p_vout->i_bytes_per_line =       p_vout->i_width*p_win->i_bytes_per_pixel;
    
    switch( p_vout->i_screen_depth )
    {
    case 8:
        intf_ErrMsg( "vout error: 8 bit mode not fully supported\n" );
        break;
    case 15:
        p_vout->i_red_mask =        0x7c00;
        p_vout->i_green_mask =      0x03e0;
        p_vout->i_blue_mask =       0x001f;
        break;
    case 16:
        p_vout->i_red_mask =        0xf800;
        p_vout->i_green_mask =      0x07e0;
        p_vout->i_blue_mask =       0x001f;
        break;
    case 24:
    case 32:
    default:
        p_vout->i_red_mask =        0xff0000;
        p_vout->i_green_mask =      0x00ff00;
        p_vout->i_blue_mask =       0x0000ff;
        break;
    }

    return( 0 );
}

/*****************************************************************************
 * BeosDisplay: close and reset dummy device
 *****************************************************************************
 * Returns all resources allocated by BeosOpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void BeosCloseDisplay( vout_thread_t *p_vout )
{    
    /* Destroy the video window */
    p_vout->p_sys->p_window->Lock();
    p_vout->p_sys->p_window->Quit();
}

} /* extern "C" */
