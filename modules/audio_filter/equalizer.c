/*****************************************************************************
 * equalizer.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <math.h>

#include <vlc/vlc.h>

#include <vlc/aout.h>
#include "aout_internal.h"

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
#define PRESET_LONGTEXT PRESET_TEXT

#define BANDS_TEXT N_( "Bands gain")
#define BANDS_LONGTEXT N_( "Override preset bands gain in dB (-20 ... 20)" )

#define TWOPASS_TEXT N_( "Two pass" )
#define TWOPASS_LONGTEXT N_( "Filter twice the audio" )

#define PREAMP_TEXT N_("Global gain" )
#define PREAMP_LONGTEXT N_("Set the global gain in dB (-20 ... 20)" )

static char *preset_list[] = {
    "flat", "classical", "club", "dance", "fullbass", "fullbasstreeble",
    "fulltreeble", "headphones","largehall", "live", "party", "pop", "reggae",
    "rock", "ska", "soft", "softrock", "techno"
};
static char *preset_list_text[] = {
    N_("Flat"), N_("Classical"), N_("Club"), N_("Dance"), N_("Full bass"),
    N_("Full bass and treeble"), N_("Full treeble"), N_("Headphones"),
    N_("Large Hall"), N_("Live"), N_("Party"), N_("Pop"), N_("Reggae"),
    N_("Rock"), N_("Ska"), N_("Soft"), N_("Soft rock"), N_("Techno"),
};

vlc_module_begin();
    set_description( _("Equalizer 10 bands") );
    set_capability( "audio filter", 0 );
    add_string( "equalizer-preset", "flat", NULL, PRESET_TEXT,
                PRESET_LONGTEXT, VLC_TRUE );
        change_string_list( preset_list, preset_list_text, 0 );
    add_string( "equalizer-bands", NULL, NULL, BANDS_TEXT,
                BANDS_LONGTEXT, VLC_TRUE );
    add_bool( "equalizer-2pass", 0, NULL, TWOPASS_TEXT,
              TWOPASS_LONGTEXT, VLC_TRUE );
    add_float( "equalizer-preamp", 0.0, NULL, PREAMP_TEXT,
               PREAMP_LONGTEXT, VLC_TRUE );
    set_callbacks( Open, Close );
    add_shortcut( "equalizer" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct aout_filter_sys_t
{
    /* Filter static config */
    int i_band;
    float *f_alpha;
    float *f_beta;
    float *f_gamma;

    /* Filter dyn config */
    float *f_amp;   /* Per band amp */
    float f_gamp;   /* Global preamp */
    vlc_bool_t b_2eqz;

    /* Filter state */
    float x[32][2];
    float y[32][128][2];

    /* Second filter state */
    float x2[32][2];
    float y2[32][128][2];

} aout_filter_sys_t;

static void DoWork( aout_instance_t *, aout_filter_t *,
                    aout_buffer_t *, aout_buffer_t * );

