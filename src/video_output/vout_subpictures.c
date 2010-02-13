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
#include "vout_internal.h"
#include <vlc_image.h>

#include <assert.h>
#include <limits.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Number of simultaneous subpictures */
#define VOUT_MAX_SUBPICTURES (__MAX(VOUT_MAX_PICTURES, SPU_MAX_PREPARE_TIME/5000))

/* */
typedef struct
{
    subpicture_t *p_subpicture;
    bool          b_reject;
} spu_heap_entry_t;

typedef struct
{
    spu_heap_entry_t p_entry[VOUT_MAX_SUBPICTURES];

} spu_heap_t;

static void SpuHeapInit( spu_heap_t * );
static int  SpuHeapPush( spu_heap_t *, subpicture_t * );
static void SpuHeapDeleteAt( spu_heap_t *, int i_index );
static int  SpuHeapDeleteSubpicture( spu_heap_t *, subpicture_t * );
static void SpuHeapClean( spu_heap_t *p_heap );

struct spu_private_t
{
    vlc_mutex_t lock;   /* lock to protect all followings fields */

    spu_heap_t heap;

    int i_channel;             /**< number of subpicture channels registered */
    filter_t *p_blend;                            /**< alpha blending module */
    filter_t *p_text;                              /**< text renderer module */
    filter_t *p_scale_yuvp;                     /**< scaling module for YUVP */
    filter_t *p_scale;                    /**< scaling module (all but YUVP) */
    bool b_force_crop;                     /**< force cropping of subpicture */
    int i_crop_x, i_crop_y, i_crop_width, i_crop_height;       /**< cropping */

    int i_margin;                        /**< force position of a subpicture */
    bool b_force_palette;             /**< force palette of subpicture */
    uint8_t palette[4][4];                               /**< forced palette */

    /* Subpiture filters */
    char           *psz_chain_update;
    filter_chain_t *p_chain;

    /* */
    mtime_t i_last_sort_date;
};

/* */
struct subpicture_region_private_t
{
    video_format_t fmt;
    picture_t      *p_picture;
};
static subpicture_region_private_t *SpuRegionPrivateNew( video_format_t * );
static void SpuRegionPrivateDelete( subpicture_region_private_t * );

/* */
typedef struct
{
    int w;
    int h;
} spu_scale_t;
static spu_scale_t spu_scale_create( int w, int h );
static spu_scale_t spu_scale_unit(void );
static spu_scale_t spu_scale_createq( int wn, int wd, int hn, int hd );
static int spu_scale_w( int v, const spu_scale_t s );
static int spu_scale_h( int v, const spu_scale_t s );
static int spu_invscale_w( int v, const spu_scale_t s );
static int spu_invscale_h( int v, const spu_scale_t s );

typedef struct
{
    int i_x;
    int i_y;
    int i_width;
    int i_height;

    spu_scale_t scale;
} spu_area_t;

static spu_area_t spu_area_create( int x, int y, int w, int h, spu_scale_t );
static spu_area_t spu_area_scaled( spu_area_t );
static spu_area_t spu_area_unscaled( spu_area_t, spu_scale_t );
static bool spu_area_overlap( spu_area_t, spu_area_t );


/* Subpicture rendered flag
 * FIXME ? it could be moved to private ? */
#define SUBPICTURE_RENDERED  (0x1000)
#if SUBPICTURE_RENDERED < SUBPICTURE_ALIGN_MASK
#   error SUBPICTURE_RENDERED too low
#endif

#define SCALE_UNIT (1000)

static void SubpictureChain( subpicture_t **pp_head, subpicture_t *p_subpic );
static int SubpictureCmp( const void *s0, const void *s1 );

static void SpuRenderRegion( spu_t *,
                             picture_t *p_pic_dst, spu_area_t *,
                             subpicture_t *, subpicture_region_t *,
                             const spu_scale_t scale_size,
                             const video_format_t *p_fmt,
                             const spu_area_t *p_subtitle_area, int i_subtitle_area,
                             mtime_t render_date );

static void UpdateSPU   ( spu_t *, vlc_object_t * );
static int  CropCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int MarginCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

static int SpuControl( spu_t *, int, va_list );

static void SpuClearChannel( spu_t *p_spu, int i_channel );

/* Buffer allocation for SPU filter (blend, scale, ...) */
static subpicture_t *spu_new_buffer( filter_t * );
static void spu_del_buffer( filter_t *, subpicture_t * );
static picture_t *spu_new_video_buffer( filter_t * );
static void spu_del_video_buffer( filter_t *, picture_t * );

/* Buffer aloccation fir SUB filter */
static int SubFilterCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );

static int SubFilterAllocationInit( filter_t *, void * );
static void SubFilterAllocationClean( filter_t * );

/* */
static void SpuRenderCreateAndLoadText( spu_t * );
static void SpuRenderCreateAndLoadScale( spu_t * );
static void FilterRelease( filter_t *p_filter );

/*****************************************************************************
 * Public API
 *****************************************************************************/

#undef spu_Create
/**
 * Creates the subpicture unit
 *
 * \param p_this the parent object which creates the subpicture unit
 */
spu_t *spu_Create( vlc_object_t *p_this )
{
    spu_t *p_spu;
    spu_private_t *p_sys;
 
    p_spu = vlc_custom_create( p_this, sizeof(spu_t) + sizeof(spu_private_t),
                               VLC_OBJECT_GENERIC, "subpicture" );
    if( !p_spu )
        return NULL;
    vlc_object_attach( p_spu, p_this );

    /* Initialize spu fields */
    p_spu->pf_control = SpuControl;
    p_spu->p = p_sys = (spu_private_t*)&p_spu[1];

    /* Initialize private fields */
    vlc_mutex_init( &p_sys->lock );

    SpuHeapInit( &p_sys->heap );

    p_sys->p_blend = NULL;
    p_sys->p_text = NULL;
    p_sys->p_scale = NULL;
    p_sys->p_scale_yuvp = NULL;

    p_sys->i_margin = var_InheritInteger( p_spu, "sub-margin" );

    /* Register the default subpicture channel */
    p_sys->i_channel = 2;

    p_sys->psz_chain_update = NULL;
    p_sys->p_chain = filter_chain_New( p_spu, "sub filter", false,
                                       SubFilterAllocationInit,
                                       SubFilterAllocationClean,
                                       p_spu );

    /* Load text and scale module */
    SpuRenderCreateAndLoadText( p_spu );
    SpuRenderCreateAndLoadScale( p_spu );

    /* */
    p_sys->i_last_sort_date = -1;

    return p_spu;
}

/**
 * Initialise the subpicture unit
 *
 * \param p_spu the subpicture unit object
 */
