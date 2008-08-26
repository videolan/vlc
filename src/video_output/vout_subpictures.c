/*****************************************************************************
 * vout_subpictures.c : subpicture management functions
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_osd.h>
#include "../libvlc.h"

#include <assert.h>

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

static int spu_ParseChain( spu_t * );
static int SubFilterCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );

static int sub_filter_allocation_init( filter_t *, void * );
static void sub_filter_allocation_clear( filter_t * );
struct filter_owner_sys_t
{
    spu_t *p_spu;
    int i_channel;
};

enum {
    SCALE_DEFAULT,
    SCALE_TEXT,
    SCALE_SIZE
};

static void FilterRelease( filter_t *p_filter )
{
    if( p_filter->p_module )
        module_Unneed( p_filter, p_filter->p_module );

    vlc_object_detach( p_filter );
    vlc_object_release( p_filter );
}

/**
 * Creates the subpicture unit
 *
 * \param p_this the parent object which creates the subpicture unit
 */
spu_t *__spu_Create( vlc_object_t *p_this )
{
    int i_index;
    spu_t *p_spu = vlc_custom_create( p_this, sizeof( spu_t ),
                                      VLC_OBJECT_GENERIC, "subpicture" );

    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++)
    {
        p_spu->p_subpicture[i_index].i_status = FREE_SUBPICTURE;
    }

    p_spu->p_blend = NULL;
    p_spu->p_text = NULL;
    p_spu->p_scale = NULL;
    p_spu->p_scale_yuvp = NULL;
    p_spu->pf_control = spu_vaControlDefault;

    /* Register the default subpicture channel */
    p_spu->i_channel = 2;

    vlc_mutex_init( &p_spu->subpicture_lock );

    vlc_object_attach( p_spu, p_this );

    p_spu->p_chain = filter_chain_New( p_spu, "sub filter", false,
                                       sub_filter_allocation_init,
                                       sub_filter_allocation_clear,
                                       p_spu );
    return p_spu;
}

/**
 * Initialise the subpicture unit
 *
 * \param p_spu the subpicture unit object
 */
int spu_Init( spu_t *p_spu )
{
    vlc_value_t val;

    /* If the user requested a sub margin, we force the position. */
    var_Create( p_spu, "sub-margin", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_spu, "sub-margin", &val );
    p_spu->i_margin = val.i_int;

    var_Create( p_spu, "sub-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_AddCallback( p_spu, "sub-filter", SubFilterCallback, p_spu );

    spu_ParseChain( p_spu );

    return VLC_SUCCESS;
}

int spu_ParseChain( spu_t *p_spu )
{
    char *psz_parser = var_GetString( p_spu, "sub-filter" );
    if( filter_chain_AppendFromString( p_spu->p_chain, psz_parser ) < 0 )
    {
        free( psz_parser );
        return VLC_EGENERIC;
    }

    free( psz_parser );
    return VLC_SUCCESS;
}

/**
 * Destroy the subpicture unit
 *
 * \param p_this the parent object which destroys the subpicture unit
 */
void spu_Destroy( spu_t *p_spu )
{
    int i_index;

    /* Destroy all remaining subpictures */
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_spu->p_subpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            spu_DestroySubpicture( p_spu, &p_spu->p_subpicture[i_index] );
        }
    }

    if( p_spu->p_blend )
        FilterRelease( p_spu->p_blend );

    if( p_spu->p_text )
        FilterRelease( p_spu->p_text );

    if( p_spu->p_scale_yuvp )
        FilterRelease( p_spu->p_scale_yuvp );

    if( p_spu->p_scale )
        FilterRelease( p_spu->p_scale );

    filter_chain_Delete( p_spu->p_chain );

    vlc_mutex_destroy( &p_spu->subpicture_lock );
    vlc_object_release( p_spu );
}

/**
 * Attach/Detach the SPU from any input
 *
 * \param p_this the object in which to destroy the subpicture unit
 * \param b_attach to select attach or detach
 */
