/*****************************************************************************
 * window_presets.h : Variable names and strings of FFT window presets
 *****************************************************************************
 * Copyright (C) 2014 Ronald Wright
 *
 * Author: Ronald Wright <logiconcepts819@gmail.com>
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

#ifndef VLC_VISUAL_WINDOW_PRESETS_H_
#define VLC_VISUAL_WINDOW_PRESETS_H_

/* Window functions supported by VLC. These are the typical window types used
 * in spectrum analyzers. */
#define NB_WINDOWS 5
static const char * const window_list[NB_WINDOWS] = {
    "none", "hann", "flattop", "blackmanharris", "kaiser",
};
static const char * const window_list_text[NB_WINDOWS] = {
    N_("None"), N_("Hann"), N_("Flat Top"), N_("Blackman-Harris"), N_("Kaiser"),
};

#endif /* include-guard */
