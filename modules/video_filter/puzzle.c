/*****************************************************************************
 * puzzle.c : Puzzle game
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "filter_picture.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ROWS_TEXT N_("Number of puzzle rows")
#define ROWS_LONGTEXT N_("Number of puzzle rows")
#define COLS_TEXT N_("Number of puzzle columns")
#define COLS_LONGTEXT N_("Number of puzzle columns")
#define BLACKSLOT_TEXT N_("Make one tile a black slot")
#define BLACKSLOT_LONGTEXT N_("Make one slot black. Other tiles can only be swapped with the black slot.")

#define CFG_PREFIX "puzzle-"

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("Puzzle interactive game video filter") )
    set_shortname( N_( "Puzzle" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer_with_range( CFG_PREFIX "rows", 4, 2, 16, NULL,
                            ROWS_TEXT, ROWS_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "cols", 4, 2, 16, NULL,
                            COLS_TEXT, COLS_LONGTEXT, false )
    add_bool( CFG_PREFIX "black-slot", false, NULL,
              BLACKSLOT_TEXT, BLACKSLOT_LONGTEXT, false )

    set_callbacks( Open, Close )
vlc_module_end()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_filter_options[] = {
    "rows", "cols", "black-slot", NULL
};

static picture_t *Filter( filter_t *, picture_t * );
static int Mouse( filter_t *, vlc_mouse_t *, const vlc_mouse_t *, const vlc_mouse_t * );

static bool IsFinished( filter_sys_t * );
static void Shuffle( filter_sys_t * );
static int PuzzleCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

struct filter_sys_t
{
    /* */
    int i_cols;
    int i_rows;
    bool b_blackslot;
    int *pi_order;
    int i_selected;
    bool b_finished;

    /* */
    vlc_mutex_t lock;
    bool b_change;
    struct
    {
        int i_cols;
        int i_rows;
        bool b_blackslot;
    } change;
};

#define SHUFFLE_WIDTH 81
#define SHUFFLE_HEIGHT 13
static const char *shuffle_button[] =
{
".................................................................................",
"..............  ............................   ........   ......  ...............",
"..............  ...........................  .........  ........  ...............",
"..............  ...........................  .........  ........  ...............",
"..     .......  .    .......  ....  ......     ......     ......  ........    ...",
".  .... ......   ...  ......  ....  .......  .........  ........  .......  ..  ..",
".  ...........  ....  ......  ....  .......  .........  ........  ......  ....  .",
".      .......  ....  ......  ....  .......  .........  ........  ......        .",
"..      ......  ....  ......  ....  .......  .........  ........  ......  .......",
"......  ......  ....  ......  ....  .......  .........  ........  ......  .......",
". ....  ......  ....  ......  ...   .......  .........  ........  .......  .... .",
"..     .......  ....  .......    .  .......  .........  ........  ........     ..",
"................................................................................."
};


/**
 * Open the filter
 */
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* */
    if( !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) )
    {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys->pi_order = NULL;

    vlc_mutex_init( &p_sys->lock );
    p_sys->change.i_rows =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "rows" );
    p_sys->change.i_cols =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "cols" );
    p_sys->change.b_blackslot =
        var_CreateGetBoolCommand( p_filter, CFG_PREFIX "black-slot" );
    p_sys->b_change = true;

    var_AddCallback( p_filter, CFG_PREFIX "rows", PuzzleCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "cols", PuzzleCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "black-slot", PuzzleCallback, p_sys );

    p_filter->pf_video_filter = Filter;
    p_filter->pf_mouse = Mouse;

    return VLC_SUCCESS;
}

/**
 * Close the filter
 */
static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, CFG_PREFIX "rows", PuzzleCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "cols", PuzzleCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "black-slot", PuzzleCallback, p_sys );

    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys->pi_order );

    free( p_sys );
}

