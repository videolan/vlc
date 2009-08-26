/*****************************************************************************
 * SSA/ASS subtitle decoder using libass.
 *****************************************************************************
 * Copyright (C) 2008-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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
#   include "config.h"
#endif

#include <string.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_osd.h>
#include <vlc_input.h>
#include <vlc_dialog.h>

#include <ass/ass.h>

#if defined(WIN32)
#   include <vlc_charset.h>
#endif

/* Compatibility with old libass */
#if !defined(LIBASS_VERSION) || LIBASS_VERSION < 0x00907010
#   define ASS_Renderer    ass_renderer_t
#   define ASS_Library     ass_library_t
#   define ASS_Track       ass_track_t
#   define ASS_Image       ass_image_t
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Subtitles (advanced)"))
    set_description( N_("Subtitle renderers using libass") )
    set_capability( "decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *, block_t ** );
static void DestroySubpicture( subpicture_t * );
static void UpdateRegions( spu_t *,
                           subpicture_t *, const video_format_t *, mtime_t );

/* Yes libass sux with threads */
typedef struct
{
    vlc_object_t   *p_libvlc;

    int             i_refcount;
    ASS_Library     *p_library;
    ASS_Renderer    *p_renderer;
    video_format_t  fmt;
} ass_handle_t;
static ass_handle_t *AssHandleHold( decoder_t *p_dec );
static void AssHandleRelease( ass_handle_t * );

/* */
struct decoder_sys_t
{
    mtime_t      i_max_stop;

    /* decoder_sys_t is shared between decoder and spu units */
    vlc_mutex_t  lock;
    int          i_refcount;

    /* */
    ass_handle_t *p_ass;

    /* */
    ASS_Track    *p_track;
};
static void DecSysRelease( decoder_sys_t *p_sys );
static void DecSysHold( decoder_sys_t *p_sys );

struct subpicture_sys_t
{
    decoder_sys_t *p_dec_sys;
    void          *p_subs_data;
    int           i_subs_len;
    mtime_t       i_pts;
};

typedef struct
{
    int x0;
    int y0;
    int x1;
    int y1;
} rectangle_t;

static int BuildRegions( spu_t *p_spu, rectangle_t *p_region, int i_max_region, ASS_Image *p_img_list, int i_width, int i_height );
static void SubpictureReleaseRegions( spu_t *p_spu, subpicture_t *p_subpic );
static void RegionDraw( subpicture_region_t *p_region, ASS_Image *p_img );

static vlc_mutex_t libass_lock = VLC_STATIC_MUTEX;

//#define DEBUG_REGION

/*****************************************************************************
 * Create: Open libass decoder.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;
    ASS_Track *p_track;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_SSA )
        return VLC_EGENERIC;

    p_dec->pf_decode_sub = DecodeBlock;

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* */
    p_sys->i_max_stop = VLC_TS_INVALID;
    p_sys->p_ass = AssHandleHold( p_dec );
    if( !p_sys->p_ass )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    vlc_mutex_init( &p_sys->lock );
    p_sys->i_refcount = 1;

    /* Add a track */
    vlc_mutex_lock( &libass_lock );
    p_sys->p_track = p_track = ass_new_track( p_sys->p_ass->p_library );
    if( !p_track )
    {
        vlc_mutex_unlock( &libass_lock );
        DecSysRelease( p_sys );
        return VLC_EGENERIC;
    }
    ass_process_codec_private( p_track, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );
    vlc_mutex_unlock( &libass_lock );

    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_RGBA;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: finish
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;

    DecSysRelease( p_dec->p_sys );
}

static void DecSysHold( decoder_sys_t *p_sys )
{
    vlc_mutex_lock( &p_sys->lock );
    p_sys->i_refcount++;
    vlc_mutex_unlock( &p_sys->lock );
}
static void DecSysRelease( decoder_sys_t *p_sys )
{
    /* */
    vlc_mutex_lock( &p_sys->lock );
    p_sys->i_refcount--;
    if( p_sys->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return;
    }
    vlc_mutex_unlock( &p_sys->lock );
    vlc_mutex_destroy( &p_sys->lock );

    vlc_mutex_lock( &libass_lock );
    if( p_sys->p_track )
        ass_free_track( p_sys->p_track );
    vlc_mutex_unlock( &libass_lock );

    AssHandleRelease( p_sys->p_ass );
    free( p_sys );
}

