/*****************************************************************************
 * puzzle.c : Puzzle game
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * Copyright (C) 2013      Vianney Boyer
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *          Vianney Boyer <vlcvboyer -at- gmail -dot- com>
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
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_mouse.h>
#include <vlc_picture.h>
#include <vlc_rand.h>

#include "filter_picture.h"

#include "puzzle.h"
#include "puzzle_bezier.h"
#include "puzzle_lib.h"
#include "puzzle_pce.h"
#include "puzzle_mgt.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ROWS_TEXT N_("Number of puzzle rows")
#define ROWS_LONGTEXT N_("Number of puzzle rows")
#define COLS_TEXT N_("Number of puzzle columns")
#define COLS_LONGTEXT N_("Number of puzzle columns")
#define MODE_TEXT N_("Game mode")
#define MODE_LONGTEXT N_("Select game mode variation from jigsaw puzzle to sliding puzzle.")
#define BORDER_TEXT N_("Border")
#define BORDER_LONGTEXT N_("Unshuffled Border width.")
#define PREVIEW_TEXT N_("Small preview")
#define PREVIEW_LONGTEXT N_("Show small preview.")
#define PREVIEWSIZE_TEXT N_("Small preview size")
#define PREVIEWSIZE_LONGTEXT N_("Show small preview size (percent of source).")
#define SHAPE_SIZE_TEXT N_("Piece edge shape size")
#define SHAPE_SIZE_LONGTEXT N_("Size of the curve along the piece's edge")
#define AUTO_SHUFFLE_TEXT N_("Auto shuffle")
#define AUTO_SHUFFLE_LONGTEXT N_("Auto shuffle delay during game")
#define AUTO_SOLVE_TEXT N_("Auto solve")
#define AUTO_SOLVE_LONGTEXT N_("Auto solve delay during game")
#define ROTATION_TEXT N_("Rotation")
#define ROTATION_LONGTEXT N_("Rotation parameter: none;180;90-270;mirror")

static const int pi_mode_values[] = { (int) 0, (int) 1, (int) 2, (int) 3 };
static const char *const ppsz_mode_descriptions[] = { N_("jigsaw puzzle"), N_("sliding puzzle"), N_("swap puzzle"), N_("exchange puzzle") };
static const int pi_rotation_values[] = { (int) 0, (int) 1, (int) 2, (int) 3 };
static const char *const ppsz_rotation_descriptions[] = { N_("0"), N_("0/180"), N_("0/90/180/270"), N_("0/90/180/270/mirror") };

#define CFG_PREFIX "puzzle-"

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("Puzzle interactive game video filter") )
    set_shortname( N_( "Puzzle" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer_with_range( CFG_PREFIX "rows", 4, 2, 42,
                            ROWS_TEXT, ROWS_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "cols", 4, 2, 42,
                            COLS_TEXT, COLS_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "border", 3, 0, 40,
              BORDER_TEXT, BORDER_LONGTEXT, false )
    add_bool( CFG_PREFIX "preview", false,
              PREVIEW_TEXT, PREVIEW_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "preview-size", 15, 0, 100,
              PREVIEWSIZE_TEXT, PREVIEWSIZE_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "shape-size", 90, 0, 100,
              SHAPE_SIZE_TEXT, SHAPE_SIZE_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "auto-shuffle", 0, 0, 30000,
              AUTO_SHUFFLE_TEXT, AUTO_SHUFFLE_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "auto-solve", 0, 0, 30000,
              AUTO_SOLVE_TEXT, AUTO_SOLVE_LONGTEXT, false )
    add_integer( CFG_PREFIX "rotation", 0,
              ROTATION_TEXT, ROTATION_LONGTEXT, false )
        change_integer_list(pi_rotation_values, ppsz_rotation_descriptions )
    add_integer( CFG_PREFIX "mode", 0,
              MODE_TEXT, MODE_LONGTEXT, false )
        change_integer_list(pi_mode_values, ppsz_mode_descriptions )

    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

const char *const ppsz_filter_options[] = {
    "rows", "cols","border", "preview", "preview-size", "mode", "shape-size", "auto-shuffle", "auto-solve",  "rotation", NULL
};

/**
 * Open the filter
 */
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Assert video in match with video out */
    if( !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) ) {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }

    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( p_filter->fmt_in.video.i_chroma );
    if( p_chroma == NULL || p_chroma->plane_count == 0 || p_chroma->pixel_size > 1 )
        return VLC_EGENERIC;

    /* Allocate structure */
    p_filter->p_sys = p_sys = calloc(1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* init some values */
    p_sys->b_shuffle_rqst    = true;
    p_sys->b_change_param    = true;
    p_sys->i_mouse_drag_pce  = NO_PCE;
    p_sys->i_pointed_pce     = NO_PCE;
    p_sys->i_magnet_accuracy = 3;

    /* Generate values of random bezier shapes */
    p_sys->ps_bezier_pts_H = calloc( SHAPES_QTY, sizeof( point_t *) );
    if( !p_sys->ps_bezier_pts_H )
    {
        free(p_filter->p_sys);
        p_filter->p_sys = NULL;
        return VLC_ENOMEM;
    }
    for (int32_t i_shape = 0; i_shape<SHAPES_QTY; i_shape++)
        p_sys->ps_bezier_pts_H[i_shape] = puzzle_rand_bezier(7);


    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    vlc_mutex_init( &p_sys->lock );
    vlc_mutex_init( &p_sys->pce_lock );

    p_sys->s_new_param.i_rows =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "rows" );
    p_sys->s_new_param.i_cols =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "cols" );
    p_sys->s_new_param.i_border =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "border" );
    p_sys->s_new_param.b_preview =
        var_CreateGetBoolCommand( p_filter, CFG_PREFIX "preview" );
    p_sys->s_new_param.i_preview_size =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "preview-size" );
    p_sys->s_new_param.i_shape_size =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "shape-size" );
    p_sys->s_new_param.i_auto_shuffle_speed =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "auto-shuffle" );
    p_sys->s_new_param.i_auto_solve_speed =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "auto-solve" );
    p_sys->s_new_param.i_rotate =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "rotation" );
    p_sys->s_new_param.i_mode =
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "mode" );

    var_AddCallback( p_filter, CFG_PREFIX "rows",         puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "cols",         puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "border",       puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "preview",      puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "preview-size", puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "shape-size",   puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "auto-shuffle", puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "auto-solve",   puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "rotation",     puzzle_Callback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "mode",     puzzle_Callback, p_sys );

    p_filter->pf_video_filter = Filter;
    p_filter->pf_video_mouse = puzzle_mouse;

    return VLC_SUCCESS;
}

