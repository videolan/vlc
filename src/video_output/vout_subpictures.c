/*****************************************************************************
 * vout_subpictures.c : subpicture management functions
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#include "vlc_video.h"
#include "video_output.h"
#include "vlc_filter.h"
#include "osd.h"

static void UpdateSPU        ( vout_thread_t *, vlc_object_t * );
static int  CropCallback     ( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );

static subpicture_t *spu_new_buffer( filter_t * );
static void spu_del_buffer( filter_t *, subpicture_t * );

/**
 * Initialise the subpicture decoder unit
 *
 * \param p_vout the vout in which to create the subpicture unit
 */
void vout_InitSPU( vout_thread_t *p_vout )
{
    vlc_object_t *p_input;
    int i_index;

    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++)
    {
        p_vout->p_subpicture[i_index].i_status = FREE_SUBPICTURE;
        p_vout->p_subpicture[i_index].i_type   = EMPTY_SUBPICTURE;
    }

    /* Load the text rendering module */
    p_vout->p_text = vlc_object_create( p_vout, sizeof(filter_t) );
    p_vout->p_text->pf_spu_buffer_new = spu_new_buffer;
    p_vout->p_text->pf_spu_buffer_del = spu_del_buffer;
    p_vout->p_text->p_owner = (filter_owner_sys_t *)p_vout;
    vlc_object_attach( p_vout->p_text, p_vout );
    p_vout->p_text->p_module =
        module_Need( p_vout->p_text, "text renderer", 0, 0 );
    if( p_vout->p_text->p_module == NULL )
    {
        msg_Warn( p_vout, "no suitable text renderer module" );
        vlc_object_detach( p_vout->p_text );
        vlc_object_destroy( p_vout->p_text );
        p_vout->p_text = NULL;
    }

    p_vout->p_blend = NULL;

    p_vout->i_crop_x = p_vout->i_crop_y =
        p_vout->i_crop_width = p_vout->i_crop_height = 0;
    p_vout->b_force_alpha = VLC_FALSE;
    p_vout->b_force_crop = VLC_FALSE;

    /* Create callback */
    p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT, FIND_PARENT );
    if( p_input )
    {
        UpdateSPU( p_vout, VLC_OBJECT(p_input) );
        var_AddCallback( p_input, "highlight", CropCallback, p_vout );
        vlc_object_release( p_input );
    }
}

/**
 * Destroy the subpicture decoder unit
 *
 * \param p_vout the vout in which to destroy the subpicture unit
 */
void vout_DestroySPU( vout_thread_t *p_vout )
{
    vlc_object_t *p_input;
    int i_index;

    /* Destroy all remaining subpictures */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_vout->p_subpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            vout_DestroySubPicture( p_vout,
                                    &p_vout->p_subpicture[i_index] );
        }
    }

    if( p_vout->p_text )
    {
        if( p_vout->p_text->p_module )
            module_Unneed( p_vout->p_text, p_vout->p_text->p_module );

        vlc_object_detach( p_vout->p_text );
        vlc_object_destroy( p_vout->p_text );
    }

    if( p_vout->p_blend )
    {
        if( p_vout->p_blend->p_module )
            module_Unneed( p_vout->p_blend, p_vout->p_blend->p_module );

        vlc_object_detach( p_vout->p_blend );
        vlc_object_destroy( p_vout->p_blend );
    }

    /* Delete callback */
    p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT, FIND_PARENT );
    if( p_input )
    {
        var_DelCallback( p_input, "highlight", CropCallback, p_vout );
        vlc_object_release( p_input );
    }
}

/**
 * Create a subpicture region
 *
 * \param p_this vlc_object_t
 * \param p_fmt the format that this subpicture region should have
 */