int spu_Init( spu_t *p_spu )
{
    var_Create( p_spu, "sub-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_AddCallback( p_spu, "sub-filter", SubFilterCallback, p_spu );
    var_TriggerCallback( p_spu, "sub-filter" );

    return VLC_SUCCESS;
}

/**
 * Destroy the subpicture unit
 *
 * \param p_this the parent object which destroys the subpicture unit
 */
void spu_Destroy( spu_t *p_spu )
{
    spu_private_t *p_sys = p_spu->p;

    var_DelCallback( p_spu, "sub-filter", SubFilterCallback, p_spu );

    if( p_sys->p_blend )
        filter_DeleteBlend( p_sys->p_blend );

    if( p_sys->p_text )
        FilterRelease( p_sys->p_text );

    if( p_sys->p_scale_yuvp )
        FilterRelease( p_sys->p_scale_yuvp );

    if( p_sys->p_scale )
        FilterRelease( p_sys->p_scale );

    filter_chain_Delete( p_sys->p_chain );
    free( p_sys->psz_chain_update );

    /* Destroy all remaining subpictures */
    SpuHeapClean( &p_sys->heap );

    vlc_mutex_destroy( &p_sys->lock );

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
    if( !p_input )
        return;

    if( b_attach )
    {
        UpdateSPU( p_spu, VLC_OBJECT(p_input) );
        var_AddCallback( p_input, "highlight", CropCallback, p_spu );
        var_AddCallback( p_input, "sub-margin", MarginCallback, p_spu->p );

        vlc_mutex_lock( &p_spu->p->lock );
        p_spu->p->i_margin = var_GetInteger( p_input, "sub-margin" );
        vlc_mutex_unlock( &p_spu->p->lock );

        vlc_object_release( p_input );
    }
    else
    {
        /* Delete callback */
        var_DelCallback( p_input, "highlight", CropCallback, p_spu );
        var_DelCallback( p_input, "sub-margin", MarginCallback, p_spu->p );
        vlc_object_release( p_input );
    }
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
    spu_private_t *p_sys = p_spu->p;

    /* DEFAULT_CHAN always reset itself */
    if( p_subpic->i_channel == DEFAULT_CHAN )
        SpuClearChannel( p_spu, DEFAULT_CHAN );

    /* p_private is for spu only and cannot be non NULL here */
    for( subpicture_region_t *r = p_subpic->p_region; r != NULL; r = r->p_next )
        assert( r->p_private == NULL );

    /* */
    vlc_mutex_lock( &p_sys->lock );
    if( SpuHeapPush( &p_sys->heap, p_subpic ) )
    {
        vlc_mutex_unlock( &p_sys->lock );
        msg_Err( p_spu, "subpicture heap full" );
        subpicture_Delete( p_subpic );
        return;
    }
    vlc_mutex_unlock( &p_sys->lock );
}

/**
 * This function renders all sub picture units in the list.
 */
void spu_RenderSubpictures( spu_t *p_spu,
                            picture_t *p_pic_dst, const video_format_t *p_fmt_dst,
                            subpicture_t *p_subpic_list,
                            const video_format_t *p_fmt_src,
                            mtime_t render_subtitle_date )
{
    spu_private_t *p_sys = p_spu->p;

    const mtime_t render_osd_date = mdate();

    const int i_source_video_width  = p_fmt_src->i_width;
    const int i_source_video_height = p_fmt_src->i_height;

    unsigned int i_subpicture;
    subpicture_t *pp_subpicture[VOUT_MAX_SUBPICTURES];

    unsigned int i_subtitle_region_count;
    spu_area_t p_subtitle_area_buffer[VOUT_MAX_SUBPICTURES];
    spu_area_t *p_subtitle_area;
    int i_subtitle_area;

    vlc_mutex_lock( &p_sys->lock );

    /* Preprocess subpictures */
    i_subpicture = 0;
    i_subtitle_region_count = 0;
    for( subpicture_t * p_subpic = p_subpic_list;
            p_subpic != NULL;
                p_subpic = p_subpic->p_next )
    {
        if( p_subpic->pf_update_regions )
        {
            video_format_t fmt_org = *p_fmt_dst;
            fmt_org.i_width =
            fmt_org.i_visible_width = i_source_video_width;
            fmt_org.i_height =
            fmt_org.i_visible_height = i_source_video_height;

            p_subpic->pf_update_regions( p_spu, p_subpic, &fmt_org,
                                         p_subpic->b_subtitle ? render_subtitle_date : render_osd_date );
        }

        /* */
        if( p_subpic->b_subtitle )
        {
            for( subpicture_region_t *r = p_subpic->p_region; r != NULL; r = r->p_next )
                i_subtitle_region_count++;
        }

        /* */
        pp_subpicture[i_subpicture++] = p_subpic;
    }

    /* Be sure we have at least 1 picture to process */
    if( i_subpicture <= 0 )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return;
    }

    /* Now order subpicture array
     * XXX The order is *really* important for overlap subtitles positionning */
    qsort( pp_subpicture, i_subpicture, sizeof(*pp_subpicture), SubpictureCmp );

    /* Allocate area array for subtitle overlap */
    i_subtitle_area = 0;
    p_subtitle_area = p_subtitle_area_buffer;
    if( i_subtitle_region_count > sizeof(p_subtitle_area_buffer)/sizeof(*p_subtitle_area_buffer) )
        p_subtitle_area = calloc( i_subtitle_region_count, sizeof(*p_subtitle_area) );

    /* Create the blending module */
    if( !p_sys->p_blend )
        p_spu->p->p_blend = filter_NewBlend( VLC_OBJECT(p_spu), p_fmt_dst );

    /* Process all subpictures and regions (in the right order) */
    for( unsigned int i_index = 0; i_index < i_subpicture; i_index++ )
    {
        subpicture_t *p_subpic = pp_subpicture[i_index];
        subpicture_region_t *p_region;

        if( !p_subpic->p_region )
            continue;

        /* FIXME when possible use a better rendering size than source size
         * (max of display size and source size for example) FIXME */
        int i_render_width  = p_subpic->i_original_picture_width;
        int i_render_height = p_subpic->i_original_picture_height;
        if( !i_render_width || !i_render_height )
        {
            if( i_render_width != 0 || i_render_height != 0 )
                msg_Err( p_spu, "unsupported original picture size %dx%d",
                         i_render_width, i_render_height );

            p_subpic->i_original_picture_width  = i_render_width = i_source_video_width;
            p_subpic->i_original_picture_height = i_render_height = i_source_video_height;
        }

        if( p_sys->p_text )
        {
            p_sys->p_text->fmt_out.video.i_width          =
            p_sys->p_text->fmt_out.video.i_visible_width  = i_render_width;

            p_sys->p_text->fmt_out.video.i_height         =
            p_sys->p_text->fmt_out.video.i_visible_height = i_render_height;
        }

        /* Compute scaling from picture to source size */
        spu_scale_t scale = spu_scale_createq( i_source_video_width,  i_render_width,
                                               i_source_video_height, i_render_height );

        /* Update scaling from source size to display size(p_fmt_dst) */
        scale.w = scale.w * p_fmt_dst->i_width  / i_source_video_width;
        scale.h = scale.h * p_fmt_dst->i_height / i_source_video_height;

        /* Set default subpicture aspect ratio
         * FIXME if we only handle 1 aspect ratio per picture, why is it set per
         * region ? */
        p_region = p_subpic->p_region;
        if( !p_region->fmt.i_sar_num || !p_region->fmt.i_sar_den )
        {
            p_region->fmt.i_sar_den = p_fmt_dst->i_sar_den;
            p_region->fmt.i_sar_num = p_fmt_dst->i_sar_num;
        }

        /* Take care of the aspect ratio */
        if( p_region->fmt.i_sar_num * p_fmt_dst->i_sar_den !=
            p_region->fmt.i_sar_den * p_fmt_dst->i_sar_num )
        {
            /* FIXME FIXME what about region->i_x/i_y ? */
            scale.w = scale.w *
                (int64_t)p_region->fmt.i_sar_num * p_fmt_dst->i_sar_den /
                p_region->fmt.i_sar_den / p_fmt_dst->i_sar_num;
        }

        /* Render all regions
         * We always transform non absolute subtitle into absolute one on the
         * first rendering to allow good subtitle overlap support.
         */
        for( p_region = p_subpic->p_region; p_region != NULL; p_region = p_region->p_next )
        {
            spu_area_t area;

            /* Check scale validity */
            if( scale.w <= 0 || scale.h <= 0 )
                continue;

            /* */
            SpuRenderRegion( p_spu, p_pic_dst, &area,
                             p_subpic, p_region, scale, p_fmt_dst,
                             p_subtitle_area, i_subtitle_area,
                             p_subpic->b_subtitle ? render_subtitle_date : render_osd_date );

            if( p_subpic->b_subtitle )
            {
                area = spu_area_unscaled( area, scale );
                if( !p_subpic->b_absolute && area.i_width > 0 && area.i_height > 0 )
                {
                    p_region->i_x = area.i_x;
                    p_region->i_y = area.i_y;
                }
                if( p_subtitle_area )
                    p_subtitle_area[i_subtitle_area++] = area;
            }
        }
        if( p_subpic->b_subtitle )
            p_subpic->b_absolute = true;
    }

    /* */
    if( p_subtitle_area != p_subtitle_area_buffer )
        free( p_subtitle_area );

    vlc_mutex_unlock( &p_sys->lock );
}