/****************************************************************************
 * DecodeBlock:
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    subpicture_t *p_spu = NULL;
    block_t *p_block;

    if( !pp_block || *pp_block == NULL )
        return NULL;

    p_block = *pp_block;
    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        p_sys->i_max_stop = VLC_TS_INVALID;
        block_Release( p_block );
        return NULL;
    }
    *pp_block = NULL;

    if( p_block->i_buffer == 0 || p_block->p_buffer[0] == '\0' )
    {
        block_Release( p_block );
        return NULL;
    }

    p_spu = decoder_NewSubpicture( p_dec );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_spu->p_sys = malloc( sizeof( subpicture_sys_t ));
    if( !p_spu->p_sys )
    {
        decoder_DeleteSubpicture( p_dec, p_spu );
        block_Release( p_block );
        return NULL;
    }

    p_spu->p_sys->i_subs_len = p_block->i_buffer;
    p_spu->p_sys->p_subs_data = malloc( p_block->i_buffer );
    if( !p_spu->p_sys->p_subs_data )
    {
        free( p_spu->p_sys );
        decoder_DeleteSubpicture( p_dec, p_spu );
        block_Release( p_block );
        return NULL;
    }
    memcpy( p_spu->p_sys->p_subs_data, p_block->p_buffer,
            p_block->i_buffer );
    p_spu->p_sys->i_pts = p_block->i_pts;

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = __MAX( p_sys->i_max_stop, p_block->i_pts + p_block->i_length );
    p_spu->b_ephemer = true;
    p_spu->b_absolute = true;

    p_sys->i_max_stop = p_spu->i_stop;

    vlc_mutex_lock( &libass_lock );
    if( p_sys->p_track )
    {
        ass_process_chunk( p_sys->p_track, p_spu->p_sys->p_subs_data, p_spu->p_sys->i_subs_len,
                           p_block->i_pts / 1000, p_block->i_length / 1000 );
    }
    vlc_mutex_unlock( &libass_lock );

    p_spu->pf_update_regions = UpdateRegions;
    p_spu->pf_destroy = DestroySubpicture;
    p_spu->p_sys->p_dec_sys = p_sys;

    DecSysHold( p_sys );

    block_Release( p_block );

    return p_spu;
}

/****************************************************************************
 *
 ****************************************************************************/
static void DestroySubpicture( subpicture_t *p_subpic )
{
    DecSysRelease( p_subpic->p_sys->p_dec_sys );

    free( p_subpic->p_sys->p_subs_data );
    free( p_subpic->p_sys );
}

