/*****************************************************************************
 * float.c: Floating point audio format conversions
 *****************************************************************************
 * Copyright (C) 2002, 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Xavier Maillard <zedek@fxgsproject.org>
 *          Henri Fallon <henri@videolan.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create_F32ToFL32 ( vlc_object_t * );
static block_t *Do_F32ToFL32( filter_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Floating-point audio format conversions") )
    set_capability( "audio filter", 10 )
    set_callbacks( Create_F32ToFL32, NULL )
vlc_module_end ()

/*****************************************************************************
 * Fixed 32 to Float 32 and backwards
 *****************************************************************************/
static int Create_F32ToFL32( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;

    if( p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32
     || !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio,
                            &p_filter->fmt_out.audio ) )
        return VLC_EGENERIC;

    switch( p_filter->fmt_in.audio.i_format )
    {
        case VLC_CODEC_FI32:
            p_filter->pf_audio_filter = Do_F32ToFL32;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static block_t *Do_F32ToFL32( filter_t * p_filter, block_t * p_in_buf )
{
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    float * p_out = (float *)p_in_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->fmt_in.audio ) ; i-- ; )
    {
        *p_out++ = (float)*p_in++ / (float)FIXED32_ONE;
    }
    return p_in_buf;
}