/*****************************************************************************
 * spu_SortSubpictures: find the subpictures to display
 *****************************************************************************
 * This function parses all subpictures and decides which ones need to be
 * displayed. If no picture has been selected, display_date will depend on
 * the subpicture.
 * We also check for ephemer DVD subpictures (subpictures that have
 * to be removed if a newer one is available), which makes it a lot
 * more difficult to guess if a subpicture has to be rendered or not.
 *****************************************************************************/
subpicture_t *spu_SortSubpictures( spu_t *p_spu, mtime_t render_subtitle_date,
                                   bool b_subtitle_only )
{
    spu_private_t *p_sys = p_spu->p;
    int i_channel;
    subpicture_t *p_subpic = NULL;
    const mtime_t render_osd_date = mdate();

    /* Update sub-filter chain */
    vlc_mutex_lock( &p_sys->lock );
    char *psz_chain_update = p_sys->psz_chain_update;
    p_sys->psz_chain_update = NULL;
    vlc_mutex_unlock( &p_sys->lock );

    if( psz_chain_update )
    {
        filter_chain_Reset( p_sys->p_chain, NULL, NULL );

        filter_chain_AppendFromString( p_spu->p->p_chain, psz_chain_update );

        free( psz_chain_update );
    }

    /* Run subpicture filters */
    filter_chain_SubFilter( p_sys->p_chain, render_osd_date );

    vlc_mutex_lock( &p_sys->lock );

    /* We get an easily parsable chained list of subpictures which
     * ends with NULL since p_subpic was initialized to NULL. */
    for( i_channel = 0; i_channel < p_sys->i_channel; i_channel++ )
    {
        subpicture_t *p_available_subpic[VOUT_MAX_SUBPICTURES];
        bool         pb_available_late[VOUT_MAX_SUBPICTURES];
        int          i_available = 0;

        mtime_t      start_date = render_subtitle_date;
        mtime_t      ephemer_subtitle_date = 0;
        mtime_t      ephemer_osd_date = 0;
        int64_t      i_ephemer_subtitle_order = INT64_MIN;
        int64_t      i_ephemer_system_order = INT64_MIN;
        int i_index;

        /* Select available pictures */
        for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
        {
            spu_heap_entry_t *p_entry = &p_sys->heap.p_entry[i_index];
            subpicture_t *p_current = p_entry->p_subpicture;
            bool b_stop_valid;
            bool b_late;

            if( !p_current || p_entry->b_reject )
            {
                if( p_entry->b_reject )
                    SpuHeapDeleteAt( &p_sys->heap, i_index );
                continue;
            }

            if( p_current->i_channel != i_channel ||
                ( b_subtitle_only && !p_current->b_subtitle ) )
            {
                continue;
            }
            const mtime_t render_date = p_current->b_subtitle ? render_subtitle_date : render_osd_date;
            if( render_date &&
                render_date < p_current->i_start )
            {
                /* Too early, come back next monday */
                continue;
            }

            mtime_t *pi_ephemer_date  = p_current->b_subtitle ? &ephemer_subtitle_date : &ephemer_osd_date;
            int64_t *pi_ephemer_order = p_current->b_subtitle ? &i_ephemer_subtitle_order : &i_ephemer_system_order;
            if( p_current->i_start >= *pi_ephemer_date )
            {
                *pi_ephemer_date = p_current->i_start;
                if( p_current->i_order > *pi_ephemer_order )
                    *pi_ephemer_order = p_current->i_order;
            }

            b_stop_valid = !p_current->b_ephemer || p_current->i_stop > p_current->i_start;

            b_late = b_stop_valid && p_current->i_stop <= render_date;

            /* start_date will be used for correct automatic overlap support
             * in case picture that should not be displayed anymore (display_time)
             * overlap with a picture to be displayed (p_current->i_start)  */
            if( p_current->b_subtitle && !b_late && !p_current->b_ephemer )
                start_date = p_current->i_start;

            /* */
            p_available_subpic[i_available] = p_current;
            pb_available_late[i_available] = b_late;
            i_available++;
        }

        /* Only forced old picture display at the transition */
        if( start_date < p_sys->i_last_sort_date )
            start_date = p_sys->i_last_sort_date;
        if( start_date <= 0 )
            start_date = INT64_MAX;

        /* Select pictures to be displayed */
        for( i_index = 0; i_index < i_available; i_index++ )
        {
            subpicture_t *p_current = p_available_subpic[i_index];
            bool b_late = pb_available_late[i_index];

            const mtime_t stop_date = p_current->b_subtitle ? __MAX( start_date, p_sys->i_last_sort_date ) : render_osd_date;
            const mtime_t ephemer_date = p_current->b_subtitle ? ephemer_subtitle_date : ephemer_osd_date;
            const int64_t i_ephemer_order = p_current->b_subtitle ? i_ephemer_subtitle_order : i_ephemer_system_order;

            /* Destroy late and obsolete ephemer subpictures */
            bool b_rejet = b_late && p_current->i_stop <= stop_date;
            if( p_current->b_ephemer )
            {
                if( p_current->i_start < ephemer_date )
                    b_rejet = true;
                else if( p_current->i_start == ephemer_date &&
                         p_current->i_order < i_ephemer_order )
                    b_rejet = true;
            }

            if( b_rejet )
                SpuHeapDeleteSubpicture( &p_sys->heap, p_current );
            else
                SubpictureChain( &p_subpic, p_current );
        }
    }

    p_sys->i_last_sort_date = render_subtitle_date;
    vlc_mutex_unlock( &p_sys->lock );

    return p_subpic;
}

void spu_OffsetSubtitleDate( spu_t *p_spu, mtime_t i_duration )
{
    spu_private_t *p_sys = p_spu->p;

    vlc_mutex_lock( &p_sys->lock );
    for( int i = 0; i < VOUT_MAX_SUBPICTURES; i++ )
    {
        spu_heap_entry_t *p_entry = &p_sys->heap.p_entry[i];
        subpicture_t *p_current = p_entry->p_subpicture;

        if( p_current && p_current->b_subtitle )
        {
            if( p_current->i_start > 0 )
                p_current->i_start += i_duration;
            if( p_current->i_stop > 0 )
                p_current->i_stop += i_duration;
        }
    }
    vlc_mutex_unlock( &p_sys->lock );
}

/*****************************************************************************
 * subpicture_t allocation
 *****************************************************************************/
