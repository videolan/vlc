/*****************************************************************************
 * vout_pictures.c : picture management functions
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: vout_pictures.c,v 1.1 2001/12/09 17:01:37 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void     NewPicture        ( picture_t *, int, int, int );

/*****************************************************************************
 * vout_DisplayPicture: display a picture
 *****************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture won't be displayed until vout_DatePicture has been
 * called.
 *****************************************************************************/
void vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    switch( p_picture->i_status )
    {
    case RESERVED_PICTURE:
        p_picture->i_status = RESERVED_DISP_PICTURE;
        break;
    case RESERVED_DATED_PICTURE:
        p_picture->i_status = READY_PICTURE;
        break;
#ifdef DEBUG
    default:
        intf_ErrMsg("error: picture %p has invalid status %d", p_picture, p_picture->i_status );
        break;
#endif
    }

#ifdef TRACE_VOUT
    intf_DbgMsg("picture %p", p_picture);
#endif
    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_DatePicture: date a picture
 *****************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready
 * for display. The picture won't be displayed until vout_DisplayPicture has
 * been called.
 *****************************************************************************/
void vout_DatePicture( vout_thread_t *p_vout,
                       picture_t *p_picture, mtime_t date )
{
#ifdef TRACE_VOUT
    char        psz_date[ MSTRTIME_MAX_SIZE ];                       /* date */
#endif

    vlc_mutex_lock( &p_vout->picture_lock );
    p_picture->date = date;
    switch( p_picture->i_status )
    {
    case RESERVED_PICTURE:
        p_picture->i_status = RESERVED_DATED_PICTURE;
        break;
    case RESERVED_DISP_PICTURE:
        p_picture->i_status = READY_PICTURE;
        break;
#ifdef DEBUG
    default:
        intf_ErrMsg("error: picture %p has invalid status %d", p_picture, p_picture->i_status );
        break;
#endif
    }

#ifdef TRACE_VOUT
    intf_DbgMsg("picture %p, display date: %s", p_picture, mstrtime( psz_date, p_picture->date) );
#endif
    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_CreatePicture: allocate a picture in the video output heap.
 *****************************************************************************
 * This function creates a reserved image in the video output heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the picture data fields. It needs locking
 * since several pictures can be created by several producers threads.
 *****************************************************************************/
picture_t *vout_CreatePicture( vout_thread_t *p_vout,
                               int i_width, int i_height,
                               int i_chroma, int i_aspect_ratio )
{
    int         i_picture;                                  /* picture index */
    picture_t * p_picture;
    picture_t * p_free_picture = NULL;                 /* first free picture */
    picture_t * p_destroyed_picture = NULL;       /* first destroyed picture */

    /* Get lock */
    vlc_mutex_lock( &p_vout->picture_lock );

    /*
     * Look for an empty place. XXX: we start at 1 because the first
     * directbuffer is reserved for memcpy()ed pictures.
     */
    for( i_picture = 1; i_picture < VOUT_MAX_PICTURES; i_picture++ )
    {
        p_picture = p_vout->p_picture + i_picture;

        if( p_picture->i_status == DESTROYED_PICTURE )
        {
            /* Picture is marked for destruction, but is still allocated.
             * Note that if width and type are the same for two pictures,
             * chroma_width should also be the same */
            if( ( p_picture->i_chroma == i_chroma ) &&
                ( p_picture->i_height == i_height ) &&
                ( p_picture->i_width  == i_width ) )
            {
                /* Memory size do match : memory will not be reallocated,
                 * and function can end immediately - this is the best
                 * possible case, since no memory allocation needs to be
                 * done */
                p_picture->i_status = RESERVED_PICTURE;
                p_vout->i_pictures++;

                vlc_mutex_unlock( &p_vout->picture_lock );

                return( p_picture );
            }
            else if( ( p_destroyed_picture == NULL )
                     && !p_picture->b_directbuffer )
            {
                /* Memory size do not match, but picture index will be kept in
                 * case no other place are left */
                p_destroyed_picture = p_picture;
            }
        }
        else if( ( p_free_picture == NULL )
                 && ( p_picture->i_status == FREE_PICTURE ) )
        {
            /* Picture is empty and ready for allocation */
            p_free_picture = p_picture;
        }
    }

    /* If no free picture is available, use a destroyed picture */
    if( ( p_free_picture == NULL ) && ( p_destroyed_picture != NULL ) )
    {
        /* No free picture or matching destroyed picture has been found, but
         * a destroyed picture is still avalaible */
        free( p_destroyed_picture->planes[0].p_data );
        p_destroyed_picture->i_planes = 0;
        p_free_picture = p_destroyed_picture;
    }

    /*
     * Prepare picture
     */
    if( p_free_picture != NULL )
    {
        NewPicture( p_free_picture, i_chroma, i_width, i_height );

        if( p_free_picture->i_planes != 0 )
        {
            /* Copy picture information, set some default values */
            p_free_picture->i_width               = i_width;
            p_free_picture->i_height              = i_height;
            p_free_picture->i_chroma              = i_chroma;
            p_free_picture->i_aspect_ratio        = i_aspect_ratio;
            p_free_picture->i_status              = RESERVED_PICTURE;
            p_free_picture->i_matrix_coefficients = 1;
            p_free_picture->i_refcount            = 0;
            p_vout->i_pictures++;
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_free_picture->i_chroma =  EMPTY_PICTURE;
            p_free_picture->i_status =  FREE_PICTURE;
            p_free_picture =            NULL;

            intf_ErrMsg( "vout error: picture allocation failed" );
        }

        vlc_mutex_unlock( &p_vout->picture_lock );

        /* Initialize mutex */
        vlc_mutex_init( &(p_free_picture->lock_deccount) );

        return( p_free_picture );
    }

    /* No free or destroyed picture could be found, but the decoder
     * will try again in a while. */
    vlc_mutex_unlock( &p_vout->picture_lock );

    return( NULL );
}

/*****************************************************************************
 * vout_DestroyPicture: remove a permanent or reserved picture from the heap
 *****************************************************************************
 * This function frees a previously reserved picture or a permanent
 * picture. It is meant to be used when the construction of a picture aborted.
 * Note that the picture will be destroyed even if it is linked !
 *****************************************************************************/
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    vlc_mutex_lock( &p_vout->picture_lock );

#ifdef DEBUG
    /* Check if picture status is valid */
    if( (p_picture->i_status != RESERVED_PICTURE) &&
        (p_picture->i_status != RESERVED_DATED_PICTURE) &&
        (p_picture->i_status != RESERVED_DISP_PICTURE) )
    {
        intf_ErrMsg( "error: picture %p has invalid status %d",
                     p_picture, p_picture->i_status );
    }
#endif

    p_picture->i_status = DESTROYED_PICTURE;
    p_vout->i_pictures--;

    /* destroy the lock that had been initialized in CreatePicture */
    vlc_mutex_destroy( &(p_picture->lock_deccount) );

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_LinkPicture: increment reference counter of a picture
 *****************************************************************************
 * This function increments the reference counter of a picture in the video
 * heap. It needs a lock since several producer threads can access the picture.
 *****************************************************************************/
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_picture->i_refcount++;

#ifdef TRACE_VOUT
    intf_DbgMsg( "picture %p refcount=%d", p_picture, p_picture->i_refcount );
#endif

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_UnlinkPicture: decrement reference counter of a picture
 *****************************************************************************
 * This function decrement the reference counter of a picture in the video heap.
 *****************************************************************************/
void vout_UnlinkPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_picture->i_refcount--;

#ifdef TRACE_VOUT
    if( p_picture->i_refcount < 0 )
    {
        intf_DbgMsg( "error: refcount < 0" );
        p_picture->i_refcount = 0;
    }
#endif

    if( ( p_picture->i_refcount == 0 ) &&
        ( p_picture->i_status == DISPLAYED_PICTURE ) )
    {
        p_picture->i_status = DESTROYED_PICTURE;
        p_vout->i_pictures--;
    }

#ifdef TRACE_VOUT
    intf_DbgMsg( "picture %p refcount=%d", p_picture, p_picture->i_refcount );
#endif

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_RenderPicture: render a picture
 *****************************************************************************
 * This function chooses whether the current picture needs to be copied
 * before rendering, does the subpicture magic, and tells the video output
 * thread which direct buffer needs to be displayed.
 *****************************************************************************/
picture_t * vout_RenderPicture( vout_thread_t *p_vout, picture_t *p_picture,
                                                       subpicture_t *p_subpic )
{
    int i_index;

    if( p_picture == NULL )
    {
        /* XXX: subtitles */

        return NULL;
    }

    if( p_picture->b_directbuffer )
    {
        if( p_picture->i_refcount )
        {
            /* Picture is in a direct buffer and is still in use,
             * we need to copy it to another direct buffer before
             * displaying it if there are subtitles. */
            if( p_subpic != NULL )
            {
                /* We have subtitles. First copy the picture to
                 * the spare direct buffer, then render the
                 * subtitles. */
                for( i_index = 0 ; i_index < p_picture->i_planes ; i_index++ )
                {
                    p_main->fast_memcpy(
                        p_vout->p_picture[0].planes[ i_index ].p_data,
                        p_picture->planes[ i_index ].p_data,
                        p_picture->planes[ i_index ].i_bytes );
                }

                vout_RenderSubPictures( &p_vout->p_picture[0], p_subpic );

                return &p_vout->p_picture[0];
            }

            /* No subtitles, picture is in a directbuffer so
             * we can display it directly even if it is still
             * in use. */
            return p_picture;
        }

        /* Picture is in a direct buffer but isn't used by the
         * decoder. We can safely render subtitles on it and
         * display it. */
        vout_RenderSubPictures( p_picture, p_subpic );

        return p_picture;
    }

    /* Not a direct buffer. We either need to copy it to a direct buffer,
     * or render it if the chroma isn't the same. */
    if( ( p_picture->i_chroma == p_vout->p_picture[0].i_chroma ) &&
        ( p_picture->i_height == p_vout->p_picture[0].i_height ) &&
        ( p_picture->i_width  == p_vout->p_picture[0].i_width ) )
    {
        /* Picture is not in a direct buffer, but is exactly the
         * same size as the direct buffers. A memcpy() is enough,
         * then render the subtitles. */
        for( i_index = 0; i_index < p_picture->i_planes; i_index++ )
        {
            p_main->fast_memcpy( p_vout->p_picture[0].planes[ i_index ].p_data,
                                 p_picture->planes[ i_index ].p_data,
                                 p_picture->planes[ i_index ].i_bytes );
        }

        vout_RenderSubPictures( &p_vout->p_picture[0], p_subpic );

        return &p_vout->p_picture[0];
    }

    /* Picture is not in a direct buffer, and needs to be converted to
     * another size/chroma. Then the subtitles need to be rendered as
     * well. */

    /* This usually means software YUV, or hardware YUV with a
     * different chroma. */

    /* XXX: render to direct buffer */

    vout_RenderSubPictures( p_picture, p_subpic );

    return &p_vout->p_picture[0];
}

/* Following functions are local */

/*****************************************************************************
 * NewPicture: allocate a new picture in the heap.
 *****************************************************************************
 * This function allocates a fake direct buffer in memory, which can be
 * used exactly like a video buffer. The video output thread then manages
 * how it gets displayed.
 *****************************************************************************/
static void NewPicture( picture_t *p_picture, int i_chroma,
                        int i_width, int i_height )
{
    int i_data_size = 0;

    p_picture->i_size = i_width * i_height;

    /* Calculate coordinates */
    switch( i_chroma )
    {
        case YUV_420_PICTURE:        /* YUV 420: 1,1/4,1/4 samples per pixel */
            p_picture->i_chroma_size = p_picture->i_size / 4;
            p_picture->i_chroma_width = i_width / 2;
            break;

        case YUV_422_PICTURE:        /* YUV 422: 1,1/2,1/2 samples per pixel */
            p_picture->i_chroma_size = p_picture->i_size / 2;
            p_picture->i_chroma_width = i_width / 2;
            break;

        case YUV_444_PICTURE:            /* YUV 444: 1,1,1 samples per pixel */
            p_picture->i_chroma_size = p_picture->i_size;
            p_picture->i_chroma_width = i_width;
            break;

        default:
            intf_ErrMsg("error: unknown chroma type %d", i_chroma );
            p_picture->i_planes = 0;
            return;
    }

    /* Allocate memory */
    switch( i_chroma )
    {
        case YUV_420_PICTURE:        /* YUV 420: 1,1/4,1/4 samples per pixel */
        case YUV_422_PICTURE:        /* YUV 422: 1,1/2,1/2 samples per pixel */
        case YUV_444_PICTURE:            /* YUV 444: 1,1,1 samples per pixel */

            i_data_size = p_picture->i_size + 2 * p_picture->i_chroma_size;

            /* The Y plane */
            p_picture->planes[ Y_PLANE ].i_bytes =
                 p_picture->i_size * sizeof(pixel_data_t);
            p_picture->planes[ Y_PLANE ].p_data =
                 memalign( 16, i_data_size * sizeof(pixel_data_t) * 4 );
            /* The U plane */
            p_picture->planes[ U_PLANE ].i_bytes =
                 p_picture->i_chroma_size * sizeof(pixel_data_t);
            p_picture->planes[ U_PLANE ].p_data =
                 p_picture->planes[ Y_PLANE ].p_data + p_picture->i_size;
            /* The V plane */
            p_picture->planes[ V_PLANE ].i_bytes =
                 p_picture->i_chroma_size * sizeof(pixel_data_t);
            p_picture->planes[ V_PLANE ].p_data =
                 p_picture->planes[ U_PLANE ].p_data + p_picture->i_chroma_size;

            p_picture->i_planes = 3;

            break;

        default:
            p_picture->i_planes = 0;

            break;
    }
}

