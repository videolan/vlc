/*****************************************************************************
 * wall.c : Wall video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
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
    add_obsolete_string( CFG_PREFIX "element-aspect" ) /* since 4.0.0 */

    add_shortcut( "wall" )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_filter_options[] = {
    "cols", "rows", "active", NULL
};

/* */
typedef struct
{
    bool b_active;
    int  i_output;
    int  i_width;
    int  i_height;
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

    /* Compute placements/size of the windows */
    int i_active = 0;
    for( int y = 0, i_top = 0; y < p_sys->i_row; y++ )
    {
        unsigned i_height = ((p_splitter->fmt.i_visible_height - i_top)
                             / (p_sys->i_row - y)) & ~1;

        for( int x = 0, i_left = 0; x < p_sys->i_col; x++ )
        {
            wall_output_t *p_output = &p_sys->pp_output[x][y];
            unsigned i_width = ((p_splitter->fmt.i_visible_width - i_left)
                                / (p_sys->i_col - x)) & ~1;

            /* */
            p_output->b_active = pb_active[y * p_sys->i_col + x];
            p_output->i_output = -1;
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

