/*****************************************************************************
 * equalizer.c:
 *****************************************************************************
 * Copyright (C) 2004-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_charset.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

#include "equalizer_presets.h"

/* TODO:
 *  - optimize a bit (you can hardly do slower ;)
 *  - add tables for more bands (15 and 32 would be cool), maybe with auto coeffs
 *    computation (not too hard once the Q is found).
 *  - support for external preset
 *  - callback to handle preset changes on the fly
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define PRESET_TEXT N_( "Equalizer preset" )
#define PRESET_LONGTEXT N_("Preset to use for the equalizer." )

#define BANDS_TEXT N_( "Bands gain")
#define BANDS_LONGTEXT N_( \
         "Don't use presets, but manually specified bands. You need to " \
         "provide 10 values between -20dB and 20dB, separated by spaces, " \
         "e.g. \"0 2 4 2 0 -2 -4 -2 0 2\"." )

#define VLC_BANDS_TEXT N_( "Use VLC frequency bands" )
#define VLC_BANDS_LONGTEXT N_( \
         "Use the VLC frequency bands. Otherwise, use the ISO Standard " \
         "frequency bands." )

#define TWOPASS_TEXT N_( "Two pass" )
#define TWOPASS_LONGTEXT N_( "Filter the audio twice. This provides a more "  \
         "intense effect.")

#define PREAMP_TEXT N_("Global gain" )
#define PREAMP_LONGTEXT N_("Set the global gain in dB (-20 ... 20)." )

vlc_module_begin ()
    set_description( N_("Equalizer with 10 bands") )
    set_shortname( N_("Equalizer" ) )
    set_capability( "audio filter", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )

    add_string( "equalizer-preset", "flat", PRESET_TEXT,
                PRESET_LONGTEXT, false )
        change_string_list( preset_list, preset_list_text )
    add_string( "equalizer-bands", NULL, BANDS_TEXT,
                BANDS_LONGTEXT, true )
    add_bool( "equalizer-2pass", false, TWOPASS_TEXT,
              TWOPASS_LONGTEXT, true )
    add_bool( "equalizer-vlcfreqs", true, VLC_BANDS_TEXT,
              VLC_BANDS_LONGTEXT, true )
    add_float( "equalizer-preamp", 12.0, PREAMP_TEXT,
               PREAMP_LONGTEXT, true )
    set_callbacks( Open, Close )
    add_shortcut( "equalizer" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    /* Filter static config */
    int i_band;
    float *f_alpha;
    float *f_beta;
    float *f_gamma;

    float f_newpreamp;
    char *psz_newbands;
    bool b_first;

    /* Filter dyn config */
    float *f_amp;   /* Per band amp */
    float f_gamp;   /* Global preamp */
    bool b_2eqz;

    /* Filter state */
    float x[32][2];
    float y[32][128][2];

    /* Second filter state */
    float x2[32][2];
    float y2[32][128][2];

    vlc_mutex_t lock;
};

static block_t *DoWork( filter_t *, block_t * );

#define EQZ_IN_FACTOR (0.25)
static int  EqzInit( filter_t *, int );
static void EqzFilter( filter_t *, float *, float *, int, int );
static void EqzClean( filter_t * );

static int PresetCallback ( vlc_object_t *, char const *, vlc_value_t,
                            vlc_value_t, void * );
static int PreampCallback ( vlc_object_t *, char const *, vlc_value_t,
                            vlc_value_t, void * );
static int BandsCallback  ( vlc_object_t *, char const *, vlc_value_t,
                            vlc_value_t, void * );
static int TwoPassCallback( vlc_object_t *, char const *, vlc_value_t,
                            vlc_value_t, void * );



