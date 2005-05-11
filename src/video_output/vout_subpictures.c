/*****************************************************************************
 * vout_subpictures.c : subpicture management functions
 *****************************************************************************
 * Copyright (C) 2000-2005 VideoLAN
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

#include "vlc_block.h"
#include "vlc_video.h"
#include "video_output.h"
#include "vlc_spu.h"
#include "vlc_filter.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void UpdateSPU   ( spu_t *, vlc_object_t * );
static int  CropCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );

static int spu_vaControlDefault( spu_t *, int, va_list );

static subpicture_t *sub_new_buffer( filter_t * );
static void sub_del_buffer( filter_t *, subpicture_t * );
static subpicture_t *spu_new_buffer( filter_t * );
static void spu_del_buffer( filter_t *, subpicture_t * );
static picture_t *spu_new_video_buffer( filter_t * );
static void spu_del_video_buffer( filter_t *, picture_t * );

struct filter_owner_sys_t
{
    spu_t *p_spu;
    int i_channel;
};

/**
 * Creates the subpicture unit
 *
 * \param p_this the parent object which creates the subpicture unit
 */
spu_t *__spu_Create( vlc_object_t *p_this )
{
    int i_index;
    spu_t *p_spu = vlc_object_create( p_this, VLC_OBJECT_SPU );

    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++)
    {
        p_spu->p_subpicture[i_index].i_status = FREE_SUBPICTURE;
    }

    p_spu->p_blend = NULL;
    p_spu->p_text = NULL;
    p_spu->p_scale = NULL;
    p_spu->i_filter = 0;
    p_spu->pf_control = spu_vaControlDefault;

    /* Register the default subpicture channel */
    p_spu->i_channel = 2;

    vlc_mutex_init( p_this, &p_spu->subpicture_lock );

    vlc_object_attach( p_spu, p_this );

    return p_spu;
}

/**
 * Initialise the subpicture unit
 *
 * \param p_spu the subpicture unit object
 */
int spu_Init( spu_t *p_spu )
{
    char *psz_filter, *psz_filter_orig;
    vlc_value_t val;

    /* If the user requested a sub margin, we force the position. */
    var_Create( p_spu, "sub-margin", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_spu, "sub-margin", &val );
    p_spu->i_margin = val.i_int;

    var_Create( p_spu, "sub-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_spu, "sub-filter", &val );
    psz_filter = psz_filter_orig = val.psz_string;
    while( psz_filter && *psz_filter )
    {
        char *psz_parser = strchr( psz_filter, ':' );

        if( psz_parser ) *psz_parser++ = 0;

        p_spu->pp_filter[p_spu->i_filter] =
            vlc_object_create( p_spu, VLC_OBJECT_FILTER );
        vlc_object_attach( p_spu->pp_filter[p_spu->i_filter], p_spu );
        p_spu->pp_filter[p_spu->i_filter]->pf_sub_buffer_new = sub_new_buffer;
        p_spu->pp_filter[p_spu->i_filter]->pf_sub_buffer_del = sub_del_buffer;
        p_spu->pp_filter[p_spu->i_filter]->p_module =
            module_Need( p_spu->pp_filter[p_spu->i_filter],
                         "sub filter", psz_filter, 0 );
        if( p_spu->pp_filter[p_spu->i_filter]->p_module )
        {
            filter_owner_sys_t *p_sys = malloc( sizeof(filter_owner_sys_t) );
            p_spu->pp_filter[p_spu->i_filter]->p_owner = p_sys;
            spu_Control( p_spu, SPU_CHANNEL_REGISTER, &p_sys->i_channel );
            p_sys->p_spu = p_spu;
            p_spu->i_filter++;
        }
        else
        {
            msg_Dbg( p_spu, "no sub filter found" );
            vlc_object_detach( p_spu->pp_filter[p_spu->i_filter] );
            vlc_object_destroy( p_spu->pp_filter[p_spu->i_filter] );
        }

        if( p_spu->i_filter >= 10 )
        {
            msg_Dbg( p_spu, "can't add anymore filters" );
        }

        psz_filter = psz_parser;
    }
    if( psz_filter_orig ) free( psz_filter_orig );

    return VLC_EGENERIC;
}

/**
 * Destroy the subpicture unit
 *
 * \param p_this the parent object which destroys the subpicture unit
 */
void spu_Destroy( spu_t *p_spu )
{
    int i_index;

    vlc_object_detach( p_spu );

    /* Destroy all remaining subpictures */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_spu->p_subpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            spu_DestroySubpicture( p_spu, &p_spu->p_subpicture[i_index] );
        }
    }

    if( p_spu->p_blend )
    {
        if( p_spu->p_blend->p_module )
            module_Unneed( p_spu->p_blend, p_spu->p_blend->p_module );

        vlc_object_detach( p_spu->p_blend );
        vlc_object_destroy( p_spu->p_blend );
    }

    if( p_spu->p_text )
    {
        if( p_spu->p_text->p_module )
            module_Unneed( p_spu->p_text, p_spu->p_text->p_module );

        vlc_object_detach( p_spu->p_text );
        vlc_object_destroy( p_spu->p_text );
    }

    if( p_spu->p_scale )
    {
        if( p_spu->p_scale->p_module )
            module_Unneed( p_spu->p_scale, p_spu->p_scale->p_module );

        vlc_object_detach( p_spu->p_scale );
        vlc_object_destroy( p_spu->p_scale );
    }

    while( p_spu->i_filter-- )
    {
        module_Unneed( p_spu->pp_filter[p_spu->i_filter],
                       p_spu->pp_filter[p_spu->i_filter]->p_module );
        free( p_spu->pp_filter[p_spu->i_filter]->p_owner );
        vlc_object_detach( p_spu->pp_filter[p_spu->i_filter] );
        vlc_object_destroy( p_spu->pp_filter[p_spu->i_filter] );
    }

    vlc_mutex_destroy( &p_spu->subpicture_lock );
    vlc_object_destroy( p_spu );
}

