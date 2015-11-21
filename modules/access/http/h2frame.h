/*****************************************************************************
 * h2frame.h: HTTP/2 frame formatting
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdbool.h>
#include <stdint.h>

struct vlc_h2_frame
{
    struct vlc_h2_frame *next;
    uint8_t data[];
};

struct vlc_h2_frame *
vlc_h2_frame_headers(uint_fast32_t stream_id, uint_fast32_t mtu, bool eos,
                     unsigned count, const char *const headers[][2]);
struct vlc_h2_frame *
vlc_h2_frame_data(uint_fast32_t stream_id, const void *buf, size_t len,
                  bool eos);
struct vlc_h2_frame *
vlc_h2_frame_rst_stream(uint_fast32_t stream_id, uint_fast32_t error_code);
struct vlc_h2_frame *vlc_h2_frame_settings(void);
struct vlc_h2_frame *vlc_h2_frame_settings_ack(void);
struct vlc_h2_frame *vlc_h2_frame_ping(uint64_t opaque);
struct vlc_h2_frame *vlc_h2_frame_pong(uint64_t opaque);
struct vlc_h2_frame *
vlc_h2_frame_goaway(uint_fast32_t last_stream_id, uint_fast32_t error_code);
struct vlc_h2_frame *
vlc_h2_frame_window_update(uint_fast32_t stream_id, uint_fast32_t credit);

enum vlc_h2_error {
    VLC_H2_NO_ERROR,
    VLC_H2_PROTOCOL_ERROR,
    VLC_H2_INTERNAL_ERROR,
    VLC_H2_FLOW_CONTROL_ERROR,
    VLC_H2_SETTINGS_TIMEOUT,
    VLC_H2_STREAM_CLOSED,
    VLC_H2_FRAME_SIZE_ERROR,
    VLC_H2_REFUSED_STREAM,
    VLC_H2_CANCEL,
    VLC_H2_COMPRESSION_ERROR,
    VLC_H2_CONNECT_ERROR,
    VLC_H2_ENHANCE_YOUR_CALM,
    VLC_H2_INADEQUATE_SECURITY,
    VLC_H2_HTTP_1_1_REQUIRED,
};

const char *vlc_h2_strerror(uint_fast32_t);

enum vlc_h2_setting {
    VLC_H2_SETTING_HEADER_TABLE_SIZE = 0x0001,
    VLC_H2_SETTING_ENABLE_PUSH,
    VLC_H2_SETTING_MAX_CONCURRENT_STREAMS,
    VLC_H2_SETTING_INITIAL_WINDOW_SIZE,
    VLC_H2_SETTING_MAX_FRAME_SIZE,
    VLC_H2_SETTING_MAX_HEADER_LIST_SIZE,
};

const char *vlc_h2_setting_name(uint_fast16_t);

/* Our settings */
#define VLC_H2_MAX_HEADER_TABLE   4096 /* Header (compression) table size */
#define VLC_H2_MAX_STREAMS           0 /* Concurrent peer-initiated streams */
#define VLC_H2_INIT_WINDOW     1048575 /* Initial congestion window size */
#define VLC_H2_MAX_FRAME       1048576 /* Frame size */
#define VLC_H2_MAX_HEADER_LIST   65536 /* Header (decompressed) list size */

/* Protocol default settings */
#define VLC_H2_DEFAULT_MAX_HEADER_TABLE  4096
#define VLC_H2_DEFAULT_INIT_WINDOW      65535
#define VLC_H2_DEFAULT_MAX_FRAME        16384
