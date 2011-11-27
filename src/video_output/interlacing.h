/*****************************************************************************
 * interlacing.h: Interlacing related helpers
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef LIBVLC_VOUT_INTERLACING_H
#define LIBVLC_VOUT_INTERLACING_H

typedef struct {
    bool    is_interlaced;
    mtime_t date;
} vout_interlacing_support_t;

void vout_InitInterlacingSupport(vout_thread_t *, bool is_interlaced);
void vout_SetInterlacingState(vout_thread_t *, vout_interlacing_support_t *, bool is_interlaced);

#endif
