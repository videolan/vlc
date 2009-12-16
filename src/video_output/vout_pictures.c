/*****************************************************************************
 * vout_pictures.c : picture management functions
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <libvlc.h>
#include <vlc_vout.h>
#include <vlc_osd.h>
#include <vlc_filter.h>
#include <vlc_image.h>
#include <vlc_block.h>
#include <vlc_picture_fifo.h>
#include <vlc_picture_pool.h>

#include "vout_pictures.h"
#include "vout_internal.h"

/**
 * Display a picture
 *
 * Remove the reservation flag of a picture, which will cause it to be ready
 * for display.
 */
void vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );

    if( p_pic->i_status == RESERVED_PICTURE )
    {
        p_pic->i_status = READY_PICTURE;
        vlc_cond_signal( &p_vout->p->picture_wait );
    }
    else
    {
        msg_Err( p_vout, "picture to display %p has invalid status %d",
                         p_pic, p_pic->i_status );
    }
    p_vout->p->i_picture_qtype = p_pic->i_qtype;
    p_vout->p->b_picture_interlaced = !p_pic->b_progressive;

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/**
 * Allocate a picture in the video output heap.
 *
 * This function creates a reserved image in the video output heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the picture data fields.
 * It needs locking since several pictures can be created by several producers
 * threads.
 */
int vout_CountPictureAvailable( vout_thread_t *p_vout )
{
    int i_free = 0;
    int i_pic;

    vlc_mutex_lock( &p_vout->picture_lock );
    for( i_pic = 0; i_pic < I_RENDERPICTURES; i_pic++ )
    {
        picture_t *p_pic = PP_RENDERPICTURE[(p_vout->render.i_last_used_pic + i_pic + 1) % I_RENDERPICTURES];

        switch( p_pic->i_status )
        {
            case DESTROYED_PICTURE:
                i_free++;
                break;

            case FREE_PICTURE:
                i_free++;
                break;

            default:
                break;
        }
    }
    vlc_mutex_unlock( &p_vout->picture_lock );

    return i_free;
}

picture_t *vout_CreatePicture( vout_thread_t *p_vout,
                               bool b_progressive,
                               bool b_top_field_first,
                               unsigned int i_nb_fields )
{
    int         i_pic;                                      /* picture index */
    picture_t * p_pic;
    picture_t * p_freepic = NULL;                      /* first free picture */

    /* Get lock */
    vlc_mutex_lock( &p_vout->picture_lock );

    /*
     * Look for an empty place in the picture heap.
     */
    for( i_pic = 0; i_pic < I_RENDERPICTURES; i_pic++ )
    {
        p_pic = PP_RENDERPICTURE[(p_vout->render.i_last_used_pic + i_pic + 1)
                                 % I_RENDERPICTURES];

        switch( p_pic->i_status )
        {
            case DESTROYED_PICTURE:
                /* Memory will not be reallocated, and function can end
                 * immediately - this is the best possible case, since no
                 * memory allocation needs to be done */
                p_pic->i_status   = RESERVED_PICTURE;
                p_pic->i_refcount = 0;
                p_pic->b_force    = 0;

                p_pic->b_progressive        = b_progressive;
                p_pic->i_nb_fields          = i_nb_fields;
                p_pic->b_top_field_first    = b_top_field_first;

                p_vout->i_heap_size++;
                p_vout->render.i_last_used_pic =
                    ( p_vout->render.i_last_used_pic + i_pic + 1 )
                    % I_RENDERPICTURES;
                vlc_mutex_unlock( &p_vout->picture_lock );
                return( p_pic );

            case FREE_PICTURE:
                /* Picture is empty and ready for allocation */
                p_vout->render.i_last_used_pic =
                    ( p_vout->render.i_last_used_pic + i_pic + 1 )
                    % I_RENDERPICTURES;
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
        vout_AllocatePicture( VLC_OBJECT(p_vout),
                              p_freepic, p_vout->render.i_chroma,
                              p_vout->render.i_width, p_vout->render.i_height,
                              p_vout->render.i_aspect * p_vout->render.i_height,
                              VOUT_ASPECT_FACTOR      * p_vout->render.i_width);

        if( p_freepic->i_planes )
        {
            /* Copy picture information, set some default values */
            p_freepic->i_status   = RESERVED_PICTURE;
            p_freepic->i_type     = MEMORY_PICTURE;
            p_freepic->b_slow     = 0;

            p_freepic->i_refcount = 0;
            p_freepic->b_force = 0;

            p_freepic->b_progressive        = b_progressive;
            p_freepic->i_nb_fields          = i_nb_fields;
            p_freepic->b_top_field_first    = b_top_field_first;

            p_vout->i_heap_size++;
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_freepic->i_status = FREE_PICTURE;
            p_freepic = NULL;

            msg_Err( p_vout, "picture allocation failed" );
        }

        vlc_mutex_unlock( &p_vout->picture_lock );

        return( p_freepic );
    }

    /* No free or destroyed picture could be found, but the decoder
     * will try again in a while. */
    vlc_mutex_unlock( &p_vout->picture_lock );

    return( NULL );
}

/* */
static void DestroyPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    vlc_assert_locked( &p_vout->picture_lock );

    p_picture->i_status = DESTROYED_PICTURE;
    p_vout->i_heap_size--;
    picture_CleanupQuant( p_picture );

    vlc_cond_signal( &p_vout->p->picture_wait );
}

