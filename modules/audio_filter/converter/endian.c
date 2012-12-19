/*****************************************************************************
 * endian.c : PCM endian converter
 *****************************************************************************
 * Copyright (C) 2002-2005 VLC authors and VideoLAN
 * Copyright (C) 2010 Laurent Aimar
 * Copyright (C) 2012 Rémi Denis-Courmont
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_filter.h>

static int  Open(vlc_object_t *);

vlc_module_begin()
    set_description(N_("Audio filter for endian conversion"))
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_MISC)
    set_capability("audio converter", 2)
    set_callbacks(Open, NULL)
vlc_module_end()

static const vlc_fourcc_t list[][2] = {
};

static int Open(vlc_object_t *object)
{
    filter_t *filter = (filter_t *)object;

    const audio_sample_format_t *src = &filter->fmt_in.audio;
    const audio_sample_format_t *dst = &filter->fmt_out.audio;

    if (!AOUT_FMTS_SIMILAR(src, dst))
        return VLC_EGENERIC;

    for (size_t i = 0; i < sizeof (list) / sizeof (list[0]); i++) {
        if (src->i_format == list[i][0]) {
            if (dst->i_format == list[i][1])
                goto ok;
            break;
        }
        if (src->i_format == list[i][1]) {
            if (dst->i_format == list[i][0])
                goto ok;
            break;
        }
    }
    return VLC_EGENERIC;

ok:
    switch (src->i_bitspersample) {
    }

    return VLC_SUCCESS;
}
