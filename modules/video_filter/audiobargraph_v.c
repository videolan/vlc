/*****************************************************************************
 * audiobargraph_v.c : audiobargraph video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 *
 * Authors: Clement CHESNIN <clement.chesnin@gmail.com>
 *          Philippe COENT <philippe.coent@tdf.fr>
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
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include <vlc_image.h>
#include <vlc_osd.h>

#ifdef LoadImage
#   undef LoadImage
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
 
#define I_VALUES_TEXT N_("Value of the audio channels levels")
#define I_VALUES_LONGTEXT N_("Value of the audio level of each channels between 0 and 1" \
    "Each level should be separated with ':'.")
#define POSX_TEXT N_("X coordinate")
#define POSX_LONGTEXT N_("X coordinate of the bargraph." )
#define POSY_TEXT N_("Y coordinate")
#define POSY_LONGTEXT N_("Y coordinate of the bargraph." )
#define TRANS_TEXT N_("Transparency of the bargraph")
#define TRANS_LONGTEXT N_("Bargraph transparency value " \
  "(from 0 for full transparency to 255 for full opacity)." )
#define POS_TEXT N_("Bargraph position")
#define POS_LONGTEXT N_( \
  "Enforce the bargraph position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg 6 = top-right).")
#define ALARM_TEXT N_("Alarm")
#define ALARM_LONGTEXT N_("Signals a silence and displays and alert " \
                "(0=no alarm, 1=alarm).")
#define BARWIDTH_TEXT N_("Bar width in pixel (default : 10)")
#define BARWIDTH_LONGTEXT N_("Width in pixel of each bar in the BarGraph to be displayed " \
                "(default : 10).")

#define CFG_PREFIX "audiobargraph_v-"

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

static int  OpenSub  ( vlc_object_t * );
static int  OpenVideo( vlc_object_t * );
static void Close    ( vlc_object_t * );

vlc_module_begin ()

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_SUBPIC )

    set_capability( "sub filter", 0 )
    set_callbacks( OpenSub, Close )
    set_description( N_("Audio Bar Graph Video sub filter") )
    set_shortname( N_("Audio Bar Graph Video") )
    add_shortcut( "audiobargraph_v" )

    add_string( CFG_PREFIX "i_values", NULL, NULL, I_VALUES_TEXT, I_VALUES_LONGTEXT, false )
    add_integer( CFG_PREFIX "x", 0, NULL, POSX_TEXT, POSX_LONGTEXT, true )
    add_integer( CFG_PREFIX "y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, true )
    add_integer_with_range( CFG_PREFIX "transparency", 255, 0, 255, NULL,
        TRANS_TEXT, TRANS_LONGTEXT, false )
    add_integer( CFG_PREFIX "position", -1, NULL, POS_TEXT, POS_LONGTEXT, false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, NULL )
    add_integer( CFG_PREFIX "alarm", 0, NULL, ALARM_TEXT, ALARM_LONGTEXT, true )
    add_integer( CFG_PREFIX "barWidth", 10, NULL, BARWIDTH_TEXT, BARWIDTH_LONGTEXT, true )

    /* video output filter submodule */
    add_submodule ()
    set_capability( "video filter2", 0 )
    set_callbacks( OpenVideo, Close )
    set_description( N_("Audio Bar Graph Video sub filter") )
    add_shortcut( "audiobargraph_v" )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/*****************************************************************************
 * Structure to hold the Bar Graph properties
 ****************************************************************************/
typedef struct
{
    int i_alpha;       /* -1 means use default alpha */
    int nbChannels;
    int *i_values;
    picture_t *p_pic;
    mtime_t date;
    int scale;
    int alarm;
    int barWidth;

} BarGraph_t;

/**
 * Private data holder
 */
struct filter_sys_t
{
    filter_t *p_blend;

    vlc_mutex_t lock;
    
    BarGraph_t p_BarGraph;

    int i_pos;
    int i_pos_x;
    int i_pos_y;
    bool b_absolute;

    /* On the fly control variable */
    bool b_spu_update;
};

static const char *const ppsz_filter_options[] = {
    "i_values", "x", "y", "transparency", "position", "alarm", "barWidth", NULL
};

static const char *const ppsz_filter_callbacks[] = {
    "audiobargraph_v-i_values",
    "audiobargraph_v-x",
    "audiobargraph_v-y",
    "audiobargraph_v-transparency",
    "audiobargraph_v-position",
    "audiobargraph_v-alarm",
    "audiobargraph_v-barWidth",
    NULL
};