/**
 * Remove a permanent or reserved picture from the heap
 *
 * This function frees a previously reserved picture or a permanent
 * picture. It is meant to be used when the construction of a picture aborted.
 * Note that the picture will be destroyed even if it is linked !
 *
 * TODO remove it, vout_DropPicture should be used instead
 */
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifndef NDEBUG
    /* Check if picture status is valid */
    vlc_mutex_lock( &p_vout->picture_lock );
    if( p_pic->i_status != RESERVED_PICTURE )
    {
        msg_Err( p_vout, "picture to destroy %p has invalid status %d",
                         p_pic, p_pic->i_status );
    }
    vlc_mutex_unlock( &p_vout->picture_lock );
#endif

    vout_DropPicture( p_vout, p_pic );
}

/* */
void vout_UsePictureLocked( vout_thread_t *p_vout, picture_t *p_picture )
{
    vlc_assert_locked( &p_vout->picture_lock );
    if( p_picture->i_refcount > 0 )
    {
        /* Pretend we displayed the picture, but don't destroy
         * it since the decoder might still need it. */
        p_picture->i_status = DISPLAYED_PICTURE;
    }
    else
    {
        /* Destroy the picture without displaying it */
        DestroyPicture( p_vout, p_picture );
    }
}

/* */
void vout_DropPicture( vout_thread_t *p_vout, picture_t *p_pic  )
{
    vlc_mutex_lock( &p_vout->picture_lock );

    if( p_pic->i_status == READY_PICTURE )
    {
        /* Grr cannot destroy ready picture by myself so be sure vout won't like it */
        p_pic->date = 1;
        vlc_cond_signal( &p_vout->p->picture_wait );
    }
    else
    {
        vout_UsePictureLocked( p_vout, p_pic );
    }

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/**
 * Increment reference counter of a picture
 *
 * This function increments the reference counter of a picture in the video
 * heap. It needs a lock since several producer threads can access the picture.
 */
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount++;
    vlc_mutex_unlock( &p_vout->picture_lock );
}

/**
 * Decrement reference counter of a picture
 *
 * This function decrement the reference counter of a picture in the video heap
 */
void vout_UnlinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );

    if( p_pic->i_refcount > 0 )
        p_pic->i_refcount--;
    else
        msg_Err( p_vout, "Invalid picture reference count (%p, %d)",
                 p_pic, p_pic->i_refcount );

    if( p_pic->i_refcount == 0 && p_pic->i_status == DISPLAYED_PICTURE )
        DestroyPicture( p_vout, p_pic );

    vlc_mutex_unlock( &p_vout->picture_lock );
}

static int vout_LockPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    if( p_picture->pf_lock )
        return p_picture->pf_lock( p_vout, p_picture );
    return VLC_SUCCESS;
}
static void vout_UnlockPicture( vout_thread_t *p_vout, picture_t *p_picture )
{
    if( p_picture->pf_unlock )
        p_picture->pf_unlock( p_vout, p_picture );
}

