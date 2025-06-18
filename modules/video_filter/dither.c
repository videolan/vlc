/*****************************************************************************
 * dither.c : Dithering video filters for VLC
 *****************************************************************************
 * Copyright (C) 2025 VideoLAN
 *
 * Authors: Brandon Li <brandonli2006ma@gmail.com>
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
# include <config.h>
#endif

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_opengl.h>
#include <vlc_rand.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void       Close  ( filter_t *p_filter );
static int        Open   ( filter_t *p_filter );
static picture_t* Filter ( filter_t *p_filter, picture_t *p_pic );

typedef void (*dither_algo_t)        ( const filter_t *p_filter, picture_t *p_pic );
static  void FilterFloydSteinberg    ( const filter_t *p_filter, picture_t *p_pic );
static  void FilterSierraFilterLite  ( const filter_t *p_filter, picture_t *p_pic );
static  void FilterJarvisJudiceNinke ( const filter_t *p_filter, picture_t *p_pic );
static  void FilterEuclidean         ( const filter_t *p_filter, picture_t *p_pic );
static  void FilterRandom            ( const filter_t *p_filter, picture_t *p_pic );

typedef void (*pixel_reader_t) ( const picture_t *p_pic, unsigned int i_x, unsigned int i_y, int *p_r, int *p_g, int *p_b );
static  void read_bgr24        ( const picture_t *p_pic, unsigned int i_x, unsigned int i_y, int *p_r, int *p_g, int *p_b );
static  void read_rgb24        ( const picture_t *p_pic, unsigned int i_x, unsigned int i_y, int *p_r, int *p_g, int *p_b );
static  void read_rgba         ( const picture_t *p_pic, unsigned int i_x, unsigned int i_y, int *p_r, int *p_g, int *p_b );
static  void read_yuv          ( const picture_t *p_pic, unsigned int i_x, unsigned int i_y, int *p_r, int *p_g, int *p_b );

typedef void (*pixel_writer_t) ( picture_t *p_pic, unsigned int i_x, unsigned int i_y, int i_r, int i_g, int i_b );
static  void write_bgr24       ( picture_t *p_pic, unsigned int i_x, unsigned int i_y, int i_r, int i_g, int i_b );
static  void write_rgb24       ( picture_t *p_pic, unsigned int i_x, unsigned int i_y, int i_r, int i_g, int i_b );
static  void write_rgba        ( picture_t *p_pic, unsigned int i_x, unsigned int i_y, int i_r, int i_g, int i_b );
static  void write_yuv         ( picture_t *p_pic, unsigned int i_x, unsigned int i_y, int i_r, int i_g, int i_b );

#define COLOR_BITS 5
#define RANDOM_NOISE_AMPLITUDE 16
#define TABLE_SIZE ( 1 << ( 3 * COLOR_BITS ) )

#define FLOYD_STEINBERG     0
#define SIERRA_FILTER_LITE  1
#define JARVIS_JUDICE_NINKE 2
#define EUCLIDEAN_DISTANCE  3
#define RANDOMIZATION       4

static const struct vlc_filter_operations filter_ops =
{
    .filter_video = Filter, .close = Close,
};

typedef struct
{
    int *p_dither_buffer[3];
    int p_lookup_table[ TABLE_SIZE ];
    int *p_palette;
    dither_algo_t pf_dither_algo;
    pixel_reader_t pf_read_pixel;
    pixel_writer_t pf_write_pixel;
} filter_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static const char *const ppsz_dither_modes[] = { "floyd-steinberg", "sierra-filter-lite", "jarvis-judice-ninke", "euclidean-distance", "randomization" };
static const char *const ppsz_dither_descriptions[] =
    { N_("Floyd-Steinberg"), N_("Sierra Filter-Lite"), N_("Jarvis, Judice & Ninke"), N_("Euclidean Distance"), N_("Randomization") };

vlc_module_begin()
    set_description( N_("Dithering video filter") )
    set_shortname( N_("Dither") )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    add_shortcut( "dither" )
    add_string( "dither-mode", "floyd-steinberg",
               N_("Dithering method"), N_("Dithering algorithm to use") )
        change_string_list( ppsz_dither_modes, ppsz_dither_descriptions )
    add_string( "palette", "#FFFFFF,#000000",
           N_("Color palette"), N_("Comma-separated list of hexadecimal colors (e.g. #FF0000,#00FF00,#0000FF)") )
    set_callback_video_filter( Open )
vlc_module_end()

/*****************************************************************************
 * Open: Create the dither filter, creating cache table
 *****************************************************************************/