/**
 * Attach/Detach the SPU from any input
 *
 * \param p_this the object in which to destroy the subpicture unit
 * \param b_attach to select attach or detach
 */
void spu_Attach( spu_t *p_spu, vlc_object_t *p_this, vlc_bool_t b_attach )
{
    vlc_object_t *p_input;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );
    if( !p_input ) return;

    if( b_attach )
    {
        UpdateSPU( p_spu, VLC_OBJECT(p_input) );
        var_AddCallback( p_input, "highlight", CropCallback, p_spu );
        vlc_object_release( p_input );
    }
    else
    {
        /* Delete callback */
        var_DelCallback( p_input, "highlight", CropCallback, p_spu );
        vlc_object_release( p_input );
    }
}

/**
 * Create a subpicture region
 *
 * \param p_this vlc_object_t
 * \param p_fmt the format that this subpicture region should have
 */
static void RegionPictureRelease( picture_t *p_pic )
{
    if( p_pic->p_data_orig ) free( p_pic->p_data_orig );
}
subpicture_region_t *__spu_CreateRegion( vlc_object_t *p_this,
                                         video_format_t *p_fmt )
{
    subpicture_region_t *p_region = malloc( sizeof(subpicture_region_t) );
    memset( p_region, 0, sizeof(subpicture_region_t) );
    p_region->p_next = 0;
    p_region->p_cache = 0;
    p_region->fmt = *p_fmt;
    p_region->psz_text = 0;
    p_region->i_text_color = 0xFFFFFF;

    if( p_fmt->i_chroma == VLC_FOURCC('Y','U','V','P') )
        p_fmt->p_palette = p_region->fmt.p_palette =
            malloc( sizeof(video_palette_t) );
    else p_fmt->p_palette = p_region->fmt.p_palette = NULL;

    p_region->picture.p_data_orig = 0;

    if( p_fmt->i_chroma == VLC_FOURCC('T','E','X','T') ) return p_region;

    vout_AllocatePicture( p_this, &p_region->picture, p_fmt->i_chroma,
                          p_fmt->i_width, p_fmt->i_height, p_fmt->i_aspect );

    if( !p_region->picture.i_planes )
    {
        free( p_region );
        free( p_fmt->p_palette );
        return NULL;
    }

    p_region->picture.pf_release = RegionPictureRelease;

    return p_region;
}

/**
 * Make a subpicture region from an existing picture_t
 *
 * \param p_this vlc_object_t
 * \param p_fmt the format that this subpicture region should have
 * \param p_pic a pointer to the picture creating the region (not freed)
 */
subpicture_region_t *__spu_MakeRegion( vlc_object_t *p_this,
                                       video_format_t *p_fmt,
                                       picture_t *p_pic )
{
    subpicture_region_t *p_region = malloc( sizeof(subpicture_region_t) );
    memset( p_region, 0, sizeof(subpicture_region_t) );
    p_region->p_next = 0;
    p_region->p_cache = 0;
    p_region->fmt = *p_fmt;
    p_region->psz_text = 0;
    p_region->i_text_color = 0xFFFFFF;

    if( p_fmt->i_chroma == VLC_FOURCC('Y','U','V','P') )
        p_fmt->p_palette = p_region->fmt.p_palette =
            malloc( sizeof(video_palette_t) );
    else p_fmt->p_palette = p_region->fmt.p_palette = NULL;

    memcpy( &p_region->picture, p_pic, sizeof(picture_t) );
    p_region->picture.pf_release = RegionPictureRelease;

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
    if( p_region->picture.pf_release )
        p_region->picture.pf_release( &p_region->picture );
    if( p_region->fmt.p_palette ) free( p_region->fmt.p_palette );
    if( p_region->psz_text ) free( p_region->psz_text );
    if( p_region->p_cache ) __spu_DestroyRegion( p_this, p_region->p_cache );
    free( p_region );
}

