/*****************************************************************************
 * wall.c : Wall video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_video_splitter.h>
#include <vlc_vout_window.h>

#define ROW_MAX (15)
#define COL_MAX (15)

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COLS_TEXT N_("Number of columns")
#define COLS_LONGTEXT N_("Number of horizontal windows in " \
    "which to split the video.")

#define ROWS_TEXT N_("Number of rows")
#define ROWS_LONGTEXT N_("Number of vertical windows in " \
    "which to split the video.")

#define ACTIVE_TEXT N_("Active windows")
#define ACTIVE_LONGTEXT N_("Comma-separated list of active windows, " \
    "defaults to all")

#define ASPECT_TEXT N_("Element aspect ratio")
#define ASPECT_LONGTEXT N_("Aspect ratio of the individual displays " \
   "building the wall.")

#define CFG_PREFIX "wall-"

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("Wall video filter") )
    set_shortname( N_("Image wall" ))
    set_capability( "video splitter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SPLITTER )

    add_integer( CFG_PREFIX "cols", 3, COLS_TEXT, COLS_LONGTEXT, false )
    change_integer_range( 1, COL_MAX )
    add_integer( CFG_PREFIX "rows", 3, ROWS_TEXT, ROWS_LONGTEXT, false )
    change_integer_range( 1, ROW_MAX )
    add_string( CFG_PREFIX "active", NULL, ACTIVE_TEXT, ACTIVE_LONGTEXT,
                 true )
    add_string( CFG_PREFIX "element-aspect", "16:9", ASPECT_TEXT, ASPECT_LONGTEXT, false )

    add_shortcut( "wall" )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_filter_options[] = {
    "cols", "rows", "active", "element-aspect", NULL
};

/* */
typedef struct
{
    bool b_active;
    int  i_output;
    int  i_width;
    int  i_height;
    vlc_video_align_t align;
    int  i_left;
    int  i_top;
} wall_output_t;

typedef struct
{
    int           i_col;
    int           i_row;
    int           i_output;
    wall_output_t pp_output[COL_MAX][ROW_MAX]; /* [x][y] */
} video_splitter_sys_t;

static int Filter( video_splitter_t *, picture_t *pp_dst[], picture_t * );
static int Mouse( video_splitter_t *, int, vout_window_mouse_event_t * );

/**
 * This function allocates and initializes a Wall splitter module.
 */
