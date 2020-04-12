/*****************************************************************************
 * visual.c : Visualisation system
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_queue.h>

#include "visual.h"

#include "window_presets.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ELIST_TEXT N_( "Effects list" )
#define ELIST_LONGTEXT N_( \
      "A list of visual effect, separated by commas.\n"  \
      "Current effects include: dummy, scope, spectrum, "\
      "spectrometer and vuMeter." )

#define WIDTH_TEXT N_( "Video width" )
#define WIDTH_LONGTEXT N_( \
      "The width of the effects video window, in pixels." )

#define HEIGHT_TEXT N_( "Video height" )
#define HEIGHT_LONGTEXT N_( \
      "The height of the effects video window, in pixels." )

#define FFT_WINDOW_TEXT N_( "FFT window" )
#define FFT_WINDOW_LONGTEXT N_( \
      "The type of FFT window to use for spectrum-based visualizations." )

#define KAISER_PARAMETER_TEXT N_( "Kaiser window parameter" )
#define KAISER_PARAMETER_LONGTEXT N_( \
      "The parameter alpha for the Kaiser window. Increasing alpha " \
      "increases the main-lobe width and decreases the side-lobe amplitude." )

#define NBBANDS_TEXT N_( "Show 80 bands instead of 20" )
#define SPNBBANDS_LONGTEXT N_( \
      "More bands for the spectrometer: 80 if enabled, else 20." )

#define SEPAR_TEXT N_( "Number of blank pixels between bands.")

#define AMP_TEXT N_( "Amplification" )
#define AMP_LONGTEXT N_( \
        "This is a coefficient that modifies the height of the bands.")

#define PEAKS_TEXT N_( "Draw peaks in the analyzer" )

#define ORIG_TEXT N_( "Enable original graphic spectrum" )
#define ORIG_LONGTEXT N_( \
        "Enable the \"flat\" spectrum analyzer in the spectrometer." )

#define BANDS_TEXT N_( "Draw bands in the spectrometer" )

#define BASE_TEXT N_( "Draw the base of the bands" )

#define RADIUS_TEXT N_( "Base pixel radius" )
#define RADIUS_LONGTEXT N_( \
        "Defines radius size in pixels, of base of bands(beginning)." )

#define SSECT_TEXT N_( "Spectral sections" )
#define SSECT_LONGTEXT N_( \
        "Determines how many sections of spectrum will exist." )

#define PEAK_HEIGHT_TEXT N_( "Peak height" )
#define PEAK_HEIGHT_LONGTEXT N_( \
        "Total pixel height of the peak items." )

#define PEAK_WIDTH_TEXT N_( "Peak extra width" )
#define PEAK_WIDTH_LONGTEXT N_( \
        "Additions or subtractions of pixels on the peak width." )

#define COLOR1_TEXT N_( "V-plane color" )
#define COLOR1_LONGTEXT N_( \
        "YUV-Color cube shifting across the V-plane ( 0 - 127 )." )

/* Default vout size */
#define VOUT_WIDTH  800
#define VOUT_HEIGHT 500

