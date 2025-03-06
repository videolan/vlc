/*****************************************************************************
 * replay_gain.c : common replay gain code
 *****************************************************************************
 * Copyright © 2002-2004 VLC authors and VideoLAN
 * Copyright © 2011-2012 Rémi Denis-Courmont
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

#include <math.h>

#include <vlc_replay_gain.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_variables.h>

int vlc_replay_gain_CopyFromMeta( audio_replay_gain_t *p_dst, const vlc_meta_t *p_meta )
{
    if( !p_dst || !p_meta )
        return VLC_EINVAL;

    /* replay gain presence flags */
    enum audio_replay_gain_flags {
        TRACK_GAIN = 0x01,
        TRACK_PEAK = 0x02,
        ALBUM_GAIN = 0x04,
        ALBUM_PEAK = 0x08,
        FLAGS_MASK = 0x0F,
        GAINS_MASK = TRACK_GAIN | ALBUM_GAIN,
    };

    static const struct {
        int mode;
        enum audio_replay_gain_flags flag;
        const char *tags[2];
    } rg_meta[4] = {
        { AUDIO_REPLAY_GAIN_TRACK, TRACK_GAIN,
            { "REPLAYGAIN_TRACK_GAIN", "replaygain_track_gain" }
        },
        { AUDIO_REPLAY_GAIN_TRACK, TRACK_PEAK,
            { "REPLAYGAIN_TRACK_PEAK", "replaygain_track_peak" }
        },
        { AUDIO_REPLAY_GAIN_ALBUM, ALBUM_GAIN,
            { "REPLAYGAIN_ALBUM_GAIN", "replaygain_album_gain" }
        },
        { AUDIO_REPLAY_GAIN_ALBUM, ALBUM_PEAK,
            { "REPLAYGAIN_ALBUM_PEAK", "replaygain_album_peak" }
        }
    };

    enum audio_replay_gain_flags found = 0;

    for( size_t i = 0; i < ARRAY_SIZE( rg_meta ) && found != FLAGS_MASK; i++ )
    {
        if( found & rg_meta[i].flag )
            continue;

        for( size_t j = 0; j < ARRAY_SIZE( rg_meta[i].tags ); j++ )
        {
            const char *psz_meta = vlc_meta_GetExtra( p_meta, rg_meta[i].tags[j] );
            if( psz_meta )
            {
                float f_value = vlc_strtof_c( psz_meta, NULL );

                if( rg_meta[i].flag & GAINS_MASK )
                {
                    p_dst->pb_gain[rg_meta[i].mode] = true;
                    p_dst->pf_gain[rg_meta[i].mode] = f_value;
                }
                else
                {
                    p_dst->pb_peak[rg_meta[i].mode] = true;
                    p_dst->pf_peak[rg_meta[i].mode] = f_value;
                }

                found |= rg_meta[i].flag;
                break;
            }
        }
    }

    static const char *rg_loudness[2] = {
        "REPLAYGAIN_REFERENCE_LOUDNESS",
        "replaygain_reference_loudness"
    };
    /* Only look for reference loudness if a track or album gain was found */
    for( size_t i = 0; i < ARRAY_SIZE( rg_loudness ) && ( found & GAINS_MASK ); i++ )
    {
        const char *psz_meta = vlc_meta_GetExtra( p_meta, rg_loudness[i] );
        if( psz_meta )
        {
            p_dst->pb_reference_loudness = true;
            p_dst->pf_reference_loudness = vlc_strtof_c( psz_meta, NULL );
            break;
        }
    }

    /* Success if either a track or album gain was found. Peak defaults to 1.0 when absent */
    return ( found & GAINS_MASK ) ? VLC_SUCCESS : VLC_EGENERIC;
}

float replay_gain_CalcMultiplier( vlc_object_t *p_obj, const audio_replay_gain_t *p_rg )
{
    unsigned mode = AUDIO_REPLAY_GAIN_MAX;

    char *psz_mode = var_InheritString( p_obj, "audio-replay-gain-mode" );
    if( likely(psz_mode != NULL) )
    {   /* Find selected mode */
        if (!strcmp (psz_mode, "track"))
            mode = AUDIO_REPLAY_GAIN_TRACK;
        else if( !strcmp( psz_mode, "album" ) )
            mode = AUDIO_REPLAY_GAIN_ALBUM;
        free( psz_mode );
    }

    /* Command line / configuration gain */
    const float config_gain = var_InheritFloat( p_obj, "gain" );

    if( mode == AUDIO_REPLAY_GAIN_MAX )
        return config_gain;

    const float preamp_gain = var_InheritFloat( p_obj, "audio-replay-gain-preamp" );
    const float default_gain = var_InheritFloat( p_obj, "audio-replay-gain-default" );
    const bool peak_protection = var_InheritBool( p_obj, "audio-replay-gain-peak-protection" );

    /* If the selected mode is not available, prefer the other one */
    if( !p_rg->pb_gain[mode] && p_rg->pb_gain[!mode] )
        mode = !mode;

    float gain;
    if( p_rg->pb_gain[mode] )
    {
        /* replay gain uses -18 LUFS as the reference level */
        const float rg_ref_lufs = -18.f;
        float rg_ref_lufs_delta =   0.f;

        if( p_rg->pb_reference_loudness )
            rg_ref_lufs_delta = rg_ref_lufs - p_rg->pf_reference_loudness;

        gain = p_rg->pf_gain[mode] + preamp_gain + rg_ref_lufs_delta;
        msg_Dbg( p_obj, "replay gain: mode %s, gain %.2f dB, pre-amp %.2f dB, reference loudness %.2f LUFS",
                        mode ? "album" : "track",
                        p_rg->pf_gain[mode],
                        preamp_gain,
                        rg_ref_lufs - rg_ref_lufs_delta );
    }
    else
    {
        gain = default_gain;
        msg_Dbg( p_obj, "replay gain: mode default, gain %.2f dB", gain );
    }

    float multiplier = powf( 10.f, gain / 20.f );

    /* Skip peak protection for default gain case, as the default peak value of 1.0 would limit gains greater than 0 dB */
    if( p_rg->pb_gain[mode] && peak_protection )
    {
        /* Use peak of 1.0 if peak value is missing or invalid */
        float peak = p_rg->pb_peak[mode] && p_rg->pf_peak[mode] > 0.f ? p_rg->pf_peak[mode] : 1.f;
        float peak_limit = 1.f / peak;

        /* To avoid clipping, max gain multiplier must be <= 1.0 / peak
         * e.g.: peak of 0.5 -> max gain +6.02 dB (can double)
         * e.g.: peak of 1.0 -> max gain  0    dB (unity gain)
         * e.g.: peak of 1.5 -> max gain -3.52 dB (reduce by third)
         * e.g.: peak of 2.0 -> max gain -6.02 dB (reduce by half)
         */
        if( multiplier > peak_limit )
        {
            multiplier = peak_limit;
            msg_Dbg( p_obj, "replay gain: peak protection reducing gain from %.2f dB to %.2f dB (peak %.6f)",
                            gain,
                            20.f * log10f( multiplier ),
                            peak );
        }
    }

    /* apply configuration gain */
    multiplier *= config_gain;
    msg_Dbg( p_obj, "replay gain: applying %.2f dB", 20.f * log10f( multiplier ) );

    return multiplier;
}