/**
 * Display a subpicture
 *
 * Remove the reservation flag of a subpicture, which will cause it to be
 * ready for display.
 * \param p_spu the subpicture unit object
 * \param p_subpic the subpicture to display
 */
void spu_DisplaySubpicture( spu_t *p_spu, subpicture_t *p_subpic )
{
    /* Check if status is valid */
    if( p_subpic->i_status != RESERVED_SUBPICTURE )
    {
        msg_Err( p_spu, "subpicture %p has invalid status #%d",
                 p_subpic, p_subpic->i_status );
    }

    /* Remove reservation flag */
    p_subpic->i_status = READY_SUBPICTURE;

    if( p_subpic->i_channel == DEFAULT_CHAN )
    {
        p_subpic->i_channel = 0xFFFF;
        spu_Control( p_spu, SPU_CHANNEL_CLEAR, DEFAULT_CHAN );
        p_subpic->i_channel = DEFAULT_CHAN;
    }
}

/**
 * Allocate a subpicture in the spu heap.
 *
 * This function create a reserved subpicture in the spu heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the spu data fields. It needs locking
 * since several pictures can be created by several producers threads.
 * \param p_spu the subpicture unit in which to create the subpicture
 * \return NULL on error, a reserved subpicture otherwise
 */
subpicture_t *spu_CreateSubpicture( spu_t *p_spu )
{
    int                 i_subpic;                        /* subpicture index */
    subpicture_t *      p_subpic = NULL;            /* first free subpicture */

    /* Get lock */
    vlc_mutex_lock( &p_spu->subpicture_lock );

    /*
     * Look for an empty place
     */
    p_subpic = NULL;
    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        if( p_spu->p_subpicture[i_subpic].i_status == FREE_SUBPICTURE )
        {
            /* Subpicture is empty and ready for allocation */
            p_subpic = &p_spu->p_subpicture[i_subpic];
            p_spu->p_subpicture[i_subpic].i_status = RESERVED_SUBPICTURE;
            break;
        }
    }

    /* If no free subpicture could be found */
    if( p_subpic == NULL )
    {
        msg_Err( p_spu, "subpicture heap is full" );
        vlc_mutex_unlock( &p_spu->subpicture_lock );
        return NULL;
    }

    /* Copy subpicture information, set some default values */
    memset( p_subpic, 0, sizeof(subpicture_t) );
    p_subpic->i_status   = RESERVED_SUBPICTURE;
    p_subpic->b_absolute = VLC_TRUE;
    p_subpic->b_fade     = VLC_FALSE;
    p_subpic->i_alpha    = 0xFF;
    p_subpic->p_region   = 0;
    p_subpic->pf_render  = 0;
    p_subpic->pf_destroy = 0;
    p_subpic->p_sys      = 0;
    vlc_mutex_unlock( &p_spu->subpicture_lock );

    p_subpic->pf_create_region = __spu_CreateRegion;
    p_subpic->pf_make_region = __spu_MakeRegion;
    p_subpic->pf_destroy_region = __spu_DestroyRegion;
    
    return p_subpic;
}

/**
 * Remove a subpicture from the heap
 *
 * This function frees a previously reserved subpicture.
 * It is meant to be used when the construction of a picture aborted.
 * This function does not need locking since reserved subpictures are ignored
 * by the spu.
 */
void spu_DestroySubpicture( spu_t *p_spu, subpicture_t *p_subpic )
{
    /* Get lock */
    vlc_mutex_lock( &p_spu->subpicture_lock );

    /* There can be race conditions so we need to check the status */
    if( p_subpic->i_status == FREE_SUBPICTURE )
    {
        vlc_mutex_unlock( &p_spu->subpicture_lock );
        return;
    }

    /* Check if status is valid */
    if( ( p_subpic->i_status != RESERVED_SUBPICTURE )
           && ( p_subpic->i_status != READY_SUBPICTURE ) )
    {
        msg_Err( p_spu, "subpicture %p has invalid status %d",
                         p_subpic, p_subpic->i_status );
    }

    while( p_subpic->p_region )
    {
        subpicture_region_t *p_region = p_subpic->p_region;
        p_subpic->p_region = p_region->p_next;
        spu_DestroyRegion( p_spu, p_region );
    }

    if( p_subpic->pf_destroy )
    {
        p_subpic->pf_destroy( p_subpic );
    }

    p_subpic->i_status = FREE_SUBPICTURE;

    vlc_mutex_unlock( &p_spu->subpicture_lock );
}

