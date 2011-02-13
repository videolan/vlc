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

#include <assert.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_input.h>
#include <vlc_vout.h>
#include <vlc_filter.h>
#include <vlc_spu.h>

#include "../libvlc.h"
#include "vout_internal.h"
#include "../misc/subpicture.h"

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

struct spu_private_t
{
    vlc_mutex_t lock;   /* lock to protect all followings fields */
    vlc_object_t *p_input;

    spu_heap_t heap;

    int i_channel;             /**< number of subpicture channels registered */
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
    vlc_mutex_t    chain_lock;
    filter_chain_t *p_chain;

    /* */
    mtime_t i_last_sort_date;
};

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

struct filter_owner_sys_t
{
    spu_t *p_spu;
    int i_channel;
};

static void FilterRelease( filter_t *p_filter )
{
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );
    if( p_filter->p_owner )
        free( p_filter->p_owner );

    vlc_object_release( p_filter );
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

static int spu_get_attachments( filter_t *p_filter,
                                input_attachment_t ***ppp_attachment,
                                int *pi_attachment )
{
    spu_t *p_spu = p_filter->p_owner->p_spu;

    int i_ret = VLC_EGENERIC;
    if( p_spu->p->p_input )
        i_ret = input_Control( (input_thread_t*)p_spu->p->p_input,
                               INPUT_GET_ATTACHMENTS,
                               ppp_attachment, pi_attachment );
    return i_ret;
}

static filter_t *SpuRenderCreateAndLoadText( spu_t *p_spu )
{
    filter_t *p_text = vlc_custom_create( p_spu, sizeof(*p_text),
                                          VLC_OBJECT_GENERIC, "spu text" );
    if( !p_text )
        return NULL;

    p_text->p_owner = xmalloc( sizeof(*p_text->p_owner) );
    p_text->p_owner->p_spu = p_spu;

    es_format_Init( &p_text->fmt_in, VIDEO_ES, 0 );

    es_format_Init( &p_text->fmt_out, VIDEO_ES, 0 );
    p_text->fmt_out.video.i_width =
    p_text->fmt_out.video.i_visible_width = 32;
    p_text->fmt_out.video.i_height =
    p_text->fmt_out.video.i_visible_height = 32;

    p_text->pf_get_attachments = spu_get_attachments;

    vlc_object_attach( p_text, p_spu );
    p_text->p_module = module_need( p_text, "text renderer", "$text-renderer", false );

    /* Create a few variables used for enhanced text rendering */
    var_Create( p_text, "spu-elapsed", VLC_VAR_TIME );
    var_Create( p_text, "text-rerender", VLC_VAR_BOOL );
    var_Create( p_text, "scale", VLC_VAR_INTEGER );

    return p_text;
}

