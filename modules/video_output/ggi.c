/*****************************************************************************
 * ggi.c : GGI plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <errno.h>                                                 /* ENOMEM */

#include <ggi/ggi.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );

static int  OpenDisplay    ( vout_thread_t * );
static void CloseDisplay   ( vout_thread_t * );
static void SetPalette     ( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
            "X11 hardware display to use.\n" \
            "By default, VLC will use the value of the DISPLAY " \
            "environment variable.")

vlc_module_begin();
    add_string( "ggi-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, VLC_TRUE );
    set_description( "General Graphics Interface video output" );
    set_capability( "video output", 30 );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: video output GGI method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the GGI specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    /* GGI system information */
    ggi_visual_t        p_display;                         /* display device */

    ggi_mode            mode;                             /* mode descriptor */
    int                 i_bits_per_pixel;

    /* Buffer information */
    ggi_directbuffer *  pp_buffer[2];                             /* buffers */
    int                 i_index;

    vlc_bool_t          b_must_acquire;   /* must be acquired before writing */
};

/*****************************************************************************
 * Create: allocate GGI video thread output method
 *****************************************************************************
 * This function allocate and initialize a GGI vout method. It uses some of the
 * vout properties to choose the correct mode, and change them according to the
 * mode actually used.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    /* Open and initialize device */
    if( OpenDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot initialize GGI display" );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return( 0 );
}

/*****************************************************************************
 * Init: initialize GGI video thread output method
 *****************************************************************************
 * This function initialize the GGI display device.
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->pp_buffer
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_width  = p_vout->p_sys->mode.visible.x;
    p_vout->output.i_height = p_vout->p_sys->mode.visible.y;
    p_vout->output.i_aspect = p_vout->p_sys->mode.visible.x
                               * VOUT_ASPECT_FACTOR
                               / p_vout->p_sys->mode.visible.y;

    switch( p_vout->p_sys->i_bits_per_pixel )
    {
        case 8:
            p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
            p_vout->output.pf_setpalette = SetPalette;
            break;
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
                     p_vout->p_sys->i_bits_per_pixel );
            return 0;
    }

    /* Only useful for bits_per_pixel != 8 */
    p_vout->output.i_rmask = p_b[ 0 ]->buffer.plb.pixelformat->red_mask;
    p_vout->output.i_gmask = p_b[ 0 ]->buffer.plb.pixelformat->green_mask;
    p_vout->output.i_bmask = p_b[ 0 ]->buffer.plb.pixelformat->blue_mask;

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

    if( p_pic == NULL )
    {
        return 0;
    }

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_vout->p_sys->i_index = 0;
    p_pic->p->p_pixels = p_b[ 0 ]->write;
    p_pic->p->i_pixel_pitch = p_b[ 0 ]->buffer.plb.pixelformat->size / 8;
    p_pic->p->i_lines = p_vout->p_sys->mode.visible.y;
    p_pic->p->i_visible_lines = p_vout->p_sys->mode.visible.y;

    p_pic->p->i_pitch = p_b[ 0 ]->buffer.plb.stride;

    if( p_b[ 0 ]->buffer.plb.pixelformat->size / 8
         * p_vout->p_sys->mode.visible.x
        != p_b[ 0 ]->buffer.plb.stride )
    {
        p_pic->p->i_visible_pitch = p_b[ 0 ]->buffer.plb.pixelformat->size
                                     / 8 * p_vout->p_sys->mode.visible.x;
    }
    else
    {
        p_pic->p->i_visible_pitch = p_b[ 0 ]->buffer.plb.stride;
    }

    p_pic->i_planes = 1;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    /* Acquire first buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceAcquire( p_b[ 0 ]->resource,
                            GGI_ACTYPE_WRITE );
    }

    /* Listen to the keyboard and the mouse buttons */
    ggiSetEventMask( p_vout->p_sys->p_display,
                     emKeyboard | emPtrButtonPress | emPtrButtonRelease );

    /* Set asynchronous display mode -- usually quite faster */
    ggiAddFlags( p_vout->p_sys->p_display, GGIFLAG_ASYNC );

    return( 0 );
