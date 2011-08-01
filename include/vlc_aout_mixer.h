/*****************************************************************************
 * vlc_aout_mixer.h : audio output mixer interface
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_AOUT_MIXER_H
#define VLC_AOUT_MIXER_H 1

/**
 * \file
 * This file defines functions, structures and macros for audio output mixer object
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_mixer audio_mixer_t;

/** 
 * audio output mixer
 */
struct audio_mixer
{
    VLC_COMMON_MEMBERS

    module_t *module; /**< Module handle */
    const audio_sample_format_t *fmt; /**< Audio format */
    void (*mix)(audio_mixer_t *, block_t *, float); /**< Amplifier */
};

#ifdef __cplusplus
}
#endif

#endif
