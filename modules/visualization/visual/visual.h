/*****************************************************************************
 * visual.h : Header for the visualisation system
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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

typedef struct visual_effect_t
{
    const char *psz_name;    /* Filter name*/

    int        (*pf_run)( struct visual_effect_t * , vlc_object_t *,
                          const block_t *, picture_t *);
    void *     p_data; /* The effect stores whatever it wants here */
    int        i_width;
    int        i_height;
    char *     psz_args;
    int        i_nb_chans;

    /* Channels index */
    int        i_idx_left;
    int        i_idx_right;
} visual_effect_t ;

typedef struct spectrum_data
{
    int *peaks;
    int *prev_heights;

    unsigned i_prev_nb_samples;
    int16_t *p_prev_s16_buff;
} spectrum_data;

typedef struct
{
    int *peaks;

    unsigned i_prev_nb_samples;
    int16_t *p_prev_s16_buff;
} spectrometer_data;

/*****************************************************************************
 * aout_filter_sys_t: visualizer audio filter method descriptor
 *****************************************************************************
 * This structure is part of the audio filter descriptor.
 * It describes some visualizer specific variables.
 *****************************************************************************/
struct filter_sys_t
{
    vout_thread_t   *p_vout;

    int             i_width;
    int             i_height;

    int             i_effect;
    visual_effect_t **effect;
};

/* Prototypes */
int scope_Run
        (visual_effect_t * , vlc_object_t *, const block_t *, picture_t *);
int vuMeter_Run
        (visual_effect_t * , vlc_object_t *, const block_t *, picture_t *);
int dummy_Run
        (visual_effect_t * , vlc_object_t *, const block_t *, picture_t *);
int random_Run
        (visual_effect_t * , vlc_object_t *, const block_t *, picture_t *);
int spectrum_Run
        (visual_effect_t * , vlc_object_t *, const block_t *, picture_t *);
int spectrometer_Run
        (visual_effect_t * , vlc_object_t *, const block_t *, picture_t *);

/* Default vout size */
#define VOUT_WIDTH  800
#define VOUT_HEIGHT 500