/**
 * Render a picture
 *
 * This function chooses whether the current picture needs to be copied
 * before rendering, does the subpicture magic, and tells the video output
 * thread which direct buffer needs to be displayed.
 */
picture_t *vout_RenderPicture( vout_thread_t *p_vout, picture_t *p_pic,
                               subpicture_t *p_subpic, mtime_t render_date )
{
    if( p_pic == NULL )
        return NULL;

    if( p_pic->i_type == DIRECT_PICTURE )
    {
        /* Picture is in a direct buffer. */

        if( p_subpic != NULL )
        {
            /* We have subtitles. First copy the picture to
             * the spare direct buffer, then render the
             * subtitles. */
            if( vout_LockPicture( p_vout, PP_OUTPUTPICTURE[0] ) )
                return NULL;

            picture_Copy( PP_OUTPUTPICTURE[0], p_pic );

            spu_RenderSubpictures( p_vout->p_spu,
                                   PP_OUTPUTPICTURE[0], &p_vout->fmt_out,
                                   p_subpic, &p_vout->fmt_in, render_date );

            vout_UnlockPicture( p_vout, PP_OUTPUTPICTURE[0] );

            return PP_OUTPUTPICTURE[0];
        }

        /* No subtitles, picture is in a directbuffer so
         * we can display it directly (even if it is still
         * in use or not). */
        return p_pic;
    }

    /* Not a direct buffer. We either need to copy it to a direct buffer,
     * or render it if the chroma isn't the same. */
    if( p_vout->p->b_direct )
    {
        /* Picture is not in a direct buffer, but is exactly the
         * same size as the direct buffers. A memcpy() is enough,
         * then render the subtitles. */

        if( vout_LockPicture( p_vout, PP_OUTPUTPICTURE[0] ) )
            return NULL;

        picture_Copy( PP_OUTPUTPICTURE[0], p_pic );
        spu_RenderSubpictures( p_vout->p_spu,
                               PP_OUTPUTPICTURE[0], &p_vout->fmt_out,
                               p_subpic, &p_vout->fmt_in, render_date );

        vout_UnlockPicture( p_vout, PP_OUTPUTPICTURE[0] );

        return PP_OUTPUTPICTURE[0];
    }

    /* Picture is not in a direct buffer, and needs to be converted to
     * another size/chroma. Then the subtitles need to be rendered as
     * well. This usually means software YUV, or hardware YUV with a
     * different chroma. */

    if( p_subpic != NULL && p_vout->p_picture[0].b_slow )
    {
        /* The picture buffer is in slow memory. We'll use
         * the "2 * VOUT_MAX_PICTURES + 1" picture as a temporary
         * one for subpictures rendering. */
        picture_t *p_tmp_pic = &p_vout->p_picture[2 * VOUT_MAX_PICTURES];
        if( p_tmp_pic->i_status == FREE_PICTURE )
        {
            vout_AllocatePicture( VLC_OBJECT(p_vout),
                                  p_tmp_pic, p_vout->fmt_out.i_chroma,
                                  p_vout->fmt_out.i_width,
                                  p_vout->fmt_out.i_height,
                                  p_vout->fmt_out.i_sar_num,
                                  p_vout->fmt_out.i_sar_den );
            p_tmp_pic->i_type = MEMORY_PICTURE;
            p_tmp_pic->i_status = RESERVED_PICTURE;
        }

        /* Convert image to the first direct buffer */
        p_vout->p->p_chroma->p_owner = (filter_owner_sys_t *)p_tmp_pic;
        p_vout->p->p_chroma->pf_video_filter( p_vout->p->p_chroma, p_pic );

        /* Render subpictures on the first direct buffer */
        spu_RenderSubpictures( p_vout->p_spu,
                               p_tmp_pic, &p_vout->fmt_out,
                               p_subpic, &p_vout->fmt_in, render_date );

        if( vout_LockPicture( p_vout, &p_vout->p_picture[0] ) )
            return NULL;

        picture_Copy( &p_vout->p_picture[0], p_tmp_pic );
    }
    else
    {
        if( vout_LockPicture( p_vout, &p_vout->p_picture[0] ) )
            return NULL;

        /* Convert image to the first direct buffer */
        p_vout->p->p_chroma->p_owner = (filter_owner_sys_t *)&p_vout->p_picture[0];
        p_vout->p->p_chroma->pf_video_filter( p_vout->p->p_chroma, p_pic );

        /* Render subpictures on the first direct buffer */
        spu_RenderSubpictures( p_vout->p_spu,
                               &p_vout->p_picture[0], &p_vout->fmt_out,
                               p_subpic, &p_vout->fmt_in, render_date );
    }

    vout_UnlockPicture( p_vout, &p_vout->p_picture[0] );

    return &p_vout->p_picture[0];
}