static filter_t *SpuRenderCreateAndLoadScale( vlc_object_t *p_obj,
                                              vlc_fourcc_t i_src_chroma, vlc_fourcc_t i_dst_chroma,
                                              bool b_resize )
{
    filter_t *p_scale = vlc_custom_create( p_obj, sizeof(*p_scale),
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

static void SpuRenderText( spu_t *p_spu, bool *pb_rerender_text,
                           subpicture_region_t *p_region,
                           mtime_t render_date )
{
    filter_t *p_text = p_spu->p->p_text;

    assert( p_region->fmt.i_chroma == VLC_CODEC_TEXT );

    if( !p_text || !p_text->p_module )
        return;

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
    var_SetTime( p_text, "spu-elapsed", render_date );
    var_SetBool( p_text, "text-rerender", false );

    if( p_text->pf_render_html && p_region->psz_html )
    {
        p_text->pf_render_html( p_text, p_region, p_region );
    }
    else if( p_text->pf_render_text )
    {
        p_text->pf_render_text( p_text, p_region, p_region );
    }
    *pb_rerender_text = var_GetBool( p_text, "text-rerender" );
}

/**
 * A few scale functions helpers.
 */

#define SCALE_UNIT (1000)
typedef struct
{
    int w;
    int h;
} spu_scale_t;

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
static spu_scale_t spu_scale_createq( int64_t wn, int64_t wd, int64_t hn, int64_t hd )
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
typedef struct
{
    int i_x;
    int i_y;
    int i_width;
    int i_height;

    spu_scale_t scale;
} spu_area_t;

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
    assert( p_region->i_x != INT_MAX && p_region->i_y != INT_MAX );
    if( p_subpic->b_absolute )
    {
        *pi_x = p_region->i_x;
        *pi_y = p_region->i_y;
    }
    else
    {
        if( p_region->i_align & SUBPICTURE_ALIGN_TOP )
        {
            *pi_y = p_region->i_y;
        }
        else if( p_region->i_align & SUBPICTURE_ALIGN_BOTTOM )
        {
            *pi_y = p_subpic->i_original_picture_height - p_region->fmt.i_height - p_region->i_y;
        }
        else
        {
            *pi_y = p_subpic->i_original_picture_height / 2 - p_region->fmt.i_height / 2;
        }

        if( p_region->i_align & SUBPICTURE_ALIGN_LEFT )
        {
            *pi_x = p_region->i_x;
        }
        else if( p_region->i_align & SUBPICTURE_ALIGN_RIGHT )
        {
            *pi_x = p_subpic->i_original_picture_width - p_region->fmt.i_width - p_region->i_x;
        }
        else
        {
            *pi_x = p_subpic->i_original_picture_width / 2 - p_region->fmt.i_width / 2;
        }
    }
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
 * SpuSelectSubpictures: find the subpictures to display
 *****************************************************************************
 * This function parses all subpictures and decides which ones need to be
 * displayed. If no picture has been selected, display_date will depend on
 * the subpicture.
 * We also check for ephemer DVD subpictures (subpictures that have
 * to be removed if a newer one is available), which makes it a lot
 * more difficult to guess if a subpicture has to be rendered or not.
 *****************************************************************************/
static void SpuSelectSubpictures( spu_t *p_spu,
                                  unsigned int *pi_subpicture,
                                  subpicture_t **pp_subpicture,
                                  mtime_t render_subtitle_date,
                                  mtime_t render_osd_date,
                                  bool b_subtitle_only )
{
    spu_private_t *p_sys = p_spu->p;

    /* */
    *pi_subpicture = 0;

    /* Create a list of channels */
    int pi_channel[VOUT_MAX_SUBPICTURES];
    int i_channel_count = 0;

    for( int i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        spu_heap_entry_t *p_entry = &p_sys->heap.p_entry[i_index];
        if( !p_entry->p_subpicture || p_entry->b_reject )
            continue;
        const int i_channel = p_entry->p_subpicture->i_channel;
        int i;
        for( i = 0; i < i_channel_count; i++ )
        {
            if( pi_channel[i] == i_channel )
                break;
        }
        if( i_channel_count <= i )
            pi_channel[i_channel_count++] = i_channel;
    }

    /* Fill up the pp_subpicture arrays with relevent pictures */
    for( int i = 0; i < i_channel_count; i++ )
    {
        const int i_channel = pi_channel[i];
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
                pp_subpicture[(*pi_subpicture)++] = p_current;
        }
    }

    p_sys->i_last_sort_date = render_subtitle_date;
}



/**
 * It will transform the provided region into another region suitable for rendering.
 */

static void SpuRenderRegion( spu_t *p_spu,
                             subpicture_region_t **pp_dst, spu_area_t *p_dst_area,
                             subpicture_t *p_subpic, subpicture_region_t *p_region,
                             const spu_scale_t scale_size,
                             const vlc_fourcc_t *p_chroma_list,
                             const video_format_t *p_fmt,
                             const spu_area_t *p_subtitle_area, int i_subtitle_area,
                             mtime_t render_date )
{
    spu_private_t *p_sys = p_spu->p;

    video_format_t fmt_original = p_region->fmt;
    bool b_restore_text = false;
    int i_x_offset;
    int i_y_offset;

    video_format_t region_fmt;
    picture_t *p_region_picture;

    /* Invalidate area by default */
    *p_dst_area = spu_area_create( 0,0, 0,0, scale_size );
    *pp_dst     = NULL;

    /* Render text region */
    if( p_region->fmt.i_chroma == VLC_CODEC_TEXT )
    {
        SpuRenderText( p_spu, &b_restore_text, p_region,
                       render_date );

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
    *p_dst_area = spu_area_create( i_x_offset, i_y_offset,
                               p_region->fmt.i_width, p_region->fmt.i_height,
                               scale_size );

    /* Handle overlapping subtitles when possible */
    if( p_subpic->b_subtitle && !p_subpic->b_absolute )
    {
        SpuAreaFixOverlap( p_dst_area, p_subtitle_area, i_subtitle_area,
                           p_region->i_align );
    }

    /* we copy the area: for the subtitle overlap support we want
     * to only save the area without margin applied */
    spu_area_t restrained = *p_dst_area;

    /* apply margin to subtitles and correct if they go over the picture edge */
    if( p_subpic->b_subtitle )
        restrained.i_y -= i_margin_y;

    spu_area_t display = spu_area_create( 0, 0, p_fmt->i_width, p_fmt->i_height,
                                          spu_scale_unit() );
    SpuAreaFitInside( &restrained, &display );

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

    bool b_convert_chroma = true;
    for( int i = 0; p_chroma_list[i] && b_convert_chroma; i++ )
    {
        if( region_fmt.i_chroma == p_chroma_list[i] )
            b_convert_chroma = false;
    }

    /* Scale from rendered size to destination size */
    if( p_sys->p_scale && p_sys->p_scale->p_module &&
        ( !b_using_palette || ( p_sys->p_scale_yuvp && p_sys->p_scale_yuvp->p_module ) ) &&
        ( scale_size.w != SCALE_UNIT || scale_size.h != SCALE_UNIT ||
          b_using_palette || b_convert_chroma) )
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

            if( b_convert_chroma && p_private->fmt.i_chroma != p_chroma_list[0] )
                b_changed = true;

            if( b_changed )
            {
                subpicture_region_private_Delete( p_private );
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

                p_scale_yuvp->fmt_out.video = p_region->fmt;
                p_scale_yuvp->fmt_out.video.i_chroma = p_chroma_list[0];

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
                  p_picture->format.i_height != i_dst_height ||
                  ( b_convert_chroma && !b_using_palette ) ) )
            {
                p_scale->fmt_in.video = p_picture->format;
                p_scale->fmt_out.video = p_picture->format;
                if( b_convert_chroma )
                    p_scale->fmt_out.i_codec        =
                    p_scale->fmt_out.video.i_chroma = p_chroma_list[0];

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
                p_region->p_private = subpicture_region_private_New( &p_picture->format );
                if( p_region->p_private )
                {
                    p_region->p_private->p_picture = p_picture;
                    if( !p_region->p_private->p_picture )
                    {
                        subpicture_region_private_Delete( p_region->p_private );
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

    subpicture_region_t *p_dst = *pp_dst = subpicture_region_New( &region_fmt );
    if( p_dst )
    {
        p_dst->i_x       = i_x_offset;
        p_dst->i_y       = i_y_offset;
        p_dst->i_align   = 0;
        if( p_dst->p_picture )
            picture_Release( p_dst->p_picture );
        p_dst->p_picture = picture_Hold( p_region_picture );
        int i_fade_alpha = 255;
        if( p_subpic->b_fade )
        {
            mtime_t fade_start = ( p_subpic->i_stop +
                                   p_subpic->i_start ) / 2;

            if( fade_start <= render_date && fade_start < p_subpic->i_stop )
                i_fade_alpha = 255 * ( p_subpic->i_stop - render_date ) /
                                     ( p_subpic->i_stop - fade_start );
        }
        p_dst->i_alpha   = i_fade_alpha * p_subpic->i_alpha * p_region->i_alpha / 65025;
    }

exit:
    if( b_restore_text )
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
            subpicture_region_private_Delete( p_region->p_private );
            p_region->p_private = NULL;
        }
        p_region->fmt = fmt_original;
    }
}

/**
 * This function renders all sub picture units in the list.
 */
static subpicture_t *SpuRenderSubpictures( spu_t *p_spu,
                                           unsigned int i_subpicture,
                                           subpicture_t **pp_subpicture,
                                           const vlc_fourcc_t *p_chroma_list,
                                           const video_format_t *p_fmt_dst,
                                           const video_format_t *p_fmt_src,
                                           mtime_t render_subtitle_date,
                                           mtime_t render_osd_date )
{
    spu_private_t *p_sys = p_spu->p;

    /* Count the number of regions and subtitle regions */
    unsigned int i_subtitle_region_count = 0;
    unsigned int i_region_count          = 0;
    for( unsigned i = 0; i < i_subpicture; i++ )
    {
        const subpicture_t *p_subpic = pp_subpicture[i];

        unsigned i_count = 0;
        for( subpicture_region_t *r = p_subpic->p_region; r != NULL; r = r->p_next )
            i_count++;

        if( p_subpic->b_subtitle )
            i_subtitle_region_count += i_count;
        i_region_count += i_count;
    }
    if( i_region_count <= 0 )
        return NULL;

    /* Create the output subpicture */
    subpicture_t *p_output = subpicture_New( NULL );
    if( !p_output )
        return NULL;
    subpicture_region_t **pp_output_last = &p_output->p_region;

    /* Allocate area array for subtitle overlap */
    spu_area_t p_subtitle_area_buffer[VOUT_MAX_SUBPICTURES];
    spu_area_t *p_subtitle_area;
    int i_subtitle_area;

    i_subtitle_area = 0;
    p_subtitle_area = p_subtitle_area_buffer;
    if( i_subtitle_region_count > sizeof(p_subtitle_area_buffer)/sizeof(*p_subtitle_area_buffer) )
        p_subtitle_area = calloc( i_subtitle_region_count, sizeof(*p_subtitle_area) );

    /* Process all subpictures and regions (in the right order) */
    for( unsigned int i_index = 0; i_index < i_subpicture; i_index++ )
    {
        subpicture_t *p_subpic = pp_subpicture[i_index];
        subpicture_region_t *p_region;

        if( !p_subpic->p_region )
            continue;

        if( p_subpic->i_original_picture_width  <= 0 ||
            p_subpic->i_original_picture_height <= 0 )
        {
            if( p_subpic->i_original_picture_width  > 0 ||
                p_subpic->i_original_picture_height > 0 )
                msg_Err( p_spu, "original picture size %dx%d is unsupported",
                         p_subpic->i_original_picture_width,
                         p_subpic->i_original_picture_height );
            else
                msg_Warn( p_spu, "original picture size is undefined" );

            p_subpic->i_original_picture_width  = p_fmt_src->i_width;
            p_subpic->i_original_picture_height = p_fmt_src->i_height;
        }

        if( p_sys->p_text )
        {
            /* FIXME aspect ratio ? */
            p_sys->p_text->fmt_out.video.i_width          =
            p_sys->p_text->fmt_out.video.i_visible_width  = p_subpic->i_original_picture_width;

            p_sys->p_text->fmt_out.video.i_height         =
            p_sys->p_text->fmt_out.video.i_visible_height = p_subpic->i_original_picture_height;

            var_SetInteger( p_sys->p_text, "scale", SCALE_UNIT );
        }

        /* Render all regions
         * We always transform non absolute subtitle into absolute one on the
         * first rendering to allow good subtitle overlap support.
         */
        for( p_region = p_subpic->p_region; p_region != NULL; p_region = p_region->p_next )
        {
            spu_area_t area;

            /* Compute region scale AR */
            video_format_t region_fmt =p_region->fmt;
            if( region_fmt.i_sar_num <= 0 || region_fmt.i_sar_den <= 0 )
            {
                region_fmt.i_sar_num = p_fmt_src->i_sar_num;
                region_fmt.i_sar_den = p_fmt_src->i_sar_den;
            }

            /* Compute scaling from original size to destination size
             * FIXME The current scaling ensure that the heights match, the width being
             * cropped.
             */
            spu_scale_t scale= spu_scale_createq( (int64_t)p_fmt_dst->i_height                 * p_fmt_dst->i_sar_den * region_fmt.i_sar_num,
                                                  (int64_t)p_subpic->i_original_picture_height * p_fmt_dst->i_sar_num * region_fmt.i_sar_den,
                                                  p_fmt_dst->i_height,
                                                  p_subpic->i_original_picture_height );

            /* Check scale validity */
            if( scale.w <= 0 || scale.h <= 0 )
                continue;

            /* */
            SpuRenderRegion( p_spu, pp_output_last, &area,
                             p_subpic, p_region, scale,
                             p_chroma_list, p_fmt_dst,
                             p_subtitle_area, i_subtitle_area,
                             p_subpic->b_subtitle ? render_subtitle_date : render_osd_date );
            if( *pp_output_last )
                pp_output_last = &(*pp_output_last)->p_next;

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
        if( p_subpic->b_subtitle && p_subpic->p_region )
            p_subpic->b_absolute = true;
    }

    /* */
    if( p_subtitle_area != p_subtitle_area_buffer )
        free( p_subtitle_area );

    return p_output;
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
 * Buffers allocation callbacks for the filters
 *****************************************************************************/

static subpicture_t *sub_new_buffer( filter_t *p_filter )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;

    subpicture_t *p_subpicture = subpicture_New( NULL );
    if( p_subpicture )
        p_subpicture->i_channel = p_sys->i_channel;
    return p_subpicture;
}
static void sub_del_buffer( filter_t *p_filter, subpicture_t *p_subpic )
{
    VLC_UNUSED( p_filter );
    subpicture_Delete( p_subpic );
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
    p_sys->i_channel = spu_RegisterChannel( p_spu );
    p_sys->p_spu = p_spu;

    return VLC_SUCCESS;
}

static void SubFilterAllocationClean( filter_t *p_filter )
{
    filter_owner_sys_t *p_sys = p_filter->p_owner;

    spu_ClearChannel( p_sys->p_spu, p_sys->i_channel );
    free( p_filter->p_owner );
}

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
    p_spu->p = p_sys = (spu_private_t*)&p_spu[1];

    /* Initialize private fields */
    vlc_mutex_init( &p_sys->lock );

    SpuHeapInit( &p_sys->heap );

    p_sys->p_text = NULL;
    p_sys->p_scale = NULL;
    p_sys->p_scale_yuvp = NULL;

    p_sys->i_margin = var_InheritInteger( p_spu, "sub-margin" );

    /* Register the default subpicture channel */
    p_sys->i_channel = SPU_DEFAULT_CHANNEL + 1;

    p_sys->psz_chain_update = NULL;
    vlc_mutex_init( &p_sys->chain_lock );
    p_sys->p_chain = filter_chain_New( p_spu, "sub filter", false,
                                       SubFilterAllocationInit,
                                       SubFilterAllocationClean,
                                       p_spu );

    /* Load text and scale module */
    p_sys->p_text = SpuRenderCreateAndLoadText( p_spu );

    /* XXX p_spu->p_scale is used for all conversion/scaling except yuvp to
     * yuva/rgba */
    p_sys->p_scale = SpuRenderCreateAndLoadScale( VLC_OBJECT(p_spu),
                                                  VLC_CODEC_YUVA, VLC_CODEC_RGBA, true );

    /* This one is used for YUVP to YUVA/RGBA without scaling
     * FIXME rename it */
    p_sys->p_scale_yuvp = SpuRenderCreateAndLoadScale( VLC_OBJECT(p_spu),
                                                       VLC_CODEC_YUVP, VLC_CODEC_YUVA, false );

    /* */
    p_sys->i_last_sort_date = -1;

    return p_spu;
}

/**
 * Destroy the subpicture unit
 *
 * \param p_this the parent object which destroys the subpicture unit
 */
void spu_Destroy( spu_t *p_spu )
{
    spu_private_t *p_sys = p_spu->p;

    if( p_sys->p_text )
        FilterRelease( p_sys->p_text );

    if( p_sys->p_scale_yuvp )
        FilterRelease( p_sys->p_scale_yuvp );

    if( p_sys->p_scale )
        FilterRelease( p_sys->p_scale );

    filter_chain_Delete( p_sys->p_chain );
    vlc_mutex_destroy( &p_sys->chain_lock );
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
void spu_Attach( spu_t *p_spu, vlc_object_t *p_input, bool b_attach )
{
    if( b_attach )
    {
        UpdateSPU( p_spu, p_input );
        var_Create( p_input, "highlight", VLC_VAR_BOOL );
        var_AddCallback( p_input, "highlight", CropCallback, p_spu );

        vlc_mutex_lock( &p_spu->p->lock );
        p_spu->p->p_input = p_input;

        if( p_spu->p->p_text )
            FilterRelease( p_spu->p->p_text );
        p_spu->p->p_text = SpuRenderCreateAndLoadText( p_spu );

        vlc_mutex_unlock( &p_spu->p->lock );
    }
    else
    {
        vlc_mutex_lock( &p_spu->p->lock );
        p_spu->p->p_input = NULL;
        vlc_mutex_unlock( &p_spu->p->lock );

        /* Delete callbacks */
        var_DelCallback( p_input, "highlight", CropCallback, p_spu );
        var_Destroy( p_input, "highlight" );
    }
}

/**
 * Inform the SPU filters of mouse event
 */
int spu_ProcessMouse( spu_t *p_spu,
                      const vlc_mouse_t *p_mouse,
                      const video_format_t *p_fmt )
{
    spu_private_t *p_sys = p_spu->p;

    vlc_mutex_lock( &p_sys->chain_lock );
    filter_chain_MouseEvent( p_sys->p_chain, p_mouse, p_fmt );
    vlc_mutex_unlock( &p_sys->chain_lock );

    return VLC_SUCCESS;
}

/**
 * Display a subpicture
 *
 * Remove the reservation flag of a subpicture, which will cause it to be
 * ready for display.
 * \param p_spu the subpicture unit object
 * \param p_subpic the subpicture to display
 */
void spu_PutSubpicture( spu_t *p_spu, subpicture_t *p_subpic )
{
    spu_private_t *p_sys = p_spu->p;

    /* SPU_DEFAULT_CHANNEL always reset itself */
    if( p_subpic->i_channel == SPU_DEFAULT_CHANNEL )
        spu_ClearChannel( p_spu, SPU_DEFAULT_CHANNEL );

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

subpicture_t *spu_Render( spu_t *p_spu,
                          const vlc_fourcc_t *p_chroma_list,
                          const video_format_t *p_fmt_dst,
                          const video_format_t *p_fmt_src,
                          mtime_t render_subtitle_date,
                          mtime_t render_osd_date,
                          bool b_subtitle_only )
{
    spu_private_t *p_sys = p_spu->p;

    /* Update sub-filter chain */
    vlc_mutex_lock( &p_sys->lock );
    char *psz_chain_update = p_sys->psz_chain_update;
    p_sys->psz_chain_update = NULL;
    vlc_mutex_unlock( &p_sys->lock );

    vlc_mutex_lock( &p_sys->chain_lock );
    if( psz_chain_update )
    {
        filter_chain_Reset( p_sys->p_chain, NULL, NULL );

        filter_chain_AppendFromString( p_spu->p->p_chain, psz_chain_update );

        free( psz_chain_update );
    }
    /* Run subpicture filters */
    filter_chain_SubFilter( p_sys->p_chain, render_osd_date );
    vlc_mutex_unlock( &p_sys->chain_lock );

    static const vlc_fourcc_t p_chroma_list_default_yuv[] = {
        VLC_CODEC_YUVA,
        VLC_CODEC_RGBA,
        VLC_CODEC_YUVP,
        0,
    };
    static const vlc_fourcc_t p_chroma_list_default_rgb[] = {
        VLC_CODEC_RGBA,
        VLC_CODEC_YUVA,
        VLC_CODEC_YUVP,
        0,
    };

    if( !p_chroma_list || *p_chroma_list == 0 )
        p_chroma_list = vlc_fourcc_IsYUV(p_fmt_dst->i_chroma) ? p_chroma_list_default_yuv
                                                              : p_chroma_list_default_rgb;

    vlc_mutex_lock( &p_sys->lock );

    unsigned int i_subpicture;
    subpicture_t *pp_subpicture[VOUT_MAX_SUBPICTURES];

    /* Get an array of subpictures to render */
    SpuSelectSubpictures( p_spu, &i_subpicture, pp_subpicture,
                          render_subtitle_date, render_osd_date, b_subtitle_only );
    if( i_subpicture <= 0 )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return NULL;
    }

    /* Updates the subpictures */
    for( unsigned i = 0; i < i_subpicture; i++ )
    {
        subpicture_t *p_subpic = pp_subpicture[i];
        subpicture_Update( p_subpic,
                           p_fmt_src, p_fmt_dst,
                           p_subpic->b_subtitle ? render_subtitle_date : render_osd_date );
    }

    /* Now order the subpicture array
     * XXX The order is *really* important for overlap subtitles positionning */
    qsort( pp_subpicture, i_subpicture, sizeof(*pp_subpicture), SubpictureCmp );

    /* Render the subpictures */
    subpicture_t *p_render = SpuRenderSubpictures( p_spu,
                                                   i_subpicture, pp_subpicture,
                                                   p_chroma_list,
                                                   p_fmt_dst,
                                                   p_fmt_src,
                                                   render_subtitle_date,
                                                   render_osd_date );
    vlc_mutex_unlock( &p_sys->lock );

    return p_render;
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

int spu_RegisterChannel( spu_t *p_spu )
{
    spu_private_t *p_sys = p_spu->p;

    vlc_mutex_lock( &p_sys->lock );
    int i_channel = p_sys->i_channel++;
    vlc_mutex_unlock( &p_sys->lock );

    return i_channel;
}

void spu_ClearChannel( spu_t *p_spu, int i_channel )
{
    spu_private_t *p_sys = p_spu->p;

    vlc_mutex_lock( &p_sys->lock );

    for( int i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        spu_heap_entry_t *p_entry = &p_sys->heap.p_entry[i_subpic];
        subpicture_t *p_subpic = p_entry->p_subpicture;

        if( !p_subpic )
            continue;
        if( p_subpic->i_channel != i_channel && ( i_channel != -1 || p_subpic->i_channel == SPU_DEFAULT_CHANNEL ) )
            continue;

        /* You cannot delete subpicture outside of spu_SortSubpictures */
        p_entry->b_reject = true;
    }

    vlc_mutex_unlock( &p_sys->lock );
}

void spu_ChangeFilters( spu_t *p_spu, const char *psz_filters )
{
    spu_private_t *p_sys = p_spu->p;

    vlc_mutex_lock( &p_sys->lock );

    free( p_sys->psz_chain_update );
    p_sys->psz_chain_update = strdup( psz_filters );

    vlc_mutex_unlock( &p_sys->lock );
}

void spu_ChangeMargin( spu_t *p_spu, int i_margin )
{
    spu_private_t *p_sys = p_spu->p;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->i_margin = i_margin;
    vlc_mutex_unlock( &p_sys->lock );
}

