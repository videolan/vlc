/*****************************************************************************
 * hd1000v.cpp: HD1000 video output display method
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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
extern "C" {
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/intf.h>
}

#include <cascade/graphics/CascadeBitmap.h>
#include <cascade/graphics/CascadeScreen.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );

static int NewPicture ( vout_thread_t *, picture_t * );
static void FreePicture( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("HD1000 video output") );
    set_capability( "video output", 100 );
    add_shortcut( "hd1000v" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the aa specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    uint32_t            i_width;                     /* width of main window */
    uint32_t            i_height;                   /* height of main window */
    uint32_t            i_screen_depth;
    vlc_bool_t          b_double_buffered;
    
    uint32_t            u_current; /* Current output resolution. */
    CascadeScreen      *p_screen;
};

struct picture_sys_t
{
    CascadeSharedMemZone *p_image;
};

/*****************************************************************************
 * Create: allocates video thread output method
 *****************************************************************************
 * This function allocates and initializes a aa vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    bool b_double_buffered = false;
    
    p_vout->p_sys = (struct vout_sys_t*) malloc( sizeof(struct vout_sys_t) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Allocate a screen for VLC vout. */
    p_vout->p_sys->p_screen = new CascadeScreen();
    if( p_vout->p_sys->p_screen == NULL )
    {
        msg_Err( p_vout, "unable to allocate screen" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    /* Get current screen resolution */
    msg_Dbg( p_vout, "number of screen resolutions supported %u",
      p_vout->p_sys->p_screen->GetNumScreenResolutionsSupported() );
      
    p_vout->p_sys->p_screen->GetCurrentScreenResolution( (u32) p_vout->p_sys->u_current );
    p_vout->p_sys->p_screen->SetScreenResolution( (u32) p_vout->p_sys->u_current );

#if 1
    msg_Dbg( p_vout, "available screen resolutions:" );
    for (u32 i=0; i<p_vout->p_sys->p_screen->GetNumScreenResolutionsSupported(); i++)
    {
        u32 i_width=0; 
	u32 i_height=0; 
	u8 i_screen_depth=0; 
	bool b_buffered;
	
        p_vout->p_sys->p_screen->GetSupportedScreenResolutionAt( i,
            i_width, i_height, i_screen_depth, b_buffered);
        msg_Dbg( p_vout, "  screen index = %u, width = %u, height = %u, depth = %u, double buffered = %s",
            i, i_width, i_height, i_screen_depth, (b_buffered ? "yes" : "no") );
    }
#endif        
    
    p_vout->p_sys->p_screen->GetSupportedScreenResolutionAt( (u32) p_vout->p_sys->u_current,
            (u32) p_vout->p_sys->i_width,
            (u32) p_vout->p_sys->i_height,
            (u8) p_vout->p_sys->i_screen_depth,
            b_double_buffered );
    p_vout->p_sys->b_double_buffered = (vlc_bool_t) b_double_buffered;
    msg_Dbg( p_vout, "using screen index = %u, width = %u, height = %u, depth = %u, double buffered = %d",
            p_vout->p_sys->u_current, /* Current screen. */
            p_vout->p_sys->i_width,
            p_vout->p_sys->i_height,
            p_vout->p_sys->i_screen_depth,
            p_vout->p_sys->b_double_buffered );
        
    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    delete p_vout->p_sys->p_screen;
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic = NULL;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
    p_vout->output.i_width = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;

    /* Only RGBA 32bpp is supported by output device. */
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
            p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2'); break;
        default:
            msg_Err( p_vout, "unknown screen depth %i",
                     p_vout->p_sys->i_screen_depth );
            return VLC_SUCCESS;
    }

    /* Find an empty picture slot */
    for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
    {
        if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
        {
            p_pic = p_vout->p_picture + i_index;
            break;
        }
    }

    if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
    {
        return -1;
    }

    /* Allocate the picture */
    p_pic->p->i_lines = p_vout->p_sys->i_height;
    p_pic->p->i_visible_lines = p_vout->p_sys->i_height;
    p_pic->p->i_pitch = p_vout->p_sys->i_width;
    p_pic->p->i_pixel_pitch = 1;
    p_pic->p->i_visible_pitch = p_vout->p_sys->i_width;
    p_pic->i_planes = 1;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;
    I_OUTPUTPICTURES++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread output method
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
 * NewPicture: Allocate shared memory zone for video output
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    CascadeDims p_dims = p_vout->p_sys->p_screen->GetDims();

    p_pic->p_sys = (picture_sys_t *) malloc( sizeof( picture_sys_t ) );
    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    /* Fill in picture_t fields */
    vout_InitPicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                      p_vout->output.i_width, p_vout->output.i_height,
                      p_vout->output.i_aspect );

    p_pic->p_sys->p_image = new CascadeSharedMemZone();
    if( p_pic->p_sys->p_image == NULL )
    {
        free( p_pic->p_sys );
        return -1;
    }

    if( p_pic->p_sys->p_image->Open( "vlc_hd1000v", p_vout->output.i_width *
            p_vout->output.i_height * p_vout->p_sys->i_screen_depth,
            true ) )
    {
        msg_Err( p_vout, "failed to allocate shared memory" );
        free( p_pic->p_sys );
        return -1;
    }
    
    p_pic->p->i_lines = p_vout->output.i_height;
    p_pic->p->i_visible_lines = p_vout->output.i_height;
    p_pic->p->p_pixels = (uint8_t*) p_pic->p_sys->p_image->MapLock();
    p_pic->p->i_pitch = p_vout->p_sys->i_screen_depth;
    p_pic->p->i_visible_pitch = p_pic->p->i_pixel_pitch
                                 * p_vout->output.i_width;

    return VLC_SUCCESS;                                 
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************
 * Destroy SharedMemZoned AND associated data. The picture normally will be
 * unlocked in the Display() function except when the video output is closed
 * before the picture is displayed.
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    if( p_pic->p_sys->p_image->Unlock() )
    { /* Just a test to see the effect described above. REMOVE THIS */
        msg_Err( p_vout, "unlocking shared memory failed, already unlocked" );
    }
    
    if( p_pic->p_sys->p_image->Close() )
    {
        msg_Err( p_vout, "closing shared memory failed. Leaking memory of %ul",
                    p_pic->p_sys->p_image->GetSize() );
    }
    
    delete p_pic->p_sys->p_image;
    free( p_pic->p_sys );
}

