/*****************************************************************************
 * srt_common.h: SRT (Secure Reliable Transport) access module
 *****************************************************************************
 *
 * Copyright (C) 2019, Haivision Systems Inc.
 *
 * Author: Aaron Boxer <aaron.boxer@collabora.com>
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
#ifndef SRT_COMMON_H
#define SRT_COMMON_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <srt/srt.h>


/* SRT parameter names */
#define SRT_PARAM_LATENCY                     "latency"
#define SRT_PARAM_PASSPHRASE                  "passphrase"
#define SRT_PARAM_PAYLOAD_SIZE                "payload-size"
#define SRT_PARAM_BANDWIDTH_OVERHEAD_LIMIT    "bandwidth-overhead-limit"
#define SRT_PARAM_CHUNK_SIZE                  "chunk-size"
#define SRT_PARAM_POLL_TIMEOUT                "poll-timeout"
#define SRT_PARAM_KEY_LENGTH                  "key-length"


#define SRT_DEFAULT_BANDWIDTH_OVERHEAD_LIMIT 25
/* libsrt defines default packet size as 1316 internally
 * so srt module takes same value. */
#define SRT_DEFAULT_CHUNK_SIZE SRT_LIVE_DEF_PLSIZE
/* libsrt tutorial uses 9000 as a default binding port */
#define SRT_DEFAULT_PORT 9000
/* Minimum/Maximum chunks to allow reading at a time from libsrt */
#define SRT_MIN_CHUNKS_TRYREAD 10
#define SRT_MAX_CHUNKS_TRYREAD 100
/* The default timeout is -1 (infinite) */
#define SRT_DEFAULT_POLL_TIMEOUT -1
/* The default latency which srt library uses internally */
#define SRT_DEFAULT_LATENCY       SRT_LIVE_DEF_LATENCY_MS
#define SRT_DEFAULT_PAYLOAD_SIZE  SRT_LIVE_DEF_PLSIZE
/* Crypto key length in bytes. */
#define SRT_KEY_LENGTH_TEXT N_("Crypto key length in bytes")
#define SRT_DEFAULT_KEY_LENGTH 16
static const int srt_key_lengths[] = { 16, 24, 32, };

extern const char * const srt_key_length_names[];

typedef struct srt_params {
    int latency;
    const char* passphrase;
    int key_length;
    int payload_size;
    int bandwidth_overhead_limit;
} srt_params_t;

bool srt_parse_url(char* url, srt_params_t* params);

int srt_set_socket_option(vlc_object_t *this, const char *srt_param,
        SRTSOCKET u, SRT_SOCKOPT opt, const void *optval, int optlen);

#endif
