/*****************************************************************************
 * vout_pictures.c : picture management functions
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: vout_pictures.c,v 1.15 2002/02/15 13:32:54 sam Exp $
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * vout_DisplayPicture: display a picture
 *****************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture won't be displayed until vout_DatePicture has been
 * called.
 *****************************************************************************/
void vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:
        p_pic->i_status = RESERVED_DISP_PICTURE;
        break;
    case RESERVED_DATED_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;
#ifdef DEBUG
    default:
        intf_ErrMsg( "error: picture %p has invalid status %d",
                     p_pic, p_pic->i_status );
        break;
#endif
    }

#ifdef TRACE_VOUT
    intf_DbgMsg("picture %p", p_pic);
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
                       picture_t *p_pic, mtime_t date )
{
#ifdef TRACE_VOUT
    char        psz_date[ MSTRTIME_MAX_SIZE ];                       /* date */
#endif

    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->date = date;
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:
        p_pic->i_status = RESERVED_DATED_PICTURE;
        break;
    case RESERVED_DISP_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;
#ifdef DEBUG
    default:
        intf_ErrMsg( "error: picture %p has invalid status %d",
                     p_pic, p_pic->i_status );
        break;
#endif
    }

#ifdef TRACE_VOUT
    intf_DbgMsg( "picture %p, display date: %s",
                 p_pic, mstrtime( psz_date, p_pic->date) );
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
                               boolean_t b_progressive,
                               boolean_t b_top_field_first,
                               boolean_t b_repeat_first_field )
{
    int         i_pic;                                      /* picture index */
    picture_t * p_pic;
    picture_t * p_freepic = NULL;                      /* first free picture */

    /* Get lock */
    vlc_mutex_lock( &p_vout->picture_lock );

    /*
     * Look for an empty place. We start at 1 because the first
     * directbuffer is reserved for memcpy()ed pictures.
     */
    for( i_pic = 0; i_pic < I_RENDERPICTURES && p_freepic == NULL; i_pic++ )
    {
        p_pic = PP_RENDERPICTURE[ i_pic ];

        /* If the picture we found is a memory buffer, and we have enough
         * pictures in the stack, and we might have enough room later for
         * a direct buffer, skip it. If no other pictures are found, the
         * video decoder will try again later. */
        if( p_vout->b_direct && ( p_vout->output.i_pictures > 5 )
             && ( p_pic->i_type != DIRECT_PICTURE ) )
        {
            break;
        }

        switch( p_pic->i_status )
        {
            case DESTROYED_PICTURE:
                /* Memory will not be reallocated, and function can end
                 * immediately - this is the best possible case, since no
                 * memory allocation needs to be done */
                p_pic->i_status   = RESERVED_PICTURE;
                p_pic->i_refcount = 0;

                p_pic->b_progressive        = b_progressive;
                p_pic->b_repeat_first_field = b_repeat_first_field;
                p_pic->b_top_field_first    = b_top_field_first;

                p_vout->i_heap_size++;
                vlc_mutex_unlock( &p_vout->picture_lock );
                return( p_pic );

            case FREE_PICTURE:
                /* Picture is empty and ready for allocation */
                p_freepic = p_pic;
                break;

            default:
                break;
        }
    }

    /*
     * Prepare picture
     */
    if( p_freepic != NULL )
    {
        vout_AllocatePicture( p_freepic,
                              p_vout->render.i_width, p_vout->render.i_height,
                              p_vout->render.i_chroma );

        if( p_freepic->i_planes )
        {
            /* Copy picture information, set some default values */
            p_freepic->i_status   = RESERVED_PICTURE;
            p_freepic->i_type     = MEMORY_PICTURE;

            p_freepic->i_refcount = 0;

            p_freepic->b_progressive        = b_progressive;
            p_freepic->b_repeat_first_field = b_repeat_first_field;
            p_freepic->b_top_field_first    = b_top_field_first;

            p_freepic->i_matrix_coefficients = 1;

            p_vout->i_heap_size++;
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_freepic->i_status = FREE_PICTURE;
            p_freepic = NULL;

            intf_ErrMsg( "vout error: picture allocation failed" );
        }

        vlc_mutex_unlock( &p_vout->picture_lock );

        /* Initialize mutex */
        vlc_mutex_init( &(p_freepic->lock_deccount) );

        return( p_freepic );
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
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );

#ifdef DEBUG
    /* Check if picture status is valid */
    if( (p_pic->i_status != RESERVED_PICTURE) &&
        (p_pic->i_status != RESERVED_DATED_PICTURE) &&
        (p_pic->i_status != RESERVED_DISP_PICTURE) )
    {
        intf_ErrMsg( "error: picture %p has invalid status %d",
                     p_pic, p_pic->i_status );
    }
#endif

    p_pic->i_status = DESTROYED_PICTURE;
    p_vout->i_heap_size--;

    /* destroy the lock that had been initialized in CreatePicture */
    vlc_mutex_destroy( &(p_pic->lock_deccount) );

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_LinkPicture: increment reference counter of a picture
 *****************************************************************************
 * This function increments the reference counter of a picture in the video
 * heap. It needs a lock since several producer threads can access the picture.
 *****************************************************************************/
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount++;

#ifdef TRACE_VOUT
    intf_DbgMsg( "picture %p refcount=%d", p_pic, p_pic->i_refcount );
#endif

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_UnlinkPicture: decrement reference counter of a picture
 *****************************************************************************
 * This function decrement the reference counter of a picture in the video heap.
 *****************************************************************************/
void vout_UnlinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount--;

#ifdef TRACE_VOUT
    if( p_pic->i_refcount < 0 )
    {
        intf_DbgMsg( "error: refcount < 0" );
        p_pic->i_refcount = 0;
    }
#endif

    if( ( p_pic->i_refcount == 0 ) &&
        ( p_pic->i_status == DISPLAYED_PICTURE ) )
    {
        p_pic->i_status = DESTROYED_PICTURE;
        p_vout->i_heap_size--;
    }

#ifdef TRACE_VOUT
    intf_DbgMsg( "picture %p refcount=%d", p_pic, p_pic->i_refcount );
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
picture_t * vout_RenderPicture( vout_thread_t *p_vout, picture_t *p_pic,
                                                       subpicture_t *p_subpic )
{
    if( p_pic == NULL )
    {
        /* XXX: subtitles */

        return NULL;
    }

    if( p_pic->i_type == DIRECT_PICTURE )
    {
        if( p_pic->i_refcount )
        {
            /* Picture is in a direct buffer and is still in use,
             * we need to copy it to another direct buffer before
             * displaying it if there are subtitles. */
            if( p_subpic != NULL )
            {
                /* We have subtitles. First copy the picture to
                 * the spare direct buffer, then render the
                 * subtitles. */
                vout_CopyPicture( p_pic, PP_OUTPUTPICTURE[0] );

                vout_RenderSubPictures( p_vout, PP_OUTPUTPICTURE[0], p_subpic );

                return PP_OUTPUTPICTURE[0];
            }

            /* No subtitles, picture is in a directbuffer so
             * we can display it directly even if it is still
             * in use. */
            return p_pic;
        }

        /* Picture is in a direct buffer but isn't used by the
         * decoder. We can safely render subtitles on it and
         * display it. */
        vout_RenderSubPictures( p_vout, p_pic, p_subpic );

        return p_pic;
    }

    /* Not a direct buffer. We either need to copy it to a direct buffer,
     * or render it if the chroma isn't the same. */
    if( p_vout->b_direct )
    {
        /* Picture is not in a direct buffer, but is exactly the
         * same size as the direct buffers. A memcpy() is enough,
         * then render the subtitles. */
        vout_CopyPicture( p_pic, PP_OUTPUTPICTURE[0] );

        vout_RenderSubPictures( p_vout, PP_OUTPUTPICTURE[0], p_subpic );

        return PP_OUTPUTPICTURE[0];
    }

    /* Picture is not in a direct buffer, and needs to be converted to
     * another size/chroma. Then the subtitles need to be rendered as
     * well. This usually means software YUV, or hardware YUV with a
     * different chroma. */

    /* Convert image to the first direct buffer */
    p_vout->chroma.pf_convert( p_vout, p_pic, &p_vout->p_picture[0] );

    /* Render subpictures on the first direct buffer */
    vout_RenderSubPictures( p_vout, &p_vout->p_picture[0], p_subpic );

    return &p_vout->p_picture[0];
}

/*****************************************************************************
 * vout_PlacePicture: calculate image window coordinates
 *****************************************************************************
 * This function will be accessed by plugins. It calculates the relative
 * position of the output window and the image window.
 *****************************************************************************/
void vout_PlacePicture( vout_thread_t *p_vout, int i_width, int i_height,
                        int *pi_x, int *pi_y, int *pi_width, int *pi_height )
{
    if( (i_width <= 0) || (i_height <=0) )
    {
        *pi_width = *pi_height = *pi_x = *pi_y = 0;

        return;
    }

    if( p_vout->b_scale )
    {
        *pi_width = i_width;
        *pi_height = i_height;
    }
    else
    {
        *pi_width = MIN( i_width, p_vout->render.i_width );
        *pi_height = MIN( i_height, p_vout->render.i_height );
    }

    if( VOUT_ASPECT_FACTOR * *pi_width / *pi_height < p_vout->render.i_aspect )
    {
        *pi_width = *pi_height * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
    }
    else
    {
        *pi_height = *pi_width * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }

    if( *pi_width > i_width )
    {
        *pi_width = i_width;
        *pi_height = VOUT_ASPECT_FACTOR * *pi_width / p_vout->render.i_aspect;
    }

    if( *pi_height > i_height )
    {
        *pi_height = i_height;
        *pi_width = *pi_height * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
    }

    *pi_x = ( i_width - *pi_width ) / 2;
    *pi_y = ( i_height - *pi_height ) / 2;
}

/*****************************************************************************
 * vout_AllocatePicture: allocate a new picture in the heap.
 *****************************************************************************
 * This function allocates a fake direct buffer in memory, which can be
 * used exactly like a video buffer. The video output thread then manages
 * how it gets displayed.
 *****************************************************************************/
void vout_AllocatePicture( picture_t *p_pic,
                           int i_width, int i_height, u32 i_chroma )
{
    int i_bytes, i_index;

    /* Store default values */
    for( i_index = 0; i_index < VOUT_MAX_PLANES; i_index++ )
    {
        p_pic->p[i_index].p_pixels = NULL;
        p_pic->p[i_index].b_margin = 0;
        p_pic->p[i_index].i_pixel_bytes = 1;
    }

    /* Calculate coordinates */
    switch( i_chroma )
    {
        case FOURCC_YV12:
        case FOURCC_I420:
        case FOURCC_IYUV:
            p_pic->p[ Y_PLANE ].i_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height / 2;
            p_pic->p[ U_PLANE ].i_pitch = i_width / 2;
            p_pic->p[ V_PLANE ].i_lines = i_height / 2;
            p_pic->p[ V_PLANE ].i_pitch = i_width / 2;
            p_pic->i_planes = 3;
            break;

        case FOURCC_I422:
            p_pic->p[ Y_PLANE ].i_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height;
            p_pic->p[ U_PLANE ].i_pitch = i_width / 2;
            p_pic->p[ V_PLANE ].i_lines = i_height;
            p_pic->p[ V_PLANE ].i_pitch = i_width / 2;
            p_pic->i_planes = 3;
            break;

        case FOURCC_I444:
            p_pic->p[ Y_PLANE ].i_lines = i_height;
            p_pic->p[ Y_PLANE ].i_pitch = i_width;
            p_pic->p[ U_PLANE ].i_lines = i_height;
            p_pic->p[ U_PLANE ].i_pitch = i_width;
            p_pic->p[ V_PLANE ].i_lines = i_height;
            p_pic->p[ V_PLANE ].i_pitch = i_width;
            p_pic->i_planes = 3;
            break;

        case FOURCC_Y211:
            p_pic->p->i_lines = i_height;
            p_pic->p->i_pitch = i_width;
            p_pic->p->i_pixel_bytes = 4;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV15:
            p_pic->p->i_lines = i_height;
            p_pic->p->i_pitch = i_width * 2;
            p_pic->p->i_pixel_bytes = 2;
            p_pic->p->i_red_mask =   0x001f;
            p_pic->p->i_green_mask = 0x03e0;
            p_pic->p->i_blue_mask =  0x7c00;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV16:
            p_pic->p->i_lines = i_height;
            p_pic->p->i_pitch = i_width * 2;
            p_pic->p->i_pixel_bytes = 2;
            p_pic->p->i_red_mask =   0x001f;
            p_pic->p->i_green_mask = 0x07e0;
            p_pic->p->i_blue_mask =  0xf800;
            p_pic->i_planes = 1;
            break;

        case FOURCC_RV32:
            p_pic->p->i_lines = i_height;
            p_pic->p->i_pitch = i_width * 4;
            p_pic->p->i_pixel_bytes = 4;
            p_pic->p->i_red_mask =   0xff0000;
            p_pic->p->i_green_mask = 0x00ff00;
            p_pic->p->i_blue_mask =  0x0000ff;
            p_pic->i_planes = 1;
            break;

        default:
            intf_ErrMsg( "vout error: unknown chroma type 0x%.8x (%4.4s)",
                         i_chroma, (char*)&i_chroma );
            p_pic->i_planes = 0;
            return;
    }

    /* Calculate how big the new image should be */
    for( i_bytes = 0, i_index = 0; i_index < p_pic->i_planes; i_index++ )
    {
        i_bytes += p_pic->p[ i_index ].i_lines * p_pic->p[ i_index ].i_pitch;
    }

    p_pic->p_data = memalign( 16, i_bytes );

    if( p_pic->p_data == NULL )
    {
        p_pic->i_planes = 0;
        return;
    }

    /* Fill the p_pixels field for each plane */
    p_pic->p[ 0 ].p_pixels = p_pic->p_data;

    for( i_index = 1; i_index < p_pic->i_planes; i_index++ )
    {
        p_pic->p[i_index].p_pixels = p_pic->p[i_index-1].p_pixels
                                          + p_pic->p[i_index-1].i_lines
                                             * p_pic->p[i_index-1].i_pitch;
    }
}