static int OpenCommon( vlc_object_t *, bool b_sub );

static subpicture_t *FilterSub( filter_t *, mtime_t );
static picture_t    *FilterVideo( filter_t *, picture_t * );

static int BarGraphCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

static void LoadBarGraph( vlc_object_t *, BarGraph_t *);
void parse_i_values( BarGraph_t *p_BarGraph, char *i_values);

/**
 * Open the sub filter
 */
static int OpenSub( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

/**
 * Open the video filter
 */
static int OpenVideo( vlc_object_t *p_this )
{
    return OpenCommon( p_this, false );
}

/**
 * Common open function
 */
static int OpenCommon( vlc_object_t *p_this, bool b_sub )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    BarGraph_t *p_BarGraph;
    char* i_values = NULL;

    /* */
    if( !b_sub && !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) )
    {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }


    /* */
    p_filter->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_BarGraph = &(p_sys->p_BarGraph);

    /* */
    p_sys->p_blend = NULL;
    if( !b_sub )
    {

        p_sys->p_blend = filter_NewBlend( VLC_OBJECT(p_filter),
                                          &p_filter->fmt_in.video );
        if( !p_sys->p_blend )
        {
            //free( p_BarGraph );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }

    /* */
    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    /* create and initialize variables */
    p_sys->i_pos = var_CreateGetIntegerCommand( p_filter, "audiobargraph_v-position" );
    p_sys->i_pos_x = var_CreateGetIntegerCommand( p_filter, "audiobargraph_v-x" );
    p_sys->i_pos_y = var_CreateGetIntegerCommand( p_filter, "audiobargraph_v-y" );
    p_BarGraph->i_alpha = var_CreateGetIntegerCommand( p_filter,
                                                        "audiobargraph_v-transparency" );
    p_BarGraph->i_alpha = __MAX( __MIN( p_BarGraph->i_alpha, 255 ), 0 );
    i_values = var_CreateGetStringCommand( p_filter, "audiobargraph_v-i_values" );
    //p_BarGraph->nbChannels = 0;
    //p_BarGraph->i_values = NULL;
    parse_i_values(p_BarGraph, i_values);
    p_BarGraph->alarm = var_CreateGetIntegerCommand( p_filter, "audiobargraph_v-alarm" );
    p_BarGraph->barWidth = var_CreateGetIntegerCommand( p_filter, "audiobargraph_v-barWidth" );
    p_BarGraph->scale = 400;

    /* Ignore aligment if a position is given for video filter */
    if( !b_sub && p_sys->i_pos_x >= 0 && p_sys->i_pos_y >= 0 )
        p_sys->i_pos = 0;

    vlc_mutex_init( &p_sys->lock );
    LoadBarGraph( p_this, p_BarGraph );
    p_sys->b_spu_update = true;

    for( int i = 0; ppsz_filter_callbacks[i]; i++ )
        var_AddCallback( p_filter, ppsz_filter_callbacks[i],
                         BarGraphCallback, p_sys );

    /* Misc init */
    if( b_sub )
    {
        p_filter->pf_sub_filter = FilterSub;
    }
    else
    {
        p_filter->pf_video_filter = FilterVideo;
    }

    free( i_values );
    return VLC_SUCCESS;
}

/**
 * Common close function
 */
static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    BarGraph_t *p_BarGraph = &(p_sys->p_BarGraph);

    for( int i = 0; ppsz_filter_callbacks[i]; i++ )
        var_DelCallback( p_filter, ppsz_filter_callbacks[i],
                         BarGraphCallback, p_sys );

    if( p_sys->p_blend )
        filter_DeleteBlend( p_sys->p_blend );

    vlc_mutex_destroy( &p_sys->lock );
    
    if( p_BarGraph->p_pic )
    {
        picture_Release( p_BarGraph->p_pic );
        p_BarGraph->p_pic = NULL;
    }
    free( p_BarGraph->i_values );
    
    free( p_sys );
}

/**
 * Sub filter
 */
