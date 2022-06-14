/*
 * media_utils.h - media utils function
 */

/**********************************************************************
 *  Copyright (C) 2021 VLC authors and VideoLAN                       *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

static void
libvlc_media_parse_finished_event(const libvlc_event_t *p_ev, void *p_data)
{
    (void) p_ev;
    vlc_sem_t *p_sem = p_data;
    vlc_sem_post(p_sem);
}

static inline void
libvlc_media_parse_sync(libvlc_instance_t *vlc, libvlc_media_t *p_m,
                        libvlc_media_parse_flag_t parse_flag, int timeout)
{
    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    libvlc_event_manager_t *p_em = libvlc_media_event_manager(p_m);
    libvlc_event_attach(p_em, libvlc_MediaParsedChanged,
                        libvlc_media_parse_finished_event, &sem);

    int i_ret = libvlc_media_parse_request(vlc, p_m, parse_flag, timeout);
    assert(i_ret == 0);

    vlc_sem_wait (&sem);

    libvlc_event_detach(p_em, libvlc_MediaParsedChanged,
                        libvlc_media_parse_finished_event, &sem);
}
