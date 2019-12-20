/*****************************************************************************
 * dmxmus.c : DMX audio music (.mus) demux module for vlc
 *****************************************************************************
 * Copyright © 2019 Rémi Denis-Courmont
 *
 * Perusing documentation by Vladimir Arnost and Adam Nielsen.
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
# include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

typedef struct
{
    es_out_id_t *es;
    date_t pts; /*< Play timestamp */
    vlc_tick_t tick; /*< Last tick timestamp */
    unsigned primaries:4;
    unsigned secondaries:3;
    unsigned end_offset:17;
    unsigned char last_volume[16];
} demux_sys_t;

static block_t *Event2(uint8_t type, uint8_t channel, uint8_t data)
{
    block_t *ev = block_Alloc(2);
    if (likely(ev != NULL)) {
        ev->p_buffer[0] = type | channel;
        ev->p_buffer[1] = data;
    }
    return ev;
}

static block_t *Event3(uint8_t type, uint8_t channel,
                       uint8_t data, uint8_t data2)
{
    block_t *ev = block_Alloc(3);
    if (likely(ev != NULL)) {
        ev->p_buffer[0] = type | channel;
        ev->p_buffer[1] = data;
        ev->p_buffer[2] = data2;
    }
    return ev;
}

static block_t *HandleControl(demux_t *demux, uint8_t channel, uint8_t num)
{
    block_t *ev = NULL;

    switch (num & 0x7f) {
        case 10: /* sound off */
            return Event3(0xB0, channel, 120, 0);

        case 11: /* notes off */
            return Event3(0xB0, channel, 123, 0);

        case 12:
        case 13:
            break; /* only meaningful for OPL3, not soft synth */

        case 14: /* reset all */
            return Event3(0xB0, channel, 121, 0);

        case 15: /* dummy */
            break;

        default:
            msg_Warn(demux, "unknown control %u", num & 0x7f);
    }

    return ev;
}

static block_t *HandleControlValue(demux_t *demux, uint8_t channel,
                                   uint8_t num, uint8_t val)
{
    val &= 0x7f;

    switch (num & 0x7f) {
        case 0: /* change program */
            return Event2(0xC0, channel, val);

        case 2: /* modulation */
            return Event3(0xB0, channel, 1, val);

        case 3: /* volume */
            return Event3(0xB0, channel, 7, val);

        case 4: /* pan */
            return Event3(0xB0, channel, 10, val);

        case 5: /* expression */
            return Event3(0xB0, channel, 11, val);

        case 6: /* reverberation */
            return Event3(0xB0, channel, 91, val);

        case 7: /* chorus */
            return Event3(0xB0, channel, 93, val);

        case 8: /* pedal hold */
            return Event3(0xB0, channel, 64, val);

        case 9: /* pedal soft */
            return Event3(0xB0, channel, 67, val);

        case 1:
        default:
            return HandleControl(demux, channel, num);
    }
}