/**
 * Filter a picture
 */
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_t *p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* */
    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->b_change )
    {
        p_sys->i_rows      = p_sys->change.i_rows;
        p_sys->i_cols      = p_sys->change.i_cols;
        p_sys->b_blackslot = p_sys->change.b_blackslot;
        p_sys->b_change = false;

        Shuffle( p_sys );
    }
    vlc_mutex_unlock( &p_sys->lock );

    /* */
    const int i_rows = p_sys->i_rows;
    const int i_cols = p_sys->i_cols;

    /* Draw each piece of the puzzle at the right place */
    for( int i_plane = 0; i_plane < p_outpic->i_planes; i_plane++ )
    {
        const plane_t *p_in = &p_pic->p[i_plane];
        const int i_pitch = p_in->i_pitch;
        plane_t *p_out = &p_outpic->p[i_plane];

        for( int i = 0; i < i_cols * i_rows; i++ )
        {
            int i_col = i % i_cols;
            int i_row = i / i_cols;
            int i_ocol = p_sys->pi_order[i] % i_cols;
            int i_orow = p_sys->pi_order[i] / i_cols;
            int i_last_row = i_row + 1;
            i_orow *= p_in->i_lines / i_rows;
            i_row *= p_in->i_lines / i_rows;
            i_last_row *= p_in->i_lines / i_rows;

            if( p_sys->b_blackslot && !p_sys->b_finished && i == p_sys->i_selected )
            {
                uint8_t color = ( i_plane == Y_PLANE ? 0x0 : 0x80 );
                for( ; i_row < i_last_row; i_row++, i_orow++ )
                {
                    memset( p_out->p_pixels + i_row * i_pitch + i_col * i_pitch / i_cols,
                            color, i_pitch / i_cols );
                }
            }
            else
            {
                for( ; i_row < i_last_row; i_row++, i_orow++ )
                {
                    memcpy( p_out->p_pixels + i_row * i_pitch + i_col * i_pitch / i_cols,
                            p_in->p_pixels + i_orow * i_pitch + i_ocol * i_pitch / i_cols,
                            i_pitch / i_cols );
                }
            }
        }
    }

    /* Draw the borders of the selected slot */
    if( p_sys->i_selected != -1 && !p_sys->b_blackslot )
    {
        const plane_t *p_in = &p_pic->p[Y_PLANE];
        const int i_pitch = p_in->i_pitch;
        plane_t *p_out = &p_outpic->p[Y_PLANE];

        int i_col = p_sys->i_selected % i_cols;
        int i_row = p_sys->i_selected / i_cols;
        int i_last_row = i_row + 1;
        i_row *= p_in->i_lines / i_rows;
        i_last_row *= p_in->i_lines / i_rows;
        memset( p_out->p_pixels + i_row * i_pitch + i_col * i_pitch / i_cols,
                0xff, i_pitch / i_cols );
        for( ; i_row < i_last_row; i_row++ )
        {
            p_out->p_pixels[   i_row * i_pitch
                             + i_col * i_pitch / i_cols ] = 0xff;
            p_out->p_pixels[ i_row * i_pitch
                             + (i_col+1) * i_pitch / i_cols - 1 ] = 0xff;
        }
        i_row--;
        memset( p_out->p_pixels + i_row * i_pitch + i_col * i_pitch / i_cols,
                0xff, i_pitch / i_cols );
    }

    /* Draw the 'Shuffle' button if the puzzle is finished */
    if( p_sys->b_finished )
    {
        plane_t *p_out = &p_outpic->p[Y_PLANE];
        for( int i = 0; i < SHUFFLE_HEIGHT; i++ )
        {
            for( int j = 0; j < SHUFFLE_WIDTH; j++ )
            {
                if( shuffle_button[i][j] == '.' )
                   p_out->p_pixels[ i * p_out->i_pitch + j ] = 0xff;
            }
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static int Mouse( filter_t *p_filter, vlc_mouse_t *p_mouse,
                  const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const video_format_t  *p_fmt = &p_filter->fmt_in.video;

    /* Only take events inside the puzzle erea */
    if( p_new->i_x < 0 || p_new->i_x >= (int)p_fmt->i_width ||
        p_new->i_y < 0 || p_new->i_y >= (int)p_fmt->i_height )
        return VLC_EGENERIC;

    /* */
    const bool b_clicked = vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT );

    /* If the puzzle is finished, shuffle it if needed */
    if( p_sys->b_finished )
    {
        if( b_clicked &&
            p_new->i_x < SHUFFLE_WIDTH && p_new->i_y < SHUFFLE_HEIGHT )
        {
            p_sys->b_change = true;
            return VLC_EGENERIC;
        }
        else
        {
            /* This is the only case where we can forward the mouse */
            *p_mouse = *p_new;
            return VLC_SUCCESS;
        }
    }
    if( !b_clicked )
        return VLC_EGENERIC;

    /* */
    const int i_pos_x = p_new->i_x * p_sys->i_cols / p_fmt->i_width;
    const int i_pos_y = p_new->i_y * p_sys->i_rows / p_fmt->i_height;
    const int i_pos = i_pos_y * p_sys->i_cols + i_pos_x;

    if( p_sys->i_selected == -1 )
    {
        p_sys->i_selected = i_pos;
    }
    else if( p_sys->i_selected == i_pos && !p_sys->b_blackslot )
    {
        p_sys->i_selected = -1;
    }
    else if( ( p_sys->i_selected == i_pos + 1 && p_sys->i_selected%p_sys->i_cols != 0 )
          || ( p_sys->i_selected == i_pos - 1 && i_pos % p_sys->i_cols != 0 )
          || p_sys->i_selected == i_pos + p_sys->i_cols
          || p_sys->i_selected == i_pos - p_sys->i_cols )
    {
        /* Swap two pieces */
        int a = p_sys->pi_order[ p_sys->i_selected ];
        p_sys->pi_order[ p_sys->i_selected ] = p_sys->pi_order[ i_pos ];
        p_sys->pi_order[ i_pos ] = a;

        p_sys->i_selected = p_sys->b_blackslot ? i_pos : -1;
        p_sys->b_finished = IsFinished( p_sys );
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Misc stuff...
 *****************************************************************************/
static int PuzzleCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    if( !strcmp( psz_var, CFG_PREFIX "rows" ) )
    {
        p_sys->change.i_rows = __MAX( 1, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "cols" ) )
    {
        p_sys->change.i_cols = __MAX( 1, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "black-slot" ) )
    {
        p_sys->change.b_blackslot = newval.b_bool;
    }
    p_sys->b_change = true;
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static bool IsFinished( filter_sys_t *p_sys )
{
    for( int i = 0; i < p_sys->i_cols * p_sys->i_rows; i++ )
    {
        if( i != p_sys->pi_order[i] )
            return false;
    }
    return true;
}

static bool IsValid( filter_sys_t *p_sys )
{
    const int i_count = p_sys->i_cols * p_sys->i_rows;

    if( !p_sys->b_blackslot )
        return true;

    int d = 0;
    for( int i = 0; i < i_count; i++ )
    {
        if( p_sys->pi_order[i] == i_count - 1 )
        {
            d += i / p_sys->i_cols + 1;
            continue;
        }
        for( int j = i+1; j < i_count; j++ )
        {
            if( p_sys->pi_order[j] == i_count - 1 )
                continue;
            if( p_sys->pi_order[i] > p_sys->pi_order[j] )
                d++;
        }
    }
    return (d%2) == 0;
}

static void Shuffle( filter_sys_t *p_sys )
{
    const int i_count = p_sys->i_cols * p_sys->i_rows;

    free( p_sys->pi_order );

    p_sys->pi_order = calloc( i_count, sizeof(*p_sys->pi_order) );
    do
    {
        for( int i = 0; i < i_count; i++ )
            p_sys->pi_order[i] = -1;

        for( int c = 0; c < i_count; )
        {
            int i = rand() % i_count;
            if( p_sys->pi_order[i] == -1 )
                p_sys->pi_order[i] = c++;
        }
        p_sys->b_finished = IsFinished( p_sys );

    } while( p_sys->b_finished || !IsValid( p_sys ) );

    if( p_sys->b_blackslot )
    {
        for( int i = 0; i < i_count; i++ )
        {
            if( p_sys->pi_order[i] == i_count - 1 )
            {
                p_sys->i_selected = i;
                break;
            }
        }
    }
    else
    {
        p_sys->i_selected = -1;
    }
}

