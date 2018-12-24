/*****************************************************************************
 * extradata.h: Muxing extradata builder/gatherer
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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

typedef struct mux_extradata_builder_t mux_extradata_builder_t;

enum mux_extradata_type_e
{
    EXTRADATA_ISOBMFF,
};

mux_extradata_builder_t * mux_extradata_builder_New(vlc_fourcc_t, enum mux_extradata_type_e);
void mux_extradata_builder_Delete(mux_extradata_builder_t *);
void mux_extradata_builder_Feed(mux_extradata_builder_t *, const uint8_t *, size_t);
size_t mux_extradata_builder_Get(mux_extradata_builder_t *, const uint8_t **);