subpicture_t *subpicture_New( void )
{
    subpicture_t *p_subpic = calloc( 1, sizeof(*p_subpic) );
    if( !p_subpic )
        return NULL;

    p_subpic->i_order    = 0;
    p_subpic->b_absolute = true;
    p_subpic->b_fade     = false;
    p_subpic->b_subtitle = false;
    p_subpic->i_alpha    = 0xFF;
    p_subpic->p_region   = NULL;
    p_subpic->pf_destroy = NULL;
    p_subpic->p_sys      = NULL;

    return p_subpic;
}

void subpicture_Delete( subpicture_t *p_subpic )
{
    subpicture_region_ChainDelete( p_subpic->p_region );
    p_subpic->p_region = NULL;

    if( p_subpic->pf_destroy )
    {
        p_subpic->pf_destroy( p_subpic );
    }
    free( p_subpic );
}

static void SubpictureChain( subpicture_t **pp_head, subpicture_t *p_subpic )
{
    p_subpic->p_next = *pp_head;

    *pp_head = p_subpic;
}

subpicture_t *subpicture_NewFromPicture( vlc_object_t *p_obj,
                                         picture_t *p_picture, vlc_fourcc_t i_chroma )
{
    /* */
    video_format_t fmt_in = p_picture->format;

    /* */
    video_format_t fmt_out;
    fmt_out = fmt_in;
    fmt_out.i_chroma = i_chroma;

    /* */
    image_handler_t *p_image = image_HandlerCreate( p_obj );
    if( !p_image )
        return NULL;

    picture_t *p_pip = image_Convert( p_image, p_picture, &fmt_in, &fmt_out );

    image_HandlerDelete( p_image );

    if( !p_pip )
        return NULL;

    subpicture_t *p_subpic = subpicture_New();
    if( !p_subpic )
    {
         picture_Release( p_pip );
         return NULL;
    }

    p_subpic->i_original_picture_width  = fmt_out.i_width;
    p_subpic->i_original_picture_height = fmt_out.i_height;

    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 0;

    p_subpic->p_region = subpicture_region_New( &fmt_out );
    if( p_subpic->p_region )
    {
        picture_Release( p_subpic->p_region->p_picture );
        p_subpic->p_region->p_picture = p_pip;
    }
    else
    {
        picture_Release( p_pip );
    }
    return p_subpic;
}

/*****************************************************************************
 * subpicture_region_t allocation
 *****************************************************************************/
subpicture_region_t *subpicture_region_New( const video_format_t *p_fmt )
{
    subpicture_region_t *p_region = calloc( 1, sizeof(*p_region ) );
    if( !p_region )
        return NULL;

    p_region->fmt = *p_fmt;
    p_region->fmt.p_palette = NULL;
    if( p_fmt->i_chroma == VLC_CODEC_YUVP )
    {
        p_region->fmt.p_palette = calloc( 1, sizeof(*p_region->fmt.p_palette) );
        if( p_fmt->p_palette )
            *p_region->fmt.p_palette = *p_fmt->p_palette;
    }
    p_region->i_alpha = 0xff;
    p_region->p_next = NULL;
    p_region->p_private = NULL;
    p_region->psz_text = NULL;
    p_region->p_style = NULL;
    p_region->p_picture = NULL;

    if( p_fmt->i_chroma == VLC_CODEC_TEXT )
        return p_region;

    p_region->p_picture = picture_NewFromFormat( p_fmt );
    if( !p_region->p_picture )
    {
        free( p_fmt->p_palette );
        free( p_region );
        return NULL;
    }

    return p_region;
}

/* */
void subpicture_region_Delete( subpicture_region_t *p_region )
{
    if( !p_region )
        return;

    if( p_region->p_private )
        SpuRegionPrivateDelete( p_region->p_private );

    if( p_region->p_picture )
        picture_Release( p_region->p_picture );

    free( p_region->fmt.p_palette );

    free( p_region->psz_text );
    free( p_region->psz_html );
    if( p_region->p_style )
        text_style_Delete( p_region->p_style );
    free( p_region );
}

/* */
void subpicture_region_ChainDelete( subpicture_region_t *p_head )
{
    while( p_head )
    {
        subpicture_region_t *p_next = p_head->p_next;

        subpicture_region_Delete( p_head );

        p_head = p_next;
    }
}



/*****************************************************************************
 * heap managment
 *****************************************************************************/
static void SpuHeapInit( spu_heap_t *p_heap )
{
    for( int i = 0; i < VOUT_MAX_SUBPICTURES; i++ )
    {
        spu_heap_entry_t *e = &p_heap->p_entry[i];

        e->p_subpicture = NULL;
        e->b_reject = false;
    }
}