static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Visualizer"))
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_VISUAL )
    set_description( N_("Visualizer filter") )
    set_section( N_( "General") , NULL )
    add_string("effect-list", "spectrum",
            ELIST_TEXT, ELIST_LONGTEXT, true )
    add_integer("effect-width",VOUT_WIDTH,
             WIDTH_TEXT, WIDTH_LONGTEXT, false )
    add_integer("effect-height" , VOUT_HEIGHT ,
             HEIGHT_TEXT, HEIGHT_LONGTEXT, false )
    add_string("effect-fft-window", "flat",
            FFT_WINDOW_TEXT, FFT_WINDOW_LONGTEXT, true )
        change_string_list( window_list, window_list_text )
    add_float("effect-kaiser-param", 3.0f,
            KAISER_PARAMETER_TEXT, KAISER_PARAMETER_LONGTEXT, true )
    set_section( N_("Spectrum analyser") , NULL )
    add_obsolete_integer( "visual-nbbands" ) /* Since 1.0.0 */
    add_bool("visual-80-bands", true,
             NBBANDS_TEXT, NBBANDS_TEXT, true );
    add_obsolete_integer( "visual-separ" ) /* Since 1.0.0 */
    add_obsolete_integer( "visual-amp" ) /* Since 1.0.0 */
    add_bool("visual-peaks", true,
             PEAKS_TEXT, PEAKS_TEXT, true )
    set_section( N_("Spectrometer") , NULL )
    add_bool("spect-show-original", false,
             ORIG_TEXT, ORIG_LONGTEXT, true )
    add_bool("spect-show-base", true,
             BASE_TEXT, BASE_TEXT, true )
    add_integer("spect-radius", 42,
             RADIUS_TEXT, RADIUS_LONGTEXT, true )
    add_integer_with_range("spect-sections", 3, 1, INT_MAX,
             SSECT_TEXT, SSECT_LONGTEXT, true )
    add_integer("spect-color", 80,
             COLOR1_TEXT, COLOR1_LONGTEXT, true )
    add_bool("spect-show-bands", true,
             BANDS_TEXT, BANDS_TEXT, true );
    add_obsolete_integer( "spect-nbbands" ) /* Since 1.0.0 */
    add_bool("spect-80-bands", true,
             NBBANDS_TEXT, NBBANDS_TEXT, true )
    add_integer("spect-separ", 1,
             SEPAR_TEXT, SEPAR_TEXT, true )
    add_integer("spect-amp", 8,
             AMP_TEXT, AMP_LONGTEXT, true )
    add_bool("spect-show-peaks", true,
             PEAKS_TEXT, PEAKS_TEXT, true )
    add_integer("spect-peak-width", 61,
             PEAK_WIDTH_TEXT, PEAK_WIDTH_LONGTEXT, true )
    add_integer("spect-peak-height", 1,
             PEAK_HEIGHT_TEXT, PEAK_HEIGHT_LONGTEXT, true )
    set_capability( "visualization", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "visualizer")
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *DoWork( filter_t *, block_t * );
static void Flush( filter_t * );
static void *Thread( void *);

typedef struct
{
    vlc_queue_t     queue;
    vout_thread_t   *p_vout;
    visual_effect_t **effect;
    int             i_effect;
    bool            dead;
    vlc_thread_t    thread;
} filter_sys_t;

