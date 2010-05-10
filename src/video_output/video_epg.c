/*****************************************************************************
 * video_epg.c : EPG manipulation functions
 *****************************************************************************
 * Copyright (C) 2010 Adrien Maglo
 *
 * Author: Adrien Maglo <magsoft@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_vout_osd.h>
#include <vlc_events.h>
#include <vlc_input_item.h>
#include <vlc_epg.h>

/* Layout percentage defines */
#define EPG_TOP 0.7
#define EPG_LEFT 0.1
#define EPG_NAME_SIZE 0.05
#define EPG_PROGRAM_SIZE 0.03

static subpicture_region_t * vout_OSDEpgSlider( int i_x, int i_y,
                                                int i_width, int i_height,
                                                float f_ratio )
{
    video_format_t fmt;
    subpicture_region_t *p_region;

    /* Create a new subpicture region */
    video_format_Init( &fmt, VLC_CODEC_YUVA );
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_sar_num = 0;
    fmt.i_sar_den = 1;

    p_region = subpicture_region_New( &fmt );
    if( !p_region )
        return NULL;

    p_region->i_x = i_x;
    p_region->i_y = i_y;

    picture_t *p_picture = p_region->p_picture;

    f_ratio = __MIN( __MAX( f_ratio, 0 ), 1 );
    int i_filled_part_width = f_ratio * i_width;

    for( int j = 0; j < i_height; j++ )
    {
        for( int i = 0; i < i_width; i++ )
        {
            #define WRITE_COMP( plane, value ) \
                p_picture->p[plane].p_pixels[p_picture->p[plane].i_pitch * j + i] = value

            /* Draw the slider. */
            bool is_outline = j == 0 || j == i_height - 1
                              || i == 0 || i == i_width - 1;
            WRITE_COMP( 0, is_outline ? 0x00 : 0xff );
            WRITE_COMP( 1, 0x80 );
            WRITE_COMP( 2, 0x80 );

            /* We can see the video through the part of the slider
               which corresponds to the leaving time. */
            bool is_border = j < 3 || j > i_height - 4
                             || i < 3 || i > i_width - 4
                             || i < i_filled_part_width;
            WRITE_COMP( 3, is_border ? 0xff : 0x00 );

            #undef WRITE_COMP
        }
    }

    return p_region;
}


static subpicture_region_t * vout_OSDEpgText( const char *psz_string,
                                              int i_x, int i_y,
                                              int i_size, uint32_t i_color )
{
    video_format_t fmt;
    subpicture_region_t *p_region;

    if( !psz_string )
        return NULL;

    /* Create a new subpicture region */
    video_format_Init( &fmt, VLC_CODEC_TEXT );
    fmt.i_sar_num = 0;
    fmt.i_sar_den = 1;

    p_region = subpicture_region_New( &fmt );
    if( !p_region )
        return NULL;

    /* Set subpicture parameters */
    p_region->psz_text = strdup( psz_string );
    p_region->i_align = 0;
    p_region->i_x = i_x;
    p_region->i_y = i_y;

    /* Set text style */
    p_region->p_style = text_style_New();
    if( p_region->p_style )
    {
        p_region->p_style->i_font_size = i_size;
        p_region->p_style->i_font_color = i_color;
        p_region->p_style->i_font_alpha = 0;
    }

    return p_region;
}


static subpicture_region_t * vout_BuildOSDEpg( vlc_epg_t *p_epg,
                                               int i_x, int i_y,
                                               int i_visible_width,
                                               int i_visible_height )
{
    subpicture_region_t *p_region_ret;
    subpicture_region_t **pp_region = &p_region_ret;

    time_t i_test = time( NULL );

    /* Display the name of the channel. */
    *pp_region = vout_OSDEpgText( p_epg->psz_name,
                                  i_x + i_visible_width * EPG_LEFT,
                                  i_y + i_visible_height * EPG_TOP,
                                  i_visible_height * EPG_NAME_SIZE,
                                  0x00ffffff );

    if( !*pp_region )
        return p_region_ret;

    /* Display the name of the current program. */
    pp_region = &(* pp_region)->p_next;
    *pp_region = vout_OSDEpgText( p_epg->p_current->psz_name,
                                  i_x + i_visible_width * ( EPG_LEFT + 0.025 ),
                                  i_y + i_visible_height * ( EPG_TOP + 0.05 ),
                                  i_visible_height * EPG_PROGRAM_SIZE,
                                  0x00ffffff );

    if( !*pp_region )
        return p_region_ret;

    /* Display the current program time slider. */
    pp_region = &(* pp_region)->p_next;
    *pp_region = vout_OSDEpgSlider( i_x + i_visible_width * EPG_LEFT,
                                    i_y + i_visible_height * ( EPG_TOP + 0.1 ),
                                    i_visible_width * ( 1 - 2 * EPG_LEFT ),
                                    i_visible_height * 0.05,
                                    ( i_test - p_epg->p_current->i_start )
                                    / (float)p_epg->p_current->i_duration );

    if( !*pp_region )
        return p_region_ret;

    /* Format the hours of the beginning and the end of the current program. */
    struct tm tm_start, tm_end;
    time_t t_start = p_epg->p_current->i_start;
    time_t t_end = p_epg->p_current->i_start + p_epg->p_current->i_duration;
    localtime_r( &t_start, &tm_start );
    localtime_r( &t_end, &tm_end );
    char psz_start[128];
    char psz_end[128];
    snprintf( psz_start, sizeof(psz_start), "%2.2d:%2.2d",
              tm_start.tm_hour, tm_start.tm_min );
    snprintf( psz_end, sizeof(psz_end), "%2.2d:%2.2d",
              tm_end.tm_hour, tm_end.tm_min );

    /* Display those hours. */
    pp_region = &(* pp_region)->p_next;
    *pp_region = vout_OSDEpgText( psz_start,
                                  i_x + i_visible_width * ( EPG_LEFT + 0.02 ),
                                  i_y + i_visible_height * ( EPG_TOP + 0.15 ),
                                  i_visible_height * EPG_PROGRAM_SIZE,
                                  0x00ffffff );

    if( !*pp_region )
        return p_region_ret;

    pp_region = &(* pp_region)->p_next;
    *pp_region = vout_OSDEpgText( psz_end,
                                  i_x + i_visible_width * ( 1 - EPG_LEFT - 0.085 ),
                                  i_y + i_visible_height * ( EPG_TOP + 0.15 ),
                                  i_visible_height * EPG_PROGRAM_SIZE,
                                  0x00ffffff );

    return p_region_ret;
}