#define EQZ_IN_FACTOR (0.25)
static int  EqzInit( aout_filter_t *, int );
static void EqzFilter( aout_filter_t *, float *, float *, int, int );
static void EqzClean( aout_filter_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys;

    if( p_filter->input.i_format != VLC_FOURCC('f','l','3','2' ) ||
        p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        msg_Warn( p_filter, "Bad input or output format" );
        return VLC_EGENERIC;
    }
    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        msg_Warn( p_filter, "input and output formats are not similar" );
        return VLC_EGENERIC;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = VLC_TRUE;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( aout_filter_sys_t ) );

    if( EqzInit( p_filter, p_filter->input.i_rate ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_filter_t     *p_filter = (aout_filter_t *)p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    EqzClean( p_filter );
    free( p_sys );
}

/*****************************************************************************
 * DoWork: process samples buffer
 *****************************************************************************
 *
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;

    EqzFilter( p_filter, (float*)p_out_buf->p_buffer,
               (float*)p_in_buf->p_buffer, p_in_buf->i_nb_samples,
               aout_FormatNbChannels( &p_filter->input ) );
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
    } band[];

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

typedef struct
{
    char *psz_name;
    int  i_band;
    float f_amp[];
} eqz_preset_t;

static const eqz_preset_t eqz_preset_flat_10b=
{
    "flat", 10,
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
};
static const eqz_preset_t eqz_preset_classical_10b=
{
    "classical", 10,
    { -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -7.2, -7.2, -7.2, -9.6 }
};
static const eqz_preset_t eqz_preset_club_10b=
{
    "club", 10,
    { -1.11022e-15, -1.11022e-15, 8, 5.6, 5.6, 5.6, 3.2, -1.11022e-15, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_dance_10b=
{
    "dance", 10,
    { 9.6, 7.2, 2.4, -1.11022e-15, -1.11022e-15, -5.6, -7.2, -7.2, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_fullbass_10b=
{
    "fullbass", 10,
    { -8, 9.6, 9.6, 5.6, 1.6, -4, -8, -10.4, -11.2, -11.2  }
};
static const eqz_preset_t eqz_preset_fullbasstreeble_10b=
{
    "fullbasstreeble", 10,
    { 7.2, 5.6, -1.11022e-15, -7.2, -4.8, 1.6, 8, 11.2, 12, 12 }
};

static const eqz_preset_t eqz_preset_fulltreeble_10b=
{
    "fulltreeble", 10,
    { -9.6, -9.6, -9.6, -4, 2.4, 11.2, 16, 16, 16, 16.8 }
};
static const eqz_preset_t eqz_preset_headphones_10b=
{
    "headphones", 10,
    { 4.8, 11.2, 5.6, -3.2, -2.4, 1.6, 4.8, 9.6, 12.8, 14.4 }
};
static const eqz_preset_t eqz_preset_largehall_10b=
{
    "largehall", 10,
    { 10.4, 10.4, 5.6, 5.6, -1.11022e-15, -4.8, -4.8, -4.8, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_live_10b=
{
    "live", 10,
    { -4.8, -1.11022e-15, 4, 5.6, 5.6, 5.6, 4, 2.4, 2.4, 2.4 }
};
static const eqz_preset_t eqz_preset_party_10b=
{
    "party", 10,
    { 7.2, 7.2, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, 7.2, 7.2 }
};
static const eqz_preset_t eqz_preset_pop_10b=
{
    "pop", 10,
    { -1.6, 4.8, 7.2, 8, 5.6, -1.11022e-15, -2.4, -2.4, -1.6, -1.6 }
};
static const eqz_preset_t eqz_preset_reggae_10b=
{
    "reggae", 10,
    { -1.11022e-15, -1.11022e-15, -1.11022e-15, -5.6, -1.11022e-15, 6.4, 6.4, -1.11022e-15, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_rock_10b=
{
    "rock", 10,
    { 8, 4.8, -5.6, -8, -3.2, 4, 8.8, 11.2, 11.2, 11.2 }
};
static const eqz_preset_t eqz_preset_ska_10b=
{
    "ska", 10,
    { -2.4, -4.8, -4, -1.11022e-15, 4, 5.6, 8.8, 9.6, 11.2, 9.6 }
};
static const eqz_preset_t eqz_preset_soft_10b=
{
    "soft", 10,
    { 4.8, 1.6, -1.11022e-15, -2.4, -1.11022e-15, 4, 8, 9.6, 11.2, 12 }
};
static const eqz_preset_t eqz_preset_softrock_10b=
{
    "softrock", 10,
    { 4, 4, 2.4, -1.11022e-15, -4, -5.6, -3.2, -1.11022e-15, 2.4, 8.8 }
};
static const eqz_preset_t eqz_preset_techno_10b=
{
    "techno", 10,
    { 8, 5.6, -1.11022e-15, -5.6, -4.8, -1.11022e-15, 8, 9.6, 9.6, 8.8 }
};

static const eqz_preset_t *eqz_preset_10b[] =
{
    &eqz_preset_flat_10b,
    &eqz_preset_classical_10b,
    &eqz_preset_club_10b,
    &eqz_preset_dance_10b,
    &eqz_preset_fullbass_10b,
    &eqz_preset_fullbasstreeble_10b,
    &eqz_preset_fulltreeble_10b,
    &eqz_preset_headphones_10b,
    &eqz_preset_largehall_10b,
    &eqz_preset_live_10b,
    &eqz_preset_party_10b,
    &eqz_preset_pop_10b,
    &eqz_preset_reggae_10b,
    &eqz_preset_rock_10b,
    &eqz_preset_ska_10b,
    &eqz_preset_soft_10b,
    &eqz_preset_softrock_10b,
    &eqz_preset_techno_10b,
    NULL
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

static int EqzInit( aout_filter_t *p_filter, int i_rate )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    const eqz_config_t *p_cfg;
    char *psz;
    int i, ch;
    float f_float;

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
        msg_Err( p_filter, "unsupported rate" );
        return VLC_EGENERIC;
    }

    /* Create the static filter config */
    p_sys->i_band = p_cfg->i_band;
    p_sys->f_alpha = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_beta  = malloc( p_sys->i_band * sizeof(float) );
    p_sys->f_gamma = malloc( p_sys->i_band * sizeof(float) );
    for( i = 0; i < p_sys->i_band; i++ )
    {
        p_sys->f_alpha[i] = p_cfg->band[i].f_alpha;
        p_sys->f_beta[i]  = p_cfg->band[i].f_beta;
        p_sys->f_gamma[i] = p_cfg->band[i].f_gamma;
    }

    /* Filter dyn config */
    p_sys->b_2eqz = VLC_FALSE;
    p_sys->f_gamp = 1.0;
    p_sys->f_amp   = malloc( p_sys->i_band * sizeof(float) );
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

    /* Now parse config */
    p_sys->b_2eqz = var_CreateGetBool( p_filter, "equalizer-2pass" );
    f_float = var_CreateGetFloat( p_filter, "equalizer-preamp" );
    if( f_float < -20.0 )
        f_float = -20.0;
    else if( f_float > 20.0 )
        f_float = 20.0;
    p_sys->f_gamp = pow( 10, f_float /20.0);

    psz = var_CreateGetString( p_filter, "equalizer-preset" );
    if( *psz && p_sys->i_band == 10 )
    {
        int i;
        /* */
        for( i = 0; eqz_preset_10b[i] != NULL; i++ )
        {
            if( !strcasecmp( eqz_preset_10b[i]->psz_name, psz ) )
            {
                int j;
                for( j = 0; j < p_sys->i_band; j++ )
                    p_sys->f_amp[j] = EqzConvertdB( eqz_preset_10b[i]->f_amp[j] );
                break;
            }
        }
        if( eqz_preset_10b[i] == NULL )
        {
            msg_Err( p_filter, "equalizer preset '%s' not found", psz );
            msg_Dbg( p_filter, "full list:" );
            for( i = 0; eqz_preset_10b[i] != NULL; i++ )
                msg_Dbg( p_filter, "  - '%s'", eqz_preset_10b[i]->psz_name );
        }
    }
    free( psz );

    psz = var_CreateGetString( p_filter, "equalizer-bands" );
    if( *psz )
    {
        char *p = psz;
        int i;
        for( i = 0; i < p_sys->i_band; i++ )
        {
            float f;

            /* Read dB -20/20*/
            f = strtof( p, &p );

            p_sys->f_amp[i] = EqzConvertdB( f );

            if( p == NULL )
                break;
            p++;
            if( *p == '\0' )
                break;
        }
    }
    free( psz );

    msg_Dbg( p_filter, "equalizer loaded for %d Hz with %d bands %d pass",
             i_rate, p_sys->i_band, p_sys->b_2eqz ? 2 : 1 );
    for( i = 0; i < p_sys->i_band; i++ )
    {
        msg_Dbg( p_filter, "   %d Hz -> factor:%f alpha:%f beta:%f gamma:%f",
                 (int)p_cfg->band[i].f_frequency, p_sys->f_amp[i],
                 p_sys->f_alpha[i], p_sys->f_beta[i], p_sys->f_gamma[i]);
    }
    return VLC_SUCCESS;
}

static void EqzFilter( aout_filter_t *p_filter, float *out, float *in,
                       int i_samples, int i_channels )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;
    int i, ch, j;

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
}

static void EqzClean( aout_filter_t *p_filter )
{
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys->f_alpha );
    free( p_sys->f_beta );
    free( p_sys->f_gamma );

    free( p_sys->f_amp );
}
