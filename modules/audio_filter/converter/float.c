/*****************************************************************************
 * float.c: Floating point audio format conversions
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
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

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create_F32ToFL32 ( vlc_object_t * );
static void Do_F32ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void Do_FL32ToF32 ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Floating-point audio format conversions") )
    add_submodule ()
        set_capability( "audio filter", 10 )
        set_callbacks( Create_F32ToFL32, NULL )
vlc_module_end ()

/*****************************************************************************
 * Fixed 32 to Float 32 and backwards
 *****************************************************************************/
static int Create_F32ToFL32( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if( ( p_filter->fmt_in.audio.i_format != VLC_CODEC_FI32
           || p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32 )
      && ( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32
            || p_filter->fmt_out.audio.i_format != VLC_CODEC_FI32 ) )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->fmt_in.audio, &p_filter->fmt_out.audio ) )
    {
        return -1;
    }

    if( p_filter->fmt_in.audio.i_format == VLC_CODEC_FI32 )
    {
        p_filter->pf_do_work = Do_F32ToFL32;
    }
    else
    {
        p_filter->pf_do_work = Do_FL32ToF32;
    }

    p_filter->b_in_place = 1;

    return 0;
}

static void Do_F32ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    float * p_out = (float *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->fmt_in.audio ) ; i-- ; )
    {
        *p_out++ = (float)*p_in++ / (float)FIXED32_ONE;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = p_in_buf->i_buffer;
}

static void Do_FL32ToF32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->fmt_in.audio ) ; i-- ; )
    {
        *p_out++ = (vlc_fixed_t)( *p_in++ * (float)FIXED32_ONE );
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = p_in_buf->i_buffer;
}