subpicture_region_t *__spu_CreateRegion( vlc_object_t *p_this,
                                         video_format_t *p_fmt )
{
    subpicture_region_t *p_region = malloc( sizeof(subpicture_region_t) );
    memset( p_region, 0, sizeof(subpicture_region_t) );
    p_region->p_next = 0;
    p_region->fmt = *p_fmt;

    vout_AllocatePicture( p_this, &p_region->picture, p_fmt->i_chroma,
                          p_fmt->i_width, p_fmt->i_height, p_fmt->i_aspect );

    if( !p_region->picture.i_planes )
    {
        free( p_region );
        return NULL;
    }

    if( p_fmt->i_chroma == VLC_FOURCC('Y','U','V','P') )
        p_fmt->p_palette = p_region->fmt.p_palette =
            malloc( sizeof(video_palette_t) );
    else p_fmt->p_palette = p_region->fmt.p_palette = NULL;

    return p_region;
}

/**
 * Destroy a subpicture region
 *
 * \param p_this vlc_object_t
 * \param p_region the subpicture region to destroy
 */
void __spu_DestroyRegion( vlc_object_t *p_this, subpicture_region_t *p_region )
{
    if( !p_region ) return;
    if( p_region->picture.p_data_orig ) free( p_region->picture.p_data_orig );
    if( p_region->fmt.p_palette ) free( p_region->fmt.p_palette );
    free( p_region );
}

/**
 * Display a subpicture unit
 *
 * Remove the reservation flag of a subpicture, which will cause it to be
 * ready for display.
 * \param p_vout the video output this subpicture should be displayed on
 * \param p_subpic the subpicture to display
 */
void vout_DisplaySubPicture( vout_thread_t *p_vout, subpicture_t *p_subpic )
{
    int         i_margin;

    /* Check if status is valid */
    if( p_subpic->i_status != RESERVED_SUBPICTURE )
    {
        msg_Err( p_vout, "subpicture %p has invalid status #%d",
                         p_subpic, p_subpic->i_status );
    }

    /* If the user requested an SPU margin, we force the position after
     * having checked that it was a valid value. */
    i_margin = config_GetInt( p_vout, "spumargin" );

    if( i_margin >= 0 )
    {
        if( p_subpic->i_height + (unsigned int)i_margin
                                                 <= p_vout->output.i_height )
        {
            p_subpic->i_y = p_vout->output.i_height
                             - i_margin - p_subpic->i_height;
        }
    }

    /* Remove reservation flag */
    p_subpic->i_status = READY_SUBPICTURE;
}

/**
 * Allocate a subpicture in the video output heap.
 *
 * This function create a reserved subpicture in the video output heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the spu data fields. It needs locking
 * since several pictures can be created by several producers threads.
 * \param p_vout the vout in which to create the subpicture
 * \param i_channel the channel this subpicture should belong to
 * \param i_type the type of the subpicture
 * \return NULL on error, a reserved subpicture otherwise
 */
subpicture_t *vout_CreateSubPicture( vout_thread_t *p_vout, int i_channel,
                                     int i_type )
{
    int                 i_subpic;                        /* subpicture index */
    subpicture_t *      p_subpic = NULL;            /* first free subpicture */

    /* Clear the default channel before writing into it */
    if( i_channel == DEFAULT_CHAN )
    {
        vout_ClearOSDChannel( p_vout, DEFAULT_CHAN );
    }

    /* Get lock */
    vlc_mutex_lock( &p_vout->subpicture_lock );

    /*
     * Look for an empty place
     */
    p_subpic = NULL;
    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        if( p_vout->p_subpicture[i_subpic].i_status == FREE_SUBPICTURE )
        {
            /* Subpicture is empty and ready for allocation */
            p_subpic = &p_vout->p_subpicture[i_subpic];
            p_vout->p_subpicture[i_subpic].i_status = RESERVED_SUBPICTURE;
            break;
        }
    }

    /* If no free subpicture could be found */
    if( p_subpic == NULL )
    {
        msg_Err( p_vout, "subpicture heap is full" );
        vlc_mutex_unlock( &p_vout->subpicture_lock );
        return NULL;
    }

    /* Copy subpicture information, set some default values */
    p_subpic->i_channel = i_channel;
    p_subpic->i_type    = i_type;
    p_subpic->i_status  = RESERVED_SUBPICTURE;

    p_subpic->i_start   = 0;
    p_subpic->i_stop    = 0;
    p_subpic->b_ephemer = VLC_FALSE;

    p_subpic->i_x       = 0;
    p_subpic->i_y       = 0;
    p_subpic->i_width   = 0;
    p_subpic->i_height  = 0;
    p_subpic->b_absolute= VLC_TRUE;
    p_subpic->i_flags   = 0;
    p_subpic->pf_render = 0;
    p_subpic->pf_destroy= 0;
    p_subpic->p_sys     = 0;

    /* Remain last subpicture displayed in DEFAULT_CHAN */
    if( i_channel == DEFAULT_CHAN )
    {
        p_vout->p_default_channel = p_subpic;
    }

    vlc_mutex_unlock( &p_vout->subpicture_lock );

    p_subpic->pf_create_region = __spu_CreateRegion;
    p_subpic->pf_destroy_region = __spu_DestroyRegion;

    return p_subpic;
}

