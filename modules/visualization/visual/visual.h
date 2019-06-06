/*****************************************************************************
 * visual.h : Header for the visualisation system
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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

typedef struct visual_effect_t visual_effect_t;
typedef int (*visual_run_t)(visual_effect_t *, vlc_object_t *,
                            const block_t *, picture_t *);
typedef void (*visual_free_t)(void *);

struct visual_effect_t
{
    visual_run_t pf_run;
    visual_free_t pf_free;
    void *     p_data; /* The effect stores whatever it wants here */
    int        i_width;
    int        i_height;
    int        i_nb_chans;

    /* Channels index */
    int        i_idx_left;
    int        i_idx_right;
};

extern const struct visual_cb_t
{
    char name[16];
    visual_run_t run_cb;
    visual_free_t free_cb;
} effectv[];
extern const unsigned effectc;
