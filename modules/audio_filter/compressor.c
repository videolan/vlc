/*****************************************************************************
 * compressor.c: dynamic range compressor, ported from plugins from LADSPA SWH
 *****************************************************************************
 * Copyright (C) 2010 Ronald Wright
 *
 * Author: Ronald Wright <logiconcepts819@gmail.com>
 * Original author: Steve Harris <steve@plugin.org.uk>
 *
 * Modified by Brandon Li <brandonli2006ma@gmail.com>, 2026
 * - Renamed file from compressor.c to dynamics.c
 * - Turned into shared static library for other audio modules
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "dynamics.h"

static float CompressorGain( float f_env,
                             float f_threshold, float f_knee, float f_rs,
                             float f_kn_lo, float f_kn_hi,
                             filter_sys_t *p_sys )
{
    if( f_env <= f_kn_lo )
        return 1.0f;

    if( f_env < f_kn_hi )
    {
        const float f_x = -( f_threshold - f_knee - vlc_dynamics_Lin2Db( f_env, p_sys ) ) / f_knee;
        return vlc_dynamics_Db2Lin( -f_knee * f_rs * f_x * f_x * 0.25f, p_sys );
    }

    return vlc_dynamics_Db2Lin( ( f_threshold - vlc_dynamics_Lin2Db( f_env, p_sys ) ) * f_rs, p_sys );
}

static const vlc_dynamics_varnames_t compressor_varnames = {
    .rms_peak    = "compressor-rms-peak",
    .attack      = "compressor-attack",
    .release     = "compressor-release",
    .threshold   = "compressor-threshold",
    .ratio       = "compressor-ratio",
    .knee        = "compressor-knee",
    .makeup_gain = "compressor-makeup-gain",
};

static int Open( vlc_object_t *p_this )
{
    return vlc_dynamics_OpenCommon( (filter_t*)p_this, &compressor_varnames, CompressorGain, -30.0f );
}

vlc_module_begin()
    set_shortname( N_("Compressor") )
    set_description( N_("Dynamic range compressor") )
    set_capability( "audio filter", 0 )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    add_float_with_range( compressor_varnames.rms_peak, 0.2, 0.0, 1.0, RMS_PEAK_TEXT, RMS_PEAK_LONGTEXT )
    add_float_with_range( compressor_varnames.attack, 25.0, 1.5, 400.0, ATTACK_TEXT, ATTACK_LONGTEXT )
    add_float_with_range( compressor_varnames.release, 100.0, 2.0, 800.0, RELEASE_TEXT, RELEASE_LONGTEXT )
    add_float_with_range( compressor_varnames.threshold, -11.0, -30.0, 0.0, THRESHOLD_TEXT, THRESHOLD_LONGTEXT )
    add_float_with_range( compressor_varnames.ratio, 4.0, 1.0, 20.0, RATIO_TEXT, RATIO_LONGTEXT )
    add_float_with_range( compressor_varnames.knee, 5.0, 1.0, 10.0, KNEE_TEXT, KNEE_LONGTEXT )
    add_float_with_range( compressor_varnames.makeup_gain, 7.0, 0.0,  24.0, MAKEUP_GAIN_TEXT, MAKEUP_GAIN_LONGTEXT )
    set_callback( Open )
    add_shortcut( "compressor" )
vlc_module_end()