static int Open( vlc_object_t *p_this )
{
    video_splitter_t *p_splitter = (video_splitter_t*)p_this;
    video_splitter_sys_t *p_sys;

    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( p_splitter->fmt.i_chroma );
    if( p_chroma == NULL || p_chroma->plane_count == 0 )
        return VLC_EGENERIC;

    p_splitter->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
        return VLC_ENOMEM;

    config_ChainParse( p_splitter, CFG_PREFIX, ppsz_filter_options,
                       p_splitter->p_cfg );

    /* */
    p_sys->i_col = var_CreateGetInteger( p_splitter, CFG_PREFIX "cols" );
    p_sys->i_col = VLC_CLIP( p_sys->i_col, 1, COL_MAX );

    p_sys->i_row = var_CreateGetInteger( p_splitter, CFG_PREFIX "rows" );
    p_sys->i_row = VLC_CLIP( p_sys->i_row, 1, ROW_MAX );

    msg_Dbg( p_splitter, "opening a %i x %i wall",
             p_sys->i_col, p_sys->i_row );

    /* */
    char *psz_state = var_CreateGetNonEmptyString( p_splitter, CFG_PREFIX "active" );

    /* */
    bool pb_active[COL_MAX*ROW_MAX];
    for( int i = 0; i < COL_MAX*ROW_MAX; i++ )
        pb_active[i] = psz_state == NULL;

    /* Parse active list if provided */
    char *psz_tmp = psz_state;
    while( psz_tmp && *psz_tmp )
    {
        char *psz_next = strchr( psz_tmp, ',' );
        if( psz_next )
            *psz_next++ = '\0';

        const int i_index = atoi( psz_tmp );
        if( i_index >= 0 && i_index < COL_MAX*ROW_MAX )
            pb_active[i_index] = true;

        psz_tmp = psz_next;
    }
    free( psz_state );

    /* Parse aspect ratio if provided */
    int i_aspect = 0;
    char *psz_aspect = var_CreateGetNonEmptyString( p_splitter,
                                                    CFG_PREFIX "element-aspect" );
    if( psz_aspect )
    {
        int i_ar_num, i_ar_den;
        if( sscanf( psz_aspect, "%d:%d", &i_ar_num, &i_ar_den ) == 2 &&
            i_ar_num > 0 && i_ar_den > 0 )
        {
            i_aspect = i_ar_num * VOUT_ASPECT_FACTOR / i_ar_den;
        }
        else
        {
            msg_Warn( p_splitter, "invalid aspect ratio specification" );
        }
        free( psz_aspect );
    }
    if( i_aspect <= 0 )
        i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;

    /* Compute placements/size of the windows */
    const unsigned w1 = ( p_splitter->fmt.i_visible_width / p_sys->i_col ) & ~1;
    const unsigned h1 = ( w1 * VOUT_ASPECT_FACTOR / i_aspect ) & ~1;

    const unsigned h2 = ( p_splitter->fmt.i_visible_height / p_sys->i_row ) & ~1;
    const unsigned w2 = ( h2 * i_aspect / VOUT_ASPECT_FACTOR ) & ~1;

    unsigned i_target_width;
    unsigned i_target_height;
    unsigned i_hstart, i_hend;
    unsigned i_vstart, i_vend;
    bool b_vstart_rounded;
    bool b_hstart_rounded;

    if( h1 * p_sys->i_row < p_splitter->fmt.i_visible_height )
    {
        i_target_width = w2;
        i_target_height = h2;

        i_vstart = 0;
        b_vstart_rounded = false;
        i_vend = p_splitter->fmt.i_visible_height;

        unsigned i_tmp = i_target_width * p_sys->i_col;
        while( i_tmp < p_splitter->fmt.i_width )
            i_tmp += p_sys->i_col;

        i_hstart = (( i_tmp - p_splitter->fmt.i_visible_width ) / 2)&~1;
        b_hstart_rounded  = ( ( i_tmp - p_splitter->fmt.i_visible_width ) % 2 ) ||
            ( ( ( i_tmp - p_splitter->fmt.i_visible_width ) / 2 ) & 1 );
        i_hend = i_hstart + p_splitter->fmt.i_visible_width;
    }
    else
    {
        i_target_height = h1;
        i_target_width = w1;

        i_hstart = 0;
        b_hstart_rounded = false;
        i_hend = p_splitter->fmt.i_visible_width;

        unsigned i_tmp = i_target_height * p_sys->i_row;
        while( i_tmp < p_splitter->fmt.i_visible_height )
            i_tmp += p_sys->i_row;

        i_vstart = ( ( i_tmp - p_splitter->fmt.i_visible_height ) / 2 ) & ~1;
        b_vstart_rounded  = ( ( i_tmp - p_splitter->fmt.i_visible_height ) % 2 ) ||
            ( ( ( i_tmp - p_splitter->fmt.i_visible_height ) / 2 ) & 1 );
        i_vend = i_vstart + p_splitter->fmt.i_visible_height;
    }
    msg_Dbg( p_splitter, "target resolution %dx%d", i_target_width, i_target_height );
    msg_Dbg( p_splitter, "target window (%d,%d)-(%d,%d)", i_hstart,i_vstart,i_hend,i_vend );

    int i_active = 0;
    for( int y = 0, i_top = 0; y < p_sys->i_row; y++ )
    {
        /* */
        int i_height = 0;
        int i_halign = 0;
        if( y * i_target_height >= i_vstart &&
            ( y + 1 ) * i_target_height <= i_vend )
        {
            i_height = i_target_height;
        }
        else if( ( y + 1 ) * i_target_height <= i_vstart ||
                 ( y * i_target_height ) >= i_vend )
        {
            i_height = 0;
        }
        else
        {
            i_height = ( i_target_height -
                         i_vstart%i_target_height );
            if(  y >= ( p_sys->i_row / 2 ) )
            {
                i_halign = VLC_VIDEO_ALIGN_TOP;
                i_height -= b_vstart_rounded ? 2: 0;
            }
            else
            {
                i_halign = VLC_VIDEO_ALIGN_BOTTOM;
            }
        }

        /* */
        for( int x = 0, i_left = 0; x < p_sys->i_col; x++ )
        {
            wall_output_t *p_output = &p_sys->pp_output[x][y];

            /* */
            int i_width;
            int i_valign = 0;
            if( x*i_target_width >= i_hstart &&
                (x+1)*i_target_width <= i_hend )
            {
                i_width = i_target_width;
            }
            else if( ( x + 1 ) * i_target_width <= i_hstart ||
                     ( x * i_target_width ) >= i_hend )
            {
                i_width = 0;
            }
            else
            {
                i_width = ( i_target_width - i_hstart % i_target_width );
                if( x >= ( p_sys->i_col / 2 ) )
                {
                    i_valign = VLC_VIDEO_ALIGN_LEFT;
                    i_width -= b_hstart_rounded ? 2: 0;
                }
                else
                {
                    i_valign = VLC_VIDEO_ALIGN_RIGHT;
                }
            }

            /* */
            p_output->b_active = pb_active[y * p_sys->i_col + x] &&
                                 i_height > 0 && i_width > 0;
            p_output->i_output = -1;
            p_output->align.vertical = i_valign;
            p_output->align.horizontal = i_halign;
            p_output->i_width = i_width;
            p_output->i_height = i_height;
            p_output->i_left = i_left;
            p_output->i_top = i_top;

            msg_Dbg( p_splitter, "window %dx%d at %d:%d size %dx%d", 
                     x, y, i_left, i_top, i_width, i_height );

            if( p_output->b_active )
                i_active++;

            i_left += i_width;
        }
        i_top += i_height;
    }
    if( i_active <= 0 )
    {
        msg_Err( p_splitter, "No active video output" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Setup output configuration */
    p_splitter->i_output = i_active;
    p_splitter->p_output = calloc( p_splitter->i_output,
                                   sizeof(*p_splitter->p_output) );
    if( !p_splitter->p_output )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    for( int y = 0, i_output = 0; y < p_sys->i_row; y++ )
    {
        for( int x = 0; x < p_sys->i_col; x++ )
        {
            wall_output_t *p_output = &p_sys->pp_output[x][y];
            if( !p_output->b_active )
                continue;

            p_output->i_output = i_output++;

            video_splitter_output_t *p_cfg = &p_splitter->p_output[p_output->i_output];

            video_format_Copy( &p_cfg->fmt, &p_splitter->fmt );
            p_cfg->fmt.i_visible_width  =
            p_cfg->fmt.i_width          = p_output->i_width;
            p_cfg->fmt.i_visible_height =
            p_cfg->fmt.i_height         = p_output->i_height;
            p_cfg->fmt.i_sar_num        = p_splitter->fmt.i_sar_num;
            p_cfg->fmt.i_sar_den        = p_splitter->fmt.i_sar_den;
            p_cfg->window.i_x   = p_output->i_left;
            p_cfg->window.i_y   = p_output->i_top;
            p_cfg->window.align = p_output->align;
            p_cfg->psz_module = NULL;
        }
    }

    /* */
    p_splitter->pf_filter = Filter;
    p_splitter->mouse = Mouse;

    return VLC_SUCCESS;
}

/**
 * Terminate a splitter module.
 */
static void Close( vlc_object_t *p_this )
{
    video_splitter_t *p_splitter = (video_splitter_t*)p_this;
    video_splitter_sys_t *p_sys = p_splitter->p_sys;

    free( p_splitter->p_output );
    free( p_sys );
}

static int Filter( video_splitter_t *p_splitter, picture_t *pp_dst[], picture_t *p_src )
{
    video_splitter_sys_t *p_sys = p_splitter->p_sys;

    if( video_splitter_NewPicture( p_splitter, pp_dst ) )
    {
        picture_Release( p_src );
        return VLC_EGENERIC;
    }

    for( int y = 0; y < p_sys->i_row; y++ )
    {
        for( int x = 0; x < p_sys->i_col; x++ )
        {
            wall_output_t *p_output = &p_sys->pp_output[x][y];
            if( !p_output->b_active )
                continue;

            picture_t *p_dst = pp_dst[p_output->i_output];

            /* */
            picture_t tmp = *p_src;
            for( int i = 0; i < tmp.i_planes; i++ )
            {
                plane_t *p0 = &tmp.p[0];
                plane_t *p = &tmp.p[i];
                const int i_y = p_output->i_top  * p->i_visible_pitch / p0->i_visible_pitch;
                const int i_x = p_output->i_left * p->i_visible_lines / p0->i_visible_lines;

                p->p_pixels += i_y * p->i_pitch + ( i_x - (i_x % p->i_pixel_pitch));
            }
            picture_Copy( p_dst, &tmp );
        }
    }

    picture_Release( p_src );
    return VLC_SUCCESS;
}
static int Mouse( video_splitter_t *p_splitter, int i_index,
                  vout_window_mouse_event_t *restrict ev )
{
    video_splitter_sys_t *p_sys = p_splitter->p_sys;

    for( int y = 0; y < p_sys->i_row; y++ )
    {
        for( int x = 0; x < p_sys->i_col; x++ )
        {
            wall_output_t *p_output = &p_sys->pp_output[x][y];

            if( p_output->b_active && p_output->i_output == i_index )
            {
                ev->x += p_output->i_left;
                ev->y += p_output->i_top;
                return VLC_SUCCESS;
            }
        }
    }
    vlc_assert_unreachable();
    return VLC_EGENERIC;
}

