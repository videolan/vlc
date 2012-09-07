/*****************************************************************************
 * equalizer_presets.h:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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

#ifndef _EQUALIZER_PRESETS_H
#define _EQUALIZER_PRESETS_H 1

/*****************************************************************************
 * Equalizer presets
 *****************************************************************************/
/* Equalizer presets values are in this file instead of equalizer.c, so you can
 * get these values even if the equalizer is not enabled.
 */

#define EQZ_BANDS_MAX 10

/* The frequency tables */
static const float f_vlc_frequency_table_10b[EQZ_BANDS_MAX] =
{
    60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000,
};

static const float f_iso_frequency_table_10b[EQZ_BANDS_MAX] =
{
    31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000,
};

#define NB_PRESETS 18
static const char *const preset_list[NB_PRESETS] = {
    "flat", "classical", "club", "dance", "fullbass", "fullbasstreble",
    "fulltreble", "headphones","largehall", "live", "party", "pop", "reggae",
    "rock", "ska", "soft", "softrock", "techno"
};
static const char *const preset_list_text[NB_PRESETS] = {
    N_("Flat"), N_("Classical"), N_("Club"), N_("Dance"), N_("Full bass"),
    N_("Full bass and treble"), N_("Full treble"), N_("Headphones"),
    N_("Large Hall"), N_("Live"), N_("Party"), N_("Pop"), N_("Reggae"),
    N_("Rock"), N_("Ska"), N_("Soft"), N_("Soft rock"), N_("Techno"),
};

typedef struct
{
    const char psz_name[16];
    int  i_band;
    float f_preamp;
    float f_amp[EQZ_BANDS_MAX];
} eqz_preset_t;

static const eqz_preset_t eqz_preset_10b[NB_PRESETS] =
{
    {
        "flat", 10, 12.0,
        { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    },
    {
        "classical", 10, 12.0,
        { -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15,
          -1.11022e-15, -1.11022e-15, -7.2, -7.2, -7.2, -9.6 }
    },
    {
        "club", 10, 6.0,
        { -1.11022e-15, -1.11022e-15, 8, 5.6, 5.6, 5.6, 3.2, -1.11022e-15,
          -1.11022e-15, -1.11022e-15 }
    },
    {
        "dance", 10, 5.0,
        { 9.6, 7.2, 2.4, -1.11022e-15, -1.11022e-15, -5.6, -7.2, -7.2,
          -1.11022e-15, -1.11022e-15 }
    },
    {
        "fullbass", 10, 5.0,
        { -8, 9.6, 9.6, 5.6, 1.6, -4, -8, -10.4, -11.2, -11.2  }
    },
    {
        "fullbasstreble", 10, 4.0,
        { 7.2, 5.6, -1.11022e-15, -7.2, -4.8, 1.6, 8, 11.2, 12, 12 }
    },
    {
        "fulltreble", 10, 3.0,
        { -9.6, -9.6, -9.6, -4, 2.4, 11.2, 16, 16, 16, 16.8 }
    },
    {
        "headphones", 10, 4.0,
        { 4.8, 11.2, 5.6, -3.2, -2.4, 1.6, 4.8, 9.6, 12.8, 14.4 }
    },
    {
        "largehall", 10, 5.0,
        { 10.4, 10.4, 5.6, 5.6, -1.11022e-15, -4.8, -4.8, -4.8, -1.11022e-15,
          -1.11022e-15 }
    },
    {
        "live", 10, 7.0,
        { -4.8, -1.11022e-15, 4, 5.6, 5.6, 5.6, 4, 2.4, 2.4, 2.4 }
    },
    {
        "party", 10, 6.0,
        { 7.2, 7.2, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15,
          -1.11022e-15, -1.11022e-15, 7.2, 7.2 }
    },
    {
        "pop", 10, 6.0,
        { -1.6, 4.8, 7.2, 8, 5.6, -1.11022e-15, -2.4, -2.4, -1.6, -1.6 }
    },
    {
        "reggae", 10, 8.0,
        { -1.11022e-15, -1.11022e-15, -1.11022e-15, -5.6, -1.11022e-15, 6.4,
          6.4, -1.11022e-15, -1.11022e-15, -1.11022e-15 }
    },
    {
        "rock", 10, 5.0,
        { 8, 4.8, -5.6, -8, -3.2, 4, 8.8, 11.2, 11.2, 11.2 }
    },
    {
        "ska", 10, 6.0,
        { -2.4, -4.8, -4, -1.11022e-15, 4, 5.6, 8.8, 9.6, 11.2, 9.6 }
    },
    {
        "soft", 10, 5.0,
        { 4.8, 1.6, -1.11022e-15, -2.4, -1.11022e-15, 4, 8, 9.6, 11.2, 12 }
    },
    {
        "softrock", 10, 7.0,
        { 4, 4, 2.4, -1.11022e-15, -4, -5.6, -3.2, -1.11022e-15, 2.4, 8.8 }
    },
    {
        "techno", 10, 5.0,
        { 8, 5.6, -1.11022e-15, -5.6, -4.8, -1.11022e-15, 8, 9.6, 9.6, 8.8 }
    },
};

#endif
