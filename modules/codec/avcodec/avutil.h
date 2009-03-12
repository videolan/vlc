/*****************************************************************************
 * avutil.h: avutil helper functions
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Export libavutil messages to the VLC message system
 *****************************************************************************/
static inline void LibavutilCallback( void *p_opaque, int i_level,
                        const char *psz_format, va_list va )
{
    AVCodecContext *p_avctx = (AVCodecContext *)p_opaque;
    const AVClass *p_avc;

    p_avc = p_avctx ? p_avctx->av_class : 0;

#define cln p_avc->class_name
    /* Make sure we can get p_this back */
    if( !p_avctx || !p_avc || !cln ||
        cln[0]!='A' || cln[1]!='V' || cln[2]!='C' || cln[3]!='o' ||
        cln[4]!='d' || cln[5]!='e' || cln[6]!='c' )
    {
        if( i_level == AV_LOG_ERROR ) vfprintf( stderr, psz_format, va );
        return;
    }
#undef cln

    switch( i_level )
    {
    case AV_LOG_DEBUG:
    case AV_LOG_INFO:
        /* Print debug messages if they were requested */
        if( !p_avctx->debug )
            break;

    case AV_LOG_ERROR:
    case AV_LOG_QUIET:
        vfprintf( stderr, psz_format, va );
        break;
    }
}