/**
 * Calculate image window coordinates
 *
 * This function will be accessed by plugins. It calculates the relative
 * position of the output window and the image window.
 */
void vout_PlacePicture( const vout_thread_t *p_vout,
                        unsigned int i_width, unsigned int i_height,
                        unsigned int *restrict pi_x,
                        unsigned int *restrict pi_y,
                        unsigned int *restrict pi_width,
                        unsigned int *restrict pi_height )
{
    if( i_width <= 0 || i_height <= 0 )
    {
        *pi_width = *pi_height = *pi_x = *pi_y = 0;
        return;
    }

    if( p_vout->b_autoscale )
    {
        *pi_width = i_width;
        *pi_height = i_height;
    }
    else
    {
        int i_zoom = p_vout->i_zoom;
        /* be realistic, scaling factor confined between .2 and 10. */
        if( i_zoom > 10 * ZOOM_FP_FACTOR )      i_zoom = 10 * ZOOM_FP_FACTOR;
        else if( i_zoom <  ZOOM_FP_FACTOR / 5 ) i_zoom = ZOOM_FP_FACTOR / 5;

        unsigned int i_original_width, i_original_height;

        if( p_vout->fmt_in.i_sar_num >= p_vout->fmt_in.i_sar_den )
        {
            i_original_width = p_vout->fmt_in.i_visible_width *
                               p_vout->fmt_in.i_sar_num / p_vout->fmt_in.i_sar_den;
            i_original_height =  p_vout->fmt_in.i_visible_height;
        }
        else
        {
            i_original_width =  p_vout->fmt_in.i_visible_width;
            i_original_height = p_vout->fmt_in.i_visible_height *
                                p_vout->fmt_in.i_sar_den / p_vout->fmt_in.i_sar_num;
        }
#ifdef WIN32
        /* On windows, inner video window exceeding container leads to black screen */
        *pi_width = __MIN( i_width, i_original_width * i_zoom / ZOOM_FP_FACTOR );
        *pi_height = __MIN( i_height, i_original_height * i_zoom / ZOOM_FP_FACTOR );
#else
        *pi_width = i_original_width * i_zoom / ZOOM_FP_FACTOR ;
        *pi_height = i_original_height * i_zoom / ZOOM_FP_FACTOR ;
#endif
    }

    int64_t i_scaled_width = p_vout->fmt_in.i_visible_width * (int64_t)p_vout->fmt_in.i_sar_num *
                              *pi_height / p_vout->fmt_in.i_visible_height / p_vout->fmt_in.i_sar_den;
    int64_t i_scaled_height = p_vout->fmt_in.i_visible_height * (int64_t)p_vout->fmt_in.i_sar_den *
                               *pi_width / p_vout->fmt_in.i_visible_width / p_vout->fmt_in.i_sar_num;

    if( i_scaled_width <= 0 || i_scaled_height <= 0 )
    {
        msg_Warn( p_vout, "ignoring broken aspect ratio" );
        i_scaled_width = *pi_width;
        i_scaled_height = *pi_height;
    }

    if( i_scaled_width > *pi_width )
        *pi_height = i_scaled_height;
    else
        *pi_width = i_scaled_width;

    switch( p_vout->i_alignment & VOUT_ALIGN_HMASK )
    {
    case VOUT_ALIGN_LEFT:
        *pi_x = 0;
        break;
    case VOUT_ALIGN_RIGHT:
        *pi_x = i_width - *pi_width;
        break;
    default:
        *pi_x = ( i_width - *pi_width ) / 2;
    }

    switch( p_vout->i_alignment & VOUT_ALIGN_VMASK )
    {
    case VOUT_ALIGN_TOP:
        *pi_y = 0;
        break;
    case VOUT_ALIGN_BOTTOM:
        *pi_y = i_height - *pi_height;
        break;
    default:
        *pi_y = ( i_height - *pi_height ) / 2;
    }
}