static void UpdateRegions( spu_t *p_spu, subpicture_t *p_subpic,
                           const video_format_t *p_fmt, mtime_t i_ts )
{
    decoder_sys_t *p_sys = p_subpic->p_sys->p_dec_sys;
    ass_handle_t *p_ass = p_sys->p_ass;

    video_format_t fmt;
    bool b_fmt_changed;

    vlc_mutex_lock( &libass_lock );

    /* */
    fmt = *p_fmt;
    fmt.i_chroma = VLC_CODEC_RGBA;
    fmt.i_width = fmt.i_visible_width;
    fmt.i_height = fmt.i_visible_height;
    fmt.i_bits_per_pixel = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;

    b_fmt_changed = memcmp( &fmt, &p_ass->fmt, sizeof(fmt) ) != 0;
    if( b_fmt_changed )
    {
        ass_set_frame_size( p_ass->p_renderer, fmt.i_width, fmt.i_height );
#if defined( LIBASS_VERSION ) && LIBASS_VERSION >= 0x00907000
    ass_set_aspect_ratio( p_ass->p_renderer, 1.0, 1.0 ); // TODO ?
#else
    ass_set_aspect_ratio( p_ass->p_renderer, 1.0 ); // TODO ?
#endif

        p_ass->fmt = fmt;
    }

    /* */
    const mtime_t i_stream_date = p_subpic->p_sys->i_pts + (i_ts - p_subpic->i_start);
    int i_changed;
    ASS_Image *p_img = ass_render_frame( p_ass->p_renderer, p_sys->p_track,
                                         i_stream_date/1000, &i_changed );

    if( !i_changed && !b_fmt_changed &&
        (p_img != NULL) == (p_subpic->p_region != NULL) )
    {
        vlc_mutex_unlock( &libass_lock );
        return;
    }

    /* */
    p_subpic->i_original_picture_height = fmt.i_height;
    p_subpic->i_original_picture_width = fmt.i_width;
    SubpictureReleaseRegions( p_spu, p_subpic );

    /* XXX to improve efficiency we merge regions that are close minimizing
     * the lost surface.
     * libass tends to create a lot of small regions and thus spu engine
     * reinstanciate a lot the scaler, and as we do not support subpel blending
     * it looks ugly (text unaligned).
     */
    const int i_max_region = 4;
    rectangle_t region[i_max_region];
    const int i_region = BuildRegions( p_spu, region, i_max_region, p_img, fmt.i_width, fmt.i_height );

    if( i_region <= 0 )
    {
        vlc_mutex_unlock( &libass_lock );
        return;
    }

    /* Allocate the regions and draw them */
    subpicture_region_t *pp_region[i_max_region];
    subpicture_region_t **pp_region_last = &p_subpic->p_region;

    for( int i = 0; i < i_region; i++ )
    {
        subpicture_region_t *r;
        video_format_t fmt_region;

        /* */
        fmt_region = fmt;
        fmt_region.i_width =
        fmt_region.i_visible_width  = region[i].x1 - region[i].x0;
        fmt_region.i_height =
        fmt_region.i_visible_height = region[i].y1 - region[i].y0;

        pp_region[i] = r = subpicture_region_New( &fmt_region );
        if( !r )
            break;
        r->i_x = region[i].x0;
        r->i_y = region[i].y0;
        r->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;

        /* */
        RegionDraw( r, p_img );

        /* */
        *pp_region_last = r;
        pp_region_last = &r->p_next;
    }
    vlc_mutex_unlock( &libass_lock );
}

static rectangle_t r_create( int x0, int y0, int x1, int y1 )
{
    rectangle_t r = { x0, y0, x1, y1 };
    return r;
}
static rectangle_t r_img( const ASS_Image *p_img )
{
    return r_create( p_img->dst_x, p_img->dst_y, p_img->dst_x+p_img->w, p_img->dst_y+p_img->h );
}
static void r_add( rectangle_t *r, const rectangle_t *n )
{
    r->x0 = __MIN( r->x0, n->x0 );
    r->y0 = __MIN( r->y0, n->y0 );
    r->x1 = __MAX( r->x1, n->x1 );
    r->y1 = __MAX( r->y1, n->y1 );
}
static int r_surface( const rectangle_t *r )
{
    return (r->x1-r->x0) * (r->y1-r->y0);
}
static bool r_overlap( const rectangle_t *a, const rectangle_t *b, int i_dx, int i_dy )
{
    return  __MAX(a->x0-i_dx, b->x0) < __MIN( a->x1+i_dx, b->x1 ) &&
            __MAX(a->y0-i_dy, b->y0) < __MIN( a->y1+i_dy, b->y1 );
}

