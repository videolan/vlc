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

/*****************************************************************************
 * Equalizer presets
 *****************************************************************************/
/* Equalizer presets values are in this file instead of equalizer.c, so you can
 * get these values even if the equalizer is not enabled.
 */

#define EQZ_BANDS_MAX 10

#define NB_PRESETS 18
static const char *const preset_list[] = {
    "flat", "classical", "club", "dance", "fullbass", "fullbasstreble",
    "fulltreble", "headphones","largehall", "live", "party", "pop", "reggae",
    "rock", "ska", "soft", "softrock", "techno"
};
static const char *const preset_list_text[] = {
    N_("Flat"), N_("Classical"), N_("Club"), N_("Dance"), N_("Full bass"),
    N_("Full bass and treble"), N_("Full treble"), N_("Headphones"),
    N_("Large Hall"), N_("Live"), N_("Party"), N_("Pop"), N_("Reggae"),
    N_("Rock"), N_("Ska"), N_("Soft"), N_("Soft rock"), N_("Techno"),
};

typedef struct
{
    const char *psz_name;
    int  i_band;
    float f_preamp;
    float f_amp[EQZ_BANDS_MAX];
} eqz_preset_t;

static const eqz_preset_t eqz_preset_flat_10b=
{
    "flat", 10, 12.0,
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
};
static const eqz_preset_t eqz_preset_classical_10b=
{
    "classical", 10, 12.0,
    { -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -7.2, -7.2, -7.2, -9.6 }
};
static const eqz_preset_t eqz_preset_club_10b=
{
    "club", 10, 6.0,
    { -1.11022e-15, -1.11022e-15, 8, 5.6, 5.6, 5.6, 3.2, -1.11022e-15, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_dance_10b=
{
    "dance", 10, 5.0,
    { 9.6, 7.2, 2.4, -1.11022e-15, -1.11022e-15, -5.6, -7.2, -7.2, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_fullbass_10b=
{
    "fullbass", 10, 5.0,
    { -8, 9.6, 9.6, 5.6, 1.6, -4, -8, -10.4, -11.2, -11.2  }
};
static const eqz_preset_t eqz_preset_fullbasstreble_10b=
{
    "fullbasstreble", 10, 4.0,
    { 7.2, 5.6, -1.11022e-15, -7.2, -4.8, 1.6, 8, 11.2, 12, 12 }
};

static const eqz_preset_t eqz_preset_fulltreble_10b=
{
    "fulltreble", 10, 3.0,
    { -9.6, -9.6, -9.6, -4, 2.4, 11.2, 16, 16, 16, 16.8 }
};
static const eqz_preset_t eqz_preset_headphones_10b=
{
    "headphones", 10, 4.0,
    { 4.8, 11.2, 5.6, -3.2, -2.4, 1.6, 4.8, 9.6, 12.8, 14.4 }
};
static const eqz_preset_t eqz_preset_largehall_10b=
{
    "largehall", 10, 5.0,
    { 10.4, 10.4, 5.6, 5.6, -1.11022e-15, -4.8, -4.8, -4.8, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_live_10b=
{
    "live", 10, 7.0,
    { -4.8, -1.11022e-15, 4, 5.6, 5.6, 5.6, 4, 2.4, 2.4, 2.4 }
};
static const eqz_preset_t eqz_preset_party_10b=
{
    "party", 10, 6.0,
    { 7.2, 7.2, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, -1.11022e-15, 7.2, 7.2 }
};
static const eqz_preset_t eqz_preset_pop_10b=
{
    "pop", 10, 6.0,
    { -1.6, 4.8, 7.2, 8, 5.6, -1.11022e-15, -2.4, -2.4, -1.6, -1.6 }
};
static const eqz_preset_t eqz_preset_reggae_10b=
{
    "reggae", 10, 8.0,
    { -1.11022e-15, -1.11022e-15, -1.11022e-15, -5.6, -1.11022e-15, 6.4, 6.4, -1.11022e-15, -1.11022e-15, -1.11022e-15 }
};
static const eqz_preset_t eqz_preset_rock_10b=
{
    "rock", 10, 5.0,
    { 8, 4.8, -5.6, -8, -3.2, 4, 8.8, 11.2, 11.2, 11.2 }
};
static const eqz_preset_t eqz_preset_ska_10b=
{
    "ska", 10, 6.0,
    { -2.4, -4.8, -4, -1.11022e-15, 4, 5.6, 8.8, 9.6, 11.2, 9.6 }
};
static const eqz_preset_t eqz_preset_soft_10b=
{
    "soft", 10, 5.0,
    { 4.8, 1.6, -1.11022e-15, -2.4, -1.11022e-15, 4, 8, 9.6, 11.2, 12 }
};
static const eqz_preset_t eqz_preset_softrock_10b=
{
    "softrock", 10, 7.0,
    { 4, 4, 2.4, -1.11022e-15, -4, -5.6, -3.2, -1.11022e-15, 2.4, 8.8 }
};
static const eqz_preset_t eqz_preset_techno_10b=
{
    "techno", 10, 5.0,
    { 8, 5.6, -1.11022e-15, -5.6, -4.8, -1.11022e-15, 8, 9.6, 9.6, 8.8 }
};

static const eqz_preset_t *eqz_preset_10b[] =
{
    &eqz_preset_flat_10b,
    &eqz_preset_classical_10b,
    &eqz_preset_club_10b,
    &eqz_preset_dance_10b,
    &eqz_preset_fullbass_10b,
    &eqz_preset_fullbasstreble_10b,
    &eqz_preset_fulltreble_10b,
    &eqz_preset_headphones_10b,
    &eqz_preset_largehall_10b,
    &eqz_preset_live_10b,
    &eqz_preset_party_10b,
    &eqz_preset_pop_10b,
    &eqz_preset_reggae_10b,
    &eqz_preset_rock_10b,
    &eqz_preset_ska_10b,
    &eqz_preset_soft_10b,
    &eqz_preset_softrock_10b,
    &eqz_preset_techno_10b,
    NULL
};