#undef p_b
}

/*****************************************************************************
 * End: terminate GGI video thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->pp_buffer
    /* Release buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceRelease( p_b[ p_vout->p_sys->i_index ]->resource );
    }
#undef p_b
}

/*****************************************************************************
 * Destroy: destroy GGI video thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    CloseDisplay( p_vout );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Manage: handle GGI events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occurred.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    struct timeval tv = { 0, 1000 };                        /* 1 millisecond */
    gii_event_mask mask;
    gii_event      event;
    vlc_value_t    val;

    mask = emKeyboard | emPtrButtonPress | emPtrButtonRelease;

    ggiEventPoll( p_vout->p_sys->p_display, mask, &tv );

    while( ggiEventsQueued( p_vout->p_sys->p_display, mask) )
    {
        ggiEventRead( p_vout->p_sys->p_display, &event, mask);

        switch( event.any.type )
        {
            case evKeyRelease:

                switch( event.key.sym )
                {
                    case 'q':
                    case 'Q':
                    case GIIUC_Escape:
                        p_vout->p_vlc->b_die = 1;
                        break;

                    default:
                        break;
                }
                break;

            case evPtrButtonRelease:

                switch( event.pbutton.button )
                {
                    case GII_PBUTTON_LEFT:
                        val.b_bool = VLC_TRUE;
                        var_Set( p_vout, "mouse-clicked", val );
                        break;

                    case GII_PBUTTON_RIGHT:
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
                break;

            default:
                break;
        }
    }

    return( 0 );
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
#define p_b p_vout->p_sys->pp_buffer
    p_pic->p->p_pixels = p_b[ p_vout->p_sys->i_index ]->write;

    /* Change display frame */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceRelease( p_b[ p_vout->p_sys->i_index ]->resource );
    }
    ggiSetDisplayFrame( p_vout->p_sys->p_display,
                        p_b[ p_vout->p_sys->i_index ]->frame );

    /* Swap buffers and change write frame */
    p_vout->p_sys->i_index ^= 1;
    p_pic->p->p_pixels = p_b[ p_vout->p_sys->i_index ]->write;

    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceAcquire( p_b[ p_vout->p_sys->i_index ]->resource,
                            GGI_ACTYPE_WRITE );
    }
    ggiSetWriteFrame( p_vout->p_sys->p_display,
                      p_b[ p_vout->p_sys->i_index ]->frame );

    /* Flush the output so that it actually displays */
    ggiFlush( p_vout->p_sys->p_display );