static int BuildRegions( spu_t *p_spu, rectangle_t *p_region, int i_max_region, ASS_Image *p_img_list, int i_width, int i_height )
{
    ASS_Image *p_tmp;
    int i_count;

    VLC_UNUSED(p_spu);

#ifdef DEBUG_REGION
    int64_t i_ck_start = mdate();
#endif

    for( p_tmp = p_img_list, i_count = 0; p_tmp != NULL; p_tmp = p_tmp->next )
        i_count++;
    if( i_count <= 0 )
        return 0;

    ASS_Image **pp_img = calloc( i_count, sizeof(*pp_img) );
    if( !pp_img )
        return 0;

    for( p_tmp = p_img_list, i_count = 0; p_tmp != NULL; p_tmp = p_tmp->next, i_count++ )
        pp_img[i_count] = p_tmp;

    /* */
    const int i_w_inc = __MAX( ( i_width + 49 ) / 50, 32 );
    const int i_h_inc = __MAX( ( i_height + 99 ) / 100, 32 );
    int i_maxh = i_w_inc;
    int i_maxw = i_h_inc;
    int i_region;
    rectangle_t region[i_max_region+1];

    i_region = 0;
    for( int i_used = 0; i_used < i_count; )
    {
        int n;
        for( n = 0; n < i_count; n++ )
        {
            if( pp_img[n] )
                break;
        }
        assert( i_region < i_max_region + 1 );
        region[i_region++] = r_img( pp_img[n] );
        pp_img[n] = NULL; i_used++;

        bool b_ok;
        do {
            b_ok = false;
            for( n = 0; n < i_count; n++ )
            {
                ASS_Image *p_img = pp_img[n];
                if( !p_img )
                    continue;
                rectangle_t r = r_img( p_img );

                int k;
                int i_best = -1;
                int i_best_s = INT_MAX;
                for( k = 0; k < i_region; k++ )
                {
                    if( !r_overlap( &region[k], &r, i_maxw, i_maxh ) )
                        continue;
                    int s = r_surface( &r );
                    if( s < i_best_s )
                    {
                        i_best_s = s;
                        i_best = k;
                    }
                }
                if( i_best >= 0 )
                {
                    r_add( &region[i_best], &r );
                    pp_img[n] = NULL; i_used++;
                    b_ok = true;
                }
            }
        } while( b_ok );

        if( i_region > i_max_region )
        {
            int i_best_i = -1;
            int i_best_j = -1;
            int i_best_ds = INT_MAX;

            /* merge best */
            for( int i = 0; i < i_region; i++ )
            {
                for( int j = i+1; j < i_region; j++ )
                {
                    rectangle_t n = region[i];
                    r_add( &n, &region[j] );
                    int ds = r_surface( &n ) - r_surface( &region[i] ) - r_surface( &region[j] );

                    if( ds < i_best_ds )
                    {
                        i_best_i = i;
                        i_best_j = j;
                        i_best_ds = ds;
                    }
                }
            }
#ifdef DEBUG_REGION
            msg_Err( p_spu, "Merging %d and %d", i_best_i, i_best_j );
#endif
            r_add( &region[i_best_i], &region[i_best_j] );

            if( i_best_j+1 < i_region )
                memmove( &region[i_best_j], &region[i_best_j+1], sizeof(*region) * ( i_region - (i_best_j+1)  ) );
            i_region--;
        }
    }

    /* */
    for( int n = 0; n < i_region; n++ )
        p_region[n] = region[n];

#ifdef DEBUG_REGION
    int64_t i_ck_time = mdate() - i_ck_start;
    msg_Err( p_spu, "ASS: %d objects merged into %d region in %d micros", i_count, i_region, (int)(i_ck_time) );
#endif

    free( pp_img );

    return i_region;
}

