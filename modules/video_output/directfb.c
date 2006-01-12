/*****************************************************************************
 * directfb.c: DirectFB video output display method
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Iuri Diniz <iuri@digizap.com.br>
 *
 * This code is based in sdl.c and fb.c, thanks for VideoLAN team.
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <directfb.h>

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

struct vout_sys_t
{
    IDirectFB *p_directfb;
    IDirectFBSurface *p_primary;
    DFBSurfacePixelFormat p_pixel_format;

    int i_width;
    int i_height;

    byte_t* p_pixels;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "DirectFB" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    set_description( _("DirectFB video output http://www.directfb.org/") );
    set_capability( "video output", 60 );
    add_shortcut( "directfb" );
    set_callbacks( Create, Destroy );
vlc_module_end();


static int Create( vlc_object_t *p_this ) 
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = NULL;
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    /* Allocate structure */
    p_vout->p_sys = p_sys = malloc( sizeof( vout_sys_t ) );
    if( !p_sys )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_sys->p_directfb = NULL;
    p_sys->p_primary = NULL;
    p_sys->p_pixels = NULL;
    p_sys->i_width = 0;
    p_sys->i_height = 0;

    /* Init DirectFB */
    if( DirectFBInit(NULL,NULL) != DFB_OK )
    {
        msg_Err(p_vout, "Cannot init DirectFB");
        return VLC_EGENERIC;
    }

    if( OpenDisplay( p_vout ) )
    {
        msg_Err(p_vout, "Cannot create primary surface");
        free( p_sys );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}


static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    IDirectFBSurface *p_primary = (IDirectFBSurface *) p_vout->p_sys->p_primary;
    byte_t* p_pixels = NULL;
    picture_t *p_pic = NULL;
    int i_rlength, i_glength, i_blength;
    int i_roffset, i_goffset, i_boffset;
    int i_line_pitch;
    int i_size;
    int i_index;

    I_OUTPUTPICTURES = 0;

    switch( p_sys->p_pixel_format )
    {
        case DSPF_RGB332: 
            /* 8 bit RGB (1 byte, red 3@5, green 3@2, blue 2@0) */
            /* i_pixel_pitch = 1; */
            i_rlength = 3;
            i_roffset = 5;
            i_glength = 3;
            i_goffset = 2;
            i_blength = 2;
            i_boffset = 0;
            p_vout->output.i_chroma = VLC_FOURCC('R','G','B','2');
            break;

        case DSPF_RGB16:
            /* 16 bit RGB (2 byte, red 5@11, green 6@5, blue 5@0) */
            /* i_pixel_pitch = 2; */
            i_rlength = 5;
            i_roffset = 11;
            i_glength = 6;
            i_goffset = 5;
            i_blength = 5;
            i_boffset = 0;
            p_vout->output.i_chroma = VLC_FOURCC('R','V','1','6');
            break;

        case DSPF_RGB24:
            /* 24 bit RGB (3 byte, red 8@16, green 8@8, blue 8@0) */
            /* i_pixel_pitch = 3; */
            i_rlength = 8;
            i_roffset = 16;
            i_glength = 8;
            i_goffset = 8;
            i_blength = 8;
            i_boffset = 0;
            p_vout->output.i_chroma = VLC_FOURCC('R','V','2','4');
            break;

        case DSPF_RGB32:
            /* 24 bit RGB (4 byte, nothing@24, red 8@16, green 8@8, blue 8@0) */
            /* i_pixel_pitch = 4; */
            i_rlength = 8;
            i_roffset = 16;
            i_glength = 8;
            i_goffset = 8;
            i_blength = 8;
            i_boffset = 0;
            p_vout->output.i_chroma = VLC_FOURCC('R','V','3','2');
            break;

        default:
            msg_Err( p_vout, "unknown screen depth %i",
                     p_sys->p_pixel_format );
            return VLC_EGENERIC;
    }
    /* Set the RGB masks */
    p_vout->output.i_rmask = ( (1 << i_rlength) - 1 ) << i_roffset;
    p_vout->output.i_gmask = ( (1 << i_glength) - 1 ) << i_goffset;
    p_vout->output.i_bmask = ( (1 << i_blength) - 1 ) << i_boffset;

    /* Width and height */
    p_vout->output.i_width  = p_sys->i_width;
    p_vout->output.i_height = p_sys->i_height;

    /* The aspect */
    p_vout->output.i_aspect = (p_sys->i_width * VOUT_ASPECT_FACTOR) /
                               p_sys->i_height;

    /* Try to initialize 1 buffer */
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
    if( !p_pic )
        return VLC_EGENERIC;

    /* get the pixels */
    if( p_primary->Lock( p_primary, DSLF_READ, (void **) &p_pixels,
                         &i_line_pitch) != DFB_OK )
        return VLC_EGENERIC;

    /* allocate p_pixels */
    i_size = i_line_pitch * p_sys->i_height;
    p_sys->p_pixels = malloc( sizeof(byte_t) * i_size );
    if( p_sys->p_pixels == NULL )
    {
        p_primary->Unlock(p_primary);
        return VLC_ENOMEM;
    }

    /* copy pixels */
    memcpy( p_sys->p_pixels,  p_pixels, i_size );
    if( p_primary->Unlock(p_primary) != DFB_OK )
    {
        return VLC_EGENERIC;
    }

    p_pic->p->p_pixels = p_sys->p_pixels;
    p_pic->p->i_pixel_pitch = i_line_pitch / p_sys->i_width;
    p_pic->p->i_lines = p_sys->i_height;
    p_pic->p->i_visible_lines = p_sys->i_height;
    p_pic->p->i_pitch = i_line_pitch;
    p_pic->p->i_visible_pitch = i_line_pitch;
    p_pic->i_planes = 1;
    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    return VLC_SUCCESS;
}