/**
 * Remove a subpicture from the heap
 *
 * This function frees a previously reserved subpicture.
 * It is meant to be used when the construction of a picture aborted.
 * This function does not need locking since reserved subpictures are ignored
 * by the output thread.
 */
void vout_DestroySubPicture( vout_thread_t *p_vout, subpicture_t *p_subpic )
{
    /* Get lock */
    vlc_mutex_lock( &p_vout->subpicture_lock );

    /* There can be race conditions so we need to check the status */
    if( p_subpic->i_status == FREE_SUBPICTURE )
    {
        vlc_mutex_unlock( &p_vout->subpicture_lock );
        return;
    }

    /* Check if status is valid */
    if( ( p_subpic->i_status != RESERVED_SUBPICTURE )
           && ( p_subpic->i_status != READY_SUBPICTURE ) )
    {
        msg_Err( p_vout, "subpicture %p has invalid status %d",
                         p_subpic, p_subpic->i_status );
    }

    while( p_subpic->p_region )
    {
        subpicture_region_t *p_region = p_subpic->p_region;
        p_subpic->p_region = p_region->p_next;
        spu_DestroyRegion( p_vout, p_region );
    }

    if( p_subpic->pf_destroy )
    {
        p_subpic->pf_destroy( p_subpic );
    }

    p_subpic->i_status = FREE_SUBPICTURE;

    vlc_mutex_unlock( &p_vout->subpicture_lock );
}

/*****************************************************************************
 * vout_RenderSubPictures: render a subpicture list
 *****************************************************************************
 * This function renders all sub picture units in the list.
 *****************************************************************************/
