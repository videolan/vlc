/*****************************************************************************
 * oggseek.h : ogg seeking functions for ogg demuxer vlc
 *****************************************************************************
 * Copyright (C) 2008 - 2010 Gabriel Finch <salsaman@gmail.com>
 *
 * Authors: Gabriel Finch <salsaman@gmail.com>
 * adapted from: http://lives.svn.sourceforge.net/viewvc/lives/trunk/lives-plugins
 * /plugins/decoders/ogg_theora_decoder.c
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#define PAGE_HEADER_BYTES 27

#define OGGSEEK_BYTES_TO_READ 8500

/* index entries are structured as follows:
 *   - for theora, highest granulepos -> pagepos (bytes) where keyframe begins
 *  - for dirac, kframe (sync point) -> pagepos of sequence start (?)
 */

/* this is typedefed to demux_index_entry_t in ogg.h */
struct oggseek_index_entry
{
    demux_index_entry_t *p_next;
    demux_index_entry_t *p_prev;

    /* value is highest granulepos for theora, sync frame for dirac */
    int64_t i_value;
    int64_t i_pagepos;

    /* not used for theora because the granulepos tells us this */
    int64_t i_pagepos_end;
};

const demux_index_entry_t *oggseek_theora_index_entry_add ( logical_stream_t *,
                                                            int64_t i_granule,
                                                            int64_t i_pagepos );

void oggseek_index_entries_free ( demux_index_entry_t * );

int64_t oggseek_get_last_frame ( demux_t *, logical_stream_t *);

int oggseek_find_frame ( demux_t *, logical_stream_t *, int64_t i_tframe );

int64_t oggseek_read_page ( demux_t * );
