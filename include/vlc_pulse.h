/*****************************************************************************
 * vlcpulse.h : PulseAudio support library for LibVLC plugins
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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

#ifndef VLCPULSE_H
# define VLCPULSE_H 1
# ifdef __cplusplus
extern "C" {
# endif

VLC_API void vlc_pa_lock (void);
VLC_API void vlc_pa_unlock (void);
VLC_API void vlc_pa_signal (int);
VLC_API void vlc_pa_wait (void);

VLC_API pa_context *vlc_pa_connect (vlc_object_t *obj);
VLC_API void vlc_pa_disconnect (vlc_object_t *obj, pa_context *ctx);

VLC_API void vlc_pa_error (vlc_object_t *, const char *msg, pa_context *);
#define vlc_pa_error(o, m, c) vlc_pa_error(VLC_OBJECT(o), m, c)

VLC_API void vlc_pa_rttime_free (pa_time_event *);

# ifdef __cplusplus
}
# endif
#endif