void vout_RenderSubPictures( vout_thread_t *p_vout, picture_t *p_pic,
                             subpicture_t *p_subpic )
{
    /* Load the blending module */
    if( !p_vout->p_blend && p_subpic && p_subpic->p_region )
    {
        p_vout->p_blend = vlc_object_create( p_vout, sizeof(filter_t) );
        vlc_object_attach( p_vout->p_blend, p_vout );
        p_vout->p_blend->fmt_out.video.i_width =
            p_vout->p_blend->fmt_out.video.i_visible_width =
                p_vout->render.i_width;
        p_vout->p_blend->fmt_out.video.i_height =
            p_vout->p_blend->fmt_out.video.i_visible_height =
                p_vout->render.i_height;
        p_vout->p_blend->fmt_out.video.i_x_offset =
            p_vout->p_blend->fmt_out.video.i_y_offset = 0;
        p_vout->p_blend->fmt_out.video.i_aspect =
            p_vout->render.i_aspect;
        p_vout->p_blend->fmt_out.video.i_chroma =
            p_vout->output.i_chroma;

        p_vout->p_blend->fmt_in.video = p_subpic->p_region->fmt;

        p_vout->p_blend->p_module =
            module_Need( p_vout->p_blend, "video blending", 0, 0 );
    }

    /* Get lock */
    vlc_mutex_lock( &p_vout->subpicture_lock );

    /* Check i_status again to make sure spudec hasn't destroyed the subpic */
    while( p_subpic != NULL && p_subpic->i_status != FREE_SUBPICTURE )
    {
        subpicture_region_t *p_region = p_subpic->p_region;

        if( p_subpic->pf_render )
        {
            p_subpic->pf_render( p_vout, p_pic, p_subpic );
        }
        else while( p_region && p_vout->p_blend &&
                    p_vout->p_blend->pf_video_blend )
        {
            int i_x_offset = p_region->i_x + p_subpic->i_x;
            int i_y_offset = p_region->i_y + p_subpic->i_y;

            if( p_subpic->i_flags & OSD_ALIGN_BOTTOM )
            {
                i_y_offset += p_vout->output.i_height - p_region->fmt.i_height;
            }
            else if ( !(p_subpic->i_flags & OSD_ALIGN_TOP) )
            {
                i_y_offset += p_vout->output.i_height / 2 -
                    p_region->fmt.i_height / 2;
            }

            if( p_subpic->i_flags & OSD_ALIGN_RIGHT )
            {
                i_x_offset += p_vout->output.i_width - p_region->fmt.i_width;
            }
            else if ( !(p_subpic->i_flags & OSD_ALIGN_LEFT) )
            {
                i_x_offset += p_vout->output.i_width / 2 -
                    p_region->fmt.i_width / 2;
            }

            if( p_subpic->b_absolute )
            {
                i_x_offset = p_region->i_x + p_subpic->i_x;
                i_y_offset = p_region->i_y + p_subpic->i_y;
            }

            p_vout->p_blend->fmt_in.video = p_region->fmt;

            /* Force cropping if requested */
            if( p_vout->b_force_crop )
            {
                p_vout->p_blend->fmt_in.video.i_x_offset = p_vout->i_crop_x;
                p_vout->p_blend->fmt_in.video.i_y_offset = p_vout->i_crop_y;
                p_vout->p_blend->fmt_in.video.i_visible_width =
                    p_vout->i_crop_width;
                p_vout->p_blend->fmt_in.video.i_visible_height =
                    p_vout->i_crop_height;
                i_x_offset += p_vout->i_crop_x;
                i_y_offset += p_vout->i_crop_y;
            }

            /* Force palette if requested */
            if( p_vout->b_force_alpha && VLC_FOURCC('Y','U','V','P') ==
                p_vout->p_blend->fmt_in.video.i_chroma )
            {
                p_vout->p_blend->fmt_in.video.p_palette->palette[0][3] =
                    p_vout->pi_alpha[0];
                p_vout->p_blend->fmt_in.video.p_palette->palette[1][3] =
                    p_vout->pi_alpha[1];
                p_vout->p_blend->fmt_in.video.p_palette->palette[2][3] =
                    p_vout->pi_alpha[2];
                p_vout->p_blend->fmt_in.video.p_palette->palette[3][3] =
                    p_vout->pi_alpha[3];
            }

            p_vout->p_blend->pf_video_blend( p_vout->p_blend,
                p_pic, p_pic, &p_region->picture, i_x_offset, i_y_offset );

            p_region = p_region->p_next;
        }

        p_subpic = p_subpic->p_next;
    }

    vlc_mutex_unlock( &p_vout->subpicture_lock );
}

/*****************************************************************************
 * vout_SortSubPictures: find the subpictures to display
 *****************************************************************************
 * This function parses all subpictures and decides which ones need to be
 * displayed. This operation does not need lock, since only READY_SUBPICTURE
 * are handled. If no picture has been selected, display_date will depend on
 * the subpicture.
 * We also check for ephemer DVD subpictures (subpictures that have
 * to be removed if a newer one is available), which makes it a lot
 * more difficult to guess if a subpicture has to be rendered or not.
 *****************************************************************************/