/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    filter_sys_t *p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sys->lock );
    if( EqzInit( p_filter, p_filter->fmt_in.audio.i_rate ) != VLC_SUCCESS )
    {
        vlc_mutex_destroy( &p_sys->lock );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    EqzClean( p_filter );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 *
 *****************************************************************************/
static block_t * DoWork( filter_t * p_filter, block_t * p_in_buf )
{
    EqzFilter( p_filter, (float*)p_in_buf->p_buffer,
               (float*)p_in_buf->p_buffer, p_in_buf->i_nb_samples,
               aout_FormatNbChannels( &p_filter->fmt_in.audio ) );
    return p_in_buf;
}

/*****************************************************************************
 * Equalizer stuff
 *****************************************************************************/
typedef struct
{
    int   i_band;

    struct
    {
        float f_frequency;
        float f_alpha;
        float f_beta;
        float f_gamma;
    } band[EQZ_BANDS_MAX];

} eqz_config_t;

/* The frequency tables */
static const float f_vlc_frequency_table_10b[EQZ_BANDS_MAX] =
{
    60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000,
};

static const float f_iso_frequency_table_10b[EQZ_BANDS_MAX] =
{
    31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000,
};

/* Equalizer coefficient calculation function based on equ-xmms */
static void EqzCoeffs( int i_rate, float f_octave_percent,
                       bool b_use_vlc_freqs,
                       eqz_config_t *p_eqz_config )
{
    const float *f_freq_table_10b = b_use_vlc_freqs
                                  ? f_vlc_frequency_table_10b
                                  : f_iso_frequency_table_10b;
    float f_rate = (float) i_rate;
    float f_nyquist_freq = 0.5 * f_rate;
    float f_octave_factor = pow( 2.0, 0.5 * f_octave_percent );
    float f_octave_factor_1 = 0.5 * ( f_octave_factor + 1.0 );
    float f_octave_factor_2 = 0.5 * ( f_octave_factor - 1.0 );

    p_eqz_config->i_band = EQZ_BANDS_MAX;

    for( int i = 0; i < EQZ_BANDS_MAX; i++ )
    {
        float f_freq = f_freq_table_10b[i];

        p_eqz_config->band[i].f_frequency = f_freq;

        if( f_freq <= f_nyquist_freq )
        {
            float f_theta_1 = ( 2.0 * M_PI * f_freq ) / f_rate;
            float f_theta_2 = f_theta_1 / f_octave_factor;
            float f_sin     = sin( f_theta_2 ) * 0.5;
            float f_sin_prd = sin( f_theta_2 * f_octave_factor_1 )
                            * sin( f_theta_2 * f_octave_factor_2 );
            /* The equation from equ-xmms simplifies to something similar to
             * this when you restrict the domain to all valid frequencies at or
             * below the Nyquist frequency (the interval 0 <= f_theta_1 <= Pi).
             * (This result for the root is twice that returned by equ-xmms,
             * but the more efficient calculations for alpha, beta, and gamma
             * below compensate for this.) */
            float f_root    = ( f_sin - f_sin_prd ) / ( f_sin + f_sin_prd );

            p_eqz_config->band[i].f_alpha = ( 1.0 - f_root ) * 0.5;
            p_eqz_config->band[i].f_beta  = f_root;
            p_eqz_config->band[i].f_gamma = ( 1.0 + f_root ) * cos( f_theta_1 );
        }
        else
        {
            /* Any frequency beyond the Nyquist frequency is no good... */
            p_eqz_config->band[i].f_alpha =
            p_eqz_config->band[i].f_beta  =
            p_eqz_config->band[i].f_gamma = 0.0;
        }
    }
}

static inline float EqzConvertdB( float db )
{
    /* Map it to gain,
     * (we do as if the input of iir is /EQZ_IN_FACTOR, but in fact it's the non iir data that is *EQZ_IN_FACTOR)
     * db = 20*log( out / in ) with out = in + amp*iir(i/EQZ_IN_FACTOR)
     * or iir(i) == i for the center freq so
     * db = 20*log( 1 + amp/EQZ_IN_FACTOR )
     * -> amp = EQZ_IN_FACTOR*(10^(db/20) - 1)
     **/

    if( db < -20.0 )
        db = -20.0;
    else if(  db > 20.0 )
        db = 20.0;
    return EQZ_IN_FACTOR * ( pow( 10, db / 20.0 ) - 1.0 );
}

static int EqzInit( filter_t *p_filter, int i_rate )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    eqz_config_t cfg;
    int i, ch;
    vlc_value_t val1, val2, val3;
    vlc_object_t *p_aout = p_filter->p_parent;
    int i_ret = VLC_ENOMEM;

    bool b_vlcFreqs = var_InheritBool( p_aout, "equalizer-vlcfreqs" );
    EqzCoeffs( i_rate, 1.0, b_vlcFreqs, &cfg );

    /* Create the static filter config */
    p_sys->i_band = cfg.i_band;
    p_sys->f_alpha = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_beta  = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_gamma = malloc( p_sys->i_band * sizeof(float) );
    if( !p_sys->f_alpha || !p_sys->f_beta || !p_sys->f_gamma )
        goto error;

    for( i = 0; i < p_sys->i_band; i++ )
    {
        p_sys->f_alpha[i] = cfg.band[i].f_alpha;
        p_sys->f_beta[i]  = cfg.band[i].f_beta;
        p_sys->f_gamma[i] = cfg.band[i].f_gamma;
    }

    /* Filter dyn config */
    p_sys->b_2eqz = false;
    p_sys->f_gamp = 1.0;
    p_sys->f_amp  = malloc( p_sys->i_band * sizeof(float) );
    if( !p_sys->f_amp )
        goto error;

    for( i = 0; i < p_sys->i_band; i++ )
    {
        p_sys->f_amp[i] = 0.0;
    }

    /* Filter state */
    for( ch = 0; ch < 32; ch++ )
    {
        p_sys->x[ch][0]  =
        p_sys->x[ch][1]  =
        p_sys->x2[ch][0] =
        p_sys->x2[ch][1] = 0.0;

        for( i = 0; i < p_sys->i_band; i++ )
        {
            p_sys->y[ch][i][0]  =
            p_sys->y[ch][i][1]  =
            p_sys->y2[ch][i][0] =
            p_sys->y2[ch][i][1] = 0.0;
        }
    }

    var_Create( p_aout, "equalizer-bands", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_aout, "equalizer-preset", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    p_sys->b_2eqz = var_CreateGetBool( p_aout, "equalizer-2pass" );

    var_Create( p_aout, "equalizer-preamp", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );

    /* Get initial values */
    var_Get( p_aout, "equalizer-preset", &val1 );
    var_Get( p_aout, "equalizer-bands", &val2 );
    var_Get( p_aout, "equalizer-preamp", &val3 );

    p_sys->b_first = true;
    PresetCallback( VLC_OBJECT( p_aout ), NULL, val1, val1, p_sys );
    BandsCallback(  VLC_OBJECT( p_aout ), NULL, val2, val2, p_sys );
    PreampCallback( VLC_OBJECT( p_aout ), NULL, val3, val3, p_sys );
    p_sys->b_first = false;

    free( val1.psz_string );

    /* Register preset bands (for intf) if : */
    /* We have no bands info --> the preset info must be given to the intf */
    /* or The bands info matches the preset */
    if (p_sys->psz_newbands == NULL)
    {
        msg_Err(p_filter, "No preset selected");
        free( val2.psz_string );
        free( p_sys->f_amp );
        i_ret = VLC_EGENERIC;
        goto error;
    }
    if( ( *(val2.psz_string) &&
        strstr( p_sys->psz_newbands, val2.psz_string ) ) || !*val2.psz_string )
    {
        var_SetString( p_aout, "equalizer-bands", p_sys->psz_newbands );
        if( p_sys->f_newpreamp == p_sys->f_gamp )
            var_SetFloat( p_aout, "equalizer-preamp", p_sys->f_newpreamp );
    }
    free( val2.psz_string );

    /* Add our own callbacks */
    var_AddCallback( p_aout, "equalizer-preset", PresetCallback, p_sys );
    var_AddCallback( p_aout, "equalizer-bands", BandsCallback, p_sys );
    var_AddCallback( p_aout, "equalizer-preamp", PreampCallback, p_sys );
    var_AddCallback( p_aout, "equalizer-2pass", TwoPassCallback, p_sys );

    msg_Dbg( p_filter, "equalizer loaded for %d Hz with %d bands %d pass",
                        i_rate, p_sys->i_band, p_sys->b_2eqz ? 2 : 1 );
    for( i = 0; i < p_sys->i_band; i++ )
    {
        msg_Dbg( p_filter, "   %d Hz -> factor:%f alpha:%f beta:%f gamma:%f",
                 (int)cfg.band[i].f_frequency, p_sys->f_amp[i],
                 p_sys->f_alpha[i], p_sys->f_beta[i], p_sys->f_gamma[i]);
    }
    return VLC_SUCCESS;

error:
    free( p_sys->f_alpha );
    free( p_sys->f_beta );
    free( p_sys->f_gamma );
    return i_ret;
}

static void EqzFilter( filter_t *p_filter, float *out, float *in,
                       int i_samples, int i_channels )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i, ch, j;

    vlc_mutex_lock( &p_sys->lock );
    for( i = 0; i < i_samples; i++ )
    {
        for( ch = 0; ch < i_channels; ch++ )
        {
            const float x = in[ch];
            float o = 0.0;

            for( j = 0; j < p_sys->i_band; j++ )
            {
                float y = p_sys->f_alpha[j] * ( x - p_sys->x[ch][1] ) +
                          p_sys->f_gamma[j] * p_sys->y[ch][j][0] -
                          p_sys->f_beta[j]  * p_sys->y[ch][j][1];

                p_sys->y[ch][j][1] = p_sys->y[ch][j][0];
                p_sys->y[ch][j][0] = y;

                o += y * p_sys->f_amp[j];
            }
            p_sys->x[ch][1] = p_sys->x[ch][0];
            p_sys->x[ch][0] = x;

            /* Second filter */
            if( p_sys->b_2eqz )
            {
                const float x2 = EQZ_IN_FACTOR * x + o;
                o = 0.0;
                for( j = 0; j < p_sys->i_band; j++ )
                {
                    float y = p_sys->f_alpha[j] * ( x2 - p_sys->x2[ch][1] ) +
                              p_sys->f_gamma[j] * p_sys->y2[ch][j][0] -
                              p_sys->f_beta[j]  * p_sys->y2[ch][j][1];

                    p_sys->y2[ch][j][1] = p_sys->y2[ch][j][0];
                    p_sys->y2[ch][j][0] = y;

                    o += y * p_sys->f_amp[j];
                }
                p_sys->x2[ch][1] = p_sys->x2[ch][0];
                p_sys->x2[ch][0] = x2;

                /* We add source PCM + filtered PCM */
                out[ch] = p_sys->f_gamp *( EQZ_IN_FACTOR * x2 + o );
            }
            else
            {
                /* We add source PCM + filtered PCM */
                out[ch] = p_sys->f_gamp *( EQZ_IN_FACTOR * x + o );
            }
        }

        in  += i_channels;
        out += i_channels;
    }
    vlc_mutex_unlock( &p_sys->lock );
}