/*****************************************************************************
 * Open: open the visualizer
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    char *psz_effects, *psz_parser;

    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( unlikely (p_sys == NULL ) )
        return VLC_EGENERIC;

    int width = var_InheritInteger( p_filter , "effect-width");
    int height = var_InheritInteger( p_filter , "effect-width");
    /* No resolution under 400x532 and no odd dimension */
    if( width < 532 )
        width  = 532;
    width &= ~1;
    if( height < 400 )
        height = 400;
    height &= ~1;

    p_sys->i_effect = 0;
    p_sys->effect   = NULL;

    /* Parse the effect list */
    psz_parser = psz_effects = var_CreateGetString( p_filter, "effect-list" );

    while( psz_parser && *psz_parser != '\0' )
    {
        visual_effect_t *p_effect;

        p_effect = malloc( sizeof( visual_effect_t ) );
        if( !p_effect )
            break;
        p_effect->i_width     = width;
        p_effect->i_height    = height;
        p_effect->i_nb_chans  = aout_FormatNbChannels( &p_filter->fmt_in.audio);
        p_effect->i_idx_left  = 0;
        p_effect->i_idx_right = __MIN( 1, p_effect->i_nb_chans-1 );

        p_effect->p_data   = NULL;
        p_effect->pf_run   = NULL;

        for( unsigned i = 0; i < effectc; i++ )
        {
            if( !strncasecmp( psz_parser, effectv[i].name,
                              strlen( effectv[i].name ) ) )
            {
                p_effect->pf_run = effectv[i].run_cb;
                p_effect->pf_free = effectv[i].free_cb;
                psz_parser += strlen( effectv[i].name );
                break;
            }
        }

        if( p_effect->pf_run != NULL )
        {
            if( *psz_parser == '{' )
            {
                char *psz_eoa;

                psz_parser++;

                if( ( psz_eoa = strchr( psz_parser, '}') ) == NULL )
                {
                   msg_Err( p_filter, "unable to parse effect list. Aborting");
                   free( p_effect );
                   break;
                }
            }
            TAB_APPEND( p_sys->i_effect, p_sys->effect, p_effect );
        }
        else
        {
            msg_Err( p_filter, "unknown visual effect: %s", psz_parser );
            free( p_effect );
        }

        if( strchr( psz_parser, ',' ) )
        {
            psz_parser = strchr( psz_parser, ',' ) + 1;
        }
        else if( strchr( psz_parser, ':' ) )
        {
            psz_parser = strchr( psz_parser, ':' ) + 1;
        }
        else
        {
            break;
        }
    }

    free( psz_effects );

    if( !p_sys->i_effect )
    {
        msg_Err( p_filter, "no effects found" );
        goto error;
    }

    /* Open the video output */
    video_format_t fmt = {
        .i_chroma = VLC_CODEC_I420,
        .i_width = width,
        .i_height = height,
        .i_visible_width = width,
        .i_visible_height = height,
        .i_sar_num = 1,
        .i_sar_den = 1,
        .transfer = TRANSFER_FUNC_SRGB,
        .primaries = COLOR_PRIMARIES_SRGB,
        .space = COLOR_SPACE_SRGB,
    };
    p_sys->p_vout = aout_filter_GetVout( p_filter, &fmt );
    if( p_sys->p_vout == NULL )
    {
        msg_Err( p_filter, "no suitable vout module" );
        goto error;
    }

    p_sys->dead = false;
    vlc_queue_Init(&p_sys->queue, offsetof (block_t, p_next));

    if( vlc_clone( &p_sys->thread, Thread, p_filter,
                   VLC_THREAD_PRIORITY_VIDEO ) )
    {
        vout_Close( p_sys->p_vout );
        goto error;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;
    p_filter->pf_flush = Flush;
    return VLC_SUCCESS;

error:
    for( int i = 0; i < p_sys->i_effect; i++ )
        free( p_sys->effect[i] );
    free( p_sys->effect );
    free( p_sys );
    return VLC_EGENERIC;
}

static block_t *DoRealWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* First, get a new picture */
    picture_t *p_outpic = vout_GetPicture( p_sys->p_vout );
    p_outpic->b_progressive = true;
    if( unlikely(p_outpic == NULL) )
        return p_in_buf;

    /* Blank the picture */
    for( int i = 0 ; i < p_outpic->i_planes ; i++ )
    {
        memset( p_outpic->p[i].p_pixels, i > 0 ? 0x80 : 0x00,
                p_outpic->p[i].i_visible_lines * p_outpic->p[i].i_pitch );
    }

    /* We can now call our visualization effects */
    for( int i = 0; i < p_sys->i_effect; i++ )
    {
#define p_effect p_sys->effect[i]
        if( p_effect->pf_run )
        {
            p_effect->pf_run( p_effect, VLC_OBJECT(p_filter),
                              p_in_buf, p_outpic );
        }
#undef p_effect
    }

    p_outpic->date = p_in_buf->i_pts + (p_in_buf->i_length / 2);

    vout_PutPicture( p_sys->p_vout, p_outpic );
    return p_in_buf;
}

static void *Thread( void *data )
{
    filter_t *p_filter = data;
    filter_sys_t *sys = p_filter->p_sys;
    block_t *block;

    while ((block = vlc_queue_DequeueKillable(&sys->queue, &sys->dead)))
        block_Release( DoRealWork( p_filter, block ) );

    return NULL;
}

static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_queue_Enqueue(&p_sys->queue, block_Duplicate(p_in_buf));
    return p_in_buf;
}

static void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vout_FlushAll( p_sys->p_vout );
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_queue_Kill(&p_sys->queue, &p_sys->dead);
    vlc_join( p_sys->thread, NULL );
    vout_Close( p_sys->p_vout );

    /* Free the list */
    for( int i = 0; i < p_sys->i_effect; i++ )
    {
#define p_effect (p_sys->effect[i])
        p_effect->pf_free( p_effect->p_data );
        free( p_effect );
#undef p_effect
    }

    free( p_sys->effect );
    free( p_sys );
}