/**
 * Allocate a new picture in the heap.
 *
 * This function allocates a fake direct buffer in memory, which can be
 * used exactly like a video buffer. The video output thread then manages
 * how it gets displayed.
 */
int __vout_AllocatePicture( vlc_object_t *p_this, picture_t *p_pic,
                            vlc_fourcc_t i_chroma,
                            int i_width, int i_height,
                            int i_sar_num, int i_sar_den )
{
    VLC_UNUSED(p_this);
    int i_index, i_width_aligned, i_height_aligned;

    /* Make sure the real dimensions are a multiple of 16 */
    i_width_aligned = (i_width + 15) >> 4 << 4;
    i_height_aligned = (i_height + 15) >> 4 << 4;

    if( picture_Setup( p_pic, i_chroma, i_width, i_height,
                       i_sar_num, i_sar_den ) != VLC_SUCCESS )
    {
        p_pic->i_planes = 0;
        return VLC_EGENERIC;
    }

    /* Calculate how big the new image should be */

    /*
     * bytes = width_aligned * height_aligned * bpp / 8
     * We need to check for an integer overflow at each multiplication since
     * height & width (and bpp?) could be arbitrary large
     */

    size_t i_bytes = 0;
    /* i_width_aligned is a multiple of 16, so we can divide by 8 now */
    size_t i_width_aligned_divided = i_width_aligned / 8;
    if( i_width_aligned_divided <= (SIZE_MAX/i_height_aligned) )
    {
        size_t i_pixels_divided = i_width_aligned_divided * i_height_aligned;
        size_t i_bpp = p_pic->format.i_bits_per_pixel;
        if( i_pixels_divided <= (SIZE_MAX/i_bpp) )
        {
            i_bytes = i_pixels_divided * i_bpp;
        }
    }

    if( i_bytes == 0 )
    {
        p_pic->i_planes = 0;
        return VLC_ENOMEM;
    }

    p_pic->p_data = vlc_memalign( &p_pic->p_data_orig, 16, i_bytes );

    if( p_pic->p_data == NULL )
    {
        p_pic->i_planes = 0;
        return VLC_EGENERIC;
    }

    /* Fill the p_pixels field for each plane */
    p_pic->p[ 0 ].p_pixels = p_pic->p_data;

    for( i_index = 1; i_index < p_pic->i_planes; i_index++ )
    {
        p_pic->p[i_index].p_pixels = p_pic->p[i_index-1].p_pixels +
            p_pic->p[i_index-1].i_lines * p_pic->p[i_index-1].i_pitch;
    }

    return VLC_SUCCESS;
}

/**
 * Compare two chroma values
 *
 * This function returns 1 if the two fourcc values given as argument are
 * the same format (eg. UYVY/UYNV) or almost the same format (eg. I420/YV12)
 */
