/*****************************************************************************
 * keythread.h:
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

typedef struct key_thread key_thread_t;

key_thread_t *vlc_CreateKeyThread (vlc_object_t *obj);
#define vlc_CreateKeyThread(o) vlc_CreateKeyThread(VLC_OBJECT(o))
void vlc_DestroyKeyThread (key_thread_t *keys);
void vlc_EmitKey (key_thread_t *keys, int value);
