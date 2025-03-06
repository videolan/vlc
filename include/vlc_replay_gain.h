/*****************************************************************************
 * vlc_replay_gain.h : common replay gain code
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

#ifndef VLC_REPLAY_GAIN_H
#define VLC_REPLAY_GAIN_H 1

#include <vlc_common.h>

/**
 * \file vlc_replay_gain.h
 * \defgroup replay_gain Replay Gain
 * \ingroup input
 * Functions to read replay gain tags.
 *
 * @{
 */

/** Index for track values */
#define AUDIO_REPLAY_GAIN_TRACK (0)
/** Index for album values */
#define AUDIO_REPLAY_GAIN_ALBUM (1)
/** Number of replay gain types */
#define AUDIO_REPLAY_GAIN_MAX (2)

/**
 * Audio replay gain
 */
typedef struct
{
    bool    pb_reference_loudness;          /**< true if we have the reference loudness */
    float   pf_reference_loudness;          /**< reference loudness in LUFS */
    bool    pb_gain[AUDIO_REPLAY_GAIN_MAX]; /**< true if we have the gain value */
    float   pf_gain[AUDIO_REPLAY_GAIN_MAX]; /**< gain value in dB */
    bool    pb_peak[AUDIO_REPLAY_GAIN_MAX]; /**< true if we have the peak value */
    float   pf_peak[AUDIO_REPLAY_GAIN_MAX]; /**< peak value where 1.0 means full sample value */
} audio_replay_gain_t;

/**
 * Extracts replay gain info from metadata and copies it into a replay gain structure.
 * Supports both capitalized and lowercase metadata tags.
 *
 * \param p_dst Destination replay gain structure to fill
 * \param p_meta Metadata structure to extract values from
 * \return VLC_SUCCESS if either an album or track gain was found,
 *         VLC_EGENERIC if no gain was found,
 *         VLC_EINVAL if either argument is null
 */
VLC_API int vlc_replay_gain_CopyFromMeta( audio_replay_gain_t *p_dst, const vlc_meta_t *p_meta );

/**
 * Calculates the replay gain multiplier according to the Replay Gain 2.0 Specification.
 * User preferences control mode, pre-amp, default gain, and peak protection.
 *
 * \param obj calling vlc object
 * \param p_rg replay gain structure
 * \return linear gain multiplier
 */
float replay_gain_CalcMultiplier( vlc_object_t *obj, const audio_replay_gain_t *p_rg );

/**
 * Merges replay gain structures
 *
 * Only copies gain/peak/reference loudness values that are:
 * - Set in the source
 * - Not set in the destination
 *
 * \param p_dst Destination replay gain structure
 * \param p_src Source replay gain structure
 */
static inline void replay_gain_Merge( audio_replay_gain_t *p_dst, const audio_replay_gain_t *p_src )
{
    if( !p_dst || !p_src )
        return;

    if( !p_dst->pb_reference_loudness && p_src->pb_reference_loudness )
    {
        p_dst->pb_reference_loudness = p_src->pb_reference_loudness;
        p_dst->pf_reference_loudness = p_src->pf_reference_loudness;
    }

    for( size_t i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
    {
        if( !p_dst->pb_gain[i] && p_src->pb_gain[i] )
        {
            p_dst->pb_gain[i] = p_src->pb_gain[i];
            p_dst->pf_gain[i] = p_src->pf_gain[i];
        }
        if( !p_dst->pb_peak[i] && p_src->pb_peak[i] )
        {
            p_dst->pb_peak[i] = p_src->pb_peak[i];
            p_dst->pf_peak[i] = p_src->pf_peak[i];
        }
    }
}

/**
 * Compares two replay gain structures
 *
 * \param p_a First replay gain structure
 * \param p_b Second replay gain structure
 * \return true if any gain/peak/reference loudness values or their validity flags differ
 */
static inline bool replay_gain_Compare( const audio_replay_gain_t *p_a, const audio_replay_gain_t *p_b )
{
    if( !p_a || !p_b )
        return true;

    if( p_a->pb_reference_loudness != p_b->pb_reference_loudness ||
        p_a->pf_reference_loudness != p_b->pf_reference_loudness )
        return true;

   for( size_t i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
   {
        if( p_a->pb_gain[i] != p_b->pb_gain[i] ||
            p_a->pb_peak[i] != p_b->pb_peak[i] )
            return true;

        if( ( p_a->pb_gain[i] && p_a->pf_gain[i] != p_b->pf_gain[i] ) ||
            ( p_a->pb_peak[i] && p_a->pf_peak[i] != p_b->pf_peak[i] ) )
            return true;
    }
    return false;
}

/**
 * Reset replay gain structure values
 *
 * \param p_dst Replay gain structure
 */
static inline void replay_gain_Reset( audio_replay_gain_t *p_rg )
{
    if( !p_rg )
        return;

    p_rg->pb_reference_loudness = false;
    p_rg->pf_reference_loudness = 0.f;

    for( size_t i = 0; i < AUDIO_REPLAY_GAIN_MAX; i++ )
    {
        p_rg->pb_gain[i] = false;
        p_rg->pf_gain[i] = 0.f;

        p_rg->pb_peak[i] = false;
        p_rg->pf_peak[i] = 0.f;
    }
}
/** @} */
#endif
