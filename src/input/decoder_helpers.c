/*****************************************************************************
 * decoder_helpers.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2019 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_codec.h>
#include <vlc_meta.h>
#include <vlc_modules.h>

void decoder_Init( decoder_t *p_dec, const es_format_t *restrict p_fmt )
{
    p_dec->i_extra_picture_buffers = 0;
    p_dec->b_frame_drop_allowed = false;

    p_dec->pf_decode = NULL;
    p_dec->pf_get_cc = NULL;
    p_dec->pf_packetize = NULL;
    p_dec->pf_flush = NULL;
    p_dec->p_module = NULL;

    es_format_Copy( &p_dec->fmt_in, p_fmt );
    es_format_Init( &p_dec->fmt_out, p_fmt->i_cat, 0 );
}

void decoder_Clean( decoder_t *p_dec )
{
    es_format_Clean( &p_dec->fmt_in );
    es_format_Clean( &p_dec->fmt_out );

    if ( p_dec->p_description )
    {
        vlc_meta_Delete(p_dec->p_description);
        p_dec->p_description = NULL;
    }
    if ( p_dec->p_module != NULL )
    {
        module_unneed(p_dec, p_dec->p_module);
        p_dec->p_module = NULL;
    }
}

void decoder_Destroy( decoder_t *p_dec )
{
    if (p_dec != NULL)
    {
        decoder_Clean( p_dec );
        vlc_object_release( p_dec );
    }
}