static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    if( p_sys->p_pixels )
        free( p_sys->p_pixels );
}

static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    CloseDisplay( p_vout );
    if( p_sys ) free( p_sys );
    p_sys = NULL;
}

static int Manage( vout_thread_t *p_vout )
{
    return VLC_SUCCESS;
}

static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    IDirectFBSurface *p_primary = (IDirectFBSurface *) p_sys->p_primary;
    byte_t* p_pixels = NULL;
    int i_size;
    int i_line_pitch;

    /* get the pixels */
    if( p_primary->Lock( p_primary, DSLF_WRITE,
                         (void **) &p_pixels,
                         &i_line_pitch) == DFB_OK )
    {
        i_size = i_line_pitch * p_vout->p_sys->i_height;

        /* copy pixels */
        memcpy( p_pixels, p_pic->p->p_pixels, i_size);
        if( p_primary->Unlock(p_primary) == DFB_OK )
        {
            p_primary->Flip(p_primary, NULL, 0);
        }
    }
}

static int OpenDisplay( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    IDirectFB *p_directfb = NULL;
    IDirectFBSurface *p_primary = NULL;
    DFBSurfaceDescription dsc;

    /*dsc.flags = DSDESC_CAPS | DSDESC_HEIGHT | DSDESC_WIDTH;*/
    dsc.flags = DSDESC_CAPS;
    dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

    /*dsc.width = 352;*/
    /*dsc.height = 240;*/
    if( DirectFBCreate( &p_directfb ) != DFB_OK )
        return VLC_EGENERIC;

    p_sys->p_directfb = p_directfb;
    if( !p_directfb )
        return VLC_EGENERIC;

    if( p_directfb->CreateSurface( p_directfb, &dsc, &p_primary ) )
        return VLC_EGENERIC;

    p_sys->p_primary = p_primary;
    if( !p_primary )
        return VLC_EGENERIC;

    p_primary->GetSize( p_primary, &p_sys->i_width,
                        &p_sys->i_height );
    p_primary->GetPixelFormat( p_primary, &p_sys->p_pixel_format );
    p_primary->FillRectangle( p_primary, 0, 0, p_sys->i_width,
                              p_sys->i_height );
    p_primary->Flip( p_primary, NULL, 0 );

    return VLC_SUCCESS;
}

static void CloseDisplay( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    IDirectFB *p_directfb = p_sys->p_directfb;
    IDirectFBSurface *p_primary = p_sys->p_primary;

    if( p_primary )
        p_primary->Release( p_primary );

    if( p_directfb )
        p_directfb->Release( p_directfb );
}
