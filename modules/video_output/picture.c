/*****************************************************************************
 * picture.c:
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_image.h"

#include "picture.h"

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct vout_sys_t
{
    picture_vout_t *p_picture_vout;
    vlc_mutex_t *p_lock;

    int i_picture_pos; /* picture position in p_picture_vout */
    image_handler_t *p_image; /* filters for resizing */
#ifdef IMAGE_2PASSES
    image_handler_t *p_image2;
#endif
    int i_height, i_width;
    mtime_t i_last_pic;
};

/* Delay after which the picture is blanked out if there hasn't been any
 * new picture. */
#define BLANK_DELAY     I64C(1000000)

typedef void (* pf_release_t)( picture_t * );
static void ReleasePicture( picture_t *p_pic )
{
    p_pic->i_refcount--;

    if ( p_pic->i_refcount <= 0 )
    {
        if ( p_pic->p_sys != NULL )
        {
            pf_release_t pf_release = (pf_release_t)p_pic->p_sys;
            p_pic->p_sys = NULL;
            pf_release( p_pic );
        }
        else
        {
            if( p_pic && p_pic->p_data_orig ) free( p_pic->p_data_orig );
            if( p_pic ) free( p_pic );
        }
    }
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static int  Init    ( vout_thread_t * );
static void End     ( vout_thread_t * );
static int  Manage  ( vout_thread_t * );
static void Display ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier string for this subpicture" )

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "Allows you to specify the output video width." )
#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "Allows you to specify the output video height." )

