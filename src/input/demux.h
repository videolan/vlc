/*****************************************************************************
 * demux.h: Input demux functions
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_INPUT_DEMUX_H
#define LIBVLC_INPUT_DEMUX_H 1

#include <vlc_common.h>
#include <vlc_demux.h>

#include "stream.h"

/* stream_t *s could be null and then it mean a access+demux in one */
demux_t *demux_NewAdvanced( vlc_object_t *p_obj, input_thread_t *p_parent_input,
                            const char *psz_demux, const char *url,
                            stream_t *s, es_out_t *out, bool );

unsigned demux_TestAndClearFlags( demux_t *, unsigned );
int demux_GetTitle( demux_t * );
int demux_GetSeekpoint( demux_t * );

/**
 * Builds an explicit chain of demux filters.
 *
 * This function creates a chain of filters according to a supplied list.
 *
 * See also stream_FilterChainNew(). Those two functions have identical
 * semantics and ownership rules, except for the use of demux vs stream.
 *
 * @param source input stream around which to build a filter chain
 * @param list colon-separated list of stream filters (upstream first)
 *
 * @note Like stream_FilterAutoNew(), this function takes ownership of the
 * source input stream, and transfers it to the first demux filter in the
 * constructed chain. Any use of the source after the function call is invalid
 * and undefined (unless the chain ends up empty).
 *
 * @return The last demux (filter) in the chain.
 * The return value is always a valid (non-NULL) demux pointer.
 */
demux_t *demux_FilterChainNew( demux_t *source, const char *list ) VLC_USED;

bool demux_FilterEnable( demux_t *p_demux_chain, const char* psz_demux );
bool demux_FilterDisable( demux_t *p_demux_chain, const char* psz_demux );

#endif