/*****************************************************************************
 * Display: Map p_image onto the screen
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    uint32_t i_width, i_height, i_x, i_y;
    uint32_t i_offset = 0;
    
    vout_PlacePicture( p_vout, p_vout->p_sys->i_width,
                       p_vout->p_sys->i_height,
                       &i_x, &i_y, &i_width, &i_height );
    msg_Dbg( p_vout, "PlacePicture at x_left = %d, y_left = %d, x_bottom = %d, y_bottom = %d",
                i_x, i_y, i_width, i_height );

    /* Currently the only pixel format supported is 32bpp RGBA.*/
    p_vout->p_sys->p_screen->LockScreen();
    
    /* Unlock the shared memory region first. */
    if( p_pic->p_sys->p_image->Unlock() ) 
    {
        msg_Err( p_vout, "unlocking shared memory failed. Expect threading problems." );
    }
    
    p_vout->p_sys->p_screen->Blit( CascadePoint( (u32) i_x, (u32) i_y ), /* Place bitmap at */
            (*p_pic->p_sys->p_image)   ,                                      /* Image data */
            (u32) i_offset,                                   /* Offset in SharedMemoryZone */
            (u32) i_width,                                           /* Source bitmap width */
            (u32) i_height,                                         /* Source bitmap height */
            (u32) p_vout->p_sys->i_screen_depth,                      /* Source pixel depth */
            CascadeRect( (u32) i_x, (u32) i_y, (u32) i_width, (u32) i_height ) );
            
    p_vout->p_sys->p_screen->UnlockScreen();
}
