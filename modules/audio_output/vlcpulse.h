/**
 * \file vlcpulse.h
 * \brief PulseAudio support library for LibVLC plugins
 */
/****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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

#ifndef VLCPULSE_H
# define VLCPULSE_H 1
# ifdef __cplusplus
extern "C" {
# endif

VLC_API pa_context *vlc_pa_connect (vlc_object_t *obj,
                                    pa_threaded_mainloop **);
VLC_API void vlc_pa_disconnect (vlc_object_t *obj, pa_context *ctx,
                                pa_threaded_mainloop *);

VLC_API void vlc_pa_error (vlc_object_t *, const char *msg, pa_context *);
#define vlc_pa_error(o, m, c) vlc_pa_error(VLC_OBJECT(o), m, c)

VLC_API vlc_tick_t vlc_pa_get_latency (vlc_object_t *, pa_context *, pa_stream *);
#define vlc_pa_get_latency(o, c, s) vlc_pa_get_latency(VLC_OBJECT(o), c, s)

VLC_API void vlc_pa_rttime_free (pa_threaded_mainloop *, pa_time_event *);

# ifdef __cplusplus
}
# endif
#endif
