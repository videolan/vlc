/*****************************************************************************
 * vlm_event.h: VLM event functions
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ fr>
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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _VLM_EVENT_H
#define _VLM_EVENT_H 1

#include <vlc_common.h>

/*****************************************************************************
 *
 *****************************************************************************/
void vlm_SendEventMediaAdded( vlm_t *, int64_t id, const char *psz_name );
void vlm_SendEventMediaRemoved( vlm_t *, int64_t id, const char *psz_name );
void vlm_SendEventMediaChanged( vlm_t *, int64_t id, const char *psz_name );

void vlm_SendEventMediaInstanceStarted( vlm_t *, int64_t id, const char *psz_name );
void vlm_SendEventMediaInstanceStopped( vlm_t *, int64_t id, const char *psz_name );


#endif