int vout_ChromaCmp( vlc_fourcc_t i_chroma, vlc_fourcc_t i_amorhc )
{
    static const vlc_fourcc_t p_I420[] = {
        VLC_CODEC_I420, VLC_CODEC_YV12, VLC_CODEC_J420, 0
    };
    static const vlc_fourcc_t p_I422[] = {
        VLC_CODEC_I422, VLC_CODEC_J422, 0
    };
    static const vlc_fourcc_t p_I440[] = {
        VLC_CODEC_I440, VLC_CODEC_J440, 0
    };
    static const vlc_fourcc_t p_I444[] = {
        VLC_CODEC_I444, VLC_CODEC_J444, 0
    };
    static const vlc_fourcc_t *pp_fcc[] = {
        p_I420, p_I422, p_I440, p_I444, NULL
    };

    /* */
    i_chroma = vlc_fourcc_GetCodec( VIDEO_ES, i_chroma );
    i_amorhc = vlc_fourcc_GetCodec( VIDEO_ES, i_amorhc );

    /* If they are the same, they are the same ! */
    if( i_chroma == i_amorhc )
        return 1;

    /* Check for equivalence classes */
    for( int i = 0; pp_fcc[i] != NULL; i++ )
    {
        bool b_fcc1 = false;
        bool b_fcc2 = false;
        for( int j = 0; pp_fcc[i][j] != 0; j++ )
        {
            if( i_chroma == pp_fcc[i][j] )
                b_fcc1 = true;
            if( i_amorhc == pp_fcc[i][j] )
                b_fcc2 = true;
        }
        if( b_fcc1 && b_fcc2 )
            return 1;
    }
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void PictureReleaseCallback( picture_t *p_picture )
{
    if( --p_picture->i_refcount > 0 )
        return;
    picture_Delete( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_Reset( picture_t *p_picture )
{
    /* */
    p_picture->date = VLC_TS_INVALID;
    p_picture->b_force = false;
    p_picture->b_progressive = false;
    p_picture->i_nb_fields = 0;
    p_picture->b_top_field_first = false;
    picture_CleanupQuant( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
int picture_Setup( picture_t *p_picture, vlc_fourcc_t i_chroma,
                   int i_width, int i_height, int i_sar_num, int i_sar_den )
{
    int i_index, i_width_aligned, i_height_aligned;

    /* Store default values */
    for( i_index = 0; i_index < VOUT_MAX_PLANES; i_index++ )
    {
        p_picture->p[i_index].p_pixels = NULL;
        p_picture->p[i_index].i_pixel_pitch = 1;
    }

    p_picture->pf_release = NULL;
    p_picture->p_release_sys = NULL;
    p_picture->pf_lock = NULL;
    p_picture->pf_unlock = NULL;
    p_picture->i_refcount = 0;

    p_picture->i_qtype = QTYPE_NONE;
    p_picture->i_qstride = 0;
    p_picture->p_q = NULL;

    video_format_Setup( &p_picture->format, i_chroma, i_width, i_height,
                        i_sar_num, i_sar_den );

    /* Make sure the real dimensions are a multiple of 16 */
    i_width_aligned = (i_width + 15) >> 4 << 4;
    i_height_aligned = (i_height + 15) >> 4 << 4;

    /* Calculate coordinates */
    switch( vlc_fourcc_GetCodec( VIDEO_ES, i_chroma ) )
    {
    case VLC_CODEC_I411:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned / 4;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width / 4;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned / 4;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width / 4;
        p_picture->i_planes = 3;
        break;

    case VLC_CODEC_I410:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned / 4;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height / 4;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned / 4;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width / 4;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned / 4;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height / 4;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned / 4;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width / 4;
        p_picture->i_planes = 3;
        break;

    case VLC_CODEC_YV12:
    case VLC_CODEC_I420:
    case VLC_CODEC_J420:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned / 2;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height / 2;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned / 2;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width / 2;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned / 2;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height / 2;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned / 2;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width / 2;
        p_picture->i_planes = 3;
        break;

    case VLC_CODEC_I422:
    case VLC_CODEC_J422:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned / 2;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width / 2;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned / 2;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width / 2;
        p_picture->i_planes = 3;
        break;

    case VLC_CODEC_I440:
    case VLC_CODEC_J440:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned / 2;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height / 2;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned / 2;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height / 2;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width;
        p_picture->i_planes = 3;
        break;

    case VLC_CODEC_I444:
    case VLC_CODEC_J444:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width;
        p_picture->i_planes = 3;
        break;

    case VLC_CODEC_YUVA:
        p_picture->p[ Y_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ Y_PLANE ].i_visible_lines = i_height;
        p_picture->p[ Y_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ Y_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ U_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ U_PLANE ].i_visible_lines = i_height;
        p_picture->p[ U_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ U_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ V_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ V_PLANE ].i_visible_lines = i_height;
        p_picture->p[ V_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ V_PLANE ].i_visible_pitch = i_width;
        p_picture->p[ A_PLANE ].i_lines = i_height_aligned;
        p_picture->p[ A_PLANE ].i_visible_lines = i_height;
        p_picture->p[ A_PLANE ].i_pitch = i_width_aligned;
        p_picture->p[ A_PLANE ].i_visible_pitch = i_width;
        p_picture->i_planes = 4;
        break;

    case VLC_CODEC_YUVP:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned;
        p_picture->p->i_visible_pitch = i_width;
        p_picture->p->i_pixel_pitch = 1;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_Y211:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned;
        p_picture->p->i_visible_pitch = i_width;
        p_picture->p->i_pixel_pitch = 4;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_UYVY:
    case VLC_CODEC_VYUY:
    case VLC_CODEC_YUYV:
    case VLC_CODEC_YVYU:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned * 2;
        p_picture->p->i_visible_pitch = i_width * 2;
        p_picture->p->i_pixel_pitch = 2;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_RGB8:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned;
        p_picture->p->i_visible_pitch = i_width;
        p_picture->p->i_pixel_pitch = 1;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_RGB15:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned * 2;
        p_picture->p->i_visible_pitch = i_width * 2;
        p_picture->p->i_pixel_pitch = 2;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_RGB16:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned * 2;
        p_picture->p->i_visible_pitch = i_width * 2;
        p_picture->p->i_pixel_pitch = 2;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_RGB24:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned * 3;
        p_picture->p->i_visible_pitch = i_width * 3;
        p_picture->p->i_pixel_pitch = 3;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_RGB32:
    case VLC_CODEC_RGBA:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned * 4;
        p_picture->p->i_visible_pitch = i_width * 4;
        p_picture->p->i_pixel_pitch = 4;
        p_picture->i_planes = 1;
        break;

    case VLC_CODEC_GREY:
    case VLC_CODEC_RGBP:
        p_picture->p->i_lines = i_height_aligned;
        p_picture->p->i_visible_lines = i_height;
        p_picture->p->i_pitch = i_width_aligned;
        p_picture->p->i_visible_pitch = i_width;
        p_picture->p->i_pixel_pitch = 1;
        p_picture->i_planes = 1;
        break;

    default:
        p_picture->i_planes = 0;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
picture_t *picture_NewFromResource( const video_format_t *p_fmt, const picture_resource_t *p_resource )
{
    video_format_t fmt = *p_fmt;

    /* It is needed to be sure all informations are filled */
    video_format_Setup( &fmt, p_fmt->i_chroma,
                              p_fmt->i_width, p_fmt->i_height,
                              p_fmt->i_sar_num, p_fmt->i_sar_den );

    /* */
    picture_t *p_picture = calloc( 1, sizeof(*p_picture) );
    if( !p_picture )
        return NULL;

    if( p_resource )
    {
        if( picture_Setup( p_picture, fmt.i_chroma, fmt.i_width, fmt.i_height,
                           fmt.i_sar_num, fmt.i_sar_den ) )
        {
            free( p_picture );
            return NULL;
        }
        p_picture->p_sys = p_resource->p_sys;

        for( int i = 0; i < p_picture->i_planes; i++ )
        {
            p_picture->p[i].p_pixels = p_resource->p[i].p_pixels;
            p_picture->p[i].i_lines  = p_resource->p[i].i_lines;
            p_picture->p[i].i_pitch  = p_resource->p[i].i_pitch;
        }
    }
    else
    {
        if( __vout_AllocatePicture( NULL, p_picture,
                                    fmt.i_chroma, fmt.i_width, fmt.i_height,
                                    fmt.i_sar_num, fmt.i_sar_den ) )
        {
            free( p_picture );
            return NULL;
        }
    }
    /* */
    p_picture->format = fmt;
    p_picture->i_refcount = 1;
    p_picture->pf_release = PictureReleaseCallback;
    p_picture->i_status = RESERVED_PICTURE;

    return p_picture;
}
picture_t *picture_NewFromFormat( const video_format_t *p_fmt )
{
    return picture_NewFromResource( p_fmt, NULL );
}
picture_t *picture_New( vlc_fourcc_t i_chroma, int i_width, int i_height, int i_sar_num, int i_sar_den )
{
    video_format_t fmt;

    memset( &fmt, 0, sizeof(fmt) );
    video_format_Setup( &fmt, i_chroma, i_width, i_height,
                        i_sar_num, i_sar_den );

    return picture_NewFromFormat( &fmt );
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_Delete( picture_t *p_picture )
{
    assert( p_picture && p_picture->i_refcount == 0 );
    assert( p_picture->p_release_sys == NULL );

    free( p_picture->p_q );
    free( p_picture->p_data_orig );
    free( p_picture->p_sys );
    free( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_CopyPixels( picture_t *p_dst, const picture_t *p_src )
{
    int i;

    for( i = 0; i < p_src->i_planes ; i++ )
        plane_CopyPixels( p_dst->p+i, p_src->p+i );
}

void plane_CopyPixels( plane_t *p_dst, const plane_t *p_src )
{
    const unsigned i_width  = __MIN( p_dst->i_visible_pitch,
                                     p_src->i_visible_pitch );
    const unsigned i_height = __MIN( p_dst->i_visible_lines,
                                     p_src->i_visible_lines );

    if( p_src->i_pitch == p_dst->i_pitch )
    {
        /* There are margins, but with the same width : perfect ! */
        vlc_memcpy( p_dst->p_pixels, p_src->p_pixels,
                    p_src->i_pitch * i_height );
    }
    else
    {
        /* We need to proceed line by line */
        uint8_t *p_in = p_src->p_pixels;
        uint8_t *p_out = p_dst->p_pixels;
        int i_line;

        assert( p_in );
        assert( p_out );

        for( i_line = i_height; i_line--; )
        {
            vlc_memcpy( p_out, p_in, i_width );
            p_in += p_src->i_pitch;
            p_out += p_dst->i_pitch;
        }
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
int picture_Export( vlc_object_t *p_obj,
                    block_t **pp_image,
                    video_format_t *p_fmt,
                    picture_t *p_picture,
                    vlc_fourcc_t i_format,
                    int i_override_width, int i_override_height )
{
    /* */
    video_format_t fmt_in = p_picture->format;
    if( fmt_in.i_sar_num <= 0 || fmt_in.i_sar_den <= 0 )
    {
        fmt_in.i_sar_num =
        fmt_in.i_sar_den = 1;
    }

    /* */
    video_format_t fmt_out;
    memset( &fmt_out, 0, sizeof(fmt_out) );
    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 1;
    fmt_out.i_chroma  = i_format;

    /* compute original width/height */
    unsigned int i_original_width;
    unsigned int i_original_height;
    if( fmt_in.i_sar_num >= fmt_in.i_sar_den )
    {
        i_original_width = fmt_in.i_width * fmt_in.i_sar_num / fmt_in.i_sar_den;
        i_original_height = fmt_in.i_height;
    }
    else
    {
        i_original_width =  fmt_in.i_width;
        i_original_height = fmt_in.i_height * fmt_in.i_sar_den / fmt_in.i_sar_num;
    }

    /* */
    fmt_out.i_width  = ( i_override_width < 0 ) ?
                       i_original_width : i_override_width;
    fmt_out.i_height = ( i_override_height < 0 ) ?
                       i_original_height : i_override_height;

    /* scale if only one direction is provided */
    if( fmt_out.i_height == 0 && fmt_out.i_width > 0 )
    {
        fmt_out.i_height = fmt_in.i_height * fmt_out.i_width
                     * fmt_in.i_sar_den / fmt_in.i_width / fmt_in.i_sar_num;
    }
    else if( fmt_out.i_width == 0 && fmt_out.i_height > 0 )
    {
        fmt_out.i_width  = fmt_in.i_width * fmt_out.i_height
                     * fmt_in.i_sar_num / fmt_in.i_height / fmt_in.i_sar_den;
    }

    image_handler_t *p_image = image_HandlerCreate( p_obj );

    block_t *p_block = image_Write( p_image, p_picture, &fmt_in, &fmt_out );

    image_HandlerDelete( p_image );

    if( !p_block )
        return VLC_EGENERIC;

    p_block->i_pts =
    p_block->i_dts = p_picture->date;

    if( p_fmt )
        *p_fmt = fmt_out;
    *pp_image = p_block;

    return VLC_SUCCESS;
}