static subpicture_t *FilterSub( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    BarGraph_t *p_BarGraph = &(p_sys->p_BarGraph);

    subpicture_t *p_spu;
    subpicture_region_t *p_region;
    video_format_t fmt;
    picture_t *p_pic;

    vlc_mutex_lock( &p_sys->lock );
    /* Basic test:  b_spu_update occurs on a dynamic change */
    if( !p_sys->b_spu_update )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return 0;
    }

    p_pic = p_BarGraph->p_pic;

    /* Allocate the subpicture internal data. */
    p_spu = filter_NewSubpicture( p_filter );
    if( !p_spu )
        goto exit;

    p_spu->b_absolute = p_sys->b_absolute;
    p_spu->i_start = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = true;

    /* Send an empty subpicture to clear the display when needed */
    if( !p_pic || !p_BarGraph->i_alpha )
        goto exit;

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_CODEC_YUVA;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = p_pic->p[Y_PLANE].i_visible_pitch;
    fmt.i_height = fmt.i_visible_height = p_pic->p[Y_PLANE].i_visible_lines;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = subpicture_region_New( &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        p_spu = NULL;
        goto exit;
    }

    /* */
    picture_Copy( p_region->p_picture, p_pic );

    /*  where to locate the bar graph: */
    if( p_sys->i_pos < 0 )
    {   /*  set to an absolute xy */
        p_region->i_align = OSD_ALIGN_RIGHT | OSD_ALIGN_TOP;
        p_spu->b_absolute = true;
    }
    else
    {   /* set to one of the 9 relative locations */
        p_region->i_align = p_sys->i_pos;
        p_spu->b_absolute = false;
    }

    p_region->i_x = p_sys->i_pos_x;
    p_region->i_y = p_sys->i_pos_y;

    p_spu->p_region = p_region;

    p_spu->i_alpha = p_BarGraph->i_alpha ;

exit:
    vlc_mutex_unlock( &p_sys->lock );

    return p_spu;
}

/**
 * Video filter
 */
static picture_t *FilterVideo( filter_t *p_filter, picture_t *p_src )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    BarGraph_t *p_BarGraph = &(p_sys->p_BarGraph);

    picture_t *p_dst = filter_NewPicture( p_filter );
    if( !p_dst )
        goto exit;

    picture_Copy( p_dst, p_src );

    /* */
    vlc_mutex_lock( &p_sys->lock );

    /* */
    const picture_t *p_pic = p_BarGraph->p_pic;
    if( p_pic )
    {
        const video_format_t *p_fmt = &p_pic->format;
        const int i_dst_w = p_filter->fmt_out.video.i_visible_width;
        const int i_dst_h = p_filter->fmt_out.video.i_visible_height;

        if( p_sys->i_pos )
        {
            if( p_sys->i_pos & SUBPICTURE_ALIGN_BOTTOM )
            {
                p_sys->i_pos_y = i_dst_h - p_fmt->i_visible_height;
            }
            else if ( !(p_sys->i_pos & SUBPICTURE_ALIGN_TOP) )
            {
                p_sys->i_pos_y = ( i_dst_h - p_fmt->i_visible_height ) / 2;
            }
            else
            {
                p_sys->i_pos_y = 0;
            }

            if( p_sys->i_pos & SUBPICTURE_ALIGN_RIGHT )
            {
                p_sys->i_pos_x = i_dst_w - p_fmt->i_visible_width;
            }
            else if ( !(p_sys->i_pos & SUBPICTURE_ALIGN_LEFT) )
            {
                p_sys->i_pos_x = ( i_dst_w - p_fmt->i_visible_width ) / 2;
            }
            else
            {
                p_sys->i_pos_x = 0;
            }
        }

        /* */
        const int i_alpha = p_BarGraph->i_alpha;
        if( filter_ConfigureBlend( p_sys->p_blend, i_dst_w, i_dst_h, p_fmt ) ||
            filter_Blend( p_sys->p_blend, p_dst, p_sys->i_pos_x, p_sys->i_pos_y,
                          p_pic, i_alpha ) )
        {
            msg_Err( p_filter, "failed to blend a picture" );
        }
    }
    vlc_mutex_unlock( &p_sys->lock );

exit:
    picture_Release( p_src );
    return p_dst;
}

/*****************************************************************************
 * Callback to update params on the fly
 *****************************************************************************/