#undef p_b
}

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: open and initialize GGI device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int OpenDisplay( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->pp_buffer
    ggi_color   col_fg;                                  /* foreground color */
    ggi_color   col_bg;                                  /* background color */
    int         i_index;                               /* all purposes index */
    char        *psz_display;

    /* Initialize library */
    if( ggiInit() )
    {
        msg_Err( p_vout, "cannot initialize GGI library" );
        return( 1 );
    }

    /* Open display */
    psz_display = config_GetPsz( p_vout, "ggi_display" );

    p_vout->p_sys->p_display = ggiOpen( psz_display, NULL );
    if( psz_display ) free( psz_display );

    if( p_vout->p_sys->p_display == NULL )
    {
        msg_Err( p_vout, "cannot open GGI default display" );
        ggiExit();
        return( 1 );
    }

    /* Find most appropriate mode */
    p_vout->p_sys->mode.frames =    2;                          /* 2 buffers */
    p_vout->p_sys->mode.visible.x = config_GetInt( p_vout, "width" );
    p_vout->p_sys->mode.visible.y = config_GetInt( p_vout, "height" );
    p_vout->p_sys->mode.virt.x =    GGI_AUTO;
    p_vout->p_sys->mode.virt.y =    GGI_AUTO;
    p_vout->p_sys->mode.size.x =    GGI_AUTO;
    p_vout->p_sys->mode.size.y =    GGI_AUTO;
    p_vout->p_sys->mode.graphtype = GT_15BIT;        /* minimum usable depth */
    p_vout->p_sys->mode.dpp.x =     GGI_AUTO;
    p_vout->p_sys->mode.dpp.y =     GGI_AUTO;
    ggiCheckMode( p_vout->p_sys->p_display, &p_vout->p_sys->mode );

    /* FIXME: Check that returned mode has some minimum properties */

    /* Set mode */
    if( ggiSetMode( p_vout->p_sys->p_display, &p_vout->p_sys->mode ) )
    {
        msg_Err( p_vout, "cannot set GGI mode" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* Check buffers properties */
    p_vout->p_sys->b_must_acquire = 0;
    for( i_index = 0; i_index < 2; i_index++ )
    {
        /* Get buffer address */
        p_vout->p_sys->pp_buffer[ i_index ] =
            (ggi_directbuffer *)ggiDBGetBuffer( p_vout->p_sys->p_display,
                                                i_index );
        if( p_b[ i_index ] == NULL )
        {
            msg_Err( p_vout, "double buffering is not possible" );
            ggiClose( p_vout->p_sys->p_display );
            ggiExit();
            return( 1 );
        }

        /* Check buffer properties */
        if( ! ( p_b[ i_index ]->type & GGI_DB_SIMPLE_PLB )
           || ( p_b[ i_index ]->page_size != 0 )
           || ( p_b[ i_index ]->write == NULL )
           || ( p_b[ i_index ]->noaccess != 0 )
           || ( p_b[ i_index ]->align != 0 ) )
        {
            msg_Err( p_vout, "incorrect video memory type" );
            ggiClose( p_vout->p_sys->p_display );
            ggiExit();
            return( 1 );
        }

        /* Check if buffer needs to be acquired before write */
        if( ggiResourceMustAcquire( p_b[ i_index ]->resource ) )
        {
            p_vout->p_sys->b_must_acquire = 1;
        }
    }

    /* Set graphic context colors */
    col_fg.r = col_fg.g = col_fg.b = -1;
    col_bg.r = col_bg.g = col_bg.b = 0;
    if( ggiSetGCForeground(p_vout->p_sys->p_display,
                           ggiMapColor(p_vout->p_sys->p_display,&col_fg)) ||
        ggiSetGCBackground(p_vout->p_sys->p_display,
                           ggiMapColor(p_vout->p_sys->p_display,&col_bg)) )
    {
        msg_Err( p_vout, "cannot set colors" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* Set clipping for text */
    if( ggiSetGCClipping( p_vout->p_sys->p_display, 0, 0,
                          p_vout->p_sys->mode.visible.x,
                          p_vout->p_sys->mode.visible.y ) )
    {
        msg_Err( p_vout, "cannot set clipping" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* FIXME: set palette in 8bpp */
    p_vout->p_sys->i_bits_per_pixel = p_b[ 0 ]->buffer.plb.pixelformat->depth;

    return( 0 );
#undef p_b
}

/*****************************************************************************
 * CloseDisplay: close and reset GGI device
 *****************************************************************************
 * This function returns all resources allocated by OpenDisplay and restore
 * the original state of the device.
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    /* Restore original mode and close display */
    ggiClose( p_vout->p_sys->p_display );

    /* Exit library */
    ggiExit();
}

/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    ggi_color colors[256];
    int i;

    /* Fill colors with color information */
    for( i = 0; i < 256; i++ )
    {
        colors[ i ].r = red[ i ];
        colors[ i ].g = green[ i ];
        colors[ i ].b = blue[ i ];
        colors[ i ].a = 0;
    }

    /* Set palette */
    if( ggiSetPalette( p_vout->p_sys->p_display, 0, 256, colors ) < 0 )
    {
        msg_Err( p_vout, "failed setting palette" );
    }
}

