/*****************************************************************************
 * picture.c:
 *****************************************************************************
 * Copyright (C) 2004-2005 VideoLAN
 * $Id: $
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>

#include <sys/types.h>
#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

/***********************************************************************
*
***********************************************************************/
struct vout_sys_t
{
    int i_picture_pos; /* picture position in p_picture_vout */
};

#include "picture.h"

/***********************************************************************
* Local prototypes
***********************************************************************/

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static int  Init    ( vout_thread_t * );
static void End     ( vout_thread_t * );
static int  Manage  ( vout_thread_t * );
static void Display ( vout_thread_t *, picture_t * );

/***********************************************************************
* Module descriptor
***********************************************************************/
vlc_module_begin();
    set_description(_("VLC Internal Picture video output") );
    set_capability( "video output", 70 );
    set_callbacks( Open, Close );
vlc_module_end();

/***********************************************************************
* Open : allocate video thread output method
***********************************************************************/
static int Open ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    libvlc_t *p_libvlc = p_vout->p_libvlc;
    struct picture_vout_t *p_picture_vout = NULL;
    vlc_value_t val;

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    if( var_Get( p_libvlc, "p_picture_vout", &val ) != VLC_SUCCESS ){
        msg_Err( p_vout, "p_picture_vout not found" );
        p_picture_vout = malloc( sizeof( struct picture_vout_t ) );
        if( p_vout->p_sys == NULL )
        {
            msg_Err( p_vout, "out of memory" );
            return VLC_ENOMEM;
        }

        vlc_mutex_init( p_libvlc, &p_picture_vout->lock );
        vlc_mutex_lock( &p_picture_vout->lock );
        var_Create( p_libvlc, "p_picture_vout", VLC_VAR_ADDRESS );
        val.p_address = p_picture_vout;
        var_Set( p_libvlc, "p_picture_vout", val );

        p_picture_vout->i_picture_num = 0;
        p_picture_vout->p_pic = NULL;
    } else {
        p_picture_vout = val.p_address;
        msg_Err( p_vout, "p_picture_vout found" );
        vlc_mutex_lock( &p_picture_vout->lock );
    }

    p_vout->p_sys->i_picture_pos = p_picture_vout->i_picture_num;

    p_picture_vout->p_pic = realloc( p_picture_vout->p_pic,
      (p_picture_vout->i_picture_num+1) * sizeof( struct picture_vout_e_t ) );

    p_picture_vout->p_pic[p_picture_vout->i_picture_num].p_picture = NULL;
    p_picture_vout->p_pic[p_picture_vout->i_picture_num].i_status
      = PICTURE_VOUT_E_OCCUPIED;
    p_picture_vout->i_picture_num++;

    vlc_mutex_unlock( &p_picture_vout->lock );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}


/***********************************************************************
* Init
***********************************************************************/
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

/***********************************************************************
* End
***********************************************************************/
static void End( vout_thread_t *p_vout )
{
    return;
}

/***********************************************************************
* Close
***********************************************************************/
static void Close ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    libvlc_t *p_libvlc = p_vout->p_libvlc;
    struct picture_vout_t *p_picture_vout = NULL;
    vlc_value_t val;
    int i_flag=0, i;

    msg_Dbg( p_vout, "Closing Picture Vout ...");
    var_Get( p_libvlc, "p_picture_vout", &val );
    p_picture_vout = val.p_address;

    vlc_mutex_lock( &p_picture_vout->lock );

    if( p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture )
    {
    /* FIXME */
        free( p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture );
    }
    p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].i_status
      = PICTURE_VOUT_E_AVAILABLE;

    for( i = 0; i < p_picture_vout->i_picture_num; i ++)
    {
        if( p_picture_vout->p_pic[i].i_status == PICTURE_VOUT_E_OCCUPIED ) {
            i_flag = 1;
            break;
        }
    }

    if( i_flag == 1 ){
        vlc_mutex_unlock( &p_picture_vout->lock );
    } else {
        free( p_picture_vout->p_pic );
        vlc_mutex_unlock( &p_picture_vout->lock );
        vlc_mutex_destroy( &p_picture_vout->lock );
        var_Destroy( p_libvlc, "p_picture_vout" );
    }

    free( p_vout->p_sys );
}

/***********************************************************************
* Manage
***********************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    return VLC_SUCCESS;
}

/***********************************************************************
* Display
***********************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    libvlc_t *p_libvlc = p_vout->p_libvlc;
    vlc_value_t val;
    struct picture_vout_t *p_picture_vout;

    var_Get( p_libvlc, "p_picture_vout", &val );
    p_picture_vout = val.p_address;

    /*
    src : p_pic
    dest : p_picture_pout->p_pic[p_vout->p_sys.i_picture_pos]->p_picture
    */

    vlc_mutex_lock( &p_picture_vout->lock );
    if( p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture )
    {
      if( p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture->p_data_orig )
      {
        free( p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos]
                        .p_picture->p_data_orig );
      }
      free( p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture );
    }

    p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture
             = (picture_t*)malloc( sizeof( picture_t )) ;
    vout_AllocatePicture( p_vout,
         p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture,
         p_pic->format.i_chroma,
         p_pic->format.i_width,
         p_pic->format.i_height,
         VOUT_ASPECT_FACTOR * p_pic->format.i_height / p_pic->format.i_width );

    p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture->i_status = DESTROYED_PICTURE;
    p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture->i_type   = DIRECT_PICTURE;

    vout_CopyPicture( p_vout,
        p_picture_vout->p_pic[p_vout->p_sys->i_picture_pos].p_picture,
        p_pic);

    vlc_mutex_unlock( &p_picture_vout->lock );
}