static int Open( filter_t *p_filter )
{

    filter_sys_t *p_sys = malloc( sizeof( filter_sys_t ) );
    if ( !p_sys )
        return VLC_ENOMEM;

    p_filter->p_sys = p_sys;

    // choose write/read functions
    const vlc_fourcc_t chroma = p_filter->fmt_in.video.i_chroma;
    switch ( chroma )
    {
    case VLC_CODEC_BGR24:
        p_sys->pf_read_pixel = read_bgr24;
        p_sys->pf_write_pixel = write_bgr24;
        break;
    case VLC_CODEC_RGB24:
        p_sys->pf_read_pixel = read_rgb24;
        p_sys->pf_write_pixel = write_rgb24;
        break;
    case VLC_CODEC_RGBA:
        p_sys->pf_read_pixel = read_rgba;
        p_sys->pf_write_pixel = write_rgba;
        break;
    // Let VLC handle YUV conversion (for future reference)
    // case VLC_CODEC_I420:
    //     p_sys->pf_read_pixel = read_yuv;
    //     p_sys->pf_write_pixel = write_yuv;
    //     break;
    default:
        msg_Err( p_filter, "Unsupported chroma" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    // choose dithering mode
    char *psz_mode = var_InheritString( p_filter, "dither-mode" );
    if ( !psz_mode )
    {
        msg_Err( p_filter, "No dithering mode specified" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if ( !strcmp(psz_mode, "floyd-steinberg") )
        p_sys->pf_dither_algo = FilterFloydSteinberg;
    else if ( !strcmp(psz_mode, "sierra-filter-lite") )
        p_sys->pf_dither_algo = FilterSierraFilterLite;
    else if ( !strcmp(psz_mode, "jarvis-judice-ninke") )
        p_sys->pf_dither_algo = FilterJarvisJudiceNinke;
    else if ( !strcmp(psz_mode, "euclidean-distance") )
        p_sys->pf_dither_algo = FilterEuclidean;
    else if ( !strcmp(psz_mode, "randomization") )
        p_sys->pf_dither_algo = FilterRandom;
    else
    {
        msg_Err( p_filter, "Invalid dithering mode" );
        free( psz_mode );
        free( p_sys );
        return VLC_EGENERIC;
    }
    free( psz_mode );

    // read color count
    char *psz_palette = var_InheritString( p_filter, "palette" );
    if ( !psz_palette )
    {
        msg_Err( p_filter, "No dithering palette specified" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    int i_num_colors = 1;
    const char *p_ptr = psz_palette;
    while ( ( p_ptr = strchr( p_ptr, ',' ) ) )
    {
        i_num_colors++;
        p_ptr++;
    }

    // parse colors
    p_sys->p_palette = malloc( i_num_colors * sizeof( int ) );
    if ( !p_sys->p_palette )
    {
        msg_Err( p_filter, "Failed to allocate memory for palette" );
        free( psz_palette );
        free( p_sys );
        return VLC_ENOMEM;
    }

    char *p_tmp_saveptr;
    const char *ppsz_token = strtok_r( psz_palette, ",", &p_tmp_saveptr );
    int i_index = 0;
    while ( ppsz_token != NULL && i_index < i_num_colors )
    {
        while ( isspace( *ppsz_token ) )
            ppsz_token++;
        if ( *ppsz_token == '#' )
            ppsz_token++;
        const unsigned long l_rgb = strtoul( ppsz_token, NULL, 16 );
        const bool parseErr = l_rgb == ULONG_MAX && errno == ERANGE;
        const bool boundsErr = l_rgb > INT_MAX; // unsigned always non-negative
        if ( parseErr || boundsErr )
        {
            msg_Err( p_filter, "Invalid hexadecimal color %s", ppsz_token );
            free( p_sys->p_palette );
            free( psz_palette );
            free( p_sys );
            return VLC_EGENERIC;
        }
        p_sys->p_palette[ i_index++ ] = (int) l_rgb;
        ppsz_token = strtok_r( NULL, ",", &p_tmp_saveptr );
    }
    free( psz_palette );

    // set default max for Euclidean distance
    for ( int i_curr_color = 0; i_curr_color < TABLE_SIZE; i_curr_color++ )
    {
        p_sys->p_lookup_table[ i_curr_color ] = INT_MAX;
    }

    // Calculate lookup table
    const int i_color_mask = ( ( 1 << COLOR_BITS ) - 1 );
    for ( int i_curr_color = 0; i_curr_color < TABLE_SIZE; i_curr_color++ )
    {
        const int i_qr = ( i_curr_color >> ( COLOR_BITS * 2 ) ) & i_color_mask;
        const int i_qg = ( i_curr_color >> COLOR_BITS ) & i_color_mask;
        const int i_qb = i_curr_color & i_color_mask;
        const int i_r = ( i_qr << ( 8 - COLOR_BITS ) ) | ( i_qr >> ( 2 * COLOR_BITS - 8) );
        const int i_g = ( i_qg << ( 8 - COLOR_BITS ) ) | ( i_qg >> ( 2 * COLOR_BITS - 8) );
        const int i_b = ( i_qb << ( 8 - COLOR_BITS ) ) | ( i_qb >> ( 2 * COLOR_BITS - 8) );
        unsigned int i_dist = INT_MAX;
        int i_indx = 0;
        for ( int i_palette_color = 0; i_palette_color < i_num_colors; i_palette_color++ )
        {
            const int i_color = p_sys->p_palette[ i_palette_color ];
            const int i_cr = ( i_color >> 16 ) & 0xFF;
            const int i_cg = ( i_color >> 8  ) & 0xFF;
            const int i_cb = ( i_color >> 0  ) & 0xFF;
            const unsigned int i_curr_dist = ( i_r - i_cr ) * ( i_r - i_cr ) +
                                 ( i_g - i_cg ) * ( i_g - i_cg ) +
                                 ( i_b - i_cb ) * ( i_b - i_cb );
            if ( i_curr_dist < i_dist ) {
                i_dist = i_curr_dist;
                i_indx = i_palette_color;
            }
        }
        p_sys->p_lookup_table[ i_curr_color ] = i_indx;
    }

    // allocate dither buffers
    const unsigned int i_width = p_filter->fmt_in.video.i_width;
    const size_t i_single = i_width * 3 * 2 * sizeof( int );
    int *p_all = malloc( 3 * i_single );
    if (!p_all) {
        free(p_sys->p_palette);
        free(p_sys);
        return VLC_ENOMEM;
    }
    p_sys->p_dither_buffer[0] = p_all;
    p_sys->p_dither_buffer[1] = p_all + ( i_single / sizeof ( int ) );
    p_sys->p_dither_buffer[2] = p_all + ( i_single / sizeof ( int ) ) * 2;

    p_filter->ops = &filter_ops;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: Releases all memory
 *****************************************************************************/
static void Close( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if ( p_sys ) {
        free( p_sys->p_palette );
        free( p_sys->p_dither_buffer[0] ); // deallocate base pointer
        free( p_sys );
    }
}

/*****************************************************************************
 * Filter: Dithers with the specified algorithm
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    const filter_sys_t *p_sys = p_filter->p_sys;
    p_sys->pf_dither_algo( p_filter, p_pic );
    return p_pic;
}

/*****************************************************************************
 * read/write pix_fmt: read and write pixel values in different formats
 *****************************************************************************/
static inline void read_bgr24( const picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, int *p_r, int *p_g, int *p_b )
{
    const uint8_t *p_src_line = &p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch ];
    const unsigned int i_index = 3 * i_x;
    *p_b = p_src_line[ i_index + 0 ]; // 8 bits blue
    *p_g = p_src_line[ i_index + 1 ]; // 8 bits green
    *p_r = p_src_line[ i_index + 2 ]; // 8 bits red
}

static inline void write_bgr24( picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, const int i_r, const int i_g, const int i_b )
{
    uint8_t *p_dst_line = &p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch ];
    const unsigned int i_index = 3 * i_x;
    p_dst_line[ i_index + 0 ] = i_b;
    p_dst_line[ i_index + 1 ] = i_g;
    p_dst_line[ i_index + 2 ] = i_r;
}

static inline void read_rgb24( const picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, int *p_r, int *p_g, int *p_b )
{
    const uint8_t *p_src_line = &p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch ];
    const unsigned int i_index = 3 * i_x;
    *p_r = p_src_line[ i_index + 0 ]; // 8 bits red
    *p_g = p_src_line[ i_index + 1 ]; // 8 bits green
    *p_b = p_src_line[ i_index + 2 ]; // 8 bits blue
}

static inline void write_rgb24( picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, const int i_r, const int i_g, const int i_b )
{
    uint8_t *p_dst_line = &p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch ];
    const unsigned int i_index = 3 * i_x;
    p_dst_line[ i_index + 0 ] = i_r;
    p_dst_line[ i_index + 1 ] = i_g;
    p_dst_line[ i_index + 2 ] = i_b;
}

static inline void read_rgba( const picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, int *p_r, int *p_g, int *p_b )
{
    const uint8_t *p_src_line = &p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch ];
    const unsigned int i_index = 4 * i_x;
    *p_r = p_src_line[ i_index + 0 ]; // 8 bits red
    *p_g = p_src_line[ i_index + 1 ]; // 8 bits green
    *p_b = p_src_line[ i_index + 2 ]; // 8 bits blue
    // ignore alpha
}

static inline void write_rgba( picture_t * p_pic, const unsigned int i_x, const unsigned int i_y, const int i_r, const int i_g, const int i_b )
{
    uint8_t *p_dst_line = &p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch ];
    const unsigned i_index = 4 * i_x;
    p_dst_line[ i_index + 0 ] = i_r;
    p_dst_line[ i_index + 1 ] = i_g;
    p_dst_line[ i_index + 2 ] = i_b;
    p_dst_line[ i_index + 3 ] = 0xFF; // always full value
}

static inline void rgb_from_yuv( int *p_r, int *p_g, int *p_b, const int i_y, const int i_u, const int i_v )
{
    *p_r = (int) ( 1.164 * i_y +               1.596 * i_v );
    *p_g = (int) ( 1.164 * i_y - 0.392 * i_u - 0.813 * i_v );
    *p_b = (int) ( 1.164 * i_y + 2.017 * i_u );

    *p_r = *p_r < 0 ? 0 : ( *p_r > 255 ? 255 : *p_r );
    *p_g = *p_g < 0 ? 0 : ( *p_g > 255 ? 255 : *p_g );
    *p_b = *p_b < 0 ? 0 : ( *p_b > 255 ? 255 : *p_b );
}

static inline void read_yuv( const picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, int *p_r, int *p_g, int *p_b )
{
    const int i_y_val = p_pic -> p[0].p_pixels[ i_y * p_pic -> p[0].i_pitch + i_x ] - 16; // 4
    const int i_u_val = p_pic -> p[1].p_pixels[ ( i_y / 2 ) * p_pic-> p[1].i_pitch + ( i_x / 2 ) ] - 128; // 2
    const int i_v_val = p_pic -> p[2].p_pixels[ ( i_y / 2 ) * p_pic-> p[2].i_pitch + ( i_x / 2 ) ] - 128; // 0
    rgb_from_yuv( p_r, p_g, p_b, i_y_val, i_u_val, i_v_val );
}

static inline void yuv_from_rgb( double *p_y, double *p_u, double *p_v, const double d_r, const double d_g, const double d_b )
{
    *p_y =   0.257 * d_r + 0.504 * d_g + 0.098 * d_b + 16;
    *p_u =  -0.148 * d_r - 0.291 * d_g + 0.439 * d_b + 128;
    *p_v =   0.439 * d_r - 0.368 * d_g - 0.071 * d_b + 128;

    *p_y = *p_y < 16 ? 16 : ( *p_y > 235 ? 235 : *p_y );
    *p_u = *p_u < 16 ? 16 : ( *p_u > 240 ? 240 : *p_u );
    *p_v = *p_v < 16 ? 16 : ( *p_v > 240 ? 240 : *p_v );
}

static inline void write_yuv( picture_t *p_pic, const unsigned int i_x, const unsigned int i_y, const int i_r, const int i_g, const int i_b )
{
    double d_y_val, d_u_val, d_v_val;
    yuv_from_rgb( &d_y_val, &d_u_val, &d_v_val, i_r, i_g, i_b );
    p_pic->p[0].p_pixels[ i_y * p_pic->p[0].i_pitch + i_x ] = (uint8_t) d_y_val;
    if ( i_x % 2 == 0 && i_y % 2 == 0 )
    {
        p_pic->p[1].p_pixels[ ( i_y / 2 ) * p_pic->p[1].i_pitch + ( i_x / 2 ) ] = (uint8_t) d_u_val;
        p_pic->p[2].p_pixels[ ( i_y / 2 ) * p_pic->p[2].i_pitch + ( i_x / 2 ) ] = (uint8_t) d_v_val;
    }
}

/*****************************************************************************
 * FilterRandom: Applies random dithering to the image
 *****************************************************************************/
static void FilterRandom( const filter_t *p_filter, picture_t *p_pic )
{
    const filter_sys_t *p_sys = p_filter->p_sys;
    const unsigned int i_width = p_pic->format.i_visible_width;
    const unsigned int i_height = p_pic->format.i_visible_height;
    const unsigned int i_x_offset = p_pic->format.i_x_offset;
    const unsigned int i_y_offset = p_pic->format.i_y_offset;
    const int i_seed = (int) vlc_mrand48();
    for ( unsigned int i_y = 0; i_y < i_height; i_y++ )
    {
        int i_row_seed = i_seed + (int) i_y;
        for ( unsigned int i_x = 0; i_x < i_width; i_x++ )
        {
            int i_r, i_g, i_b;
            const unsigned int i_x_abs = i_x + i_x_offset;
            const unsigned int i_y_abs = i_y + i_y_offset;
            p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
            i_r += ( i_row_seed % ( 2 * RANDOM_NOISE_AMPLITUDE + 1 ) ) - RANDOM_NOISE_AMPLITUDE;
            i_row_seed = i_row_seed * 1103515245 + 12345;
            i_g += ( i_row_seed % ( 2 * RANDOM_NOISE_AMPLITUDE + 1 ) ) - RANDOM_NOISE_AMPLITUDE;
            i_row_seed = i_row_seed * 1103515245 + 12345;
            i_b += ( i_row_seed % ( 2 * RANDOM_NOISE_AMPLITUDE + 1 ) ) - RANDOM_NOISE_AMPLITUDE;
            i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
            i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
            i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
            const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
            const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
            const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
            const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
            const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
            const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
            const int i_pr = ( i_palette_color >> 16 ) & 0xFF;
            const int i_pg = ( i_palette_color >> 8  ) & 0xFF;
            const int i_pb = ( i_palette_color >> 0  ) & 0xFF;
            p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
        }
    }
}

/*****************************************************************************
 * FilterEuclidean: Uses the nearest pixel (Euclidean distance) algorithm
 *****************************************************************************/
static void FilterEuclidean( const filter_t *p_filter, picture_t *p_pic )
{
    const filter_sys_t *p_sys = p_filter->p_sys;
    const unsigned int i_width = p_pic->format.i_visible_width;
    const unsigned int i_height = p_pic->format.i_visible_height;
    const unsigned int i_x_offset = p_pic->format.i_x_offset;
    const unsigned int i_y_offset = p_pic->format.i_y_offset;
    for ( unsigned int i_y = 0; i_y < i_height; i_y++ )
    {
        for ( unsigned int i_x = 0; i_x < i_width; i_x++ )
        {
            int i_r, i_g, i_b;
            const unsigned int i_x_abs = i_x + i_x_offset;
            const unsigned int i_y_abs = i_y + i_y_offset;
            p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
            i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
            i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
            i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
            const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
            const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
            const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
            const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
            const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
            const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
            const int i_pr = ( i_palette_color >> 16) & 0xFF;
            const int i_pg = ( i_palette_color >>  8) & 0xFF;
            const int i_pb = ( i_palette_color >>  0) & 0xFF;
            p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
        }
    }
}

/*****************************************************************************
 * FilterFloydSteinberg: Dithers using the Floyd-Steinberg algorithm with
 * serpentine scanning (Boustrophedon transform).
 *
 *        *   7
 *    3   5   1     (1/16)
 *
 * https://en.wikipedia.org/wiki/Boustrophedon_transform
 * https://en.wikipedia.org/wiki/Floyd-Steinberg_dithering
 * https://gist.github.com/robertlugg/f0b618587c2981b744716999573c5b65#file-dhalf-txt-L656
 *****************************************************************************/
static void FilterFloydSteinberg( const filter_t *p_filter, picture_t *p_pic )
{
    const filter_sys_t *p_sys = p_filter->p_sys;
    const unsigned int i_width = p_pic->format.i_visible_width;
    const unsigned int i_height = p_pic->format.i_visible_height;
    const unsigned int i_width_minus = i_width - 1;
    const unsigned int i_height_minus = i_height - 1;
    const unsigned int i_x_offset = p_pic->format.i_x_offset;
    const unsigned int i_y_offset = p_pic->format.i_y_offset;
    for ( unsigned int i_y = 0; i_y < i_height; i_y++ )
    {
        const bool b_has_next_y = ( i_y < i_height_minus );
        const unsigned int i_y_abs = i_y + i_y_offset;
        if ( ( i_y & 0x1 ) == 0 )
        { // go left to right
            int i_buffer_index = 0;
            int *pi_buf1 = p_sys->p_dither_buffer[0];
            int *pi_buf2 = p_sys->p_dither_buffer[1];
            for ( unsigned int i_x = 0; i_x < i_width; i_x++ )
            {
                const bool b_has_next_x = ( i_x < i_width_minus );
                int i_r, i_g, i_b;
                const unsigned int i_x_abs = i_x + i_x_offset;
                p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
                i_r = ( i_r + pi_buf1[ i_buffer_index++ ] );
                i_g = ( i_g + pi_buf1[ i_buffer_index++ ] );
                i_b = ( i_b + pi_buf1[ i_buffer_index++ ] );
                i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
                i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
                i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
                const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
                const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
                const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
                const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
                const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
                const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
                const int i_pr = ( i_palette_color >> 16) & 0xFF;
                const int i_pg = ( i_palette_color >>  8) & 0xFF;
                const int i_pb = ( i_palette_color >>  0) & 0xFF;
                const int i_delta_r = i_r - i_pr;
                const int i_delta_g = i_g - i_pg;
                const int i_delta_b = i_b - i_pb;
                if ( b_has_next_x )
                {
                    pi_buf1[ i_buffer_index + 0 ] = ( i_delta_r >> 4 ) * 7;
                    pi_buf1[ i_buffer_index + 1 ] = ( i_delta_g >> 4 ) * 7;
                    pi_buf1[ i_buffer_index + 2 ] = ( i_delta_b >> 4 ) * 7;
                }
                if ( b_has_next_y )
                {
                    if ( i_x > 0 )
                    {
                        pi_buf2[ i_buffer_index - 6 ] = ( i_delta_r >> 4 ) * 3;
                        pi_buf2[ i_buffer_index - 5 ] = ( i_delta_g >> 4 ) * 3;
                        pi_buf2[ i_buffer_index - 4 ] = ( i_delta_b >> 4 ) * 3;
                    }
                    pi_buf2[ i_buffer_index - 3 ] = ( i_delta_r >> 4 ) * 5;
                    pi_buf2[ i_buffer_index - 2 ] = ( i_delta_g >> 4 ) * 5;
                    pi_buf2[ i_buffer_index - 1 ] = ( i_delta_b >> 4 ) * 5;
                    if ( b_has_next_x )
                    {
                        pi_buf2[ i_buffer_index + 0 ] = i_delta_r >> 4;
                        pi_buf2[ i_buffer_index + 1 ] = i_delta_g >> 4;
                        pi_buf2[ i_buffer_index + 2 ] = i_delta_b >> 4;
                    }
                }
                p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
            }
        } else
        { // go right to left
            int i_buffer_index = (int) ( i_width * 3 ) - 1;
            int *pi_buf1 = p_sys->p_dither_buffer[1];
            int *pi_buf2 = p_sys->p_dither_buffer[0];
            for ( unsigned int i_x = i_width; i_x-- > 0; )
            {
                const bool b_has_next_x = ( i_x > 0 );
                int i_r, i_g, i_b;
                const unsigned int i_x_abs = i_x + i_x_offset;
                p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
                i_b = ( i_b + pi_buf1[ i_buffer_index-- ] );
                i_g = ( i_g + pi_buf1[ i_buffer_index-- ] );
                i_r = ( i_r + pi_buf1[ i_buffer_index-- ] );
                i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
                i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
                i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
                const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
                const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
                const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
                const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
                const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
                const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
                const int i_pr = ( i_palette_color >> 16) & 0xFF;
                const int i_pg = ( i_palette_color >>  8) & 0xFF;
                const int i_pb = ( i_palette_color >>  0) & 0xFF;
                const int i_delta_r = i_r - i_pr;
                const int i_delta_g = i_g - i_pg;
                const int i_delta_b = i_b - i_pb;
                if ( b_has_next_x )
                {
                    pi_buf1[ i_buffer_index - 0 ] = ( i_delta_b >> 4 ) * 7;
                    pi_buf1[ i_buffer_index - 1 ] = ( i_delta_g >> 4 ) * 7;
                    pi_buf1[ i_buffer_index - 2 ] = ( i_delta_r >> 4 ) * 7;
                }
                if ( b_has_next_y )
                {
                    if ( i_x < i_width_minus )
                    {
                        pi_buf2[ i_buffer_index + 6 ] = ( i_delta_b >> 4 ) * 3;
                        pi_buf2[ i_buffer_index + 5 ] = ( i_delta_g >> 4 ) * 3;
                        pi_buf2[ i_buffer_index + 4 ] = ( i_delta_r >> 4 ) * 3;
                    }
                    pi_buf2[ i_buffer_index + 3 ] = ( i_delta_b >> 4 ) * 5;
                    pi_buf2[ i_buffer_index + 2 ] = ( i_delta_g >> 4 ) * 5;
                    pi_buf2[ i_buffer_index + 1 ] = ( i_delta_r >> 4 ) * 5;
                    if ( b_has_next_x )
                    {
                        pi_buf2[ i_buffer_index - 0 ] = i_delta_b >> 4;
                        pi_buf2[ i_buffer_index - 1 ] = i_delta_g >> 4;
                        pi_buf2[ i_buffer_index - 2 ] = i_delta_r >> 4;
                    }
                }
                p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
            }
        }
        // reset buffers
        memset( p_sys->p_dither_buffer[ i_y & 0x1 ], 0, ( i_width * 3 ) * 2 * sizeof( int ) );
    }
}

/*****************************************************************************
 * FilterSierraFilterLite: Dithers using the Sierra Filter-Lite algorithm with
 * serpentine scanning (Boustrophedon transform). Faster than Floyd-Steinberg
 * with similar results.
 *
 *       *   2
 *   1   1               (1/4)
 *
 * https://gist.github.com/robertlugg/f0b618587c2981b744716999573c5b65#file-dhalf-txt-L796
 *****************************************************************************/
static void FilterSierraFilterLite( const filter_t *p_filter, picture_t *p_pic )
{
const filter_sys_t *p_sys = p_filter->p_sys;
    const unsigned int i_width = p_pic->format.i_visible_width;
    const unsigned int i_height = p_pic->format.i_visible_height;
    const unsigned int i_width_minus = i_width - 1;
    const unsigned int i_height_minus = i_height - 1;
    const unsigned int i_x_offset = p_pic->format.i_x_offset;
    const unsigned int i_y_offset = p_pic->format.i_y_offset;
    for ( unsigned int i_y = 0; i_y < i_height; i_y++ )
    {
        const bool b_has_next_y = ( i_y < i_height_minus );
        const unsigned int i_y_abs = i_y + i_y_offset;
        if ( ( i_y & 0x1 ) == 0 )
        { // go left to right
            int i_buffer_index = 0;
            int *pi_buf1 = p_sys->p_dither_buffer[0];
            int *pi_buf2 = p_sys->p_dither_buffer[1];
            for ( unsigned int i_x = 0; i_x < i_width; i_x++ )
            {
                const bool b_has_next_x = ( i_x < i_width_minus );
                int i_r, i_g, i_b;
                const unsigned int i_x_abs = i_x + i_x_offset;
                p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
                i_r = ( i_r + pi_buf1[ i_buffer_index++ ] );
                i_g = ( i_g + pi_buf1[ i_buffer_index++ ] );
                i_b = ( i_b + pi_buf1[ i_buffer_index++ ] );
                i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
                i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
                i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
                const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
                const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
                const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
                const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
                const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
                const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
                const int i_pr = ( i_palette_color >> 16) & 0xFF;
                const int i_pg = ( i_palette_color >>  8) & 0xFF;
                const int i_pb = ( i_palette_color >>  0) & 0xFF;
                const int i_delta_r = i_r - i_pr;
                const int i_delta_g = i_g - i_pg;
                const int i_delta_b = i_b - i_pb;
                if ( b_has_next_x )
                {
                    pi_buf1[ i_buffer_index + 0 ] = ( i_delta_r >> 2 ) * 2; // 2/4
                    pi_buf1[ i_buffer_index + 1 ] = ( i_delta_g >> 2 ) * 2;
                    pi_buf1[ i_buffer_index + 2 ] = ( i_delta_b >> 2 ) * 2;
                }
                if ( b_has_next_y )
                {
                    if ( i_x > 0 )
                    {
                        pi_buf2[ i_buffer_index - 6 ] = ( i_delta_r >> 2 );
                        pi_buf2[ i_buffer_index - 5 ] = ( i_delta_g >> 2 );
                        pi_buf2[ i_buffer_index - 4 ] = ( i_delta_b >> 2 );
                    }
                    pi_buf2[ i_buffer_index - 3 ] = ( i_delta_r >> 2 );
                    pi_buf2[ i_buffer_index - 2 ] = ( i_delta_g >> 2 );
                    pi_buf2[ i_buffer_index - 1 ] = ( i_delta_b >> 2 );
                }
                p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
            }
        } else
        { // go right to left
            int i_buffer_index = (int) ( i_width * 3 ) - 1;
            int *pi_buf1 = p_sys->p_dither_buffer[1];
            int *pi_buf2 = p_sys->p_dither_buffer[0];
            for ( unsigned int i_x = i_width; i_x-- > 0; )
            {
                const bool b_has_next_x = ( i_x > 0 );
                int i_r, i_g, i_b;
                const unsigned int i_x_abs = i_x + i_x_offset;
                p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
                i_b = ( i_b + pi_buf1[ i_buffer_index-- ] );
                i_g = ( i_g + pi_buf1[ i_buffer_index-- ] );
                i_r = ( i_r + pi_buf1[ i_buffer_index-- ] );
                i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
                i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
                i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
                const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
                const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
                const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
                const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
                const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
                const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
                const int i_pr = ( i_palette_color >> 16) & 0xFF;
                const int i_pg = ( i_palette_color >>  8) & 0xFF;
                const int i_pb = ( i_palette_color >>  0) & 0xFF;
                const int i_delta_r = i_r - i_pr;
                const int i_delta_g = i_g - i_pg;
                const int i_delta_b = i_b - i_pb;
                if ( b_has_next_x )
                {
                    pi_buf1[ i_buffer_index - 0 ] = ( i_delta_b >> 2 ) * 2;
                    pi_buf1[ i_buffer_index - 1 ] = ( i_delta_g >> 2 ) * 2;
                    pi_buf1[ i_buffer_index - 2 ] = ( i_delta_r >> 2 ) * 2;
                }
                if ( b_has_next_y )
                {
                    if ( i_x < i_width_minus )
                    {
                        pi_buf2[ i_buffer_index + 6 ] = ( i_delta_b >> 2 );
                        pi_buf2[ i_buffer_index + 5 ] = ( i_delta_g >> 2 );
                        pi_buf2[ i_buffer_index + 4 ] = ( i_delta_r >> 2 );
                    }
                    pi_buf2[ i_buffer_index + 3 ] = ( i_delta_b >> 2 );
                    pi_buf2[ i_buffer_index + 2 ] = ( i_delta_g >> 2 );
                    pi_buf2[ i_buffer_index + 1 ] = ( i_delta_r >> 2 );
                }
                p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
            }
        }
        memset(p_sys->p_dither_buffer[ i_y & 0x1 ], 0, ( i_width * 3 ) * 2 * sizeof( int ) );
    }
}

/*****************************************************************************
 * FilterJarvisJudiceNinke: Dithers using the Jarvis, Judice, and Ninke algorithm with
 * serpentine scanning (Boustrophedon transform). Slowest, but gives the best results.
 *
 *            *   7   5
 *    3   5   7   5   3
 *    1   3   5   3   1   (1/48)
 *
 * https://gist.github.com/robertlugg/f0b618587c2981b744716999573c5b65#file-dhalf-txt-L716
 *****************************************************************************/
static void FilterJarvisJudiceNinke( const filter_t *p_filter, picture_t *p_pic )
{
    const filter_sys_t *p_sys = p_filter->p_sys;
    const unsigned int i_width = p_pic->format.i_visible_width;
    const unsigned int i_height = p_pic->format.i_visible_height;
    const unsigned int i_width_minus = i_width - 1;
    const unsigned int i_height_minus = i_height - 1;
    const unsigned int i_x_offset = p_pic->format.i_x_offset;
    const unsigned int i_y_offset = p_pic->format.i_y_offset;
    for ( unsigned int i_y = 0; i_y < i_height; i_y++ )
    {
        const bool b_has_next_y = ( i_y < i_height_minus );
        const bool b_has_next_next_y = ( i_y < i_height - 2 );
        const unsigned int i_y_abs = i_y + i_y_offset;
        if( ( i_y & 0x1 ) == 0 )
        {
            int i_buffer_index = 0;
            int *pi_buf1 = p_sys->p_dither_buffer[0];
            int *pi_buf2 = p_sys->p_dither_buffer[1];
            int *pi_buf3 = p_sys->p_dither_buffer[2];
            for ( unsigned int i_x = 0; i_x < i_width; i_x++ )
            {
                const bool b_has_next_x = ( i_x < i_width_minus );
                int i_r, i_g, i_b;
                const unsigned int i_x_abs = i_x + i_x_offset;
                p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
                i_r = ( i_r + pi_buf1[ i_buffer_index++ ] );
                i_g = ( i_g + pi_buf1[ i_buffer_index++ ] );
                i_b = ( i_b + pi_buf1[ i_buffer_index++ ] );
                i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
                i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
                i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
                const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
                const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
                const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
                const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2 ) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
                const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
                const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
                const int i_pr = ( i_palette_color >> 16 ) & 0xFF;
                const int i_pg = ( i_palette_color >>  8 ) & 0xFF;
                const int i_pb = ( i_palette_color >>  0 ) & 0xFF;
                const int i_delta_r = i_r - i_pr;
                const int i_delta_g = i_g - i_pg;
                const int i_delta_b = i_b - i_pb;
                if ( b_has_next_x )
                {
                    pi_buf1[ i_buffer_index + 0 ] = ( i_delta_r * 7 ) / 48;
                    pi_buf1[ i_buffer_index + 1 ] = ( i_delta_g * 7 ) / 48;
                    pi_buf1[ i_buffer_index + 2 ] = ( i_delta_b * 7 ) / 48;
                    if( i_x < i_width - 2 )
                    {
                        pi_buf1[ i_buffer_index + 3 ] = ( i_delta_r * 5 ) / 48;
                        pi_buf1[ i_buffer_index + 4 ] = ( i_delta_g * 5 ) / 48;
                        pi_buf1[ i_buffer_index + 5 ] = ( i_delta_b * 5 ) / 48;
                    }
                }
                if ( b_has_next_y )
                {
                    if ( i_x > 0 )
                    {
                        pi_buf2[ i_buffer_index - 6 ] = ( i_delta_r * 5 ) / 48;
                        pi_buf2[ i_buffer_index - 5 ] = ( i_delta_g * 5 ) / 48;
                        pi_buf2[ i_buffer_index - 4 ] = ( i_delta_b * 5 ) / 48;
                        if ( i_x > 1 )
                        {
                            pi_buf2[ i_buffer_index - 9 ] = ( i_delta_r * 3 ) / 48;
                            pi_buf2[ i_buffer_index - 8 ] = ( i_delta_g * 3 ) / 48;
                            pi_buf2[ i_buffer_index - 7 ] = ( i_delta_b * 3 ) / 48;
                        }
                    }
                    pi_buf2[ i_buffer_index - 3 ] = ( i_delta_r * 7 ) / 48;
                    pi_buf2[ i_buffer_index - 2 ] = ( i_delta_g * 7 ) / 48;
                    pi_buf2[ i_buffer_index - 1 ] = ( i_delta_b * 7 ) / 48;
                    if ( b_has_next_x )
                    {
                        pi_buf2[ i_buffer_index + 0 ] = ( i_delta_r * 5 ) / 48;
                        pi_buf2[ i_buffer_index + 1 ] = ( i_delta_g * 5 ) / 48;
                        pi_buf2[ i_buffer_index + 2 ] = ( i_delta_b * 5 ) / 48;
                        if ( i_x < i_width - 2 )
                        {
                            pi_buf2[ i_buffer_index + 3 ] = ( i_delta_r * 3 ) / 48;
                            pi_buf2[ i_buffer_index + 4 ] = ( i_delta_g * 3 ) / 48;
                            pi_buf2[ i_buffer_index + 5 ] = ( i_delta_b * 3 ) / 48;
                        }
                    }
                }
                if ( b_has_next_next_y )
                {
                    if ( i_x > 0 )
                    {
                        pi_buf3[ i_buffer_index - 6 ] = ( i_delta_r * 3 ) / 48;
                        pi_buf3[ i_buffer_index - 5 ] = ( i_delta_g * 3 ) / 48;
                        pi_buf3[ i_buffer_index - 4 ] = ( i_delta_b * 3 ) / 48;
                        if ( i_x > 1 )
                        {
                            pi_buf3[ i_buffer_index - 9 ] = ( i_delta_r * 1 ) / 48;
                            pi_buf3[ i_buffer_index - 8 ] = ( i_delta_g * 1 ) / 48;
                            pi_buf3[ i_buffer_index - 7 ] = ( i_delta_b * 1 ) / 48;
                        }
                    }
                    pi_buf3[ i_buffer_index - 3 ] = ( i_delta_r * 5 ) / 48;
                    pi_buf3[ i_buffer_index - 2 ] = ( i_delta_g * 5 ) / 48;
                    pi_buf3[ i_buffer_index - 1 ] = ( i_delta_b * 5 ) / 48;
                    if ( b_has_next_x )
                    {
                        pi_buf3[ i_buffer_index + 0 ] = ( i_delta_r * 3 ) / 48;
                        pi_buf3[ i_buffer_index + 1 ] = ( i_delta_g * 3 ) / 48;
                        pi_buf3[ i_buffer_index + 2 ] = ( i_delta_b * 3 ) / 48;
                        if ( i_x < i_width - 2 )
                        {
                            pi_buf3[ i_buffer_index + 3 ] = ( i_delta_r * 1 ) / 48;
                            pi_buf3[ i_buffer_index + 4 ] = ( i_delta_g * 1 ) / 48;
                            pi_buf3[ i_buffer_index + 5 ] = ( i_delta_b * 1 ) / 48;
                        }
                    }
                }
                p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
            }
        }
        else
        {
            int i_buffer_index = (int) ( i_width * 3 ) - 1;
            int *pi_buf1 = p_sys->p_dither_buffer[1];
            int *pi_buf2 = p_sys->p_dither_buffer[0];
            int *pi_buf3 = p_sys->p_dither_buffer[2];
            for ( unsigned int i_x = i_width; i_x-- > 0; )
            {
                const bool b_has_next_x = ( i_x > 0 );
                int i_r, i_g, i_b;
                const unsigned int i_x_abs = i_x + i_x_offset;
                p_sys->pf_read_pixel( p_pic, i_x_abs, i_y_abs, &i_r, &i_g, &i_b );
                i_b = ( i_b + pi_buf1[ i_buffer_index-- ] );
                i_g = ( i_g + pi_buf1[ i_buffer_index-- ] );
                i_r = ( i_r + pi_buf1[ i_buffer_index-- ] );
                i_r = i_r > 255 ? 255 : ( i_r < 0 ? 0 : i_r );
                i_g = i_g > 255 ? 255 : ( i_g < 0 ? 0 : i_g );
                i_b = i_b > 255 ? 255 : ( i_b < 0 ? 0 : i_b );
                const int i_lookup_r = i_r >> ( 8 - COLOR_BITS );
                const int i_lookup_g = i_g >> ( 8 - COLOR_BITS );
                const int i_lookup_b = i_b >> ( 8 - COLOR_BITS );
                const int i_lookup_idx = ( i_lookup_r << ( COLOR_BITS * 2) ) | ( i_lookup_g << COLOR_BITS ) | i_lookup_b;
                const int i_palette_idx = p_sys->p_lookup_table[ i_lookup_idx ];
                const int i_palette_color = p_sys->p_palette[ i_palette_idx ];
                const int i_pr = ( i_palette_color >> 16 ) & 0xFF;
                const int i_pg = ( i_palette_color >>  8 ) & 0xFF;
                const int i_pb = ( i_palette_color >>  0 ) & 0xFF;
                const int i_delta_r = i_r - i_pr;
                const int i_delta_g = i_g - i_pg;
                const int i_delta_b = i_b - i_pb;
                if ( b_has_next_x )
                {
                    pi_buf1[ i_buffer_index - 0 ] = ( i_delta_b * 7 ) / 48;
                    pi_buf1[ i_buffer_index - 1 ] = ( i_delta_g * 7 ) / 48;
                    pi_buf1[ i_buffer_index - 2 ] = ( i_delta_r * 7 ) / 48;
                    if ( i_x > 1 )
                    {
                        pi_buf1[ i_buffer_index - 3 ] = ( i_delta_b * 5 ) / 48;
                        pi_buf1[ i_buffer_index - 4 ] = ( i_delta_g * 5 ) / 48;
                        pi_buf1[ i_buffer_index - 5 ] = ( i_delta_r * 5 ) / 48;
                    }
                }
                if ( b_has_next_y )
                {
                    if ( i_x < i_width_minus )
                    {
                        pi_buf2[ i_buffer_index + 6 ] = ( i_delta_b * 5 ) / 48;
                        pi_buf2[ i_buffer_index + 5 ] = ( i_delta_g * 5 ) / 48;
                        pi_buf2[ i_buffer_index + 4 ] = ( i_delta_r * 5 ) / 48;
                        if ( i_x < i_width - 2 )
                        {
                            pi_buf2[ i_buffer_index + 9 ] = ( i_delta_b * 3 ) / 48;
                            pi_buf2[ i_buffer_index + 8 ] = ( i_delta_g * 3 ) / 48;
                            pi_buf2[ i_buffer_index + 7 ] = ( i_delta_r * 3 ) / 48;
                        }
                    }
                    pi_buf2[ i_buffer_index + 3 ] = ( i_delta_b * 7 ) / 48;
                    pi_buf2[ i_buffer_index + 2 ] = ( i_delta_g * 7 ) / 48;
                    pi_buf2[ i_buffer_index + 1 ] = ( i_delta_r * 7 ) / 48;
                    if ( b_has_next_x )
                    {
                        pi_buf2[ i_buffer_index - 0 ] = ( i_delta_b * 5 ) / 48;
                        pi_buf2[ i_buffer_index - 1 ] = ( i_delta_g * 5 ) / 48;
                        pi_buf2[ i_buffer_index - 2 ] = ( i_delta_r * 5 ) / 48;
                        if ( i_x > 1 )
                        {
                            pi_buf2[ i_buffer_index - 3 ] = ( i_delta_b * 3 ) / 48;
                            pi_buf2[ i_buffer_index - 4 ] = ( i_delta_g * 3 ) / 48;
                            pi_buf2[ i_buffer_index - 5 ] = ( i_delta_r * 3 ) / 48;
                        }
                    }
                }
                if ( b_has_next_next_y )
                {
                    if ( i_x < i_width_minus )
                    {
                        pi_buf3[ i_buffer_index + 6 ] = ( i_delta_b * 3 ) / 48;
                        pi_buf3[ i_buffer_index + 5 ] = ( i_delta_g * 3 ) / 48;
                        pi_buf3[ i_buffer_index + 4 ] = ( i_delta_r * 3 ) / 48;
                        if ( i_x < i_width - 2 )
                        {
                            pi_buf3[ i_buffer_index + 9 ] = ( i_delta_b * 1 ) / 48;
                            pi_buf3[ i_buffer_index + 8 ] = ( i_delta_g * 1 ) / 48;
                            pi_buf3[ i_buffer_index + 7 ] = ( i_delta_r * 1 ) / 48;
                        }
                    }
                    pi_buf3[ i_buffer_index + 3 ] = ( i_delta_b * 5 ) / 48;
                    pi_buf3[ i_buffer_index + 2 ] = ( i_delta_g * 5 ) / 48;
                    pi_buf3[ i_buffer_index + 1 ] = ( i_delta_r * 5 ) / 48;
                    if ( b_has_next_x )
                    {
                        pi_buf3[ i_buffer_index - 0 ] = ( i_delta_b * 3 ) / 48;
                        pi_buf3[ i_buffer_index - 1 ] = ( i_delta_g * 3 ) / 48;
                        pi_buf3[ i_buffer_index - 2 ] = ( i_delta_r * 3 ) / 48;
                        if ( i_x > 1 )
                        {
                            pi_buf3[ i_buffer_index - 3 ] = ( i_delta_b * 1 ) / 48;
                            pi_buf3[ i_buffer_index - 4 ] = ( i_delta_g * 1 ) / 48;
                            pi_buf3[ i_buffer_index - 5 ] = ( i_delta_r * 1 ) / 48;
                        }
                    }
                }
                p_sys->pf_write_pixel( p_pic, i_x_abs, i_y_abs, i_pr, i_pg, i_pb );
            }
        }
        if ( ( i_y & 0x1 ) == 0 )
        {
            memcpy( p_sys->p_dither_buffer[0], p_sys->p_dither_buffer[1], i_width * 3 * 2 * sizeof ( int ) );
            memcpy( p_sys->p_dither_buffer[1], p_sys->p_dither_buffer[2], i_width * 3 * 2 * sizeof( int ) );
        }
        else
        {
            memcpy( p_sys->p_dither_buffer[1], p_sys->p_dither_buffer[0], i_width * 3 * 2 * sizeof ( int ) );
            memcpy( p_sys->p_dither_buffer[0], p_sys->p_dither_buffer[2], i_width * 3 * 2 * sizeof( int ) );
        }
        memset( p_sys->p_dither_buffer[2], 0, i_width * 3 * 2 * sizeof( int ) );
    }
}