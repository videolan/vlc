/*****************************************************************************
 * vaapi.h: VAAPI helpers for the ffmpeg decoder
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#ifndef _VLC_VAAPI_H
#define _VLC_VAAPI_H 1

typedef struct vlc_va_t vlc_va_t;

vlc_va_t *VaNew( int i_codec_id );
void VaDelete( vlc_va_t *p_va );

void VaVersion( vlc_va_t *p_va, char *psz_version, size_t i_version );

int VaSetup( vlc_va_t *p_va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
             int i_width, int i_height );

int VaExtract( vlc_va_t *p_va, picture_t *p_picture, AVFrame *p_ff );

int VaGrabSurface( vlc_va_t *p_va, AVFrame *p_ff );

void VaUngrabSurface( vlc_va_t *p_va, AVFrame *p_ff );

#endif
