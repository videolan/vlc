/*****************************************************************************
 * hls.c: HLS stream output module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_configuration.h>
#include <vlc_frame.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include "hls.h"

typedef struct
{
    /** All the plugin constants. */
    struct hls_config config;
} sout_stream_sys_t;

static void *Add(sout_stream_t *stream, const es_format_t *fmt, const char *es_id)
{
    return NULL; // TODO: Create HLS Playlists.
    (void)stream; (void)fmt; (void)es_id;
}

static void Del(sout_stream_t *stream, void *id) { (void)stream; (void)id; }

static int Send(sout_stream_t *stream, void *id, vlc_frame_t *frame)
{
    return VLC_ENOTSUP; // TODO: Support once Muxing is implemented.
    (void)stream; (void)id; (void)frame;
}

#define SOUT_CFG_PREFIX "sout-hls-"

static int Open(vlc_object_t *this)
{
    sout_stream_t *stream = (sout_stream_t *)this;

    sout_stream_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    stream->p_sys = sys;

    static const char *const options[] = {
        "base-url", "num-seg", "out-dir", "pace", "seg-len", NULL};
    config_ChainParse(stream, SOUT_CFG_PREFIX, options, stream->p_cfg);

    sys->config.base_url = var_GetString(stream, SOUT_CFG_PREFIX "base-url");
    sys->config.outdir =
        var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "out-dir");
    sys->config.max_segments =
        var_GetInteger(stream, SOUT_CFG_PREFIX "num-seg");
    sys->config.pace = var_GetBool(stream, SOUT_CFG_PREFIX "pace");
    sys->config.segment_length =
        VLC_TICK_FROM_SEC(var_GetInteger(stream, SOUT_CFG_PREFIX "seg-len"));

    static const struct sout_stream_operations ops = {
        .add = Add,
        .del = Del,
        .send = Send,
    };
    stream->ops = &ops;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *this)
{
    sout_stream_t *stream = (sout_stream_t *)this;
    sout_stream_sys_t *sys = stream->p_sys;

    hls_config_Clean(&sys->config);

    free(sys);
}

#define BASEURL_TEXT N_("Base of the URL")
#define NUMSEG_TEXT N_("Number of maximum segment exposed")
#define OUTDIR_TEXT N_("Output directory path")
#define OUTDIR_LONGTEXT                                                        \
    N_("Output directory path. If not specified and HTTP is enabled, the "     \
       "segments will be stored in memory")
#define PACE_LONGTEXT                                                          \
    N_("Enable input pacing, the media will play at playback rate")
#define PACE_TEXT N_("Enable pacing")
#define SEGLEN_LONGTEXT N_("Length of segments in seconds")
#define SEGLEN_TEXT N_("Segment length (sec)")

vlc_module_begin()
    set_shortname("HLS")
    set_description(N_("HLS stream output"))
    set_capability("sout output", 50)
    add_shortcut("hls")
    set_subcategory(SUBCAT_SOUT_STREAM)

    add_string(SOUT_CFG_PREFIX "base-url", "", BASEURL_TEXT, BASEURL_TEXT)
    add_integer(SOUT_CFG_PREFIX "num-seg", 0, NUMSEG_TEXT, NUMSEG_TEXT)
    add_string(SOUT_CFG_PREFIX "out-dir", NULL, OUTDIR_TEXT, OUTDIR_LONGTEXT)
    add_bool(SOUT_CFG_PREFIX "pace", false, PACE_TEXT, PACE_LONGTEXT)
    add_integer(SOUT_CFG_PREFIX "seg-len", 4, SEGLEN_TEXT, SEGLEN_LONGTEXT)

    set_callbacks(Open, Close)
vlc_module_end()