/*****************************************************************************
 * spu_RenderSubpictures: render a subpicture list
 *****************************************************************************
 * This function renders all sub picture units in the list.
 *****************************************************************************/
void spu_RenderSubpictures( spu_t *p_spu, video_format_t *p_fmt,
                            picture_t *p_pic_dst, picture_t *p_pic_src,
                            subpicture_t *p_subpic,
                            int i_scale_width_orig, int i_scale_height_orig )
{
    /* Get lock */
    vlc_mutex_lock( &p_spu->subpicture_lock );

    /* Check i_status again to make sure spudec hasn't destroyed the subpic */
    while( p_subpic != NULL && p_subpic->i_status != FREE_SUBPICTURE )
    {
        subpicture_region_t *p_region = p_subpic->p_region;
        int i_scale_width, i_scale_height;
        int i_subpic_x = p_subpic->i_x;

        /* Load the blending module */
        if( !p_spu->p_blend && p_region )
        {
            p_spu->p_blend = vlc_object_create( p_spu, VLC_OBJECT_FILTER );
            vlc_object_attach( p_spu->p_blend, p_spu );
            p_spu->p_blend->fmt_out.video.i_x_offset =
                p_spu->p_blend->fmt_out.video.i_y_offset = 0;
            p_spu->p_blend->fmt_out.video.i_aspect = p_fmt->i_aspect;
            p_spu->p_blend->fmt_out.video.i_chroma = p_fmt->i_chroma;
            p_spu->p_blend->fmt_in.video.i_chroma = VLC_FOURCC('Y','U','V','P');

            p_spu->p_blend->p_module =
                module_Need( p_spu->p_blend, "video blending", 0, 0 );
        }

        /* Load the text rendering module */
        if( !p_spu->p_text && p_region )
        {
            p_spu->p_text = vlc_object_create( p_spu, VLC_OBJECT_FILTER );
            vlc_object_attach( p_spu->p_text, p_spu );

            p_spu->p_text->fmt_out.video.i_width =
                p_spu->p_text->fmt_out.video.i_visible_width =
                    p_fmt->i_width;
            p_spu->p_text->fmt_out.video.i_height =
                p_spu->p_text->fmt_out.video.i_visible_height =
                    p_fmt->i_height;

            p_spu->p_text->pf_sub_buffer_new = spu_new_buffer;
            p_spu->p_text->pf_sub_buffer_del = spu_del_buffer;
            p_spu->p_text->p_module =
                module_Need( p_spu->p_text, "text renderer", 0, 0 );
        }
        else if( p_region )
        {
            p_spu->p_text->fmt_out.video.i_width =
                p_spu->p_text->fmt_out.video.i_visible_width =
                    p_fmt->i_width;
            p_spu->p_text->fmt_out.video.i_height =
                p_spu->p_text->fmt_out.video.i_visible_height =
                    p_fmt->i_height;
        }

        i_scale_width = i_scale_width_orig;
        i_scale_height = i_scale_height_orig;

        if( p_subpic->i_original_picture_width &&
            p_subpic->i_original_picture_height )
        {
            i_scale_width = i_scale_width * p_fmt->i_width /
                             p_subpic->i_original_picture_width;
            i_scale_height = i_scale_height * p_fmt->i_height /
                             p_subpic->i_original_picture_height;
        }

        /* Set default subpicture aspect ratio */
        if( p_region && p_region->fmt.i_aspect &&
            (!p_region->fmt.i_sar_num || !p_region->fmt.i_sar_den) )
        {
            p_region->fmt.i_sar_den = p_region->fmt.i_aspect;
            p_region->fmt.i_sar_num = VOUT_ASPECT_FACTOR;
        }
        if( p_region &&
            (!p_region->fmt.i_sar_num || !p_region->fmt.i_sar_den) )
        {
            p_region->fmt.i_sar_den = p_fmt->i_sar_den;
            p_region->fmt.i_sar_num = p_fmt->i_sar_num;
        }

        /* Take care of the aspect ratio */
        if( p_region && p_region->fmt.i_sar_num * p_fmt->i_sar_den !=
            p_region->fmt.i_sar_den * p_fmt->i_sar_num )
        {
            i_scale_width = i_scale_width *
                (int64_t)p_region->fmt.i_sar_num * p_fmt->i_sar_den /
                p_region->fmt.i_sar_den / p_fmt->i_sar_num;
            i_subpic_x = p_subpic->i_x * i_scale_width / 1000;
        }

        /* Load the scaling module */
        if( !p_spu->p_scale && (i_scale_width != 1000 ||
            i_scale_height != 1000) )
        {
            p_spu->p_scale = vlc_object_create( p_spu, VLC_OBJECT_FILTER );
            vlc_object_attach( p_spu->p_scale, p_spu );
            p_spu->p_scale->fmt_out.video.i_chroma =
                p_spu->p_scale->fmt_in.video.i_chroma =
                    VLC_FOURCC('Y','U','V','P');
            p_spu->p_scale->fmt_in.video.i_width =
                p_spu->p_scale->fmt_in.video.i_height = 32;
            p_spu->p_scale->fmt_out.video.i_width =
                p_spu->p_scale->fmt_out.video.i_height = 16;

            p_spu->p_scale->pf_vout_buffer_new = spu_new_video_buffer;
            p_spu->p_scale->pf_vout_buffer_del = spu_del_video_buffer;
            p_spu->p_scale->p_module =
                module_Need( p_spu->p_scale, "video filter2", 0, 0 );
        }

        while( p_region && p_spu->p_blend && p_spu->p_blend->pf_video_blend )
        {
            int i_fade_alpha = 255;
            int i_x_offset = p_region->i_x + i_subpic_x;
            int i_y_offset = p_region->i_y + p_subpic->i_y;

            if( p_region->fmt.i_chroma == VLC_FOURCC('T','E','X','T') )
            {
                if( p_spu->p_text && p_spu->p_text->p_module &&
                    p_spu->p_text->pf_render_text )
                {
                    p_region->i_text_align = p_subpic->i_flags & 0x3;
                    p_spu->p_text->pf_render_text( p_spu->p_text,
                                                   p_region, p_region ); 
                }
            }

            /* Force palette if requested */
            if( p_spu->b_force_alpha && VLC_FOURCC('Y','U','V','P') ==
                p_region->fmt.i_chroma )
            {
                p_region->fmt.p_palette->palette[0][3] = p_spu->pi_alpha[0];
                p_region->fmt.p_palette->palette[1][3] = p_spu->pi_alpha[1];
                p_region->fmt.p_palette->palette[2][3] = p_spu->pi_alpha[2];
                p_region->fmt.p_palette->palette[3][3] = p_spu->pi_alpha[3];
            }

            /* Scale SPU if necessary */
            if( p_region->p_cache )
            {
                if( i_scale_width * p_region->fmt.i_width / 1000 !=
                    p_region->p_cache->fmt.i_width ||
                    i_scale_height * p_region->fmt.i_height / 1000 !=
                    p_region->p_cache->fmt.i_height )
                {
                    p_subpic->pf_destroy_region( VLC_OBJECT(p_spu),
                                                 p_region->p_cache );
                    p_region->p_cache = 0;
                }
            }

            if( (i_scale_width != 1000 || i_scale_height != 1000) &&
                p_spu->p_scale && !p_region->p_cache )
            {
                picture_t *p_pic;

                p_spu->p_scale->fmt_in.video = p_region->fmt;
                p_spu->p_scale->fmt_out.video = p_region->fmt;

                p_region->p_cache =
                    p_subpic->pf_create_region( VLC_OBJECT(p_spu),
                        &p_spu->p_scale->fmt_out.video );
                if( p_spu->p_scale->fmt_out.video.p_palette )
                    *p_spu->p_scale->fmt_out.video.p_palette =
                        *p_region->fmt.p_palette;
                p_region->p_cache->p_next = p_region->p_next;

                vout_CopyPicture( p_spu, &p_region->p_cache->picture,
                                  &p_region->picture );

                p_spu->p_scale->fmt_out.video.i_width =
                    p_region->fmt.i_width * i_scale_width / 1000;
                p_spu->p_scale->fmt_out.video.i_visible_width =
                    p_region->fmt.i_visible_width * i_scale_width / 1000;
                p_spu->p_scale->fmt_out.video.i_height =
                    p_region->fmt.i_height * i_scale_height / 1000;
                p_spu->p_scale->fmt_out.video.i_visible_height =
                    p_region->fmt.i_visible_height * i_scale_height / 1000;
                p_region->p_cache->fmt = p_spu->p_scale->fmt_out.video;
                p_region->p_cache->i_x = p_region->i_x * i_scale_width / 1000;
                p_region->p_cache->i_y = p_region->i_y * i_scale_height / 1000;

                p_pic = p_spu->p_scale->pf_video_filter(
                                 p_spu->p_scale, &p_region->p_cache->picture );
                if( p_pic )
                {
                    picture_t p_pic_tmp = p_region->p_cache->picture;
                    p_region->p_cache->picture = *p_pic;
                    *p_pic = p_pic_tmp;
                    free( p_pic );
                }
            }
            if( (i_scale_width != 1000 || i_scale_height != 1000) &&
                p_spu->p_scale && p_region->p_cache )
            {
                p_region = p_region->p_cache;
            }

            if( p_subpic->i_flags & SUBPICTURE_ALIGN_BOTTOM )
            {
                i_y_offset = p_fmt->i_height - p_region->fmt.i_height -
                    p_subpic->i_y;
            }
            else if ( !(p_subpic->i_flags & SUBPICTURE_ALIGN_TOP) )
            {
                i_y_offset = p_fmt->i_height / 2 - p_region->fmt.i_height / 2;
            }

            if( p_subpic->i_flags & SUBPICTURE_ALIGN_RIGHT )
            {
                i_x_offset = p_fmt->i_width - p_region->fmt.i_width -
                    i_subpic_x;
            }
            else if ( !(p_subpic->i_flags & SUBPICTURE_ALIGN_LEFT) )
            {
                i_x_offset = p_fmt->i_width / 2 - p_region->fmt.i_width / 2;
            }

            if( p_subpic->b_absolute )
            {
                i_x_offset = p_region->i_x +
                    i_subpic_x * i_scale_width / 1000;
                i_y_offset = p_region->i_y +
                    p_subpic->i_y * i_scale_height / 1000;

                if( p_spu->i_margin >= 0 )
                {
                    if( p_subpic->i_height + (unsigned int)p_spu->i_margin <=
                        p_fmt->i_height )
                    {
                        i_y_offset = p_fmt->i_height -
                            p_spu->i_margin - p_subpic->i_height;
                    }
                }
            }

            p_spu->p_blend->fmt_in.video = p_region->fmt;

            /* Force cropping if requested */
            if( p_spu->b_force_crop )
            {
                video_format_t *p_fmt = &p_spu->p_blend->fmt_in.video;
                int i_crop_x = p_spu->i_crop_x * i_scale_width / 1000;
                int i_crop_y = p_spu->i_crop_y * i_scale_height / 1000;
                int i_crop_width = p_spu->i_crop_width * i_scale_width / 1000;
                int i_crop_height = p_spu->i_crop_height * i_scale_height/1000;

                /* Find the intersection */
                if( i_crop_x + i_crop_width <= i_x_offset ||
                    i_x_offset + (int)p_fmt->i_visible_width < i_crop_x ||
                    i_crop_y + i_crop_height <= i_y_offset ||
                    i_y_offset + (int)p_fmt->i_visible_height < i_crop_y )
                {
                    /* No intersection */
                    p_fmt->i_visible_width = p_fmt->i_visible_height = 0;
                }
                else
                {
                    int i_x, i_y, i_x_end, i_y_end;
                    i_x = __MAX( i_crop_x, i_x_offset );
                    i_y = __MAX( i_crop_y, i_y_offset );
                    i_x_end = __MIN( i_crop_x + i_crop_width,
                                   i_x_offset + (int)p_fmt->i_visible_width );
                    i_y_end = __MIN( i_crop_y + i_crop_height,
                                   i_y_offset + (int)p_fmt->i_visible_height );

                    p_fmt->i_x_offset = i_x - i_x_offset;
                    p_fmt->i_y_offset = i_y - i_y_offset;
                    p_fmt->i_visible_width = i_x_end - i_x;
                    p_fmt->i_visible_height = i_y_end - i_y;

                    i_x_offset = i_x;
                    i_y_offset = i_y;
                }
            }

            /* Update the output picture size */
            p_spu->p_blend->fmt_out.video.i_width =
                p_spu->p_blend->fmt_out.video.i_visible_width =
                    p_fmt->i_width;
            p_spu->p_blend->fmt_out.video.i_height =
                p_spu->p_blend->fmt_out.video.i_visible_height =
                    p_fmt->i_height;

            if( p_subpic->b_fade )
            {
                mtime_t i_fade_start = ( p_subpic->i_stop +
                                         p_subpic->i_start ) / 2;
                mtime_t i_now = mdate();
                if( i_now >= i_fade_start && p_subpic->i_stop > i_fade_start )
                {
                    i_fade_alpha = 255 * ( p_subpic->i_stop - i_now ) /
                                   ( p_subpic->i_stop - i_fade_start );
                }
            }

            p_spu->p_blend->pf_video_blend( p_spu->p_blend, p_pic_dst,
                p_pic_src, &p_region->picture, i_x_offset, i_y_offset,
                i_fade_alpha * p_subpic->i_alpha / 255 );

            p_region = p_region->p_next;
        }

        p_subpic = p_subpic->p_next;
    }

    vlc_mutex_unlock( &p_spu->subpicture_lock );
}