vlc_module_begin();
    set_shortname( _( "Picture" ) );
    set_description(_("VLC internal picture video output") );
    set_capability( "video output", 0 );

    add_string( "picture-id", "Id", NULL, ID_TEXT, ID_LONGTEXT, VLC_FALSE );
    add_integer( "picture-width", 0, NULL, WIDTH_TEXT,
                 WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( "picture-height", 0, NULL, HEIGHT_TEXT,
                 HEIGHT_LONGTEXT, VLC_TRUE );

    set_callbacks( Open, Close );

    var_Create( p_module->p_libvlc, "picture-lock", VLC_VAR_MUTEX );
vlc_module_end();

/*****************************************************************************
 * Open : allocate video thread output method
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;
    libvlc_t *p_libvlc = p_vout->p_libvlc;
    picture_vout_t *p_picture_vout = NULL;
    picture_vout_e_t *p_pic;
    vlc_value_t val, lockval;

    p_sys = p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    var_Get( p_libvlc, "picture-lock", &lockval );
    p_sys->p_lock = lockval.p_address;
    vlc_mutex_lock( p_sys->p_lock );

    p_sys->i_picture_pos = -1;
    if( var_Get( p_libvlc, "p_picture_vout", &val ) != VLC_SUCCESS )
    {
        p_picture_vout = malloc( sizeof( struct picture_vout_t ) );
        if( p_picture_vout == NULL )
        {
            msg_Err( p_vout, "out of memory" );
            return VLC_ENOMEM;
        }

        var_Create( p_libvlc, "p_picture_vout", VLC_VAR_ADDRESS );
        val.p_address = p_picture_vout;
        var_Set( p_libvlc, "p_picture_vout", val );

        p_picture_vout->i_picture_num = 0;
        p_picture_vout->p_pic = NULL;
    }
    else
    {
        int i;
        p_picture_vout = val.p_address;
        for ( i = 0; i < p_picture_vout->i_picture_num; i++ )
        {
            if ( p_picture_vout->p_pic[i].i_status == PICTURE_VOUT_E_AVAILABLE )
                break;
        }

        if ( i != p_picture_vout->i_picture_num )
            p_sys->i_picture_pos = i;
    }

    p_sys->p_picture_vout = p_picture_vout;

    if ( p_sys->i_picture_pos == -1 )
    {
        p_picture_vout->p_pic = realloc( p_picture_vout->p_pic,
                                         (p_picture_vout->i_picture_num + 1)
                                           * sizeof(picture_vout_e_t) );
        p_sys->i_picture_pos = p_picture_vout->i_picture_num;
        p_picture_vout->i_picture_num++;
    }

    p_pic = &p_picture_vout->p_pic[p_sys->i_picture_pos];
    p_pic->p_picture = NULL;
    p_pic->i_status = PICTURE_VOUT_E_OCCUPIED;

    var_Create( p_vout, "picture-id", VLC_VAR_STRING );
    var_Change( p_vout, "picture-id", VLC_VAR_INHERITVALUE, &val, NULL );
    p_pic->psz_id = val.psz_string;

    vlc_mutex_unlock( p_sys->p_lock );

    var_Create( p_vout, "picture-height", VLC_VAR_INTEGER );
    var_Change( p_vout, "picture-height", VLC_VAR_INHERITVALUE, &val, NULL );
    p_sys->i_height = val.i_int; 

    var_Create( p_vout, "picture-width", VLC_VAR_INTEGER );
    var_Change( p_vout, "picture-width", VLC_VAR_INHERITVALUE, &val, NULL );
    p_sys->i_width = val.i_int; 

    if ( p_sys->i_height || p_sys->i_width )
    {
        p_sys->p_image = image_HandlerCreate( p_vout );
#ifdef IMAGE_2PASSES
        p_sys->p_image2 = image_HandlerCreate( p_vout );
#endif
    }

    p_sys->i_last_pic = 0;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Init
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    picture_t *p_pic;
    int i_index;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    while( VLC_TRUE )
    {
        p_pic = NULL;

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
            return VLC_SUCCESS;
        }

        vout_AllocatePicture( VLC_OBJECT(p_vout), p_pic,
                              p_vout->output.i_chroma,
                              p_vout->output.i_width, p_vout->output.i_height,
                              p_vout->output.i_aspect );

        if( p_pic->i_planes == 0 )
        {
            return VLC_EGENERIC;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;

        //return VLC_SUCCESS;
    }

}

/*****************************************************************************
 * End
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    return;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_vout_t *p_picture_vout = p_sys->p_picture_vout;
    picture_vout_e_t *p_pic;
    vlc_bool_t b_last_picture = VLC_TRUE;
    int i;

    vlc_mutex_lock( p_sys->p_lock );
    p_pic = &p_picture_vout->p_pic[p_sys->i_picture_pos];

    if( p_pic->p_picture )
    {
        p_pic->p_picture->pf_release( p_pic->p_picture );
        p_pic->p_picture = NULL;
    }
    p_pic->i_status = PICTURE_VOUT_E_AVAILABLE;
    if( p_pic->psz_id )
        free( p_pic->psz_id );

    for( i = 0; i < p_picture_vout->i_picture_num; i ++)
    {
        if( p_picture_vout->p_pic[i].i_status == PICTURE_VOUT_E_OCCUPIED )
        {
            b_last_picture = VLC_FALSE;
            break;
        }
    }

    if( b_last_picture )
    {
        free( p_picture_vout->p_pic );
        free( p_picture_vout );
        var_Destroy( p_this->p_libvlc, "p_picture_vout" );
    }

    vlc_mutex_unlock( p_sys->p_lock );

    if ( p_sys->i_height || p_sys->i_width )
    {
        image_HandlerDelete( p_sys->p_image );
#ifdef IMAGE_2PASSES
        image_HandlerDelete( p_sys->p_image2 );
#endif
    }

    free( p_sys );
}

/*****************************************************************************
 * PushPicture : push a picture in the p_picture_vout structure
 *****************************************************************************/
static void PushPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_vout_t *p_picture_vout = p_sys->p_picture_vout;
    picture_vout_e_t *p_pic;

    vlc_mutex_lock( p_sys->p_lock );
    p_pic = &p_picture_vout->p_pic[p_sys->i_picture_pos];

    if( p_pic->p_picture != NULL )
    {
        p_pic->p_picture->pf_release( p_pic->p_picture );
    }
    p_pic->p_picture = p_picture;

    vlc_mutex_unlock( p_sys->p_lock );
}

/*****************************************************************************
 * Manage
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    if ( mdate() - p_sys->i_last_pic > BLANK_DELAY )
    {
        /* Display black */