subpicture_t *vout_SortSubPictures( vout_thread_t *p_vout,
                                    mtime_t display_date )
{
    int i_index;
    subpicture_t *p_subpic     = NULL;
    subpicture_t *p_ephemer    = NULL;
    mtime_t       ephemer_date = 0;

    /* We get an easily parsable chained list of subpictures which
     * ends with NULL since p_subpic was initialized to NULL. */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_vout->p_subpicture[i_index].i_status == READY_SUBPICTURE )
        {
            /* If it is a DVD subpicture, check its date */
            if( p_vout->p_subpicture[i_index].i_type == MEMORY_SUBPICTURE )
            {
                if( !p_vout->p_subpicture[i_index].b_ephemer
                     && display_date > p_vout->p_subpicture[i_index].i_stop )
                {
                    /* Too late, destroy the subpic */
                    vout_DestroySubPicture( p_vout,
                                    &p_vout->p_subpicture[i_index] );
                    continue;
                }

                if( display_date
                     && display_date < p_vout->p_subpicture[i_index].i_start )
                {
                    /* Too early, come back next monday */
                    continue;
                }

                /* If this is an ephemer subpic, see if it's the
                 * youngest we have */
                if( p_vout->p_subpicture[i_index].b_ephemer )
                {
                    if( p_ephemer == NULL )
                    {
                        p_ephemer = &p_vout->p_subpicture[i_index];
                        continue;
                    }

                    if( p_vout->p_subpicture[i_index].i_start
                                                     < p_ephemer->i_start )
                    {
                        /* Link the previous ephemer subpicture and
                         * replace it with the current one */
                        p_ephemer->p_next = p_subpic;
                        p_subpic = p_ephemer;
                        p_ephemer = &p_vout->p_subpicture[i_index];

                        /* If it's the 2nd youngest subpicture,
                         * register its date */
                        if( !ephemer_date
                              || ephemer_date > p_subpic->i_start )
                        {
                            ephemer_date = p_subpic->i_start;
                        }

                        continue;
                    }
                }

                p_vout->p_subpicture[i_index].p_next = p_subpic;
                p_subpic = &p_vout->p_subpicture[i_index];

                /* If it's the 2nd youngest subpicture, register its date */
                if( !ephemer_date || ephemer_date > p_subpic->i_start )
                {
                    ephemer_date = p_subpic->i_start;
                }
            }
            /* If it's not a DVD subpicture, just register it */
            else
            {
                p_vout->p_subpicture[i_index].p_next = p_subpic;
                p_subpic = &p_vout->p_subpicture[i_index];
            }
        }
    }

    /* If we found an ephemer subpicture, check if it has to be
     * displayed */
    if( p_ephemer != NULL )
    {
        if( p_ephemer->i_start < ephemer_date )
        {
            /* Ephemer subpicture has lived too long */
            vout_DestroySubPicture( p_vout, p_ephemer );
        }
        else
        {
            /* Ephemer subpicture can still live a bit */
            p_ephemer->p_next = p_subpic;
            return p_ephemer;
        }
    }

    return p_subpic;
}

/*****************************************************************************
 * vout_RegisterOSDChannel: register an OSD channel
 *****************************************************************************
 * This function affects an ID to an OSD channel
 *****************************************************************************/
int vout_RegisterOSDChannel( vout_thread_t *p_vout )
{
    msg_Dbg( p_vout, "Registering OSD channel, ID: %i",
             p_vout->i_channel_count + 1 );
    return ++p_vout->i_channel_count;
}

/*****************************************************************************
 * vout_ClearOSDChannel: clear an OSD channel
 *****************************************************************************
 * This function destroys the subpictures which belong to the OSD channel
 * corresponding to i_channel_id.
 *****************************************************************************/
