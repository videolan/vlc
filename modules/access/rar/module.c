/*****************************************************************************
 * stream.c: uncompressed RAR stream filter
 *****************************************************************************
 * Copyright (C) 2008-2010 Laurent Aimar
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "rar.h"

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_description(N_("Uncompressed RAR"))
    set_capability("access", 0)
    set_callbacks(RarAccessOpen, RarAccessClose)
    add_submodule()
    set_capability("stream_filter", 15)
    set_callbacks(RarStreamOpen, RarStreamClose)
vlc_module_end()
