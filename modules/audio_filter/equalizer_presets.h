/*****************************************************************************
 * equalizer_presets.h:
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it it
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
 * along with this program; if not, write to the Free Software Foundation, Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Equalizer presets
 *****************************************************************************/
/* Equalizer presets values are in this file instead of equalizer.c, so you can
 * get these values even if the equalizer is not enabled.
 */

#define EQZ_BANDS_MAX 10

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
        "flat", 10, 12.0f,
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    },
    {
        "classical", 10, 12.0f,
        { -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f,
          -1.11022e-15f, -1.11022e-15f, -7.2f, -7.2f, -7.2f, -9.6f }
    },
    {
        "club", 10, 6.0f,
        { -1.11022e-15f, -1.11022e-15f, 8.0f, 5.6f, 5.6f, 5.6f, 3.2f,
          -1.11022e-15f, -1.11022e-15f, -1.11022e-15f }
    },
    {
        "dance", 10, 5.0f,
        { 9.6f, 7.2f, 2.4f, -1.11022e-15f, -1.11022e-15f, -5.6f, -7.2f, -7.2f,
          -1.11022e-15f, -1.11022e-15f }
    },
    {
        "fullbass", 10, 5.0f,
        { -8.0f, 9.6f, 9.6f, 5.6f, 1.6f, -4.0f, -8.0f, -10.4f, -11.2f, -11.2f }
    },
    {
        "fullbasstreble", 10, 4.0f,
        { 7.2f, 5.6f, -1.11022e-15f, -7.2f, -4.8f, 1.6f, 8.0f, 11.2f,
          12.0f, 12.0f }
    },
    {
        "fulltreble", 10, 3.0f,
        { -9.6f, -9.6f, -9.6f, -4.0f, 2.4f, 11.2f, 16.0f, 16.0f, 16.0f, 16.8f }
    },
    {
        "headphones", 10, 4.0f,
        { 4.8f, 11.2f, 5.6f, -3.2f, -2.4f, 1.6f, 4.8f, 9.6f, 12.8f, 14.4f }
    },
    {
        "largehall", 10, 5.0f,
        { 10.4f, 10.4f, 5.6f, 5.6f, -1.11022e-15f, -4.8f, -4.8f, -4.8f,
          -1.11022e-15f, -1.11022e-15f }
    },
    {
        "live", 10, 7.0f,
        { -4.8f, -1.11022e-15f, 4.0f, 5.6f, 5.6f, 5.6f, 4.0f, 2.4f,
          2.4f, 2.4f }
    },
    {
        "party", 10, 6.0f,
        { 7.2f, 7.2f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f,
          -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, 7.2f, 7.2f }
    },
    {
        "pop", 10, 6.0f,
        { -1.6f, 4.8f, 7.2f, 8.0f, 5.6f, -1.11022e-15f, -2.4f, -2.4f,
          -1.6f, -1.6f }
    },
    {
        "reggae", 10, 8.0f,
        { -1.11022e-15f, -1.11022e-15f, -1.11022e-15f, -5.6f, -1.11022e-15f,
          6.4f, 6.4f, -1.11022e-15f, -1.11022e-15f, -1.11022e-15f }
    },
    {
        "rock", 10, 5.0f,
        { 8.0f, 4.8f, -5.6f, -8.0f, -3.2f, 4.0f, 8.8f, 11.2f, 11.2f, 11.2f }
    },
    {
        "ska", 10, 6.0f,
        { -2.4f, -4.8f, -4.0f, -1.11022e-15f, 4.0f, 5.6f, 8.8f, 9.6f,
          11.2f, 9.6f }
    },
    {
        "soft", 10, 5.0f,
        { 4.8f, 1.6f, -1.11022e-15f, -2.4f, -1.11022e-15f, 4.0f, 8.0f, 9.6f,
          11.2f, 12.0f }
    },
    {
        "softrock", 10, 7.0f,
        { 4.0f, 4.0f, 2.4f, -1.11022e-15f, -4.0f, -5.6f, -3.2f, -1.11022e-15f,
          2.4f, 8.8f }
    },
    {
        "techno", 10, 5.0f,
        { 8.0f, 5.6f, -1.11022e-15f, -5.6f, -4.8f, -1.11022e-15f, 8.0f, 9.6f,
          9.6f, 8.8f }
    },
};