void spu_Attach( spu_t *p_spu, vlc_object_t *p_this, bool b_attach )
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
    free( p_pic->p_data_orig );
    /* We use pf_release nullity to know if the picture has already been released. */
    p_pic->pf_release = NULL;
}
subpicture_region_t *__spu_CreateRegion( vlc_object_t *p_this,
                                         video_format_t *p_fmt )
{
    subpicture_region_t *p_region = malloc( sizeof(subpicture_region_t) );
    if( !p_region ) return NULL;

    memset( p_region, 0, sizeof(subpicture_region_t) );
    p_region->i_alpha = 0xff;
    p_region->p_next = NULL;
    p_region->p_cache = NULL;
    p_region->fmt = *p_fmt;
    p_region->psz_text = NULL;
    p_region->p_style = NULL;

    if( p_fmt->i_chroma == VLC_FOURCC('Y','U','V','P') )
        p_fmt->p_palette = p_region->fmt.p_palette =
            malloc( sizeof(video_palette_t) );
    else p_fmt->p_palette = p_region->fmt.p_palette = NULL;

    p_region->picture.p_data_orig = NULL;

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
    (void)p_this;
    if( !p_region ) return NULL;
    memset( p_region, 0, sizeof(subpicture_region_t) );
    p_region->i_alpha = 0xff;
    p_region->p_next = 0;
    p_region->p_cache = 0;
    p_region->fmt = *p_fmt;
    p_region->psz_text = 0;
    p_region->p_style = NULL;

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
    free( p_region->fmt.p_palette );
    if( p_region->p_cache ) __spu_DestroyRegion( p_this, p_region->p_cache );

    free( p_region->psz_text );
    free( p_region->psz_html );
    //free( p_region->p_style ); FIXME --fenrir plugin does not allocate the memory for it. I think it might lead to segfault, video renderer can live longer than the decoder
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
    p_subpic->b_absolute = true;
    p_subpic->b_pausable = false;
    p_subpic->b_fade     = false;
    p_subpic->i_alpha    = 0xFF;
    p_subpic->p_region   = NULL;
    p_subpic->pf_render  = NULL;
    p_subpic->pf_destroy = NULL;
    p_subpic->p_sys      = NULL;
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
static void SpuRenderCreateBlend( spu_t *p_spu, vlc_fourcc_t i_chroma, int i_aspect )
{
    filter_t *p_blend;

    assert( !p_spu->p_blend );

    p_spu->p_blend =
    p_blend        = vlc_custom_create( p_spu, sizeof(filter_t),
                                        VLC_OBJECT_GENERIC, "blend" );
    if( !p_blend )
        return;

    es_format_Init( &p_blend->fmt_in, VIDEO_ES, 0 );

    es_format_Init( &p_blend->fmt_out, VIDEO_ES, 0 );
    p_blend->fmt_out.video.i_x_offset = 0;
    p_blend->fmt_out.video.i_y_offset = 0;
    p_blend->fmt_out.video.i_chroma = i_chroma;
    p_blend->fmt_out.video.i_aspect = i_aspect;

    /* The blend module will be loaded when needed with the real
    * input format */
    p_blend->p_module = NULL;

    /* */
    vlc_object_attach( p_blend, p_spu );
}
static void SpuRenderUpdateBlend( spu_t *p_spu, int i_out_width, int i_out_height, const video_format_t *p_in_fmt )
{
    filter_t *p_blend = p_spu->p_blend;

    assert( p_blend );

    /* */
    if( p_blend->p_module && p_blend->fmt_in.video.i_chroma != p_in_fmt->i_chroma )
    {
        /* The chroma is not the same, we need to reload the blend module
         * XXX to match the old behaviour just test !p_blend->fmt_in.video.i_chroma */
        module_Unneed( p_blend, p_blend->p_module );
        p_blend->p_module = NULL;
    }

    /* */
    p_blend->fmt_in.video = *p_in_fmt;

    /* */
    p_blend->fmt_out.video.i_width =
    p_blend->fmt_out.video.i_visible_width = i_out_width;
    p_blend->fmt_out.video.i_height =
    p_blend->fmt_out.video.i_visible_height = i_out_height;

    /* */
    if( !p_blend->p_module )
        p_blend->p_module = module_Need( p_blend, "video blending", 0, 0 );
}
static void SpuRenderCreateAndLoadText( spu_t *p_spu, int i_width, int i_height )
{
    filter_t *p_text;

    assert( !p_spu->p_text );

    p_spu->p_text =
    p_text        = vlc_custom_create( p_spu, sizeof(filter_t),
                                       VLC_OBJECT_GENERIC, "spu text" );
    if( !p_text )
        return;

    es_format_Init( &p_text->fmt_in, VIDEO_ES, 0 );

    es_format_Init( &p_text->fmt_out, VIDEO_ES, 0 );
    p_text->fmt_out.video.i_width =
    p_text->fmt_out.video.i_visible_width = i_width;
    p_text->fmt_out.video.i_height =
    p_text->fmt_out.video.i_visible_height = i_height;

    p_text->pf_sub_buffer_new = spu_new_buffer;
    p_text->pf_sub_buffer_del = spu_del_buffer;

    vlc_object_attach( p_text, p_spu );

    /* FIXME TOCHECK shouldn't module_Need( , , psz_modulename, false ) do the
     * same than these 2 calls ? */
    char *psz_modulename = var_CreateGetString( p_spu, "text-renderer" );
    if( psz_modulename && *psz_modulename )
    {
        p_text->p_module = module_Need( p_text, "text renderer",
                                        psz_modulename, true );
    }
    free( psz_modulename );

    if( !p_text->p_module )
        p_text->p_module = module_Need( p_text, "text renderer", NULL, false );
}

static filter_t *CreateAndLoadScale( vlc_object_t *p_obj, vlc_fourcc_t i_chroma )
{
    filter_t *p_scale;

    p_scale = vlc_custom_create( p_obj, sizeof(filter_t),
                                 VLC_OBJECT_GENERIC, "scale" );
    if( !p_scale )
        return NULL;

    es_format_Init( &p_scale->fmt_in, VIDEO_ES, 0 );
    p_scale->fmt_in.video.i_chroma = i_chroma;
    p_scale->fmt_in.video.i_width =
    p_scale->fmt_in.video.i_height = 32;

    es_format_Init( &p_scale->fmt_out, VIDEO_ES, 0 );
    p_scale->fmt_out.video.i_chroma = i_chroma;
    p_scale->fmt_out.video.i_width =
    p_scale->fmt_out.video.i_height = 16;

    p_scale->pf_vout_buffer_new = spu_new_video_buffer;
    p_scale->pf_vout_buffer_del = spu_del_video_buffer;

    vlc_object_attach( p_scale, p_obj );
    p_scale->p_module = module_Need( p_scale, "video filter2", 0, 0 );

    return p_scale;
}

static void SpuRenderCreateAndLoadScale( spu_t *p_spu )
{
    /* FIXME: We'll also be using it for YUVA and RGBA blending ... */

    assert( !p_spu->p_scale );
    assert( !p_spu->p_scale_yuvp );
    p_spu->p_scale = CreateAndLoadScale( VLC_OBJECT(p_spu), VLC_FOURCC('Y','U','V','A') );
    p_spu->p_scale_yuvp = p_spu->p_scale_yuvp = CreateAndLoadScale( VLC_OBJECT(p_spu), VLC_FOURCC('Y','U','V','P') );
}

static void SpuRenderText( spu_t *p_spu, bool *pb_rerender_text,
                           subpicture_t *p_subpic, subpicture_region_t *p_region, int i_min_scale_ratio )
{
    assert( p_region->fmt.i_chroma == VLC_FOURCC('T','E','X','T') );

    if( !p_spu->p_text || !p_spu->p_text->p_module )
        goto exit;

    /* Setup 3 variables which can be used to render
     * time-dependent text (and effects). The first indicates
     * the total amount of time the text will be on screen,
     * the second the amount of time it has already been on
     * screen (can be a negative value as text is layed out
     * before it is rendered) and the third is a feedback
     * variable from the renderer - if the renderer sets it
     * then this particular text is time-dependent, eg. the
     * visual progress bar inside the text in karaoke and the
     * text needs to be rendered multiple times in order for
     * the effect to work - we therefore need to return the
     * region to its original state at the end of the loop,
     * instead of leaving it in YUVA or YUVP.
     * Any renderer which is unaware of how to render
     * time-dependent text can happily ignore the variables
     * and render the text the same as usual - it should at
     * least show up on screen, but the effect won't change
     * the text over time.
     */

    /* FIXME why these variables are recreated every time and not
     * when text renderer module was created ? */
    var_Create( p_spu->p_text, "spu-duration", VLC_VAR_TIME );
    var_Create( p_spu->p_text, "spu-elapsed", VLC_VAR_TIME );
    var_Create( p_spu->p_text, "text-rerender", VLC_VAR_BOOL );
    var_Create( p_spu->p_text, "scale", VLC_VAR_INTEGER );

    var_SetTime( p_spu->p_text, "spu-duration", p_subpic->i_stop - p_subpic->i_start );
    var_SetTime( p_spu->p_text, "spu-elapsed", mdate() - p_subpic->i_start );
    var_SetBool( p_spu->p_text, "text-rerender", false );
    var_SetInteger( p_spu->p_text, "scale", i_min_scale_ratio );

    if( p_spu->p_text->pf_render_html && p_region->psz_html )
    {
        p_spu->p_text->pf_render_html( p_spu->p_text,
                                       p_region, p_region );
    }
    else if( p_spu->p_text->pf_render_text )
    {
        p_spu->p_text->pf_render_text( p_spu->p_text,
                                       p_region, p_region );
    }
    *pb_rerender_text = var_GetBool( p_spu->p_text, "text-rerender" );

    var_Destroy( p_spu->p_text, "spu-duration" );
    var_Destroy( p_spu->p_text, "spu-elapsed" );
    var_Destroy( p_spu->p_text, "text-rerender" );
    var_Destroy( p_spu->p_text, "scale" );

exit:
    p_region->i_align |= SUBPICTURE_RENDERED;
}

static void SpuRenderRegion( spu_t *p_spu,
                             picture_t *p_pic_dst, picture_t *p_pic_src,
                             subpicture_t *p_subpic, subpicture_region_t *p_region,
                             const int i_scale_width_orig, const int i_scale_height_orig,
                             const int pi_subpic_x[SCALE_SIZE],
                             const int pi_scale_width[SCALE_SIZE],
                             const int pi_scale_height[SCALE_SIZE],
                             const video_format_t *p_fmt )
{
    video_format_t fmt_original;
    bool b_rerender_text;
    bool b_restore_format = false;
    int i_fade_alpha;
    int i_x_offset;
    int i_y_offset;
    int i_scale_idx;
    int i_inv_scale_x;
    int i_inv_scale_y;
    filter_t *p_scale;

    vlc_assert_locked( &p_spu->subpicture_lock );

    fmt_original = p_region->fmt;
    b_rerender_text = false;
    if( p_region->fmt.i_chroma == VLC_FOURCC('T','E','X','T') )
    {
        SpuRenderText( p_spu, &b_rerender_text, p_subpic, p_region, __MIN(i_scale_width_orig, i_scale_height_orig) );
        b_restore_format = b_rerender_text;

        /* Check if the rendering has failed ... */
        if( p_region->fmt.i_chroma == VLC_FOURCC('T','E','X','T') )
            goto exit;
    }

    if( p_region->i_align & SUBPICTURE_RENDERED )
    {
        /* We are using a region which come from rendered text */
        i_scale_idx   = SCALE_TEXT;
        i_inv_scale_x = i_scale_width_orig;
        i_inv_scale_y = i_scale_height_orig;
    }
    else
    {
        i_scale_idx   = SCALE_DEFAULT;
        i_inv_scale_x = 1000;
        i_inv_scale_y = 1000;
    }

    i_x_offset = (p_region->i_x + pi_subpic_x[ i_scale_idx ]) * i_inv_scale_x / 1000;
    i_y_offset = (p_region->i_y + p_subpic->i_y) * i_inv_scale_y / 1000;

    /* Force palette if requested
     * FIXME b_force_palette and b_force_crop are applied to all subpictures using palette
     * instead of only the right one (being the dvd spu).
     */
    const bool b_using_palette = p_region->fmt.i_chroma == VLC_FOURCC('Y','U','V','P');
    const bool b_force_palette = b_using_palette && p_spu->b_force_palette;
    const bool b_force_crop    = b_force_palette && p_spu->b_force_crop;

    if( b_force_palette )
    {
        /* It looks so wrong I won't comment
         * p_palette->palette is [256][4] with a int i_entries
         * p_spu->palette is [4][4]
         * */
        p_region->fmt.p_palette->i_entries = 4;
        memcpy( p_region->fmt.p_palette->palette, p_spu->palette, 4*sizeof(uint32_t) );
    }

    if( b_using_palette )
        p_scale = p_spu->p_scale_yuvp;
    else
        p_scale = p_spu->p_scale;

    if( p_scale &&
        ( ( pi_scale_width[i_scale_idx]  > 0 && pi_scale_width[i_scale_idx]  != 1000 ) ||
          ( pi_scale_height[i_scale_idx] > 0 && pi_scale_height[i_scale_idx] != 1000 ) ||
          ( b_force_palette ) ) )
    {
        const unsigned i_dst_width  = p_region->fmt.i_width  * pi_scale_width[i_scale_idx] / 1000;
        const unsigned i_dst_height = p_region->fmt.i_height * pi_scale_height[i_scale_idx] / 1000;

        /* Destroy if cache is unusable */
        if( p_region->p_cache )
        {
            if( p_region->p_cache->fmt.i_width  != i_dst_width ||
                p_region->p_cache->fmt.i_height != i_dst_height ||
                b_force_palette )
            {
                p_subpic->pf_destroy_region( VLC_OBJECT(p_spu),
                                             p_region->p_cache );
                p_region->p_cache = NULL;
            }
        }

        /* Scale if needed into cache */
        if( !p_region->p_cache )
        {
            picture_t *p_pic;

            p_scale->fmt_in.video = p_region->fmt;
            p_scale->fmt_out.video = p_region->fmt;

            p_region->p_cache =
                p_subpic->pf_create_region( VLC_OBJECT(p_spu),
                                            &p_scale->fmt_out.video );
            p_region->p_cache->p_next = p_region->p_next;

            if( p_scale->fmt_out.video.p_palette )
                *p_scale->fmt_out.video.p_palette =
                    *p_region->fmt.p_palette;

            vout_CopyPicture( p_spu, &p_region->p_cache->picture,
                              &p_region->picture );

            p_scale->fmt_out.video.i_width = i_dst_width;
            p_scale->fmt_out.video.i_height = i_dst_height;

            p_scale->fmt_out.video.i_visible_width =
                p_region->fmt.i_visible_width * pi_scale_width[ i_scale_idx ] / 1000;
            p_scale->fmt_out.video.i_visible_height =
                p_region->fmt.i_visible_height * pi_scale_height[ i_scale_idx ] / 1000;

            p_region->p_cache->fmt = p_scale->fmt_out.video;
            p_region->p_cache->i_x = p_region->i_x * pi_scale_width[ i_scale_idx ] / 1000;
            p_region->p_cache->i_y = p_region->i_y * pi_scale_height[ i_scale_idx ] / 1000;
            p_region->p_cache->i_align = p_region->i_align;
            p_region->p_cache->i_alpha = p_region->i_alpha;

            p_pic = NULL;
            if( p_scale->p_module )
                p_pic = p_scale->pf_video_filter( p_scale, &p_region->p_cache->picture );
            else
                msg_Err( p_spu, "scaling failed (module not loaded)" );

            if( p_pic )
            {
                p_region->p_cache->picture = *p_pic;
                free( p_pic );
            }
            else
            {
                p_subpic->pf_destroy_region( VLC_OBJECT(p_spu),
                                             p_region->p_cache );
                p_region->p_cache = NULL;
            }
        }

        /* And use the scaled picture */
        if( p_region->p_cache )
        {
            p_region = p_region->p_cache;
            fmt_original = p_region->fmt;
        }
    }

    if( p_region->i_align & SUBPICTURE_ALIGN_BOTTOM )
    {
        i_y_offset = p_fmt->i_height - p_region->fmt.i_height -
            (p_subpic->i_y + p_region->i_y) * i_inv_scale_y / 1000;
    }
    else if ( !(p_region->i_align & SUBPICTURE_ALIGN_TOP) )
    {
        i_y_offset = p_fmt->i_height / 2 - p_region->fmt.i_height / 2;
    }

    if( p_region->i_align & SUBPICTURE_ALIGN_RIGHT )
    {
        i_x_offset = p_fmt->i_width - p_region->fmt.i_width -
            (pi_subpic_x[ i_scale_idx ] + p_region->i_x)
            * i_inv_scale_x / 1000;
    }
    else if ( !(p_region->i_align & SUBPICTURE_ALIGN_LEFT) )
    {
        i_x_offset = p_fmt->i_width / 2 - p_region->fmt.i_width / 2;
    }

    if( p_subpic->b_absolute )
    {
        i_x_offset = (p_region->i_x +
            pi_subpic_x[ i_scale_idx ] *
                             pi_scale_width[ i_scale_idx ] / 1000)
            * i_inv_scale_x / 1000;
        i_y_offset = (p_region->i_y +
            p_subpic->i_y * pi_scale_height[ i_scale_idx ] / 1000)
            * i_inv_scale_y / 1000;

    }

    i_x_offset = __MAX( i_x_offset, 0 );
    i_y_offset = __MAX( i_y_offset, 0 );

    if( p_spu->i_margin != 0 && !b_force_crop )
    {
        int i_diff = 0;
        int i_low = (i_y_offset - p_spu->i_margin) * i_inv_scale_y / 1000;
        int i_high = i_low + p_region->fmt.i_height;

        /* crop extra margin to keep within bounds */
        if( i_low < 0 )
            i_diff = i_low;
        if( i_high > (int)p_fmt->i_height )
            i_diff = i_high - p_fmt->i_height;
        i_y_offset -= ( p_spu->i_margin * i_inv_scale_y / 1000 + i_diff );
    }

    /* Force cropping if requested */
    if( b_force_crop )
    {
        video_format_t *p_fmt = &p_region->fmt;
        int i_crop_x = p_spu->i_crop_x * pi_scale_width[ i_scale_idx ] / 1000
                            * i_inv_scale_x / 1000;
        int i_crop_y = p_spu->i_crop_y * pi_scale_height[ i_scale_idx ] / 1000
                            * i_inv_scale_y / 1000;
        int i_crop_width = p_spu->i_crop_width * pi_scale_width[ i_scale_idx ] / 1000
                            * i_inv_scale_x / 1000;
        int i_crop_height = p_spu->i_crop_height * pi_scale_height[ i_scale_idx ] / 1000
                            * i_inv_scale_y / 1000;

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
        b_restore_format = true;
    }

    i_x_offset = __MAX( i_x_offset, 0 );
    i_y_offset = __MAX( i_y_offset, 0 );

    /* Compute alpha blend value */
    i_fade_alpha = 255;
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

    /* Update the blender */
    SpuRenderUpdateBlend( p_spu, p_fmt->i_width, p_fmt->i_height, &p_region->fmt );

    if( p_spu->p_blend->p_module )
    {
        p_spu->p_blend->pf_video_blend( p_spu->p_blend, p_pic_dst,
            p_pic_src, &p_region->picture, i_x_offset, i_y_offset,
            i_fade_alpha * p_subpic->i_alpha * p_region->i_alpha / 65025 );
    }
    else
    {
        msg_Err( p_spu, "blending %4.4s to %4.4s failed",
                 (char *)&p_spu->p_blend->fmt_out.video.i_chroma,
                 (char *)&p_spu->p_blend->fmt_out.video.i_chroma );
    }

exit:
    if( b_rerender_text )
    {
        /* Some forms of subtitles need to be re-rendered more than
         * once, eg. karaoke. We therefore restore the region to its
         * pre-rendered state, so the next time through everything is
         * calculated again.
         */
        p_region->picture.pf_release( &p_region->picture );
        memset( &p_region->picture, 0, sizeof( picture_t ) );
        p_region->i_align &= ~SUBPICTURE_RENDERED;
    }
    if( b_restore_format )
        p_region->fmt = fmt_original;
}

void spu_RenderSubpictures( spu_t *p_spu, video_format_t *p_fmt,
                            picture_t *p_pic_dst, picture_t *p_pic_src,
                            subpicture_t *p_subpic,
                            int i_scale_width_orig, int i_scale_height_orig )
{
    int i_source_video_width;
    int i_source_video_height;
    subpicture_t *p_subpic_v;

    /* Get lock */
    vlc_mutex_lock( &p_spu->subpicture_lock );

    for( p_subpic_v = p_subpic;
            p_subpic_v != NULL && p_subpic_v->i_status != FREE_SUBPICTURE;
            p_subpic_v = p_subpic_v->p_next )
    {
        if( p_subpic_v->pf_pre_render )
            p_subpic_v->pf_pre_render( p_fmt, p_spu, p_subpic_v );
    }

    if( i_scale_width_orig <= 0 )
        i_scale_width_orig = 1000;
    if( i_scale_height_orig <= 0 )
        i_scale_height_orig = 1000;

    i_source_video_width  = p_fmt->i_width  * 1000 / i_scale_width_orig;
    i_source_video_height = p_fmt->i_height * 1000 / i_scale_height_orig;

    /* Check i_status again to make sure spudec hasn't destroyed the subpic */
    for( ; ( p_subpic != NULL ) && ( p_subpic->i_status != FREE_SUBPICTURE ); p_subpic = p_subpic->p_next )
    {
        subpicture_region_t *p_region;
        int pi_scale_width[ SCALE_SIZE ];
        int pi_scale_height[ SCALE_SIZE ];
        int pi_subpic_x[ SCALE_SIZE ];
        int k;

        /* If the source video and subtitles stream agree on the size of
         * the video then disregard all further references to the subtitle
         * stream.
         */
        if( ( i_source_video_height == p_subpic->i_original_picture_height ) &&
            ( i_source_video_width  == p_subpic->i_original_picture_width ) )
        {
            /* FIXME this looks wrong */
            p_subpic->i_original_picture_height = 0;
            p_subpic->i_original_picture_width = 0;
        }

        for( k = 0; k < SCALE_SIZE ; k++ )
            pi_subpic_x[ k ] = p_subpic->i_x;

        if( p_subpic->pf_update_regions )
        {
            /* TODO do not reverse the scaling that was done before calling
             * spu_RenderSubpictures, just pass it along (or do it inside
             * spu_RenderSubpictures) */
            video_format_t fmt_org = *p_fmt;
            fmt_org.i_width =
            fmt_org.i_visible_width = i_source_video_width;
            fmt_org.i_height =
            fmt_org.i_visible_height = i_source_video_height;

            p_subpic->pf_update_regions( &fmt_org, p_spu, p_subpic, mdate() );
        }

        /* */
        p_region = p_subpic->p_region;
        if( !p_region )
            continue;

        /* Create the blending module */
        if( !p_spu->p_blend )
            SpuRenderCreateBlend( p_spu, p_fmt->i_chroma, p_fmt->i_aspect );

        /* Load the text rendering module; it is possible there is a
         * text region somewhere in the subpicture other than the first
         * element in the region list, so just load it anyway as we'll
         * probably want it sooner or later. */
        if( !p_spu->p_text )
            SpuRenderCreateAndLoadText( p_spu, p_fmt->i_width, p_fmt->i_height );

        if( p_spu->p_text )
        {
            subpicture_region_t *p_text_region = p_subpic->p_region;

            /* Only overwrite the size fields if the region is still in
             * pre-rendered TEXT format. We have to traverse the subregion
             * list because if more than one subregion is present, the text
             * region isn't guarentteed to be the first in the list, and
             * only text regions use this flag. All of this effort assists
             * with the rescaling of text that has been rendered at native
             * resolution, rather than video resolution.
             */
            while( p_text_region &&
                   p_text_region->fmt.i_chroma != VLC_FOURCC('T','E','X','T') )
            {
                p_text_region = p_text_region->p_next;
            }

            if( p_text_region &&
                ( ( p_text_region->i_align & SUBPICTURE_RENDERED ) == 0 ) )
            {
                if( p_subpic->i_original_picture_height > 0 &&
                    p_subpic->i_original_picture_width  > 0 )
                {
                    p_spu->p_text->fmt_out.video.i_width =
                    p_spu->p_text->fmt_out.video.i_visible_width =
                        p_subpic->i_original_picture_width;
                    p_spu->p_text->fmt_out.video.i_height =
                    p_spu->p_text->fmt_out.video.i_visible_height =
                        p_subpic->i_original_picture_height;
                }
                else
                {
                    p_spu->p_text->fmt_out.video.i_width =
                    p_spu->p_text->fmt_out.video.i_visible_width =
                        p_fmt->i_width;
                    p_spu->p_text->fmt_out.video.i_height =
                    p_spu->p_text->fmt_out.video.i_visible_height =
                        p_fmt->i_height;
                }
            }

            /* XXX for text:
             *  scale[] allows to pass from rendered size (by text module) to video output size */
            pi_scale_width[SCALE_TEXT] = p_fmt->i_width * 1000 /
                                          p_spu->p_text->fmt_out.video.i_width;
            pi_scale_height[SCALE_TEXT]= p_fmt->i_height * 1000 /
                                          p_spu->p_text->fmt_out.video.i_height;
        }
        else
        {
            /* Just set a value to avoid using invalid memory while looping over the array */
            pi_scale_width[SCALE_TEXT] =
            pi_scale_height[SCALE_TEXT]= 1000;
        }

        /* XXX for default:
         *  scale[] allows to pass from native (either video or original) size to output size */

        if( p_subpic->i_original_picture_height > 0 &&
            p_subpic->i_original_picture_width  > 0 )
        {
            pi_scale_width[SCALE_DEFAULT]  = p_fmt->i_width  * 1000 / p_subpic->i_original_picture_width;
            pi_scale_height[SCALE_DEFAULT] = p_fmt->i_height * 1000 / p_subpic->i_original_picture_height;
        }
        else
        {
            pi_scale_width[ SCALE_DEFAULT ]  = i_scale_width_orig;
            pi_scale_height[ SCALE_DEFAULT ] = i_scale_height_orig;
        }

        for( k = 0; k < SCALE_SIZE ; k++ )
        {
            /* Case of both width and height being specified has been dealt
             * with above by instead rendering to an output pane of the
             * explicit dimensions specified - we don't need to scale it.
             */
            if( p_subpic->i_original_picture_height > 0 &&
                p_subpic->i_original_picture_width <= 0 )
            {
                pi_scale_height[ k ] = pi_scale_height[ k ] * i_source_video_height /
                                 p_subpic->i_original_picture_height;
                pi_scale_width[ k ]  = pi_scale_width[ k ]  * i_source_video_height /
                                 p_subpic->i_original_picture_height;
            }
        }

        /* Set default subpicture aspect ratio */
        if( !p_region->fmt.i_sar_num || !p_region->fmt.i_sar_den )
        {
            if( p_region->fmt.i_aspect != 0 )
            {
                p_region->fmt.i_sar_den = p_region->fmt.i_aspect;
                p_region->fmt.i_sar_num = VOUT_ASPECT_FACTOR;
            }
            else
            {
                p_region->fmt.i_sar_den = p_fmt->i_sar_den;
                p_region->fmt.i_sar_num = p_fmt->i_sar_num;
            }
        }

        /* Take care of the aspect ratio */
        if( ( p_region->fmt.i_sar_num * p_fmt->i_sar_den ) !=
            ( p_region->fmt.i_sar_den * p_fmt->i_sar_num ) )
        {
            for( k = 0; k < SCALE_SIZE; k++ )
            {
                pi_scale_width[k] = pi_scale_width[ k ] *
                    (int64_t)p_region->fmt.i_sar_num * p_fmt->i_sar_den /
                    p_region->fmt.i_sar_den / p_fmt->i_sar_num;

                pi_subpic_x[k] = p_subpic->i_x * pi_scale_width[ k ] / 1000;
            }
        }

        /* Load the scaling module when needed */
        if( !p_spu->p_scale )
        {
            bool b_scale_used = false;

            for( k = 0; k < SCALE_SIZE; k++ )
            {
                const int i_scale_w = pi_scale_width[k];
                const int i_scale_h = pi_scale_height[k];
                if( ( i_scale_w > 0 && i_scale_w != 1000 ) || ( i_scale_h > 0 && i_scale_h != 1000 ) )
                    b_scale_used = true;
            }

            if( b_scale_used )
                SpuRenderCreateAndLoadScale( p_spu );
        }

        for( ; p_region != NULL; p_region = p_region->p_next )
            SpuRenderRegion( p_spu, p_pic_dst, p_pic_src,
                             p_subpic, p_region, i_scale_width_orig, i_scale_height_orig,
                             pi_subpic_x, pi_scale_width, pi_scale_height,
                             p_fmt );
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
subpicture_t *spu_SortSubpictures( spu_t *p_spu, mtime_t display_date,
                                   bool b_paused )
{
    int i_index, i_channel;
    subpicture_t *p_subpic = NULL;
    subpicture_t *p_ephemer;
    mtime_t      ephemer_date;

    /* Run subpicture filters */
    filter_chain_SubFilter( p_spu->p_chain, display_date );

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
                  p_spu->p_subpicture[i_index].i_start ) &&
                !( p_spu->p_subpicture[i_index].b_pausable &&
                   b_paused ) )
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
static void SpuClearChannel( spu_t *p_spu, int i_channel, bool b_locked )
{
    int          i_subpic;                               /* subpicture index */
    subpicture_t *p_subpic = NULL;                  /* first free subpicture */

    if( !b_locked )
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

    if( !b_locked )
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
        break;

    case SPU_CHANNEL_CLEAR:
        i = (int)va_arg( args, int );
        SpuClearChannel( p_spu, i, false );
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

    vlc_mutex_lock( &p_spu->subpicture_lock );

    p_spu->b_force_palette = false;
    p_spu->b_force_crop = false;

    if( var_Get( p_object, "highlight", &val ) || !val.b_bool )
    {
        vlc_mutex_unlock( &p_spu->subpicture_lock );
        return;
    }

    p_spu->b_force_crop = true;
    var_Get( p_object, "x-start", &val );
    p_spu->i_crop_x = val.i_int;
    var_Get( p_object, "y-start", &val );
    p_spu->i_crop_y = val.i_int;
    var_Get( p_object, "x-end", &val );
    p_spu->i_crop_width = val.i_int - p_spu->i_crop_x;
    var_Get( p_object, "y-end", &val );
    p_spu->i_crop_height = val.i_int - p_spu->i_crop_y;

    if( var_Get( p_object, "menu-palette", &val ) == VLC_SUCCESS )
    {
        memcpy( p_spu->palette, val.p_address, 16 );
        p_spu->b_force_palette = true;
    }
    vlc_mutex_unlock( &p_spu->subpicture_lock );

    msg_Dbg( p_object, "crop: %i,%i,%i,%i, palette forced: %i",
             p_spu->i_crop_x, p_spu->i_crop_y,
             p_spu->i_crop_width, p_spu->i_crop_height,
             p_spu->b_force_palette );
}

/*****************************************************************************
 * CropCallback: called when the highlight properties are changed
 *****************************************************************************
 * This callback is called from the input thread when we need cropping
 *****************************************************************************/
static int CropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_var; (void)oldval; (void)newval;
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
    (void)p_filter;
    subpicture_t *p_subpic = (subpicture_t *)malloc(sizeof(subpicture_t));
    if( !p_subpic ) return NULL;
    memset( p_subpic, 0, sizeof(subpicture_t) );
    p_subpic->b_absolute = true;

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
    if( !p_picture ) return NULL;
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
    (void)p_filter;
    if( p_pic )
    {
        free( p_pic->p_data_orig );
        free( p_pic );
    }
}

static int SubFilterCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_object); VLC_UNUSED(oldval);
    VLC_UNUSED(newval); VLC_UNUSED(psz_var);

    spu_t *p_spu = (spu_t *)p_data;
    vlc_mutex_lock( &p_spu->subpicture_lock );
    filter_chain_Reset( p_spu->p_chain, NULL, NULL );
    spu_ParseChain( p_spu );
    vlc_mutex_unlock( &p_spu->subpicture_lock );
    return VLC_SUCCESS;
}

static int sub_filter_allocation_init( filter_t *p_filter, void *p_data )
{
    spu_t *p_spu = (spu_t *)p_data;

    p_filter->pf_sub_buffer_new = sub_new_buffer;
    p_filter->pf_sub_buffer_del = sub_del_buffer;

    filter_owner_sys_t *p_sys = malloc( sizeof(filter_owner_sys_t) );
    if( !p_sys ) return VLC_EGENERIC;

    p_filter->p_owner = p_sys;
    spu_Control( p_spu, SPU_CHANNEL_REGISTER, &p_sys->i_channel );
    p_sys->p_spu = p_spu;

    return VLC_SUCCESS;
}

static void sub_filter_allocation_clear( filter_t *p_filter )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;
    SpuClearChannel( p_sys->p_spu, p_sys->i_channel, true );
    free( p_filter->p_owner );
}
