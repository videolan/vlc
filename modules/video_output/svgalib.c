/*****************************************************************************
 * svgalib.c : SVGAlib plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include <vga.h>
#include <vgagl.h>
#include <vgakeyboard.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );

static void SetPalette( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "SVGAlib" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    set_description( _("SVGAlib video output") );
    set_capability( "video output", 0 );
    set_callbacks( Create, Destroy );
    linked_with_a_crap_library_which_uses_atexit();
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: video output descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the SVGAlib specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int i_vgamode;
};

/*****************************************************************************
 * Create: allocates video thread
 *****************************************************************************
 * This function allocates and initializes a vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    /* FIXME: find a way to test for SVGAlib availability. We cannot call
     * vga_init from here because it needs to be closed from the same thread
     * and we might give away the video output to another thread. Bah, it
     * doesn't matter anyway ... SVGAlib is being an UGLY DIRTY PIG and calls
     * atexit() and exit() so what can we do? */

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize video thread
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* SVGAlib shut up! */
    vga_disabledriverreport();

    /* We cannot call vga_init() in Create() because it has to be run
     * from the same thread as the other vga calls. */
    vga_init();

    /* Check that we have a 8bpp mode available */
    p_vout->p_sys->i_vgamode = vga_getdefaultmode();
    if( p_vout->p_sys->i_vgamode == -1
         || vga_getmodeinfo(p_vout->p_sys->i_vgamode)->bytesperpixel != 1 )
    {
        p_vout->p_sys->i_vgamode = G320x200x256;
    }

    if( !vga_hasmode( p_vout->p_sys->i_vgamode ) )
    {
        msg_Err( p_vout, "mode %i not available", p_vout->p_sys->i_vgamode );
        return VLC_EGENERIC;
    }

    vga_setmode( p_vout->p_sys->i_vgamode );
    gl_setcontextvga( p_vout->p_sys->i_vgamode );
    gl_enableclipping();

    if( keyboard_init() )
    {
        msg_Err( p_vout, "could not initialize keyboard" );
        vga_setmode( TEXT );
        return VLC_EGENERIC;
    }

    /* Just in case */
    keyboard_translatekeys( TRANSLATE_CURSORKEYS |
                            TRANSLATE_KEYPADENTER |
                            TRANSLATE_DIAGONAL );

    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
    p_vout->output.pf_setpalette = SetPalette;
    p_vout->output.i_width = vga_getxdim();
    p_vout->output.i_height = vga_getydim();
    p_vout->output.i_aspect = p_vout->output.i_width
                               * VOUT_ASPECT_FACTOR / p_vout->output.i_height;

    /* Try to initialize 1 direct buffer */
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
    if( p_pic == NULL )
    {
        return VLC_SUCCESS;
    }

    vout_AllocatePicture( p_vout, p_pic, p_vout->output.i_chroma,
                          p_vout->output.i_width, p_vout->output.i_height,
                          p_vout->output.i_aspect );

    if( p_pic->i_planes == 0 )
    {
        return VLC_SUCCESS;
    }

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    keyboard_close();
    vga_clear();
    vga_setmode( TEXT );
}

/*****************************************************************************
 * Destroy: destroy video thread
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Manage: handle SVGAlib events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    keyboard_update();

    if( keyboard_keypressed(SCANCODE_ESCAPE)
         || keyboard_keypressed(SCANCODE_Q ) )
    {
        p_vout->p_vlc->b_die = VLC_TRUE;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the VGA card.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    gl_putbox( 0, 0, p_vout->output.i_width,
               p_vout->output.i_height, p_pic->p->p_pixels );
}

/*****************************************************************************
 * SetPalette: set a 8bpp palette
 *****************************************************************************
 * TODO: support 8 bits clut (for Mach32 cards and others).
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    int i = 256;

    while( i-- )
    {
        vga_setpalette( i, red[i]>>10, green[i]>>10, blue[i]>>10 );
    }
}