/**
 * Close the filter
 */
static void Close( vlc_object_t *p_this ) {
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, CFG_PREFIX "rows",          puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "cols",          puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "border",        puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "preview",       puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "preview-size",  puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "shape-size",    puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "auto-shuffle",  puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "auto-solve",    puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "rotation",      puzzle_Callback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "mode",          puzzle_Callback, p_sys );

    /* Free allocated memory */
    puzzle_free_ps_puzzle_array ( p_filter );
    puzzle_free_ps_pieces_shapes ( p_filter);
    puzzle_free_ps_pieces ( p_filter );
    free(p_sys->ps_desk_planes);
    free(p_sys->ps_pict_planes);
    free( p_sys->pi_order );

    for (int32_t i_shape = 0; i_shape<SHAPES_QTY; i_shape++)
        free(p_sys->ps_bezier_pts_H[i_shape]);
    free(p_sys->ps_bezier_pts_H);

    free( p_sys );
}

/**
 * Filter a picture
 */
picture_t *Filter( filter_t *p_filter, picture_t *p_pic_in ) {
    if( !p_pic_in || !p_filter) return NULL;

    const video_format_t  *p_fmt_in = &p_filter->fmt_in.video;
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_t *p_pic_out = filter_NewPicture( p_filter );
    if( !p_pic_out ) {
        picture_Release( p_pic_in );
        return NULL;
    }

    int i_ret = 0;
    p_sys->b_bake_request = false;

    if ((p_sys->pi_order == NULL) || (p_sys->ps_desk_planes == NULL) || (p_sys->ps_pict_planes == NULL)  || (p_sys->ps_puzzle_array == NULL) || (p_sys->ps_pieces == NULL))
        p_sys->b_init = false;

    if ((p_sys->ps_pieces_shapes == NULL) && p_sys->s_current_param.b_advanced && (p_sys->s_current_param.i_shape_size != 0))
        p_sys->b_init = false;

    /* assert initialized & allocated data match with current frame characteristics */
    if ( p_sys->s_allocated.i_planes != p_pic_out->i_planes)
        p_sys->b_init = false;
    p_sys->s_current_param.i_planes = p_pic_out->i_planes;
    if (p_sys->ps_pict_planes != NULL) {
        for (uint8_t i_plane = 0; i_plane < p_sys->s_allocated.i_planes; i_plane++) {
            if ( (p_sys->ps_pict_planes[i_plane].i_lines != p_pic_in->p[i_plane].i_visible_lines)
                    || (p_sys->ps_pict_planes[i_plane].i_width != p_pic_in->p[i_plane].i_visible_pitch / p_pic_in->p[i_plane].i_pixel_pitch)
                    || (p_sys->ps_desk_planes[i_plane].i_lines != p_pic_out->p[i_plane].i_visible_lines)
                    || (p_sys->ps_desk_planes[i_plane].i_width != p_pic_out->p[i_plane].i_visible_pitch / p_pic_out->p[i_plane].i_pixel_pitch) )
                p_sys->b_init = false;
        }
    }

    p_sys->s_current_param.i_pict_width  = (int) p_pic_in->p[0].i_visible_pitch / p_pic_in->p[0].i_pixel_pitch;
    p_sys->s_current_param.i_pict_height = (int) p_pic_in->p[0].i_visible_lines;
    p_sys->s_current_param.i_desk_width  = (int) p_pic_out->p[0].i_visible_pitch / p_pic_out->p[0].i_pixel_pitch;
    p_sys->s_current_param.i_desk_height = (int) p_pic_out->p[0].i_visible_lines;

    /* assert no mismatch between sizes */
    if (    p_sys->s_current_param.i_pict_width  != p_sys->s_current_param.i_desk_width
         || p_sys->s_current_param.i_pict_height != p_sys->s_current_param.i_desk_height
         || p_sys->s_current_param.i_pict_width  != (int) p_fmt_in->i_visible_width
         || p_sys->s_current_param.i_pict_height != (int) p_fmt_in->i_visible_height ) {
        picture_Release(p_pic_in);
        picture_Release(p_pic_out);
        return NULL;
    }

    vlc_mutex_lock( &p_sys->lock );

    /* check if we have to compute initial data */
    if ( p_sys->b_change_param || p_sys->b_bake_request || !p_sys->b_init ) {
        if ( p_sys->s_allocated.i_rows != p_sys->s_new_param.i_rows
                || p_sys->s_allocated.i_cols != p_sys->s_new_param.i_cols
                || p_sys->s_allocated.i_rotate != p_sys->s_new_param.i_rotate
                || p_sys->s_allocated.i_mode != p_sys->s_new_param.i_mode
                || p_sys->b_bake_request  || !p_sys->b_init )
        {
            p_sys->b_bake_request = true;
            p_sys->b_init = false;
            p_sys->b_shuffle_rqst = true;
            p_sys->b_shape_init = false;
        }

        if ( p_sys->s_current_param.i_border != p_sys->s_new_param.i_border
                || p_sys->s_current_param.i_shape_size != p_sys->s_new_param.i_shape_size )
        {
            p_sys->b_bake_request = true;
            p_sys->b_shape_init = false;
        }

        /* depending on the game selected, set associated internal flags */
        switch ( p_sys->s_new_param.i_mode )
        {
          case 0:  /* jigsaw puzzle */
            p_sys->s_new_param.b_advanced    = true;
            p_sys->s_new_param.b_blackslot   = false;
            p_sys->s_new_param.b_near        = false;
            break;
          case 1:  /* sliding puzzle */
            p_sys->s_new_param.b_advanced    = false;
            p_sys->s_new_param.b_blackslot   = true;
            p_sys->s_new_param.b_near        = true;
            break;
          case 2:  /* swap puzzle */
            p_sys->s_new_param.b_advanced    = false;
            p_sys->s_new_param.b_blackslot   = false;
            p_sys->s_new_param.b_near        = true;
            break;
          case 3:  /* exchange puzzle */
            p_sys->s_new_param.b_advanced    = false;
            p_sys->s_new_param.b_blackslot   = false;
            p_sys->s_new_param.b_near        = false;
            break;
        }
        p_sys->s_current_param.i_mode = p_sys->s_new_param.i_mode;

        if ( p_sys->s_current_param.b_blackslot != p_sys->s_new_param.b_blackslot
                && p_sys->i_selected == NO_PCE
                && p_sys->s_current_param.b_blackslot )
            p_sys->i_selected = 0;

        if ( p_sys->s_current_param.i_auto_shuffle_speed != p_sys->s_new_param.i_auto_shuffle_speed )
            p_sys->i_auto_shuffle_countdown_val = init_countdown(p_sys->s_new_param.i_auto_shuffle_speed);

        if ( p_sys->s_current_param.i_auto_solve_speed != p_sys->s_new_param.i_auto_solve_speed )
            p_sys->i_auto_solve_countdown_val = init_countdown(p_sys->s_current_param.i_auto_solve_speed);

        p_sys->s_current_param.i_rows       = p_sys->s_new_param.i_rows;
        p_sys->s_current_param.i_cols       = p_sys->s_new_param.i_cols;
        p_sys->s_current_param.i_pieces_nbr = p_sys->s_current_param.i_rows * p_sys->s_current_param.i_cols;
        p_sys->s_current_param.b_advanced   = p_sys->s_new_param.b_advanced;
        if (!p_sys->s_new_param.b_advanced) {
            p_sys->s_current_param.b_blackslot   = p_sys->s_new_param.b_blackslot;
            p_sys->s_current_param.b_near        = p_sys->s_new_param.b_near || p_sys->s_new_param.b_blackslot;
            p_sys->s_current_param.i_border      = 0;
            p_sys->s_current_param.b_preview     = false;
            p_sys->s_current_param.i_preview_size= 0;
            p_sys->s_current_param.i_shape_size  = 0;
            p_sys->s_current_param.i_auto_shuffle_speed  = 0;
            p_sys->s_current_param.i_auto_solve_speed    = 0;
            p_sys->s_current_param.i_rotate      = 0;
        }
        else
        {
            p_sys->s_current_param.b_blackslot = false;
            p_sys->s_current_param.b_near      = false;
            p_sys->s_current_param.i_border    = p_sys->s_new_param.i_border;
            p_sys->s_current_param.b_preview   = p_sys->s_new_param.b_preview;
            p_sys->s_current_param.i_preview_size        = p_sys->s_new_param.i_preview_size;
            p_sys->s_current_param.i_shape_size          = p_sys->s_new_param.i_shape_size;
            p_sys->s_current_param.i_auto_shuffle_speed  = p_sys->s_new_param.i_auto_shuffle_speed;
            p_sys->s_current_param.i_auto_solve_speed    = p_sys->s_new_param.i_auto_solve_speed;
            p_sys->s_current_param.i_rotate     = p_sys->s_new_param.i_rotate;
        }
        p_sys->b_change_param = false;
    }

    vlc_mutex_unlock( &p_sys->lock );

    /* generate initial puzzle data when needed */
    if ( p_sys->b_bake_request ) {
        if (!p_sys->b_shuffle_rqst) {
            /* here we have to keep the same position
             * we have to save locations before generating new data
             */
            save_game_t *ps_save_game = puzzle_save(p_filter);
            if (!ps_save_game)
                return CopyInfoAndRelease( p_pic_out, p_pic_in );
            i_ret = puzzle_bake( p_filter, p_pic_out, p_pic_in );
            if ( i_ret != VLC_SUCCESS )
            {
                free(ps_save_game->ps_pieces);
                free(ps_save_game);
                return CopyInfoAndRelease( p_pic_out, p_pic_in );
            }
            puzzle_load( p_filter, ps_save_game);
            free(ps_save_game->ps_pieces);
            free(ps_save_game);
        }
        else {
            i_ret = puzzle_bake( p_filter, p_pic_out, p_pic_in );
            if ( i_ret != VLC_SUCCESS )
                return CopyInfoAndRelease( p_pic_out, p_pic_in );
        }
    }

    /* shuffle the desk and generate pieces data  */
    if ( p_sys->b_shuffle_rqst && p_sys->b_init ) {
        i_ret = puzzle_bake_piece ( p_filter );
        if (i_ret != VLC_SUCCESS)
            return CopyInfoAndRelease( p_pic_out, p_pic_in );
    }

    /* preset output pic */
    if ( !p_sys->b_bake_request && !p_sys->b_shuffle_rqst && p_sys->b_init && !p_sys->b_finished )
        puzzle_preset_desk_background(p_pic_out, 0, 127, 127);
    else {
        /* copy src to dst during init & bake process */
        for( uint8_t i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ )
            memcpy( p_pic_out->p[i_plane].p_pixels, p_pic_in->p[i_plane].p_pixels,
                p_pic_in->p[i_plane].i_pitch * (int32_t) p_pic_in->p[i_plane].i_visible_lines );
    }

    vlc_mutex_lock( &p_sys->pce_lock );

    /* manage the game, adjust locations, groups and regenerate some corrupted data if any */
    for (uint32_t i = 0; i < __MAX( 4, p_sys->s_allocated.i_pieces_nbr / 4 )
                             && ( !p_sys->b_bake_request && !p_sys->b_mouse_drag
                             && p_sys->b_init && p_sys->s_current_param.b_advanced ); i++)
    {
        puzzle_solve_pces_accuracy( p_filter );
    }

    for (uint32_t i = 0; i < __MAX( 4, p_sys->s_allocated.i_pieces_nbr / 4 )
                             && ( !p_sys->b_bake_request && !p_sys->b_mouse_drag
                             && p_sys->b_init && p_sys->s_current_param.b_advanced ); i++)
    {
        puzzle_solve_pces_group( p_filter );
    }

    if ( !p_sys->b_bake_request && !p_sys->b_mouse_drag && p_sys->b_init
            && p_sys->s_current_param.b_advanced )
        puzzle_count_pce_group( p_filter);
    if ( !p_sys->b_bake_request && !p_sys->b_mouse_drag && p_sys->b_init
            && p_sys->s_current_param.b_advanced ) {
        i_ret = puzzle_sort_layers( p_filter);
        if (i_ret != VLC_SUCCESS)
        {
            vlc_mutex_unlock( &p_sys->pce_lock );
            return CopyInfoAndRelease( p_pic_out, p_pic_in );
        }
    }

    for (uint32_t i = 0; i < __MAX( 4, p_sys->s_allocated.i_pieces_nbr / 24 )
                            && ( !p_sys->b_bake_request && !p_sys->b_mouse_drag
                            && p_sys->b_init && p_sys->s_current_param.b_advanced ); i++)
    {
        p_sys->i_calc_corn_loop++;
        p_sys->i_calc_corn_loop %= p_sys->s_allocated.i_pieces_nbr;
        puzzle_calculate_corners( p_filter, p_sys->i_calc_corn_loop );
    }

    /* computer moves some piece depending on auto_solve and auto_shuffle param */
    if ( !p_sys->b_bake_request && !p_sys->b_mouse_drag && p_sys->b_init
             && p_sys->ps_puzzle_array != NULL && p_sys->s_current_param.b_advanced )
    {
        puzzle_auto_shuffle( p_filter );
        puzzle_auto_solve( p_filter );
    }

    vlc_mutex_unlock( &p_sys->pce_lock );

    /* draw output pic */
    if ( !p_sys->b_bake_request && p_sys->b_init  && p_sys->ps_puzzle_array != NULL ) {

        puzzle_draw_borders(p_filter, p_pic_in, p_pic_out);

        p_sys->i_pointed_pce = NO_PCE;
        puzzle_draw_pieces(p_filter, p_pic_in, p_pic_out);

        /* when puzzle_draw_pieces() has not updated p_sys->i_pointed_pce,
         * use puzzle_find_piece to define the piece pointed by the mouse
         */
        if (p_sys->i_pointed_pce == NO_PCE)
            p_sys->i_mouse_drag_pce = puzzle_find_piece( p_filter, p_sys->i_mouse_x, p_sys->i_mouse_y, -1);
        else
            p_sys->i_mouse_drag_pce = p_sys->i_pointed_pce;

        if (p_sys->s_current_param.b_preview )
            puzzle_draw_preview(p_filter, p_pic_in, p_pic_out);

        /* highlight the selected piece when not playing jigsaw mode */
        if ( p_sys->i_selected != NO_PCE && !p_sys->s_current_param.b_blackslot
                && !p_sys->s_current_param.b_advanced )
        {
            int32_t c = (p_sys->i_selected % p_sys->s_allocated.i_cols);
            int32_t r = (p_sys->i_selected / p_sys->s_allocated.i_cols);

            puzzle_draw_rectangle(p_pic_out,
                p_sys->ps_puzzle_array[r][c][0].i_x,
                p_sys->ps_puzzle_array[r][c][0].i_y,
                p_sys->ps_puzzle_array[r][c][0].i_width,
                p_sys->ps_puzzle_array[r][c][0].i_lines,
                255, 127, 127);
        }

        /* draw the blackslot when playing sliding puzzle mode */
        if ( p_sys->i_selected != NO_PCE && p_sys->s_current_param.b_blackslot
                && !p_sys->s_current_param.b_advanced )
        {
            int32_t c = (p_sys->i_selected % p_sys->s_allocated.i_cols);
            int32_t r = (p_sys->i_selected / p_sys->s_allocated.i_cols);

            puzzle_fill_rectangle(p_pic_out,
                p_sys->ps_puzzle_array[r][c][0].i_x,
                p_sys->ps_puzzle_array[r][c][0].i_y,
                p_sys->ps_puzzle_array[r][c][0].i_width,
                p_sys->ps_puzzle_array[r][c][0].i_lines,
                0, 127, 127);
        }

        /* Draw the 'puzzle_shuffle' button if the puzzle is finished */
        if ( p_sys->b_finished )
            puzzle_draw_sign(p_pic_out, 0, 0, SHUFFLE_WIDTH, SHUFFLE_LINES, ppsz_shuffle_button, false);

        /* draw an arrow at mouse pointer to indicate current action (rotation...) */
        if ((p_sys->i_mouse_drag_pce != NO_PCE) && !p_sys->b_mouse_drag
                && !p_sys->b_finished && p_sys->s_current_param.b_advanced )
        {
            vlc_mutex_lock( &p_sys->pce_lock );

            int32_t i_delta_x;

            if (p_sys->s_current_param.i_rotate != 3)
                i_delta_x = 0;
            else if ( (p_sys->ps_pieces[p_sys->i_mouse_drag_pce].i_actual_angle & 1) == 0)
                i_delta_x = p_sys->ps_desk_planes[0].i_pce_max_width / 6;
            else
                i_delta_x = p_sys->ps_desk_planes[0].i_pce_max_lines / 6;

            if (p_sys->s_current_param.i_rotate == 0)
                p_sys->i_mouse_action = 0;
            else if (p_sys->s_current_param.i_rotate == 1)
                p_sys->i_mouse_action = 2;
            else if ( p_sys->i_mouse_x >= ( p_sys->ps_pieces[p_sys->i_mouse_drag_pce].i_center_x + i_delta_x) )
                p_sys->i_mouse_action = -1;  /* rotate counterclockwise */
            else if ( p_sys->i_mouse_x <= ( p_sys->ps_pieces[p_sys->i_mouse_drag_pce].i_center_x - i_delta_x) )
                p_sys->i_mouse_action = +1;
            else
                p_sys->i_mouse_action = 4;   /* center click: only mirror */

            if ( p_sys->i_mouse_action == +1 )
                puzzle_draw_sign(p_pic_out, p_sys->i_mouse_x - ARROW_WIDTH,
                                 p_sys->i_mouse_y, ARROW_WIDTH, ARROW_LINES, ppsz_rot_arrow_sign, false);
            else if ( p_sys->i_mouse_action == -1 )
                puzzle_draw_sign(p_pic_out, p_sys->i_mouse_x - ARROW_WIDTH,
                                 p_sys->i_mouse_y, ARROW_WIDTH, ARROW_LINES, ppsz_rot_arrow_sign, true);
            else if ( p_sys->i_mouse_action == 4 )
                puzzle_draw_sign(p_pic_out, p_sys->i_mouse_x - ARROW_WIDTH,
                                 p_sys->i_mouse_y, ARROW_WIDTH, ARROW_LINES, ppsz_mir_arrow_sign, false);

            vlc_mutex_unlock( &p_sys->pce_lock );
        }
    }

    return CopyInfoAndRelease( p_pic_out, p_pic_in );
}