void vout_ClearOSDChannel( vout_thread_t *p_vout, int i_channel )
{
    int                 i_subpic;                        /* subpicture index */
    subpicture_t *      p_subpic = NULL;            /* first free subpicture */

    if( i_channel == DEFAULT_CHAN )
    {
        if( p_vout->p_default_channel != NULL )
        {
            vout_DestroySubPicture( p_vout, p_vout->p_default_channel );
        }
        p_vout->p_default_channel = NULL;
        return;
    }

    vlc_mutex_lock( &p_vout->subpicture_lock );

    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        p_subpic = &p_vout->p_subpicture[i_subpic];
        if( p_subpic->i_status == FREE_SUBPICTURE
            || ( p_subpic->i_status != RESERVED_SUBPICTURE
                 && p_subpic->i_status != READY_SUBPICTURE ) )
        {
            continue;
        }

        if( p_subpic->i_channel == i_channel )
        {
            while( p_subpic->p_region )
            {
                subpicture_region_t *p_region = p_subpic->p_region;
                p_subpic->p_region = p_region->p_next;
                spu_DestroyRegion( p_vout, p_region );
            }

            if( p_subpic->pf_destroy )
            {
                p_subpic->pf_destroy( p_subpic );
            }
            p_subpic->i_status = FREE_SUBPICTURE;
        }
    }

    vlc_mutex_unlock( &p_vout->subpicture_lock );
}

/*****************************************************************************
 * UpdateSPU: update subpicture settings
 *****************************************************************************
 * This function is called from CropCallback and at initialization time, to
 * retrieve crop information from the input.
 *****************************************************************************/
static void UpdateSPU( vout_thread_t *p_vout, vlc_object_t *p_object )
{
    vlc_value_t val;

    p_vout->b_force_alpha = VLC_FALSE;
    p_vout->b_force_crop = VLC_FALSE;

    if( var_Get( p_object, "highlight", &val ) || !val.b_bool ) return;

    p_vout->b_force_crop = VLC_TRUE;
    var_Get( p_object, "x-start", &val );
    p_vout->i_crop_x = val.i_int;
    var_Get( p_object, "y-start", &val );
    p_vout->i_crop_y = val.i_int;
    var_Get( p_object, "x-end", &val );
    p_vout->i_crop_width = val.i_int - p_vout->i_crop_x;
    var_Get( p_object, "y-end", &val );
    p_vout->i_crop_height = val.i_int - p_vout->i_crop_y;

#if 0
    if( var_Get( p_object, "color", &val ) == VLC_SUCCESS )
    {
        int i;
        for( i = 0; i < 4; i++ )
        {
            p_vout->pi_color[i] = ((uint8_t *)val.p_address)[i];
        }
    }
#endif

    if( var_Get( p_object, "contrast", &val ) == VLC_SUCCESS )
    {
        int i;
        for( i = 0; i < 4; i++ )
        {
            p_vout->pi_alpha[i] = ((uint8_t *)val.p_address)[i];
            p_vout->pi_alpha[i] = p_vout->pi_alpha[i] == 0xf ?
                0xff : p_vout->pi_alpha[i] << 4;
        }
        p_vout->b_force_alpha = VLC_TRUE;
    }

    msg_Dbg( p_vout, "crop: %i,%i,%i,%i, alpha: %i",
             p_vout->i_crop_x, p_vout->i_crop_y,
             p_vout->i_crop_width, p_vout->i_crop_height,
             p_vout->b_force_alpha );
}

/*****************************************************************************
 * CropCallback: called when the highlight properties are changed
 *****************************************************************************
 * This callback is called from the input thread when we need cropping
 *****************************************************************************/
static int CropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    UpdateSPU( (vout_thread_t *)p_data, p_object );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Buffers allocation callbacks for the filters
 *****************************************************************************/
static subpicture_t *spu_new_buffer( filter_t *p_filter )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_filter->p_owner;
    subpicture_t *p_spu;

    p_spu = vout_CreateSubPicture( p_vout, !DEFAULT_CHAN, MEMORY_SUBPICTURE );
    return p_spu;
}

static void spu_del_buffer( filter_t *p_filter, subpicture_t *p_spu )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_filter->p_owner;
    vout_DestroySubPicture( p_vout, p_spu );
}