struct subpicture_updater_sys_t
{
    vlc_epg_t *p_epg;
};

static int OSDEpgValidate( subpicture_t *p_subpic,
                           bool has_src_changed, const video_format_t *p_fmt_src,
                           bool has_dst_changed, const video_format_t *p_fmt_dst,
                           mtime_t i_ts )
{
    VLC_UNUSED(p_subpic); VLC_UNUSED(i_ts); VLC_UNUSED(p_fmt_src);
    VLC_UNUSED(has_dst_changed); VLC_UNUSED(p_fmt_dst);

    if( !has_src_changed && !has_dst_changed)
        return VLC_SUCCESS;
    return VLC_EGENERIC;
}

static void OSDEpgUpdate( subpicture_t *p_subpic,
                          const video_format_t *p_fmt_src,
                          const video_format_t *p_fmt_dst,
                          mtime_t i_ts )
{
    subpicture_updater_sys_t *p_sys = p_subpic->updater.p_sys;
    VLC_UNUSED(p_fmt_dst); VLC_UNUSED(i_ts);

    p_subpic->i_original_picture_width  = p_fmt_src->i_width;
    p_subpic->i_original_picture_height = p_fmt_src->i_height;
    p_subpic->p_region = vout_BuildOSDEpg( p_sys->p_epg,
                                           p_fmt_src->i_x_offset,
                                           p_fmt_src->i_y_offset,
                                           p_fmt_src->i_visible_width,
                                           p_fmt_src->i_visible_height );
}

static void OSDEpgDestroy( subpicture_t *p_subpic )
{
    subpicture_updater_sys_t *p_sys = p_subpic->updater.p_sys;

    vlc_epg_Delete( p_sys->p_epg );
    free( p_sys );
}

/**
 * \brief Show EPG information about the current program of an input item
 * \param p_vout pointer to the vout the information is to be showed on
 * \param p_input pointer to the input item the information is to be showed
 */
int vout_OSDEpg( vout_thread_t *p_vout, input_item_t *p_input )
{
    subpicture_t *p_spu;
    mtime_t i_now = mdate();

    char *psz_now_playing = input_item_GetNowPlaying( p_input );
    vlc_epg_t *p_epg = NULL;

    vlc_mutex_lock( &p_input->lock );

    /* Look for the current program EPG event */
    for( int i = 0; i < p_input->i_epg; i++ )
    {
        vlc_epg_t *p_tmp = p_input->pp_epg[i];

        if( p_tmp->p_current && p_tmp->p_current->psz_name
            && psz_now_playing != NULL
            && !strcmp( p_tmp->p_current->psz_name, psz_now_playing ) )
        {
            p_epg = vlc_epg_New( p_tmp->psz_name );
            vlc_epg_Merge( p_epg, p_tmp );
            break;
        }
    }

    vlc_mutex_unlock( &p_input->lock );

    /* If no EPG event has been found. */
    if( p_epg == NULL )
        return VLC_EGENERIC;

    subpicture_updater_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
    {
        vlc_epg_Delete( p_epg );
        return VLC_EGENERIC;
    }
    p_sys->p_epg = p_epg;
    subpicture_updater_t updater = {
        .pf_validate = OSDEpgValidate,
        .pf_update   = OSDEpgUpdate,
        .pf_destroy  = OSDEpgDestroy,
        .p_sys       = p_sys
    };

    p_spu = subpicture_New( &updater );
    if( !p_spu )
    {
        vlc_epg_Delete( p_sys->p_epg );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_spu->i_channel = SPU_DEFAULT_CHANNEL;
    p_spu->i_start = i_now;
    p_spu->i_stop = i_now + 3000 * INT64_C(1000);
    p_spu->b_ephemer = true;
    p_spu->b_absolute = true;
    p_spu->b_fade = true;

    vout_PutSubpicture( p_vout, p_spu );

    return VLC_SUCCESS;
}
