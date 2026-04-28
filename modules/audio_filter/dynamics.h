/*****************************************************************************
 * dynamics.h: shared declarations for dynamic modules
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

#ifndef VLC_AUDIO_FILTER_DYNAMICS_H_
#define VLC_AUDIO_FILTER_DYNAMICS_H_

#include <vlc_common.h>

#define RMS_PEAK_TEXT        N_( "RMS/peak" )
#define RMS_PEAK_LONGTEXT    N_( "Set the RMS/peak." )
#define ATTACK_TEXT          N_( "Attack time" )
#define ATTACK_LONGTEXT      N_( "Set the attack time in milliseconds." )
#define RELEASE_TEXT         N_( "Release time" )
#define RELEASE_LONGTEXT     N_( "Set the release time in milliseconds." )
#define THRESHOLD_TEXT       N_( "Threshold level" )
#define THRESHOLD_LONGTEXT   N_( "Set the threshold level in dB." )
#define RATIO_TEXT           N_( "Ratio" )
#define RATIO_LONGTEXT       N_( "Set the ratio (n:1)." )
#define KNEE_TEXT            N_( "Knee radius" )
#define KNEE_LONGTEXT        N_( "Set the knee radius in dB." )
#define MAKEUP_GAIN_TEXT     N_( "Makeup gain" )
#define MAKEUP_GAIN_LONGTEXT N_( "Set the makeup gain in dB (0 ... 24)." )

typedef struct filter_sys filter_sys_t;

typedef float (*gain_fn_t)( float f_env,
                            float f_threshold, float f_knee, float f_rs,
                            float f_kn_lo, float f_kn_hi,
                            filter_sys_t *p_sys );

typedef struct vlc_dynamics_varnames
{
    const char *rms_peak;
    const char *attack;
    const char *release;
    const char *threshold;
    const char *ratio;
    const char *knee;
    const char *makeup_gain;
} vlc_dynamics_varnames_t;

/* dB <-> linear conversion using the filter's internal lookup tables. */
float vlc_dynamics_Db2Lin( float f_db,  filter_sys_t *p_sys );
float vlc_dynamics_Lin2Db( float f_lin, filter_sys_t *p_sys );

int vlc_dynamics_OpenCommon( filter_t *p_filter,
                             const vlc_dynamics_varnames_t *p_varnames,
                             gain_fn_t pf_gain, float f_threshold_min );

#endif /* VLC_AUDIO_FILTER_DYNAMICS_H_ */