static void EqzClean( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_object_t *p_aout = p_filter->p_parent;

    var_DelCallback( p_aout, "equalizer-bands", BandsCallback, p_sys );
    var_DelCallback( p_aout, "equalizer-preset", PresetCallback, p_sys );
    var_DelCallback( p_aout, "equalizer-preamp", PreampCallback, p_sys );
    var_DelCallback( p_aout, "equalizer-2pass", TwoPassCallback, p_sys );

    free( p_sys->f_alpha );
    free( p_sys->f_beta );
    free( p_sys->f_gamma );

    free( p_sys->f_amp );
    free( p_sys->psz_newbands );
}


static int PresetCallback( vlc_object_t *p_aout, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = p_data;

    const char *psz_preset = newval.psz_string;

    vlc_mutex_lock( &p_sys->lock );
    if( !*psz_preset || p_sys->i_band != 10 )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return VLC_SUCCESS;
    }

    for( unsigned i = 0; i < NB_PRESETS; i++ )
    {
        if( !strcasecmp( eqz_preset_10b[i].psz_name, psz_preset ) )
        {
            char *psz_newbands = NULL;

            p_sys->f_gamp *= pow( 10, eqz_preset_10b[i].f_preamp / 20.0 );
            for( int j = 0; j < p_sys->i_band; j++ )
            {
                lldiv_t d;
                char *psz;

                p_sys->f_amp[j] = EqzConvertdB( eqz_preset_10b[i].f_amp[j] );
                d = lldiv( eqz_preset_10b[i].f_amp[j] * 10000000, 10000000 );
                if( asprintf( &psz, "%s %lld.%07llu",
                              psz_newbands ? psz_newbands : "",
                              d.quot, d.rem ) == -1 )
                {
                    free( psz_newbands );
                    vlc_mutex_unlock( &p_sys->lock );
                    return VLC_ENOMEM;
                }
                free( psz_newbands );
                psz_newbands = psz;
            }
            if( !p_sys->b_first )
            {
                vlc_mutex_unlock( &p_sys->lock );
                var_SetString( p_aout, "equalizer-bands", psz_newbands );
                var_SetFloat( p_aout, "equalizer-preamp",
                              eqz_preset_10b[i].f_preamp );
                free( psz_newbands );
            }
            else
            {
                p_sys->psz_newbands = psz_newbands;
                p_sys->f_newpreamp = eqz_preset_10b[i].f_preamp;
                vlc_mutex_unlock( &p_sys->lock );
            }
            return VLC_SUCCESS;
        }
    }
    vlc_mutex_unlock( &p_sys->lock );
    msg_Err( p_aout, "equalizer preset '%s' not found", psz_preset );
    msg_Info( p_aout, "full list:" );
    for( unsigned i = 0; i < NB_PRESETS; i++ )
         msg_Info( p_aout, "  - '%s'", eqz_preset_10b[i].psz_name );
    return VLC_SUCCESS;
}

static int PreampCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = p_data;

    if( newval.f_float < -20.0 )
        newval.f_float = -20.0;
    else if( newval.f_float > 20.0 )
        newval.f_float = 20.0;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->f_gamp = pow( 10, newval.f_float /20.0);
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int BandsCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = p_data;
    const char *psz_bands = newval.psz_string;
    const char *p = psz_bands;
    char *psz_next;

    /* Same thing for bands */
    vlc_mutex_lock( &p_sys->lock );
    for( int i = 0; i < p_sys->i_band; i++ )
    {
        float f;

        if( *psz_bands == '\0' )
            break;

        /* Read dB -20/20 */
        f = us_strtof( p, &psz_next );

        if( psz_next == p )
            break; /* no conversion */

        p_sys->f_amp[i] = EqzConvertdB( f );

        if( *psz_next == '\0' )
            break; /* end of line */
        p = &psz_next[1];
    }
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}
static int TwoPassCallback( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = p_data;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_2eqz = newval.b_bool;
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}

