/*****************************************************************************
 * equalizer.c:
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_charset.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

#include "equalizer_presets.h"
/* TODO:
 *  - add tables for other rates ( 22500, 11250, ...)
 *  - optimize a bit (you can hardly do slower ;)
 *  - add tables for more bands (15 and 32 would be cool), maybe with auto coeffs
 *  computation (not too hard once the Q is found).
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
        change_string_list( preset_list, preset_list_text, 0 )
    add_string( "equalizer-bands", NULL, BANDS_TEXT,
                BANDS_LONGTEXT, true )
    add_bool( "equalizer-2pass", false, TWOPASS_TEXT,
              TWOPASS_LONGTEXT, true )
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
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 ||
        p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32 )
    {
        p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
        p_filter->fmt_out.audio.i_format = VLC_CODEC_FL32;
        msg_Warn( p_filter, "bad input or output format" );
        return VLC_EGENERIC;
    }
    if ( !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio, &p_filter->fmt_out.audio ) )
    {
        memcpy( &p_filter->fmt_out.audio, &p_filter->fmt_in.audio,
                sizeof(audio_sample_format_t) );
        msg_Warn( p_filter, "input and output formats are not similar" );
        return VLC_EGENERIC;
    }

    p_filter->pf_audio_filter = DoWork;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sys->lock );
    if( EqzInit( p_filter, p_filter->fmt_in.audio.i_rate ) != VLC_SUCCESS )
    {
        vlc_mutex_destroy( &p_sys->lock );
        free( p_sys );
        return VLC_EGENERIC;
    }

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

/* Value from equ-xmms */
static const eqz_config_t eqz_config_44100_10b =
{
    10,
    {
        {    60, 0.003013, 0.993973, 1.993901 },
        {   170, 0.008490, 0.983019, 1.982437 },
        {   310, 0.015374, 0.969252, 1.967331 },
        {   600, 0.029328, 0.941343, 1.934254 },
        {  1000, 0.047918, 0.904163, 1.884869 },
        {  3000, 0.130408, 0.739184, 1.582718 },
        {  6000, 0.226555, 0.546889, 1.015267 },
        { 12000, 0.344937, 0.310127, -0.181410 },
        { 14000, 0.366438, 0.267123, -0.521151 },
        { 16000, 0.379009, 0.241981, -0.808451 },
    }
};
static const eqz_config_t eqz_config_48000_10b =
{
    10,
    {
        {    60, 0.002769, 0.994462, 1.994400 },
        {   170, 0.007806, 0.984388, 1.983897 },
        {   310, 0.014143, 0.971714, 1.970091 },
        {   600, 0.027011, 0.945978, 1.939979 },
        {  1000, 0.044203, 0.911595, 1.895241 },
        {  3000, 0.121223, 0.757553, 1.623767 },
        {  6000, 0.212888, 0.574224, 1.113145 },
        { 12000, 0.331347, 0.337307, 0.000000 },
        { 14000, 0.355263, 0.289473, -0.333740 },
        { 16000, 0.371900, 0.256201, -0.628100 }
    }
};

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
    const eqz_config_t *p_cfg;
    int i, ch;
    vlc_value_t val1, val2, val3;
    vlc_object_t *p_aout = p_filter->p_parent;
    int i_ret = VLC_ENOMEM;

    /* Select the config */
    if( i_rate == 48000 )
    {
        p_cfg = &eqz_config_48000_10b;
    }
    else if( i_rate == 44100 )
    {
        p_cfg = &eqz_config_44100_10b;
    }
    else
    {
        /* TODO compute the coeffs on the fly */
        msg_Err( p_filter, "rate not supported" );
        return VLC_EGENERIC;
    }

    /* Create the static filter config */
    p_sys->i_band = p_cfg->i_band;
    p_sys->f_alpha = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_beta  = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_gamma = malloc( p_sys->i_band * sizeof(float) );
    if( !p_sys->f_alpha || !p_sys->f_beta || !p_sys->f_gamma )
        goto error;

    for( i = 0; i < p_sys->i_band; i++ )
    {
        p_sys->f_alpha[i] = p_cfg->band[i].f_alpha;
        p_sys->f_beta[i]  = p_cfg->band[i].f_beta;
        p_sys->f_gamma[i] = p_cfg->band[i].f_gamma;
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
                 (int)p_cfg->band[i].f_frequency, p_sys->f_amp[i],
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