#if 0
        picture_t *p_new_pic = (picture_t*)malloc( sizeof(picture_t) );
        int i;

        if ( p_sys->i_height || p_sys->i_width )
        {
            vout_AllocatePicture( p_vout, p_new_pic,
                                  VLC_FOURCC('Y','U','V','A'),
                                  p_sys->i_width, p_sys->i_height,
                                  p_vout->render.i_aspect );
        }
        else
        {
            vout_AllocatePicture( p_vout, p_new_pic, p_vout->render.i_chroma,
                                  p_vout->render.i_width,
                                  p_vout->render.i_height,
                                  p_vout->render.i_aspect );
        }

        p_new_pic->i_refcount++;
        p_new_pic->i_status = DESTROYED_PICTURE;
        p_new_pic->i_type   = DIRECT_PICTURE;
        p_new_pic->pf_release = ReleasePicture;

        for ( i = 0; i < p_pic->i_planes; i++ )
        {
            /* This assumes planar YUV format */
            p_vout->p_vlc->pf_memset( p_pic->p[i].p_pixels, i ? 0x80 : 0,
                                      p_pic->p[i].i_lines
                                       * p_pic->p[i].i_pitch );
        }

        PushPicture( p_vout, p_new_pic );
#else
        PushPicture( p_vout, NULL );
#endif
        p_sys->i_last_pic = INT64_MAX;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Display
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *p_new_pic;

    if ( p_sys->i_height || p_sys->i_width )
    {
        video_format_t fmt_out = {0}, fmt_in = {0};
#ifdef IMAGE_2PASSES
        vide_format_t fmt_middle = {0};
        picture_t *p_new_pic2;
#endif

        fmt_in = p_vout->fmt_in;

#ifdef IMAGE_2PASSES
        fmt_middle.i_chroma = p_vout->render.i_chroma;
        fmt_middle.i_width = p_vout->p_sys->i_width;
        fmt_middle.i_height = p_vout->p_sys->i_height;
        fmt_middle.i_visible_width = fmt_middle.i_width;
        fmt_middle.i_visible_height = fmt_middle.i_height;

        p_new_pic2 = image_Convert( p_vout->p_sys->p_image2,
                                    p_pic, &fmt_in, &fmt_middle );
        if ( p_new_pic2 == NULL )
        {
            msg_Err( p_vout, "image resizing failed %dx%d->%dx%d %4.4s",
                     p_vout->render.i_width, p_vout->render.i_height,
                     fmt_middle.i_width, fmt_middle.i_height,
                     (char *)&p_vout->render.i_chroma);
            return;
        }
#endif

        fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
        fmt_out.i_width = p_sys->i_width;
        fmt_out.i_height = p_sys->i_height;
        fmt_out.i_visible_width = fmt_out.i_width;
        fmt_out.i_visible_height = fmt_out.i_height;

#ifdef IMAGE_2PASSES
        p_new_pic = image_Convert( p_vout->p_sys->p_image,
                                   p_new_pic2, &fmt_middle, &fmt_out );
        p_new_pic2->pf_release( p_new_pic2 );
#else
        p_new_pic = image_Convert( p_vout->p_sys->p_image,
                                   p_pic, &fmt_in, &fmt_out );
#endif
        if ( p_new_pic == NULL )
        {
            msg_Err( p_vout, "image conversion failed" );
            return;
        }
    }
    else
    {
        p_new_pic = (picture_t*)malloc( sizeof(picture_t) );
        vout_AllocatePicture( p_vout, p_new_pic, p_pic->format.i_chroma,
                              p_pic->format.i_width, p_pic->format.i_height,
                              p_vout->render.i_aspect );

        vout_CopyPicture( p_vout, p_new_pic, p_pic );
    }

    p_new_pic->i_refcount = 1;
    p_new_pic->i_status = DESTROYED_PICTURE;
    p_new_pic->i_type   = DIRECT_PICTURE;
    p_new_pic->p_sys = (picture_sys_t *)p_new_pic->pf_release;
    p_new_pic->pf_release = ReleasePicture;

    PushPicture( p_vout, p_new_pic );
    p_sys->i_last_pic = p_pic->date;
}