static int BarGraphCallback( vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    BarGraph_t *p_BarGraph = &(p_sys->p_BarGraph);
    char* i_values;
    char* res = NULL;

    vlc_mutex_lock( &p_sys->lock );
    if ( !strcmp( psz_var, "audiobargraph_v-x" ) )
    {
        p_sys->i_pos_x = newval.i_int;
    }
    else if ( !strcmp( psz_var, "audiobargraph_v-y" ) )
    {
        p_sys->i_pos_y = newval.i_int;
    }
    else if ( !strcmp( psz_var, "audiobargraph_v-position" ) )
    {
        p_sys->i_pos = newval.i_int;
    }
    else if ( !strcmp( psz_var, "audiobargraph_v-transparency" ) )
    {
        p_BarGraph->i_alpha = __MAX( __MIN( newval.i_int, 255 ), 0 );
    }
    else if ( !strcmp( psz_var, "audiobargraph_v-i_values" ) )
    {
        if( p_BarGraph->p_pic )
        {
            picture_Release( p_BarGraph->p_pic );
            p_BarGraph->p_pic = NULL;
        }
        i_values = strdup( newval.psz_string );
        free(p_BarGraph->i_values);
        //p_BarGraph->i_values = NULL;
        //p_BarGraph->nbChannels = 0;
        // in case many answer are received at the same time, only keep one
        res = strchr(i_values, '@');
        if (res)
            *res = 0;
        parse_i_values( p_BarGraph, i_values);
        LoadBarGraph(p_this,p_BarGraph);
    }
    else if ( !strcmp( psz_var, "audiobargraph_v-alarm" ) )
    {
        if( p_BarGraph->p_pic )
        {
            picture_Release( p_BarGraph->p_pic );
            p_BarGraph->p_pic = NULL;
        }
        p_BarGraph->alarm = newval.i_int;
        LoadBarGraph(p_this,p_BarGraph);
    }
    else if ( !strcmp( psz_var, "audiobargraph_v-barWidth" ) )
    {
        if( p_BarGraph->p_pic )
        {
            picture_Release( p_BarGraph->p_pic );
            p_BarGraph->p_pic = NULL;
        }
        p_BarGraph->barWidth = newval.i_int;
        LoadBarGraph(p_this,p_BarGraph);
    }
    p_sys->b_spu_update = true;
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * LoadImage: creates and returns the bar graph image
 *****************************************************************************/
static picture_t *LoadImage( vlc_object_t *p_this, int nbChannels, int* i_values, int scale, int alarm, int barWidth)
{
    VLC_UNUSED(p_this);
    picture_t *p_pic;
    int i,j;
    int i_width = 0;
    int i_line;
    int moinsTrois, moinsCinq, moinsSept, moinsDix, moinsVingt;
    
    if (nbChannels == 0) {
        i_width = 20;
    } else {
        i_width = 2 * nbChannels * barWidth + 10;
    }
    
    moinsTrois = 0.71*scale + 20;
    moinsCinq = 0.56*scale + 20;
    moinsSept = 0.45*scale + 20;
    moinsDix = 0.32*scale + 20;
    moinsVingt = 0.1*scale + 20;
    
    p_pic = picture_New(VLC_FOURCC('Y','U','V','A'), i_width+20, scale+30, 1, 1);
    
    // blacken the whole picture
    for( i = 0 ; i < p_pic->i_planes ; i++ )
    {
        memset( p_pic->p[i].p_pixels, 0x00,
                p_pic->p[i].i_visible_lines * p_pic->p[i].i_pitch );
    }
    
    // side bar
    for ( i_line = 20; i_line < scale+20; i_line++ ) {
    
#define DrawPointsBlack(a,b) {\
        for (i=a; i<b; i++) {\
            *(p_pic->p[0].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[0].i_pitch + i ) = 0x00; \
            *(p_pic->p[1].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[1].i_pitch + i ) = 128; \
            *(p_pic->p[2].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[2].i_pitch + i ) = 128; \
            *(p_pic->p[3].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[3].i_pitch + i ) = 0xFF; \
        }\
    }
#define DrawPointsWhite(a,b) {\
        for (i=a; i<b; i++) {\
            *(p_pic->p[0].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[0].i_pitch + i ) = 0xFF;\
            *(p_pic->p[1].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[1].i_pitch + i ) = 128;\
            *(p_pic->p[2].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[2].i_pitch + i ) = 128;\
            *(p_pic->p[3].p_pixels + (scale + 30 - i_line - 1) * p_pic->p[3].i_pitch + i ) = 0xFF; \
        }\
    }    
        
        // vertical line
        DrawPointsBlack(20,22);
        DrawPointsWhite(22,24);
    
        // -3dB
        if (i_line == moinsTrois - 2) {
            // 3
            DrawPointsBlack(16,19);
        }
        if (i_line == moinsTrois - 1) {
            // 3
            DrawPointsBlack(18,19);
            // limit
            DrawPointsWhite(24,27);
        }
        if (i_line == moinsTrois) {
            // 3
            DrawPointsBlack(16,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsTrois + 1) {
            // 3
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsTrois + 2) {
            // 3
            DrawPointsBlack(16,19);
        }
        
        // -5dB
        if (i_line == moinsCinq - 2) {
            // 5
            DrawPointsBlack(16,19);
        }
        if (i_line == moinsCinq - 1) {
            // 5
            DrawPointsBlack(18,19);
            // limit
            DrawPointsWhite(24,27);
        }
        if (i_line == moinsCinq) {
            // 5
            DrawPointsBlack(16,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsCinq + 1) {
            // 5
            DrawPointsBlack(16,17);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsCinq + 2) {
            // 5
            DrawPointsBlack(16,19);
        }
        
        // -7dB
        if (i_line == moinsSept - 2) {
            // 7
            DrawPointsBlack(18,19);
        }
        if (i_line == moinsSept - 1) {
            // 7
            DrawPointsBlack(18,19);
            // limit
            DrawPointsWhite(24,27);
        }
        if (i_line == moinsSept) {
            // 7
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsSept + 1) {
            // 7
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsSept + 2) {
            // 7
            DrawPointsBlack(16,19);
        }
        
        
        // -10dB
        if (i_line == moinsDix - 2) {
            // 1
            DrawPointsBlack(14,15);
            // 0
            DrawPointsBlack(16,19);
        }
        if (i_line == moinsDix - 1) {
            // 1
            DrawPointsBlack(14,15);
            // 0
            DrawPointsBlack(16,17);
            DrawPointsBlack(18,19);
            // limit
            DrawPointsWhite(24,27);
        }
        if (i_line == moinsDix) {
            // 1
            DrawPointsBlack(14,15);
            // 0
            DrawPointsBlack(16,17);
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsDix + 1) {
            // 1
            DrawPointsBlack(14,15);
            // 0
            DrawPointsBlack(16,17);
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsDix + 2) {
            // 1
            DrawPointsBlack(14,15);
            // 0
            DrawPointsBlack(16,19);
        }
        
        // -20dB
        if (i_line == moinsVingt - 2) {
            // 2
            DrawPointsBlack(12,15);
            // 0
            DrawPointsBlack(16,19);
        }
        if (i_line == moinsVingt - 1) {
            // 2
            DrawPointsBlack(12,13);
            // 0
            DrawPointsBlack(16,17);
            DrawPointsBlack(18,19);
            // limit
            DrawPointsWhite(24,27);
        }
        if (i_line == moinsVingt) {
            // 2
            DrawPointsBlack(12,15);
            // 0
            DrawPointsBlack(16,17);
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsVingt + 1) {
            // 2
            DrawPointsBlack(14,15);
            // 0
            DrawPointsBlack(16,17);
            DrawPointsBlack(18,19);
            // limit
            DrawPointsBlack(24,27);
        }
        if (i_line == moinsVingt + 2) {
            // 2
            DrawPointsBlack(12,15);
            // 0
            DrawPointsBlack(16,19);
        }
        
        
    }
    
    // draw the bars and channel indicators
    for (i=0; i<nbChannels; i++) {
        for( j = barWidth+20 ; j < 2*barWidth+20; j++)
        {
            // channel indicators
            for ( i_line = 12; i_line < 18; i_line++ ) {
                // white
                *(p_pic->p[0].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[0].i_pitch +
                    ( (2*i*barWidth)+j ) ) = 255;
                *(p_pic->p[1].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[1].i_pitch +
                    ( (2*i*barWidth)+j ) ) = 128;
                *(p_pic->p[2].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[2].i_pitch +
                    ( (2*i*barWidth)+j ) ) = 128;
                *(p_pic->p[3].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[3].i_pitch +
                    ( (2*i*barWidth)+j )) = 0xFF;
            }
            // bars
            for( i_line = 20; i_line < i_values[i]+20; i_line++ )
            {
                if (i_line < moinsDix) { // green if < -10 dB
                    *(p_pic->p[0].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[0].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 150;
                    *(p_pic->p[1].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[1].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 44;
                    *(p_pic->p[2].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[2].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 21;
                    *(p_pic->p[3].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[3].i_pitch +
                        ( (2*i*barWidth)+j )) = 0xFF;
                } else if (i_line < moinsTrois) { // yellow if > -10dB and < -3dB
                    *(p_pic->p[0].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[0].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 226;
                    *(p_pic->p[1].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[1].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 1;
                    *(p_pic->p[2].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[2].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 148;
                    *(p_pic->p[3].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[3].i_pitch +
                        ( (2*i*barWidth)+j )) = 0xFF;
                } else { // red if > -3 dB
                    *(p_pic->p[0].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[0].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 76;
                    *(p_pic->p[1].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[1].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 85;
                    *(p_pic->p[2].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[2].i_pitch +
                        ( (2*i*barWidth)+j ) ) = 0xFF;
                    *(p_pic->p[3].p_pixels +
                        (scale + 30 - i_line - 1) *
                        p_pic->p[3].i_pitch +
                        ( (2*i*barWidth)+j )) = 0xFF;
                }
            }
        }
    }
    
    
    
    if (alarm) {// draw the alarm square
        // bottom
        for ( i_line = 0; i_line < 10; i_line++ ) {
            for (i=0; i<i_width+20; i++) {
                *(p_pic->p[0].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[0].i_pitch + i ) = 76;
                *(p_pic->p[1].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[1].i_pitch + i ) = 85;
                *(p_pic->p[2].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[2].i_pitch + i ) = 0xFF;
                *(p_pic->p[3].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[3].i_pitch + i ) = 0xFF;
            }
        }
        // top
        for ( i_line = scale+21; i_line < scale+30; i_line++ ) {
            for (i=0; i<i_width+20; i++) {
                *(p_pic->p[0].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[0].i_pitch + i ) = 76;
                *(p_pic->p[1].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[1].i_pitch + i ) = 85;
                *(p_pic->p[2].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[2].i_pitch + i ) = 0xFF;
                *(p_pic->p[3].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[3].i_pitch + i ) = 0xFF;
            }
        }
        // sides
        for ( i_line = 9; i_line < scale+21; i_line++ ) {
            for (i=0; i<10; i++) {
                *(p_pic->p[0].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[0].i_pitch + i ) = 76;
                *(p_pic->p[1].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[1].i_pitch + i ) = 85;
                *(p_pic->p[2].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[2].i_pitch + i ) = 0xFF;
                *(p_pic->p[3].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[3].i_pitch + i ) = 0xFF;
            }
            for (i=i_width+11; i<i_width+20; i++) {
                *(p_pic->p[0].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[0].i_pitch + i ) = 76;
                *(p_pic->p[1].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[1].i_pitch + i ) = 85;
                *(p_pic->p[2].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[2].i_pitch + i ) = 0xFF;
                *(p_pic->p[3].p_pixels +
                    (scale + 30 - i_line - 1) *
                    p_pic->p[3].i_pitch + i ) = 0xFF;
            }
        }
    }
    

    return p_pic;
}

/*****************************************************************************
 * LoadBarGraph: loads the BarGraph images into memory
 *****************************************************************************/
static void LoadBarGraph( vlc_object_t *p_this, BarGraph_t *p_BarGraph )
{

    p_BarGraph->p_pic = LoadImage( p_this, p_BarGraph->nbChannels, p_BarGraph->i_values, p_BarGraph->scale, p_BarGraph->alarm, p_BarGraph->barWidth);
    if( !p_BarGraph->p_pic )
    {
        msg_Warn( p_this, "error while creating picture" );
    }

}

/*****************************************************************************
 * parse_i_values : parse i_values parameter and store the corresponding values
 *****************************************************************************/
void parse_i_values( BarGraph_t *p_BarGraph, char *i_values)
{
    char* res = NULL;
    char delim[] = ":";
    char* tok;

    p_BarGraph->nbChannels = 0;
    p_BarGraph->i_values = NULL;
    res = strtok_r(i_values, delim, &tok);
    while (res != NULL) {
        p_BarGraph->nbChannels++;
        p_BarGraph->i_values = xrealloc(p_BarGraph->i_values,
                                          p_BarGraph->nbChannels*sizeof(int));
        p_BarGraph->i_values[p_BarGraph->nbChannels-1] = __MAX( __MIN( atof(res)*p_BarGraph->scale, p_BarGraph->scale ), 0 );
        res = strtok_r(NULL, delim, &tok);
    }

}