/*****************************************************************************
 * spu_SortSubpictures: find the subpictures to display
 *****************************************************************************
 * This function parses all subpictures and decides which ones need to be
 * displayed. This operation does not need lock, since only READY_SUBPICTURE
 * are handled. If no picture has been selected, display_date will depend on
 * the subpicture.
 * We also check for ephemer DVD subpictures (subpictures that have
 * to be removed if a newer one is available), which makes it a lot
 * more difficult to guess if a subpicture has to be rendered or not.
 *****************************************************************************/
subpicture_t *spu_SortSubpictures( spu_t *p_spu, mtime_t display_date )
{
    int i_index, i_channel;
    subpicture_t *p_subpic = NULL;
    subpicture_t *p_ephemer;
    mtime_t      ephemer_date;

    /* Run subpicture filters */
    for( i_index = 0; i_index < p_spu->i_filter; i_index++ )
    {
        subpicture_t *p_subpic_filter;
        p_subpic_filter = p_spu->pp_filter[i_index]->
            pf_sub_filter( p_spu->pp_filter[i_index], display_date );
        if( p_subpic_filter )
        {
            spu_DisplaySubpicture( p_spu, p_subpic_filter );
        }
    }

    /* We get an easily parsable chained list of subpictures which
     * ends with NULL since p_subpic was initialized to NULL. */
    for( i_channel = 0; i_channel < p_spu->i_channel; i_channel++ )
    {
        p_ephemer = 0;
        ephemer_date = 0;

        for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
        {
            if( p_spu->p_subpicture[i_index].i_channel != i_channel ||
                p_spu->p_subpicture[i_index].i_status != READY_SUBPICTURE )
            {
                continue;
            }
            if( display_date &&
                display_date < p_spu->p_subpicture[i_index].i_start )
            {
                /* Too early, come back next monday */
                continue;
            }

            if( p_spu->p_subpicture[i_index].i_start > ephemer_date )
                ephemer_date = p_spu->p_subpicture[i_index].i_start;

            if( display_date > p_spu->p_subpicture[i_index].i_stop &&
                ( !p_spu->p_subpicture[i_index].b_ephemer ||
                  p_spu->p_subpicture[i_index].i_stop >
                  p_spu->p_subpicture[i_index].i_start ) )
            {
                /* Too late, destroy the subpic */
                spu_DestroySubpicture( p_spu, &p_spu->p_subpicture[i_index] );
                continue;
            }

            /* If this is an ephemer subpic, add it to our list */
            if( p_spu->p_subpicture[i_index].b_ephemer )
            {
                p_spu->p_subpicture[i_index].p_next = p_ephemer;
                p_ephemer = &p_spu->p_subpicture[i_index];

                continue;
            }

            p_spu->p_subpicture[i_index].p_next = p_subpic;
            p_subpic = &p_spu->p_subpicture[i_index];
        }

        /* If we found ephemer subpictures, check if they have to be
         * displayed or destroyed */
        while( p_ephemer != NULL )
        {
            subpicture_t *p_tmp = p_ephemer;
            p_ephemer = p_ephemer->p_next;

            if( p_tmp->i_start < ephemer_date )
            {
                /* Ephemer subpicture has lived too long */
                spu_DestroySubpicture( p_spu, p_tmp );
            }
            else
            {
                /* Ephemer subpicture can still live a bit */
                p_tmp->p_next = p_subpic;
                p_subpic = p_tmp;
            }
        }
    }

    return p_subpic;
}