static int SpuHeapPush( spu_heap_t *p_heap, subpicture_t *p_subpic )
{
    for( int i = 0; i < VOUT_MAX_SUBPICTURES; i++ )
    {
        spu_heap_entry_t *e = &p_heap->p_entry[i];

        if( e->p_subpicture )
            continue;

        e->p_subpicture = p_subpic;
        e->b_reject = false;
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static void SpuHeapDeleteAt( spu_heap_t *p_heap, int i_index )
{
    spu_heap_entry_t *e = &p_heap->p_entry[i_index];

    if( e->p_subpicture )
        subpicture_Delete( e->p_subpicture );

    e->p_subpicture = NULL;
}

static int SpuHeapDeleteSubpicture( spu_heap_t *p_heap, subpicture_t *p_subpic )
{
    for( int i = 0; i < VOUT_MAX_SUBPICTURES; i++ )
    {
        spu_heap_entry_t *e = &p_heap->p_entry[i];

        if( e->p_subpicture != p_subpic )
            continue;

        SpuHeapDeleteAt( p_heap, i );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static void SpuHeapClean( spu_heap_t *p_heap )
{
    for( int i = 0; i < VOUT_MAX_SUBPICTURES; i++ )
    {
        spu_heap_entry_t *e = &p_heap->p_entry[i];
        if( e->p_subpicture )
            subpicture_Delete( e->p_subpicture );
    }
}

static subpicture_region_private_t *SpuRegionPrivateNew( video_format_t *p_fmt )
{
    subpicture_region_private_t *p_private = malloc( sizeof(*p_private) );

    if( !p_private )
        return NULL;

    p_private->fmt = *p_fmt;
    if( p_fmt->p_palette )
    {
        p_private->fmt.p_palette = malloc( sizeof(*p_private->fmt.p_palette) );
        if( p_private->fmt.p_palette )
            *p_private->fmt.p_palette = *p_fmt->p_palette;
    }
    p_private->p_picture = NULL;

    return p_private;
}
static void SpuRegionPrivateDelete( subpicture_region_private_t *p_private )
{
    if( p_private->p_picture )
        picture_Release( p_private->p_picture );
    free( p_private->fmt.p_palette );
    free( p_private );
}

static void FilterRelease( filter_t *p_filter )
{
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );

    vlc_object_release( p_filter );
}

static void SpuRenderCreateAndLoadText( spu_t *p_spu )
{
    filter_t *p_text;

    assert( !p_spu->p->p_text );

    p_spu->p->p_text =
    p_text        = vlc_custom_create( p_spu, sizeof(filter_t),
                                       VLC_OBJECT_GENERIC, "spu text" );
    if( !p_text )
        return;

    es_format_Init( &p_text->fmt_in, VIDEO_ES, 0 );

    es_format_Init( &p_text->fmt_out, VIDEO_ES, 0 );
    p_text->fmt_out.video.i_width =
    p_text->fmt_out.video.i_visible_width = 32;
    p_text->fmt_out.video.i_height =
    p_text->fmt_out.video.i_visible_height = 32;

    p_text->pf_sub_buffer_new = spu_new_buffer;
    p_text->pf_sub_buffer_del = spu_del_buffer;

    vlc_object_attach( p_text, p_spu );

    /* FIXME TOCHECK shouldn't module_need( , , psz_modulename, false ) do the
     * same than these 2 calls ? */
    char *psz_modulename = var_CreateGetString( p_spu, "text-renderer" );
    if( psz_modulename && *psz_modulename )
    {
        p_text->p_module = module_need( p_text, "text renderer",
                                        psz_modulename, true );
    }
    free( psz_modulename );

    if( !p_text->p_module )
        p_text->p_module = module_need( p_text, "text renderer", NULL, false );

    /* Create a few variables used for enhanced text rendering */
    var_Create( p_text, "spu-duration", VLC_VAR_TIME );
    var_Create( p_text, "spu-elapsed", VLC_VAR_TIME );
    var_Create( p_text, "text-rerender", VLC_VAR_BOOL );
    var_Create( p_text, "scale", VLC_VAR_INTEGER );
}

static filter_t *CreateAndLoadScale( vlc_object_t *p_obj,
                                     vlc_fourcc_t i_src_chroma, vlc_fourcc_t i_dst_chroma,
                                     bool b_resize )
{
    filter_t *p_scale;

    p_scale = vlc_custom_create( p_obj, sizeof(filter_t),
                                 VLC_OBJECT_GENERIC, "scale" );
    if( !p_scale )
        return NULL;

    es_format_Init( &p_scale->fmt_in, VIDEO_ES, 0 );
    p_scale->fmt_in.video.i_chroma = i_src_chroma;
    p_scale->fmt_in.video.i_width =
    p_scale->fmt_in.video.i_height = 32;

    es_format_Init( &p_scale->fmt_out, VIDEO_ES, 0 );
    p_scale->fmt_out.video.i_chroma = i_dst_chroma;
    p_scale->fmt_out.video.i_width =
    p_scale->fmt_out.video.i_height = b_resize ? 16 : 32;

    p_scale->pf_video_buffer_new = spu_new_video_buffer;
    p_scale->pf_video_buffer_del = spu_del_video_buffer;

    vlc_object_attach( p_scale, p_obj );
    p_scale->p_module = module_need( p_scale, "video filter2", NULL, false );

    return p_scale;
}
static void SpuRenderCreateAndLoadScale( spu_t *p_spu )
{
    assert( !p_spu->p->p_scale );
    assert( !p_spu->p->p_scale_yuvp );
    /* XXX p_spu->p_scale is used for all conversion/scaling except yuvp to
     * yuva/rgba */
    p_spu->p->p_scale = CreateAndLoadScale( VLC_OBJECT(p_spu),
                                            VLC_CODEC_YUVA, VLC_CODEC_YUVA, true );
    /* This one is used for YUVP to YUVA/RGBA without scaling
     * FIXME rename it */
    p_spu->p->p_scale_yuvp = CreateAndLoadScale( VLC_OBJECT(p_spu),
                                                 VLC_CODEC_YUVP, VLC_CODEC_YUVA, false );
}

static void SpuRenderText( spu_t *p_spu, bool *pb_rerender_text,
                           subpicture_t *p_subpic, subpicture_region_t *p_region,
                           int i_min_scale_ratio, mtime_t render_date )
{
    filter_t *p_text = p_spu->p->p_text;

    assert( p_region->fmt.i_chroma == VLC_CODEC_TEXT );

    if( !p_text || !p_text->p_module )
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
    var_SetTime( p_text, "spu-duration", p_subpic->i_stop - p_subpic->i_start );
    var_SetTime( p_text, "spu-elapsed", render_date );
    var_SetBool( p_text, "text-rerender", false );
    var_SetInteger( p_text, "scale", i_min_scale_ratio );

    if( p_text->pf_render_html && p_region->psz_html )
    {
        p_text->pf_render_html( p_text, p_region, p_region );
    }
    else if( p_text->pf_render_text )
    {
        p_text->pf_render_text( p_text, p_region, p_region );
    }
    *pb_rerender_text = var_GetBool( p_text, "text-rerender" );

exit:
    p_region->i_align |= SUBPICTURE_RENDERED;
}

/**
 * A few scale functions helpers.
 */
static spu_scale_t spu_scale_create( int w, int h )
{
    spu_scale_t s = { .w = w, .h = h };
    if( s.w <= 0 )
        s.w = SCALE_UNIT;
    if( s.h <= 0 )
        s.h = SCALE_UNIT;
    return s;
}
static spu_scale_t spu_scale_unit( void )
{
    return spu_scale_create( SCALE_UNIT, SCALE_UNIT );
}
static spu_scale_t spu_scale_createq( int wn, int wd, int hn, int hd )
{
    return spu_scale_create( wn * SCALE_UNIT / wd,
                             hn * SCALE_UNIT / hd );
}
static int spu_scale_w( int v, const spu_scale_t s )
{
    return v * s.w / SCALE_UNIT;
}
static int spu_scale_h( int v, const spu_scale_t s )
{
    return v * s.h / SCALE_UNIT;
}
static int spu_invscale_w( int v, const spu_scale_t s )
{
    return v * SCALE_UNIT / s.w;
}
static int spu_invscale_h( int v, const spu_scale_t s )
{
    return v * SCALE_UNIT / s.h;
}

/**
 * A few area functions helpers
 */
static spu_area_t spu_area_create( int x, int y, int w, int h, spu_scale_t s )
{
    spu_area_t a = { .i_x = x, .i_y = y, .i_width = w, .i_height = h, .scale = s };
    return a;
}
static spu_area_t spu_area_scaled( spu_area_t a )
{
    if( a.scale.w == SCALE_UNIT && a.scale.h == SCALE_UNIT )
        return a;

    a.i_x = spu_scale_w( a.i_x, a.scale );
    a.i_y = spu_scale_h( a.i_y, a.scale );

    a.i_width  = spu_scale_w( a.i_width,  a.scale );
    a.i_height = spu_scale_h( a.i_height, a.scale );

    a.scale = spu_scale_unit();
    return a;
}
static spu_area_t spu_area_unscaled( spu_area_t a, spu_scale_t s )
{
    if( a.scale.w == s.w && a.scale.h == s.h )
        return a;

    a = spu_area_scaled( a );

    a.i_x = spu_invscale_w( a.i_x, s );
    a.i_y = spu_invscale_h( a.i_y, s );

    a.i_width  = spu_invscale_w( a.i_width, s );
    a.i_height = spu_invscale_h( a.i_height, s );

    a.scale = s;
    return a;
}
static bool spu_area_overlap( spu_area_t a, spu_area_t b )
{
    const int i_dx = 0;
    const int i_dy = 0;

    a = spu_area_scaled( a );
    b = spu_area_scaled( b );

    return  __MAX( a.i_x-i_dx, b.i_x ) < __MIN( a.i_x+a.i_width +i_dx, b.i_x+b.i_width  ) &&
            __MAX( a.i_y-i_dy, b.i_y ) < __MIN( a.i_y+a.i_height+i_dy, b.i_y+b.i_height );
}

/**
 * Avoid area overlapping
 */
static void SpuAreaFixOverlap( spu_area_t *p_dst,
                               const spu_area_t *p_sub, int i_sub, int i_align )
{
    spu_area_t a = spu_area_scaled( *p_dst );
    bool b_moved = false;
    bool b_ok;

    /* Check for overlap
     * XXX It is not fast O(n^2) but we should not have a lot of region */
    do
    {
        b_ok = true;
        for( int i = 0; i < i_sub; i++ )
        {
            spu_area_t sub = spu_area_scaled( p_sub[i] );

            if( !spu_area_overlap( a, sub ) )
                continue;

            if( i_align & SUBPICTURE_ALIGN_TOP )
            {
                /* We go down */
                int i_y = sub.i_y + sub.i_height;
                a.i_y = i_y;
                b_moved = true;
            }
            else if( i_align & SUBPICTURE_ALIGN_BOTTOM )
            {
                /* We go up */
                int i_y = sub.i_y - a.i_height;
                a.i_y = i_y;
                b_moved = true;
            }
            else
            {
                /* TODO what to do in this case? */
                //fprintf( stderr, "Overlap with unsupported alignment\n" );
                break;
            }

            b_ok = false;
            break;
        }
    } while( !b_ok );

    if( b_moved )
        *p_dst = spu_area_unscaled( a, p_dst->scale );
}


static void SpuAreaFitInside( spu_area_t *p_area, const spu_area_t *p_boundary )
{
  spu_area_t a = spu_area_scaled( *p_area );

  const int i_error_x = (a.i_x + a.i_width) - p_boundary->i_width;
  if( i_error_x > 0 )
      a.i_x -= i_error_x;
  if( a.i_x < 0 )
      a.i_x = 0;

  const int i_error_y = (a.i_y + a.i_height) - p_boundary->i_height;
  if( i_error_y > 0 )
      a.i_y -= i_error_y;
  if( a.i_y < 0 )
      a.i_y = 0;

  *p_area = spu_area_unscaled( a, p_area->scale );
}

/**
 * Place a region
 */
static void SpuRegionPlace( int *pi_x, int *pi_y,
                            const subpicture_t *p_subpic,
                            const subpicture_region_t *p_region )
{
    const int i_delta_x = p_region->i_x;
    const int i_delta_y = p_region->i_y;
    int i_x, i_y;

    assert( p_region->i_x != INT_MAX && p_region->i_y != INT_MAX );
    if( p_region->i_align & SUBPICTURE_ALIGN_TOP )
    {
        i_y = i_delta_y;
    }
    else if( p_region->i_align & SUBPICTURE_ALIGN_BOTTOM )
    {
        i_y = p_subpic->i_original_picture_height - p_region->fmt.i_height - i_delta_y;
    }
    else
    {
        i_y = p_subpic->i_original_picture_height / 2 - p_region->fmt.i_height / 2;
    }

    if( p_region->i_align & SUBPICTURE_ALIGN_LEFT )
    {
        i_x = i_delta_x;
    }
    else if( p_region->i_align & SUBPICTURE_ALIGN_RIGHT )
    {
        i_x = p_subpic->i_original_picture_width - p_region->fmt.i_width - i_delta_x;
    }
    else
    {
        i_x = p_subpic->i_original_picture_width / 2 - p_region->fmt.i_width / 2;
    }

    if( p_subpic->b_absolute )
    {
        i_x = i_delta_x;
        i_y = i_delta_y;
    }

    /* Margin shifts all subpictures */
    /* NOTE We have margin only for subtitles, so we don't really need this here
    if( i_margin_y != 0 )
        i_y -= i_margin_y;*/

    /* Clamp offset to not go out of the screen (when possible) */
    /* NOTE Again, useful only for subtitles, otherwise goes against the alignment logic above
    const int i_error_x = (i_x + p_region->fmt.i_width) - p_subpic->i_original_picture_width;
    if( i_error_x > 0 )
        i_x -= i_error_x;
    if( i_x < 0 )
        i_x = 0;

    const int i_error_y = (i_y + p_region->fmt.i_height) - p_subpic->i_original_picture_height;
    if( i_error_y > 0 )
        i_y -= i_error_y;
    if( i_y < 0 )
        i_y = 0;*/

    *pi_x = i_x;
    *pi_y = i_y;
}

/**
 * This function computes the current alpha value for a given region.
 */
static int SpuRegionAlpha( subpicture_t *p_subpic, subpicture_region_t *p_region )
{
    /* Compute alpha blend value */
    int i_fade_alpha = 255;
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
    return i_fade_alpha * p_subpic->i_alpha * p_region->i_alpha / 65025;
}

/**
 * It will render the provided region onto p_pic_dst.
 */

static void SpuRenderRegion( spu_t *p_spu,
                             picture_t *p_pic_dst, spu_area_t *p_area,
                             subpicture_t *p_subpic, subpicture_region_t *p_region,
                             const spu_scale_t scale_size,
                             const video_format_t *p_fmt,
                             const spu_area_t *p_subtitle_area, int i_subtitle_area,
                             mtime_t render_date )
{
    spu_private_t *p_sys = p_spu->p;

    video_format_t fmt_original = p_region->fmt;
    bool b_rerender_text = false;
    bool b_restore_format = false;
    int i_x_offset;
    int i_y_offset;

    video_format_t region_fmt;
    picture_t *p_region_picture;

    /* Invalidate area by default */
    *p_area = spu_area_create( 0,0, 0,0, scale_size );

    /* Render text region */
    if( p_region->fmt.i_chroma == VLC_CODEC_TEXT )
    {
        const int i_min_scale_ratio = SCALE_UNIT; /* FIXME what is the right value? (scale_size is not) */
        SpuRenderText( p_spu, &b_rerender_text, p_subpic, p_region,
                       i_min_scale_ratio, render_date );
        b_restore_format = b_rerender_text;

        /* Check if the rendering has failed ... */
        if( p_region->fmt.i_chroma == VLC_CODEC_TEXT )
            goto exit;
    }

    /* Force palette if requested
     * FIXME b_force_palette and b_force_crop are applied to all subpictures using palette
     * instead of only the right one (being the dvd spu).
     */
    const bool b_using_palette = p_region->fmt.i_chroma == VLC_CODEC_YUVP;
    const bool b_force_palette = b_using_palette && p_sys->b_force_palette;
    const bool b_force_crop    = b_force_palette && p_sys->b_force_crop;
    bool b_changed_palette     = false;


    /* Compute the margin which is expressed in destination pixel unit
     * The margin is applied only to subtitle and when no forced crop is
     * requested (dvd menu) */
    int i_margin_y = 0;
    if( !b_force_crop && p_subpic->b_subtitle )
        i_margin_y = spu_invscale_h( p_sys->i_margin, scale_size );

    /* Place the picture
     * We compute the position in the rendered size */
    SpuRegionPlace( &i_x_offset, &i_y_offset,
                    p_subpic, p_region );

    /* Save this position for subtitle overlap support
     * it is really important that there are given without scale_size applied */
    *p_area = spu_area_create( i_x_offset, i_y_offset,
                               p_region->fmt.i_width, p_region->fmt.i_height,
                               scale_size );

    /* Handle overlapping subtitles when possible */
    if( p_subpic->b_subtitle && !p_subpic->b_absolute )
    {
        SpuAreaFixOverlap( p_area, p_subtitle_area, i_subtitle_area,
                           p_region->i_align );
    }

    /* we copy the area: for the subtitle overlap support we want
     * to only save the area without margin applied */
    spu_area_t restrained = *p_area;

    /* apply margin to subtitles and correct if they go over the picture edge */
    if( p_subpic->b_subtitle )
    {
        restrained.i_y -= i_margin_y;
        spu_area_t display = spu_area_create( 0, 0, p_fmt->i_width, p_fmt->i_height,
                                              spu_scale_unit() );
        SpuAreaFitInside( &restrained, &display );
    }

    /* Fix the position for the current scale_size */
    i_x_offset = spu_scale_w( restrained.i_x, restrained.scale );
    i_y_offset = spu_scale_h( restrained.i_y, restrained.scale );

    /* */
    if( b_force_palette )
    {
        video_palette_t *p_palette = p_region->fmt.p_palette;
        video_palette_t palette;

        /* We suppose DVD palette here */
        palette.i_entries = 4;
        for( int i = 0; i < 4; i++ )
            for( int j = 0; j < 4; j++ )
                palette.palette[i][j] = p_sys->palette[i][j];

        if( p_palette->i_entries == palette.i_entries )
        {
            for( int i = 0; i < p_palette->i_entries; i++ )
                for( int j = 0; j < 4; j++ )
                    b_changed_palette |= p_palette->palette[i][j] != palette.palette[i][j];
        }
        else
        {
            b_changed_palette = true;
        }
        *p_palette = palette;
    }

    /* */
    region_fmt = p_region->fmt;
    p_region_picture = p_region->p_picture;


    /* Scale from rendered size to destination size */
    if( p_sys->p_scale && p_sys->p_scale->p_module &&
        ( !b_using_palette || ( p_sys->p_scale_yuvp && p_sys->p_scale_yuvp->p_module ) ) &&
        ( scale_size.w != SCALE_UNIT || scale_size.h != SCALE_UNIT || b_using_palette ) )
    {
        const unsigned i_dst_width  = spu_scale_w( p_region->fmt.i_width, scale_size );
        const unsigned i_dst_height = spu_scale_h( p_region->fmt.i_height, scale_size );

        /* Destroy the cache if unusable */
        if( p_region->p_private )
        {
            subpicture_region_private_t *p_private = p_region->p_private;
            bool b_changed = false;

            /* Check resize changes */
            if( i_dst_width  != p_private->fmt.i_width ||
                i_dst_height != p_private->fmt.i_height )
                b_changed = true;

            /* Check forced palette changes */
            if( b_changed_palette )
                b_changed = true;

            if( b_changed )
            {
                SpuRegionPrivateDelete( p_private );
                p_region->p_private = NULL;
            }
        }

        /* Scale if needed into cache */
        if( !p_region->p_private && i_dst_width > 0 && i_dst_height > 0 )
        {
            filter_t *p_scale = p_sys->p_scale;

            picture_t *p_picture = p_region->p_picture;
            picture_Hold( p_picture );

            /* Convert YUVP to YUVA/RGBA first for better scaling quality */
            if( b_using_palette )
            {
                filter_t *p_scale_yuvp = p_sys->p_scale_yuvp;

                p_scale_yuvp->fmt_in.video = p_region->fmt;

                /* TODO converting to RGBA for RGB video output is better */
                p_scale_yuvp->fmt_out.video = p_region->fmt;
                p_scale_yuvp->fmt_out.video.i_chroma = VLC_CODEC_YUVA;

                p_picture = p_scale_yuvp->pf_video_filter( p_scale_yuvp, p_picture );
                if( !p_picture )
                {
                    /* Well we will try conversion+scaling */
                    msg_Warn( p_spu, "%4.4s to %4.4s conversion failed",
                             (const char*)&p_scale_yuvp->fmt_in.video.i_chroma,
                             (const char*)&p_scale_yuvp->fmt_out.video.i_chroma );
                }
            }

            /* Conversion(except from YUVP)/Scaling */
            if( p_picture &&
                ( p_picture->format.i_width != i_dst_width ||
                  p_picture->format.i_height != i_dst_height ) )
            {
                p_scale->fmt_in.video = p_picture->format;
                p_scale->fmt_out.video = p_picture->format;

                p_scale->fmt_out.video.i_width = i_dst_width;
                p_scale->fmt_out.video.i_height = i_dst_height;

                p_scale->fmt_out.video.i_visible_width =
                    spu_scale_w( p_region->fmt.i_visible_width, scale_size );
                p_scale->fmt_out.video.i_visible_height =
                    spu_scale_h( p_region->fmt.i_visible_height, scale_size );

                p_picture = p_scale->pf_video_filter( p_scale, p_picture );
                if( !p_picture )
                    msg_Err( p_spu, "scaling failed" );
            }

            /* */
            if( p_picture )
            {
                p_region->p_private = SpuRegionPrivateNew( &p_picture->format );
                if( p_region->p_private )
                {
                    p_region->p_private->p_picture = p_picture;
                    if( !p_region->p_private->p_picture )
                    {
                        SpuRegionPrivateDelete( p_region->p_private );
                        p_region->p_private = NULL;
                    }
                }
                else
                {
                    picture_Release( p_picture );
                }
            }
        }

        /* And use the scaled picture */
        if( p_region->p_private )
        {
            region_fmt = p_region->p_private->fmt;
            p_region_picture = p_region->p_private->p_picture;
        }
    }

    /* Force cropping if requested */
    if( b_force_crop )
    {
        int i_crop_x = spu_scale_w( p_sys->i_crop_x, scale_size );
        int i_crop_y = spu_scale_h( p_sys->i_crop_y, scale_size );
        int i_crop_width = spu_scale_w( p_sys->i_crop_width, scale_size );
        int i_crop_height= spu_scale_h( p_sys->i_crop_height,scale_size );

        /* Find the intersection */
        if( i_crop_x + i_crop_width <= i_x_offset ||
            i_x_offset + (int)region_fmt.i_visible_width < i_crop_x ||
            i_crop_y + i_crop_height <= i_y_offset ||
            i_y_offset + (int)region_fmt.i_visible_height < i_crop_y )
        {
            /* No intersection */
            region_fmt.i_visible_width =
            region_fmt.i_visible_height = 0;
        }
        else
        {
            int i_x, i_y, i_x_end, i_y_end;
            i_x = __MAX( i_crop_x, i_x_offset );
            i_y = __MAX( i_crop_y, i_y_offset );
            i_x_end = __MIN( i_crop_x + i_crop_width,
                           i_x_offset + (int)region_fmt.i_visible_width );
            i_y_end = __MIN( i_crop_y + i_crop_height,
                           i_y_offset + (int)region_fmt.i_visible_height );

            region_fmt.i_x_offset = i_x - i_x_offset;
            region_fmt.i_y_offset = i_y - i_y_offset;
            region_fmt.i_visible_width = i_x_end - i_x;
            region_fmt.i_visible_height = i_y_end - i_y;

            i_x_offset = __MAX( i_x, 0 );
            i_y_offset = __MAX( i_y, 0 );
        }
    }

    /* Update the blender */
    if( filter_ConfigureBlend( p_spu->p->p_blend,
                               p_fmt->i_width, p_fmt->i_height,
                               &region_fmt ) ||
        filter_Blend( p_spu->p->p_blend,
                      p_pic_dst, i_x_offset, i_y_offset,
                      p_region_picture, SpuRegionAlpha( p_subpic, p_region ) ) )
    {
        msg_Err( p_spu, "blending %4.4s to %4.4s failed",
                 (char *)&p_sys->p_blend->fmt_in.video.i_chroma,
                 (char *)&p_sys->p_blend->fmt_out.video.i_chroma );
    }

exit:
    if( b_rerender_text )
    {
        /* Some forms of subtitles need to be re-rendered more than
         * once, eg. karaoke. We therefore restore the region to its
         * pre-rendered state, so the next time through everything is
         * calculated again.
         */
        if( p_region->p_picture )
        {
            picture_Release( p_region->p_picture );
            p_region->p_picture = NULL;
        }
        if( p_region->p_private )
        {
            SpuRegionPrivateDelete( p_region->p_private );
            p_region->p_private = NULL;
        }
        p_region->i_align &= ~SUBPICTURE_RENDERED;
    }
    if( b_restore_format )
        p_region->fmt = fmt_original;
}

/**
 * This function compares two 64 bits integers.
 * It can be used by qsort.
 */
static int IntegerCmp( int64_t i0, int64_t i1 )
{
    return i0 < i1 ? -1 : i0 > i1 ? 1 : 0;
}
/**
 * This function compares 2 subpictures using the following properties
 * (ordered by priority)
 * 1. absolute positionning
 * 2. start time
 * 3. creation order (per channel)
 *
 * It can be used by qsort.
 *
 * XXX spu_RenderSubpictures depends heavily on this order.
 */
static int SubpictureCmp( const void *s0, const void *s1 )
{
    subpicture_t *p_subpic0 = *(subpicture_t**)s0;
    subpicture_t *p_subpic1 = *(subpicture_t**)s1;
    int r;

    r = IntegerCmp( !p_subpic0->b_absolute, !p_subpic1->b_absolute );
    if( !r )
        r = IntegerCmp( p_subpic0->i_start, p_subpic1->i_start );
    if( !r )
        r = IntegerCmp( p_subpic0->i_channel, p_subpic1->i_channel );
    if( !r )
        r = IntegerCmp( p_subpic0->i_order, p_subpic1->i_order );
    return r;
}

/*****************************************************************************
 * SpuClearChannel: clear an spu channel
 *****************************************************************************
 * This function destroys the subpictures which belong to the spu channel
 * corresponding to i_channel_id.
 *****************************************************************************/
static void SpuClearChannel( spu_t *p_spu, int i_channel )
{
    spu_private_t *p_sys = p_spu->p;
    int          i_subpic;                               /* subpicture index */

    vlc_mutex_lock( &p_sys->lock );

    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        spu_heap_entry_t *p_entry = &p_sys->heap.p_entry[i_subpic];
        subpicture_t *p_subpic = p_entry->p_subpicture;

        if( !p_subpic )
            continue;
        if( p_subpic->i_channel != i_channel && ( i_channel != -1 || p_subpic->i_channel == DEFAULT_CHAN ) )
            continue;

        /* You cannot delete subpicture outside of spu_SortSubpictures */
        p_entry->b_reject = true;
    }

    vlc_mutex_unlock( &p_sys->lock );
}

/*****************************************************************************
 * spu_ControlDefault: default methods for the subpicture unit control.
 *****************************************************************************/
static int SpuControl( spu_t *p_spu, int i_query, va_list args )
{
    spu_private_t *p_sys = p_spu->p;
    int *pi, i;

    switch( i_query )
    {
    case SPU_CHANNEL_REGISTER:
        pi = (int *)va_arg( args, int * );
        vlc_mutex_lock( &p_sys->lock );
        if( pi )
            *pi = p_sys->i_channel++;
        vlc_mutex_unlock( &p_sys->lock );
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
    spu_private_t *p_sys = p_spu->p;
    vlc_value_t val;

    vlc_mutex_lock( &p_sys->lock );

    p_sys->b_force_palette = false;
    p_sys->b_force_crop = false;

    if( var_Get( p_object, "highlight", &val ) || !val.b_bool )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return;
    }

    p_sys->b_force_crop = true;
    p_sys->i_crop_x = var_GetInteger( p_object, "x-start" );
    p_sys->i_crop_y = var_GetInteger( p_object, "y-start" );
    p_sys->i_crop_width  = var_GetInteger( p_object, "x-end" ) - p_sys->i_crop_x;
    p_sys->i_crop_height = var_GetInteger( p_object, "y-end" ) - p_sys->i_crop_y;

    if( var_Get( p_object, "menu-palette", &val ) == VLC_SUCCESS )
    {
        memcpy( p_sys->palette, val.p_address, 16 );
        p_sys->b_force_palette = true;
    }
    vlc_mutex_unlock( &p_sys->lock );

    msg_Dbg( p_object, "crop: %i,%i,%i,%i, palette forced: %i",
             p_sys->i_crop_x, p_sys->i_crop_y,
             p_sys->i_crop_width, p_sys->i_crop_height,
             p_sys->b_force_palette );
}

/*****************************************************************************
 * CropCallback: called when the highlight properties are changed
 *****************************************************************************
 * This callback is called from the input thread when we need cropping
 *****************************************************************************/
static int CropCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval); VLC_UNUSED(newval); VLC_UNUSED(psz_var);

    UpdateSPU( (spu_t *)p_data, p_object );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * MarginCallback: called when requested subtitle position has changed         *
 *****************************************************************************/

static int MarginCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( psz_var ); VLC_UNUSED( oldval ); VLC_UNUSED( p_object );
    spu_private_t *p_sys = ( spu_private_t* ) p_data;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->i_margin = newval.i_int;
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Buffers allocation callbacks for the filters
 *****************************************************************************/
struct filter_owner_sys_t
{
    spu_t *p_spu;
    int i_channel;
};

static subpicture_t *sub_new_buffer( filter_t *p_filter )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;

    subpicture_t *p_subpicture = subpicture_New();
    if( p_subpicture )
        p_subpicture->i_channel = p_sys->i_channel;
    return p_subpicture;
}
static void sub_del_buffer( filter_t *p_filter, subpicture_t *p_subpic )
{
    VLC_UNUSED( p_filter );
    subpicture_Delete( p_subpic );
}