static int Demux(demux_t *demux)
{
    stream_t *stream = demux->s;
    demux_sys_t *sys = demux->p_sys;

    if (vlc_stream_Tell(stream) >= sys->end_offset)
        return VLC_DEMUXER_EOF;
    /* We might overflow the end offset by 2 bytes. Not really a problem. */

    /* Inject the MIDI Tick every 10 ms */
    if (sys->tick < date_Get(&sys->pts)) {
        block_t *tick = block_Alloc(1);
        if (unlikely(tick == NULL))
            return VLC_ENOMEM;

        tick->p_buffer[0] = 0xF9;
        tick->i_dts = tick->i_pts = sys->tick;

        es_out_Send(demux->out, sys->es, tick);
        es_out_SetPCR(demux->out, sys->tick);

        sys->tick += VLC_TICK_FROM_MS(10);
        return VLC_DEMUXER_SUCCESS;
    }

    /* Read one MIDI event */
    block_t *ev = NULL;
    unsigned char byte, data;

    if (vlc_stream_Read(stream, &byte, 1) < 1)
        return VLC_DEMUXER_EGENERIC;

    uint_fast8_t type = (byte >> 4) & 0x7;
    uint_fast8_t channel = byte & 0xf;

    if (type != 5 && type != 6 && vlc_stream_Read(stream, &data, 1) < 1)
        return VLC_DEMUXER_EGENERIC;

    if (channel >= ((channel < 10) ? sys->primaries : (10 + sys->secondaries)))
        channel = 9;

    switch (type) {
        case 0: /* release note */
            ev = Event2(0x80, channel, data & 0x7f);
            break;

        case 1: { /* play note */
            uint8_t vol;

            if (data & 0x80) {
                if (vlc_stream_Read(stream, &vol, 1) < 1)
                    return VLC_DEMUXER_EGENERIC;

                vol &= 0x7f;
                sys->last_volume[channel] = vol;
            } else
                vol = sys->last_volume[channel];

            ev = Event3(0x90, channel, data & 0x7f, vol);
            break;
        }

        case 2: /* pitch bend */
            ev = Event3(0xE0, channel, (data << 6) & 0x7f, (data >> 1) & 0x7f);
            break;

        case 3: /* control w/o value */
            ev = HandleControl(demux, channel, data);
            break;

        case 4: { /* control w/ value */
            uint8_t val;

            if (vlc_stream_Read(stream, &val, 1) < 1)
                return VLC_DEMUXER_EGENERIC;

            ev = HandleControlValue(demux, channel, data, val);
            break;
        }

        case 5: /* end of measure */
            break;

        case 6: /* end of track */
            return VLC_DEMUXER_EOF;

        case 7: /* dummy */
            break;

        default:
            vlc_assert_unreachable();
    }

    if (ev != NULL) {
        ev->i_pts = ev->i_dts = date_Get(&sys->pts);
        es_out_Send(demux->out, sys->es, ev);
    }

    /* Compute delay until next event */
    uint32_t delay = 0;

    while (byte & 0x80) {
        if (vlc_stream_Read(stream, &byte, 1) < 1)
            return VLC_DEMUXER_EGENERIC;

        delay <<= 7;
        delay |= byte & 0x7f;
    }

    date_Increment(&sys->pts, delay);
    return VLC_DEMUXER_SUCCESS;
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query) {
        case DEMUX_CAN_SEEK:
            *va_arg(args, bool *) = false; /* TODO */
            break;

        case DEMUX_GET_POSITION:
        case DEMUX_SET_POSITION:
        case DEMUX_GET_LENGTH:
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            *va_arg(args, vlc_tick_t *) = date_Get(&sys->pts) - VLC_TICK_0;
            break;

        case DEMUX_SET_TIME:
            return VLC_EGENERIC;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper(demux->s, 0, -1, 0, 1, query, args);

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    stream_t *stream = demux->s;
    const uint8_t *hdr;

    if (vlc_stream_Peek(stream, &hdr, 16) < 16)
        return VLC_EGENERIC;
    if (memcmp(hdr, "MUS\x1A", 4))
        return VLC_EGENERIC;

    uint_fast16_t length = GetWLE(hdr + 4);
    uint_fast16_t offset = GetWLE(hdr + 6);
    uint_fast16_t primaries = GetWLE(hdr + 8);
    uint_fast16_t secondaries = GetWLE(hdr + 10);

    if (primaries > 8 || secondaries > 5)
        return VLC_EGENERIC;

    /* Ignore the patch list and jump to the event offset. */
    size_t instc = GetWLE(hdr + 12);
    size_t hdrlen = 16 + 2 * instc;

    if (offset < hdrlen
     || vlc_stream_Read(stream, NULL, offset) < (ssize_t)offset)
        return VLC_EGENERIC;

    msg_Dbg(demux, "MIDI channels: %u primary, %u secondary",
            GetWLE(hdr + 8), GetWLE(hdr + 10));

    demux_sys_t *sys = vlc_obj_malloc(obj, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->end_offset = (uint_fast32_t)offset + (uint_fast32_t)length;
    sys->primaries = primaries;
    sys->secondaries = secondaries;
    memset(sys->last_volume, 0, sizeof (sys->last_volume));

    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MIDI);
    fmt.audio.i_channels = 2;
    fmt.audio.i_rate = 44100;
    sys->es = es_out_Add(demux->out, &fmt);

    date_Init(&sys->pts, 140, 1);
    date_Set(&sys->pts, VLC_TICK_0);
    sys->tick = VLC_TICK_0;

    demux->p_sys = sys;
    demux->pf_demux = Demux;
    demux->pf_control = Control;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_description(N_("DMX music demuxer"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_capability("demux", 20)
    set_callbacks(Open, NULL)
vlc_module_end()