/*****************************************************************************
 * SpuClearChannel: clear an spu channel
 *****************************************************************************
 * This function destroys the subpictures which belong to the spu channel
 * corresponding to i_channel_id.
 *****************************************************************************/
static void SpuClearChannel( spu_t *p_spu, int i_channel )
{
    int          i_subpic;                               /* subpicture index */
    subpicture_t *p_subpic = NULL;                  /* first free subpicture */

    vlc_mutex_lock( &p_spu->subpicture_lock );

    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        p_subpic = &p_spu->p_subpicture[i_subpic];
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
                spu_DestroyRegion( p_spu, p_region );
            }

            if( p_subpic->pf_destroy ) p_subpic->pf_destroy( p_subpic );
            p_subpic->i_status = FREE_SUBPICTURE;
        }
    }

    vlc_mutex_unlock( &p_spu->subpicture_lock );
}

/*****************************************************************************
 * spu_ControlDefault: default methods for the subpicture unit control.
 *****************************************************************************/
static int spu_vaControlDefault( spu_t *p_spu, int i_query, va_list args )
{
    int *pi, i;

    switch( i_query )
    {
    case SPU_CHANNEL_REGISTER:
        pi = (int *)va_arg( args, int * );
        if( pi ) *pi = p_spu->i_channel++;
        msg_Dbg( p_spu, "Registering subpicture channel, ID: %i",
                 p_spu->i_channel - 1 );
        break;

    case SPU_CHANNEL_CLEAR:
        i = (int)va_arg( args, int );
        SpuClearChannel( p_spu, i );
        break;

    default:
        msg_Dbg( p_spu, "control query not supported" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Object variables callbacks
 *****************************************************************************/

/*****************************************************************************
 * UpdateSPU: update subpicture settings
 *****************************************************************************
 * This function is called from CropCallback and at initialization time, to
 * retrieve crop information from the input.
 *****************************************************************************/
static void UpdateSPU( spu_t *p_spu, vlc_object_t *p_object )
{
    vlc_value_t val;

    p_spu->b_force_alpha = VLC_FALSE;
    p_spu->b_force_crop = VLC_FALSE;

    if( var_Get( p_object, "highlight", &val ) || !val.b_bool ) return;

    p_spu->b_force_crop = VLC_TRUE;
    var_Get( p_object, "x-start", &val );
    p_spu->i_crop_x = val.i_int;
    var_Get( p_object, "y-start", &val );
    p_spu->i_crop_y = val.i_int;
    var_Get( p_object, "x-end", &val );
    p_spu->i_crop_width = val.i_int - p_spu->i_crop_x;
    var_Get( p_object, "y-end", &val );
    p_spu->i_crop_height = val.i_int - p_spu->i_crop_y;

#if 0
    if( var_Get( p_object, "color", &val ) == VLC_SUCCESS )
    {
        int i;
        for( i = 0; i < 4; i++ )
        {
            p_spu->pi_color[i] = ((uint8_t *)val.p_address)[i];
        }
    }
#endif

    if( var_Get( p_object, "menu-contrast", &val ) == VLC_SUCCESS )
    {
        int i;
        for( i = 0; i < 4; i++ )
        {
            p_spu->pi_alpha[i] = ((uint8_t *)val.p_address)[i];
            p_spu->pi_alpha[i] = p_spu->pi_alpha[i] == 0xf ?
                0xff : p_spu->pi_alpha[i] << 4;
        }
        p_spu->b_force_alpha = VLC_TRUE;
    }

    msg_Dbg( p_object, "crop: %i,%i,%i,%i, alpha: %i",
             p_spu->i_crop_x, p_spu->i_crop_y,
             p_spu->i_crop_width, p_spu->i_crop_height, p_spu->b_force_alpha );
}

/*****************************************************************************
 * CropCallback: called when the highlight properties are changed
 *****************************************************************************
 * This callback is called from the input thread when we need cropping
 *****************************************************************************/
static int CropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    UpdateSPU( (spu_t *)p_data, p_object );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Buffers allocation callbacks for the filters
 *****************************************************************************/
static subpicture_t *sub_new_buffer( filter_t *p_filter )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;
    subpicture_t *p_subpicture = spu_CreateSubpicture( p_sys->p_spu );
    if( p_subpicture ) p_subpicture->i_channel = p_sys->i_channel;
    return p_subpicture;
}

static void sub_del_buffer( filter_t *p_filter, subpicture_t *p_subpic )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;
    spu_DestroySubpicture( p_sys->p_spu, p_subpic );
}

static subpicture_t *spu_new_buffer( filter_t *p_filter )
{
    subpicture_t *p_subpic = (subpicture_t *)malloc(sizeof(subpicture_t));
    memset( p_subpic, 0, sizeof(subpicture_t) );
    p_subpic->b_absolute = VLC_TRUE;

    p_subpic->pf_create_region = __spu_CreateRegion;
    p_subpic->pf_make_region = __spu_MakeRegion;
    p_subpic->pf_destroy_region = __spu_DestroyRegion;

    return p_subpic;
}

static void spu_del_buffer( filter_t *p_filter, subpicture_t *p_subpic )
{
    while( p_subpic->p_region )
    {
        subpicture_region_t *p_region = p_subpic->p_region;
        p_subpic->p_region = p_region->p_next;
        p_subpic->pf_destroy_region( VLC_OBJECT(p_filter), p_region );
    }

    free( p_subpic );
}

static picture_t *spu_new_video_buffer( filter_t *p_filter )
{
    picture_t *p_picture = malloc( sizeof(picture_t) );

    if( vout_AllocatePicture( p_filter, p_picture,
                              p_filter->fmt_out.video.i_chroma,
                              p_filter->fmt_out.video.i_width,
                              p_filter->fmt_out.video.i_height,
                              p_filter->fmt_out.video.i_aspect )
        != VLC_SUCCESS )
    {
        free( p_picture );
        return NULL;
    }

    p_picture->pf_release = RegionPictureRelease;

    return p_picture;
}

static void spu_del_video_buffer( filter_t *p_filter, picture_t *p_pic )
{
    if( p_pic && p_pic->p_data_orig ) free( p_pic->p_data_orig );
    if( p_pic ) free( p_pic );
}