static void RegionDraw( subpicture_region_t *p_region, ASS_Image *p_img )
{
    const plane_t *p = &p_region->p_picture->p[0];
    const int i_x = p_region->i_x;
    const int i_y = p_region->i_y;
    const int i_width  = p_region->fmt.i_width;
    const int i_height = p_region->fmt.i_height;

    memset( p->p_pixels, 0x00, p->i_pitch * p->i_lines );
    for( ; p_img != NULL; p_img = p_img->next )
    {
        if( p_img->dst_x < i_x || p_img->dst_x + p_img->w > i_x + i_width ||
            p_img->dst_y < i_y || p_img->dst_y + p_img->h > i_y + i_height )
            continue;

        const unsigned r = (p_img->color >> 24)&0xff;
        const unsigned g = (p_img->color >> 16)&0xff;
        const unsigned b = (p_img->color >>  8)&0xff;
        const unsigned a = (p_img->color      )&0xff;
        int x, y;

        for( y = 0; y < p_img->h; y++ )
        {
            for( x = 0; x < p_img->w; x++ )
            {
                const unsigned alpha = p_img->bitmap[y*p_img->stride+x];
                const unsigned an = (255 - a) * alpha / 255;

                uint8_t *p_rgba = &p->p_pixels[(y+p_img->dst_y-i_y) * p->i_pitch + 4 * (x+p_img->dst_x-i_x)];
                const unsigned ao = p_rgba[3];

                /* Native endianness, but RGBA ordering */
                if( ao == 0 )
                {
                    /* Optimized but the else{} will produce the same result */
                    p_rgba[0] = r;
                    p_rgba[1] = g;
                    p_rgba[2] = b;
                    p_rgba[3] = an;
                }
                else
                {
                    p_rgba[3] = 255 - ( 255 - p_rgba[3] ) * ( 255 - an ) / 255;
                    if( p_rgba[3] != 0 )
                    {
                        p_rgba[0] = ( p_rgba[0] * ao * (255-an) / 255 + r * an ) / p_rgba[3];
                        p_rgba[1] = ( p_rgba[1] * ao * (255-an) / 255 + g * an ) / p_rgba[3];
                        p_rgba[2] = ( p_rgba[2] * ao * (255-an) / 255 + b * an ) / p_rgba[3];
                    }
                }
            }
        }
    }

#ifdef DEBUG_REGION
    /* XXX Draw a box for debug */
#define P(x,y) ((uint32_t*)&p->p_pixels[(y)*p->i_pitch + 4*(x)])
    for( int y = 0; y < p->i_lines; y++ )
        *P(0,y) = *P(p->i_visible_pitch/4-1,y) = 0xff000000;
    for( int x = 0; x < p->i_visible_pitch; x++ )
        *P(x/4,0) = *P(x/4,p->i_visible_lines-1) = 0xff000000;
#undef P
#endif
}


static void SubpictureReleaseRegions( spu_t *p_spu, subpicture_t *p_subpic )
{
    VLC_UNUSED( p_spu );
    subpicture_region_ChainDelete( p_subpic->p_region );
    p_subpic->p_region = NULL;
}

/* */
static ass_handle_t *AssHandleHold( decoder_t *p_dec )
{
    vlc_mutex_lock( &libass_lock );

    ass_handle_t *p_ass = NULL;
    ASS_Library *p_library = NULL;
    ASS_Renderer *p_renderer = NULL;
    vlc_value_t val;

    var_Create( p_dec->p_libvlc, "libass-handle", VLC_VAR_ADDRESS );
    if( var_Get( p_dec->p_libvlc, "libass-handle", &val ) )
        val.p_address = NULL;

    if( val.p_address )
    {
        p_ass = val.p_address;

        p_ass->i_refcount++;

        vlc_mutex_unlock( &libass_lock );
        return p_ass;
    }

    /* */
    p_ass = malloc( sizeof(*p_ass) );
    if( !p_ass )
        goto error;

    /* */
    p_ass->p_libvlc = VLC_OBJECT(p_dec->p_libvlc);
    p_ass->i_refcount = 1;

    /* Create libass library */
    p_ass->p_library = p_library = ass_library_init();
    if( !p_library )
        goto error;

    /* load attachments */
    input_attachment_t  **pp_attachments;
    int                   i_attachments;

    if( decoder_GetInputAttachments( p_dec, &pp_attachments, &i_attachments ))
    {
        i_attachments = 0;
        pp_attachments = NULL;
    }
    for( int k = 0; k < i_attachments; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        if( !strcasecmp( p_attach->psz_mime, "application/x-truetype-font" ) )
        {
            msg_Dbg( p_dec, "adding embedded font %s", p_attach->psz_name );

            ass_add_font( p_ass->p_library, p_attach->psz_name, p_attach->p_data, p_attach->i_data );
        }
        vlc_input_attachment_Delete( p_attach );
    }
    free( pp_attachments );

    char *psz_font_dir = NULL;


#if defined(WIN32)
    dialog_progress_bar_t *p_dialog = dialog_ProgressCreate( p_dec,
        _("Building font cache"),
        _( "Please wait while your font cache is rebuilt.\n"
        "This should take less than a minute." ), NULL );
    /* This makes Windows build of VLC hang */
    const UINT uPath = GetSystemWindowsDirectoryW( NULL, 0 );
    if( uPath > 0 )
    {
        wchar_t *psw_path = calloc( uPath + 1, sizeof(wchar_t) );
        if( psw_path )
        {
            if( GetSystemWindowsDirectoryW( psw_path, uPath + 1 ) > 0 )
            {
                char *psz_tmp = FromWide( psw_path );
                if( psz_tmp &&
                    asprintf( &psz_font_dir, "%s\\Fonts", psz_tmp ) < 0 )
                    psz_font_dir = NULL;
                free( psz_tmp );
            }
            free( psw_path );
        }
    }
#endif
    if( !psz_font_dir )
        psz_font_dir = config_GetUserDir( VLC_CACHE_DIR );

    if( !psz_font_dir )
        goto error;
    msg_Dbg( p_dec, "Setting libass fontdir: %s", psz_font_dir );
    ass_set_fonts_dir( p_library, psz_font_dir );
    free( psz_font_dir );
#ifdef WIN32
    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.1 );