/*****************************************************************************
 * Misc stuff...
 *****************************************************************************/
int puzzle_Callback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    if( !strcmp( psz_var, CFG_PREFIX "rows" ) ) {
        p_sys->s_new_param.i_rows = __MAX( 1, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "cols" ) ) {
        p_sys->s_new_param.i_cols = __MAX( 1, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "border" ) ) {
        p_sys->s_new_param.i_border = __MAX( 0, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "preview" ) ) {
        p_sys->s_new_param.b_preview = newval.b_bool;
    }
    else if( !strcmp( psz_var, CFG_PREFIX "preview-size" ) ) {
        p_sys->s_new_param.i_preview_size = newval.i_int;
    }
    else if( !strcmp( psz_var, CFG_PREFIX "shape-size" ) ) {
        p_sys->s_new_param.i_shape_size = newval.i_int;
    }
    else if( !strcmp( psz_var, CFG_PREFIX "auto-shuffle" ) ) {
        p_sys->s_new_param.i_auto_shuffle_speed = newval.i_int;
    }
    else if( !strcmp( psz_var, CFG_PREFIX "auto-solve" ) ) {
        p_sys->s_new_param.i_auto_solve_speed = newval.i_int;
    }
    else if( !strcmp( psz_var, CFG_PREFIX "rotation" ) ) {
        p_sys->s_new_param.i_rotate = newval.i_int;
    }
    else if( !strcmp( psz_var, CFG_PREFIX "mode" ) ) {
        p_sys->s_new_param.i_mode = newval.i_int;
    }

    p_sys->b_change_param = true;
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/* mouse callback */
int puzzle_mouse( filter_t *p_filter, vlc_mouse_t *p_mouse,
                  const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const video_format_t  *p_fmt_in = &p_filter->fmt_in.video;

    /* Only take events inside the puzzle area */
    if( p_new->i_x < 0 || p_new->i_x >= (int)p_fmt_in->i_width ||
        p_new->i_y < 0 || p_new->i_y >= (int)p_fmt_in->i_height )
        return VLC_EGENERIC;

    if (! p_sys->b_init || p_sys->b_change_param) {
        *p_mouse = *p_new;
        return VLC_SUCCESS;
    }

    p_sys->i_mouse_x = p_new->i_x;
    p_sys->i_mouse_y = p_new->i_y;

    /* If the puzzle is finished, shuffle it if needed */
    if( p_sys->b_finished ) {
        p_sys->b_mouse_drag = false;
        p_sys->b_mouse_mvt = false;
        if( vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT ) &&
            p_new->i_x < SHUFFLE_WIDTH && p_new->i_y < SHUFFLE_LINES )
        {
            p_sys->b_shuffle_rqst = true;
            return VLC_EGENERIC;
        }
        else
        {
            /* otherwise we can forward the mouse */
            *p_mouse = *p_new;
            return VLC_SUCCESS;
        }
    }

    if ( !p_sys->s_current_param.b_advanced ) {
        /* "square" game mode (sliding puzzle, swap...) */
        const bool b_clicked = vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT );

        if( b_clicked )
        {
            /* */
            const int32_t i_border_width = p_fmt_in->i_width * p_sys->s_current_param.i_border / 100 / 2;
            const int32_t i_border_height = p_fmt_in->i_height * p_sys->s_current_param.i_border / 100 / 2;
            const int32_t i_pos_x = (p_new->i_x - i_border_width) * p_sys->s_allocated.i_cols / (p_fmt_in->i_width - 2*i_border_width);
            const int32_t i_pos_y = (p_new->i_y - i_border_height) * p_sys->s_allocated.i_rows / (p_fmt_in->i_height - 2*i_border_height);

            const int32_t i_pos = i_pos_y * p_sys->s_allocated.i_cols + i_pos_x;
            p_sys->i_mouse_drag_pce = i_pos;

            /* do not take into account if border clicked */
            if ((p_new->i_x <= i_border_width) || (p_new->i_y <=  i_border_height) || (p_new->i_x >= (int) p_fmt_in->i_width -  i_border_width) || (p_new->i_y >= (int) p_fmt_in->i_height -  i_border_height ) )
            {
                *p_mouse = *p_new;
                return VLC_SUCCESS;
            }
            else if( p_sys->i_selected == NO_PCE )
                p_sys->i_selected = i_pos;
            else if( p_sys->i_selected == i_pos && !p_sys->s_current_param.b_blackslot )
                p_sys->i_selected = -1;
            else if( ( p_sys->i_selected == i_pos + 1 && p_sys->i_selected%p_sys->s_allocated.i_cols != 0 )
                  || ( p_sys->i_selected == i_pos - 1 && i_pos % p_sys->s_allocated.i_cols != 0 )
                  || p_sys->i_selected == i_pos +  p_sys->s_allocated.i_cols
                  || p_sys->i_selected == i_pos -  p_sys->s_allocated.i_cols
                  || !p_sys->s_current_param.b_near )

            {
                /* Swap two pieces */
                int32_t a = p_sys->pi_order[ p_sys->i_selected ];
                p_sys->pi_order[ p_sys->i_selected ] = p_sys->pi_order[ i_pos ];
                p_sys->pi_order[ i_pos ] = a;

                /* regen piece location from updated pi_order */
                if ( p_sys->ps_pieces != NULL && p_sys->pi_order != NULL )
                {
                    int32_t i = 0;
                    for (int32_t row = 0; row < p_sys->s_allocated.i_rows; row++) {
                        for (int32_t col = 0; col < p_sys->s_allocated.i_cols; col++) {
                            int32_t orow = p_sys->pi_order[i] / (p_sys->s_allocated.i_cols);
                            int32_t ocol = p_sys->pi_order[i] % (p_sys->s_allocated.i_cols);

                            p_sys->ps_pieces[i].i_original_row = orow;
                            p_sys->ps_pieces[i].i_original_col = ocol;
                            p_sys->ps_pieces[i].i_top_shape    = 0;
                            p_sys->ps_pieces[i].i_btm_shape    = 0;
                            p_sys->ps_pieces[i].i_right_shape  = 0;
                            p_sys->ps_pieces[i].i_left_shape   = 0;
                            p_sys->ps_pieces[i].i_actual_angle = 0;
                            p_sys->ps_pieces[i].i_actual_mirror = +1;
                            p_sys->ps_pieces[i].b_overlap      = false;
                            p_sys->ps_pieces[i].b_finished     = false;
                            p_sys->ps_pieces[i].i_group_ID     = i;

                            for (uint8_t i_plane = 0; i_plane < p_sys->s_allocated.i_planes; i_plane++) {
                                p_sys->ps_pieces[i].ps_piece_in_plane[i_plane].i_width     = p_sys->ps_puzzle_array[row][col][i_plane].i_width;
                                p_sys->ps_pieces[i].ps_piece_in_plane[i_plane].i_lines     = p_sys->ps_puzzle_array[row][col][i_plane].i_lines;
                                p_sys->ps_pieces[i].ps_piece_in_plane[i_plane].i_original_x = p_sys->ps_puzzle_array[orow][ocol][i_plane].i_x;
                                p_sys->ps_pieces[i].ps_piece_in_plane[i_plane].i_original_y = p_sys->ps_puzzle_array[orow][ocol][i_plane].i_y;
                                p_sys->ps_pieces[i].ps_piece_in_plane[i_plane].i_actual_x   = p_sys->ps_puzzle_array[row][col][i_plane].i_x;
                                p_sys->ps_pieces[i].ps_piece_in_plane[i_plane].i_actual_y   = p_sys->ps_puzzle_array[row][col][i_plane].i_y;
                            }
                            i++;
                        }
                    }
                }

                p_sys->i_selected = p_sys->s_current_param.b_blackslot ? i_pos : NO_PCE;
                p_sys->b_finished = puzzle_is_finished( p_sys, p_sys->pi_order );
            }
        }
    }
    else /* jigsaw puzzle mode */
    {
        if ((p_sys->ps_desk_planes == NULL)  || (p_sys->ps_pict_planes == NULL)  || (p_sys->ps_puzzle_array == NULL) || (p_sys->ps_pieces == NULL)) {
            *p_mouse = *p_new;
            return VLC_SUCCESS;
        }

        if( vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT ) )
        {

            vlc_mutex_lock( &p_sys->pce_lock );

            if (p_sys->i_mouse_drag_pce != NO_PCE) {
                int i_ret = puzzle_piece_foreground( p_filter, p_sys->i_mouse_drag_pce);
                if (i_ret != VLC_SUCCESS)
                {
                    vlc_mutex_unlock( &p_sys->pce_lock );
                    return i_ret;
                }
                p_sys->i_mouse_drag_pce = 0;

                uint32_t i_group_ID = p_sys->ps_pieces[0].i_group_ID;
                for (uint32_t i = 0; i < p_sys->s_allocated.i_pieces_nbr; i++) {
                    if ( i_group_ID == p_sys->ps_pieces[i].i_group_ID ) {
                        p_sys->ps_pieces[i].b_finished = false;
                    }
                    else {
                        break;
                    }
                }

                p_sys->b_mouse_drag = true;
                p_sys->b_mouse_mvt = false;
            }
            else {
            /* player click an empty area then search a piece which is overlapping another one and place it here */
                p_sys->b_mouse_drag = false;
                for (uint32_t i = 0; i < p_sys->s_allocated.i_pieces_nbr; i++)
                    if ( p_sys->ps_pieces[i].b_overlap ) {
                        puzzle_move_group( p_filter, i, p_new->i_x - p_sys->ps_pieces[i].i_center_x,  p_new->i_y - p_sys->ps_pieces[i].i_center_y );
                        p_sys->ps_pieces[i].b_overlap = false;
                        break;
                    }
                p_sys->b_mouse_drag = false;
            }

            vlc_mutex_unlock( &p_sys->pce_lock );

        }
        else if( vlc_mouse_HasReleased( p_old, p_new, MOUSE_BUTTON_LEFT ) )
        {
            if ( !p_sys->b_mouse_mvt && p_sys->b_mouse_drag ) {
                /* piece clicked without any mouse mvt => rotate it or mirror */
                if ( p_sys->s_current_param.i_rotate != 0) {
                    vlc_mutex_lock( &p_sys->pce_lock );

                    uint32_t i_group_ID = p_sys->ps_pieces[0].i_group_ID;

                    for (uint32_t i = 0; i < p_sys->s_allocated.i_pieces_nbr; i++)
                        if ( i_group_ID == p_sys->ps_pieces[i].i_group_ID )
                            puzzle_rotate_pce( p_filter, i, p_sys->i_mouse_action, p_sys->ps_pieces[0].i_center_x, p_sys->ps_pieces[0].i_center_y, p_sys->i_mouse_action != 4 ? true : false );

                    vlc_mutex_unlock( &p_sys->pce_lock );
                }
            }
            p_sys->b_mouse_drag = false;
            p_sys->b_mouse_mvt = false;
        }
        else /* no action on left button */
        {
            /* check if the mouse is in the preview area */
            switch ( p_sys->i_preview_pos )
            {
              case 0:
                if ( p_new->i_x < (int)p_fmt_in->i_width / 2 && p_new->i_y < (int)p_fmt_in->i_height / 2 )
                    p_sys->i_preview_pos++;
                break;
              case 1:
                if ( p_new->i_x > (int)p_fmt_in->i_width / 2 && p_new->i_y < (int)p_fmt_in->i_height / 2 )
                    p_sys->i_preview_pos++;
                break;
              case 2:
                if ( p_new->i_x > (int)p_fmt_in->i_width / 2 && p_new->i_y > (int)p_fmt_in->i_height / 2 )
                    p_sys->i_preview_pos++;
                break;
              case 3:
                if ( p_new->i_x < (int)p_fmt_in->i_width / 2 && p_new->i_y > (int)p_fmt_in->i_height / 2 )
                    p_sys->i_preview_pos++;
                break;
            }
            p_sys->i_preview_pos %= 4;

            if ( !vlc_mouse_IsLeftPressed( p_new ) )
                p_sys->b_mouse_drag = false;

            int i_dx, i_dy;
            vlc_mouse_GetMotion( &i_dx, &i_dy, p_old, p_new );
            if ( i_dx != 0 || i_dy != 0 )
                p_sys->b_mouse_mvt = true;

            if (p_sys->b_mouse_drag) {
                if ( ( p_new->i_x <= 0 ) || ( p_new->i_y <=  0 ) || ( p_new->i_x >= (int) p_fmt_in->i_width )
                        || ( p_new->i_y >= (int) p_fmt_in->i_height ) )
                {
                    /* if the mouse is outside the window, stop moving the piece/group */
                    p_sys->b_mouse_drag = false;
                    p_sys->b_mouse_mvt = true;
                }
                else if ( i_dx != 0 || i_dy != 0 )
                {
                    vlc_mutex_lock( &p_sys->pce_lock );

                    puzzle_move_group( p_filter, p_sys->i_mouse_drag_pce, i_dx, i_dy);

                    vlc_mutex_unlock( &p_sys->pce_lock );
                }
            }
        }
    }
    return VLC_EGENERIC;
}