static subpicture_t *spu_new_buffer( filter_t *p_filter )
{
    VLC_UNUSED(p_filter);
    return subpicture_New();
}
static void spu_del_buffer( filter_t *p_filter, subpicture_t *p_subpic )
{
    VLC_UNUSED(p_filter);
    subpicture_Delete( p_subpic );
}

static picture_t *spu_new_video_buffer( filter_t *p_filter )
{
    const video_format_t *p_fmt = &p_filter->fmt_out.video;

    VLC_UNUSED(p_filter);
    return picture_NewFromFormat( p_fmt );
}
static void spu_del_video_buffer( filter_t *p_filter, picture_t *p_picture )
{
    VLC_UNUSED(p_filter);
    picture_Release( p_picture );
}

static int SubFilterAllocationInit( filter_t *p_filter, void *p_data )
{
    spu_t *p_spu = p_data;

    filter_owner_sys_t *p_sys = malloc( sizeof(filter_owner_sys_t) );
    if( !p_sys )
        return VLC_EGENERIC;

    p_filter->pf_sub_buffer_new = sub_new_buffer;
    p_filter->pf_sub_buffer_del = sub_del_buffer;

    p_filter->p_owner = p_sys;
    spu_Control( p_spu, SPU_CHANNEL_REGISTER, &p_sys->i_channel );
    p_sys->p_spu = p_spu;

    return VLC_SUCCESS;
}

static void SubFilterAllocationClean( filter_t *p_filter )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;

    SpuClearChannel( p_sys->p_spu, p_sys->i_channel );
    free( p_filter->p_owner );
}

static int SubFilterCallback( vlc_object_t *p_object, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    spu_t *p_spu = p_data;
    spu_private_t *p_sys = p_spu->p;

    VLC_UNUSED(p_object); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);

    vlc_mutex_lock( &p_sys->lock );

    free( p_sys->psz_chain_update );
    p_sys->psz_chain_update = strdup( newval.psz_string );

    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}