#endif

    ass_set_extract_fonts( p_library, true );
    ass_set_style_overrides( p_library, NULL );

    /* Create the renderer */
    p_ass->p_renderer = p_renderer = ass_renderer_init( p_library );
    if( !p_renderer )
        goto error;

    ass_set_use_margins( p_renderer, false);
    //if( false )
    //    ass_set_margins( p_renderer, int t, int b, int l, int r);
    ass_set_hinting( p_renderer, ASS_HINTING_LIGHT );
    ass_set_font_scale( p_renderer, 1.0 );
    ass_set_line_spacing( p_renderer, 0.0 );

    const char *psz_font = NULL; /* We don't ship a default font with VLC */
    const char *psz_family = "Arial"; /* Use Arial if we can't find anything more suitable */

#ifdef HAVE_FONTCONFIG
#ifdef WIN32
    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 0.2 );
#endif
#if defined( LIBASS_VERSION ) && LIBASS_VERSION >= 0x00907000
    ass_set_fonts( p_renderer, psz_font, psz_family, true, NULL, 1 );  // setup default font/family
#else
    ass_set_fonts( p_renderer, psz_font, psz_family );  // setup default font/family
#endif
#ifdef WIN32
    if( p_dialog )
        dialog_ProgressSet( p_dialog, NULL, 1.0 );
#endif
#else
    /* FIXME you HAVE to give him a font if no fontconfig */
#if defined( LIBASS_VERSION ) && LIBASS_VERSION >= 0x00907000
    ass_set_fonts( p_renderer, psz_font, psz_family, false, NULL, 1 );
#else
    ass_set_fonts_nofc( p_renderer, psz_font, psz_family );
#endif
#endif
    memset( &p_ass->fmt, 0, sizeof(p_ass->fmt) );

    /* */
    val.p_address = p_ass;
    var_Set( p_dec->p_libvlc, "libass-handle", val );

    /* */
    vlc_mutex_unlock( &libass_lock );
#ifdef WIN32
    if( p_dialog )
        dialog_ProgressDestroy( p_dialog );
#endif
    return p_ass;

error:
    if( p_renderer )
        ass_renderer_done( p_renderer );
    if( p_library )
        ass_library_done( p_library );

    msg_Warn( p_dec, "Libass creation failed" );

    free( p_ass );
    vlc_mutex_unlock( &libass_lock );
    return NULL;
}
static void AssHandleRelease( ass_handle_t *p_ass )
{
    vlc_mutex_lock( &libass_lock );
    p_ass->i_refcount--;
    if( p_ass->i_refcount > 0 )
    {
        vlc_mutex_unlock( &libass_lock );
        return;
    }

    ass_renderer_done( p_ass->p_renderer );
    ass_library_done( p_ass->p_library );

    vlc_value_t val;
    val.p_address = NULL;
    var_Set( p_ass->p_libvlc, "libass-handle", val );

    vlc_mutex_unlock( &libass_lock );
    free( p_ass );
}
