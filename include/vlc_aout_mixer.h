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

//#include <vlc_aout.h>

/* */
typedef struct aout_mixer_sys_t aout_mixer_sys_t;
typedef struct aout_mixer_t aout_mixer_t;

typedef struct {
    aout_fifo_t fifo;
} aout_mixer_input_t;

/** 
 * audio output mixer
 */
struct aout_mixer_t {
    VLC_COMMON_MEMBERS

    /* Module */
    module_t *module;

    /* Mixer format.
     *
     * You cannot modify it.
     */
    audio_sample_format_t fmt;

    /* Array of mixer inputs */
    aout_mixer_input_t    *input;

    /* Mix buffer (mandatory) */
    void (*mix)(aout_mixer_t *, aout_buffer_t *, float);

    /* Private place holder for the aout_mixer_t module (optional)
     *
     * A module is free to use it as it wishes.
     */
    aout_mixer_sys_t *sys;
};

#ifdef __cplusplus
}
#endif

#endif
