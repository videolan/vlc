/*****************************************************************************
 * smf.c : Standard MIDI File (.mid) demux module for vlc
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_charset.h>
#include <limits.h>

#include <assert.h>

#define TEMPO_MIN  20
#define TEMPO_MAX 250 /* Beats per minute */

/**
 * Reads MIDI variable length (7, 14, 21 or 28 bits) integer.
 * @return read value, or -1 on EOF/error.
 */
static int32_t ReadVarInt (stream_t *s)
{
    uint32_t val = 0;
    uint8_t byte;

    for (unsigned i = 0; i < 4; i++)
    {
        if (vlc_stream_Read (s, &byte, 1) < 1)
            return -1;

        val = (val << 7) | (byte & 0x7f);
        if ((byte & 0x80) == 0)
            return val;
    }

    return -1;
}

typedef struct smf_track_t
{
    uint64_t next;   /*< Time of next message (in term of pulses) */
    uint64_t start;  /*< Start offset in the file */
    uint32_t length; /*< Bytes length */
    uint32_t offset; /*< Read offset relative to the start offset */
    uint8_t  running_event; /*< Running (previous) event */
} mtrk_t;

/**
 * Reads (delta) time from the next event of a given track.
 * @param s stream to read data from (must be positioned at the right offset)
 */
static int ReadDeltaTime (stream_t *s, mtrk_t *track)
{
    int32_t delta_time;

    assert (vlc_stream_Tell (s) == track->start + track->offset);

    if (track->offset >= track->length)
    {
        /* This track is done */
        track->next = UINT64_MAX;
        return 0;
    }

    delta_time = ReadVarInt (s);
    if (delta_time < 0)
        return -1;

    track->next += delta_time;
    track->offset = vlc_stream_Tell (s) - track->start;
    return 0;
}

typedef struct
{
    es_out_id_t *es;
    date_t       pts; /*< Play timestamp */
    uint64_t     pulse; /*< Pulses counter */
    vlc_tick_t   tick; /*< Last tick timestamp */

    vlc_tick_t   duration; /*< Total duration */
    unsigned     ppqn;   /*< Pulses Per Quarter Note */
    /* by the way, "quarter note" is "noire" in French */

    unsigned     trackc; /*< Number of tracks */
    mtrk_t       trackv[]; /*< Track states */
} demux_sys_t;

/**
 * Non-MIDI Meta events handler
 */
static
int HandleMeta (demux_t *p_demux, mtrk_t *tr)
{
    stream_t *s = p_demux->s;
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t *payload;
    uint8_t type;
    int32_t length;
    int ret = 0;

    if (vlc_stream_Read (s, &type, 1) != 1)
        return -1;

    length = ReadVarInt (s);
    if (length < 0)
        return -1;

    payload = malloc (length + 1);
    if ((payload == NULL)
     || (vlc_stream_Read (s, payload, length) != length))
    {
        free (payload);
        return -1;
    }

    payload[length] = '\0';

    switch (type)
    {
        case 0x00: /* Sequence Number */
            break;

        case 0x01: /* Text (comment) */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Text      : %s", (char *)payload);
            break;

        case 0x02: /* Copyright */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Copyright : %s", (char *)payload);
            break;

        case 0x03: /* Track name */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Track name: %s", (char *)payload);
            break;

        case 0x04: /* Instrument name */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Instrument: %s", (char *)payload);
            break;

        case 0x05: /* Lyric (one syllable) */
            /*EnsureUTF8 ((char *)payload);*/
            break;

        case 0x06: /* Marker text */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Marker    : %s", (char *)payload);
            break;

        case 0x07: /* Cue point (WAVE filename) */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Cue point : %s", (char *)payload);
            break;

        case 0x08: /* Program/Patch name */
            EnsureUTF8 ((char *)payload);
            msg_Info (p_demux, "Patch name: %s", (char *)payload);
            break;

        case 0x09: /* MIDI port name */
            EnsureUTF8 ((char *)payload);
            msg_Dbg (p_demux, "MIDI port : %s", (char *)payload);
            break;

        case 0x2F: /* End of track */
            if (tr->start + tr->length != vlc_stream_Tell (s))
            {
                msg_Err (p_demux, "misplaced end of track");
                ret = -1;
            }
            break;

        case 0x51: /* Tempo */
            if (length == 3)
            {
                uint32_t uspqn = (payload[0] << 16)
                               | (payload[1] << 8) | payload[2];
                unsigned tempo = 60 * 1000000 / (uspqn ? uspqn : 1);
                msg_Dbg (p_demux, "tempo: %uus/qn -> %u BPM",
                         (unsigned)uspqn, tempo);

                if (tempo < TEMPO_MIN)
                {
                    msg_Warn (p_demux, "tempo too slow -> %u BPM", TEMPO_MIN);
                    tempo = TEMPO_MIN;
                }
                else
                if (tempo > TEMPO_MAX)
                {
                    msg_Warn (p_demux, "tempo too fast -> %u BPM", TEMPO_MAX);
                    tempo = TEMPO_MAX;
                }
                date_Change (&p_sys->pts, p_sys->ppqn * tempo, 60);
            }
            else
                ret = -1;
            break;

        case 0x54: /* SMPTE offset */
            if (length == 5)
                msg_Warn (p_demux, "SMPTE offset not implemented");
            else
                ret = -1;
            break;

        case 0x58: /* Time signature */
            if (length == 4)
                ;
            else
                ret = -1;
            break;

        case 0x59: /* Key signature */
            if (length != 2)
                msg_Warn(p_demux, "invalid key signature");
            break;

        case 0x7f: /* Proprietary event */
            msg_Dbg (p_demux, "ignored proprietary SMF Meta Event (%d bytes)",
                     length);
            break;

        default:
            msg_Warn (p_demux, "unknown SMF Meta Event type 0x%02X (%d bytes)",
                      type, length);
    }

    free (payload);
    return ret;
}

static
int HandleMessage (demux_t *p_demux, mtrk_t *tr, es_out_t *out)
{
    stream_t *s = p_demux->s;
    demux_sys_t *sys = p_demux->p_sys;
    block_t *block;
    uint8_t first, event;
    int datalen;

    if (vlc_stream_Seek (s, tr->start + tr->offset)
     || (vlc_stream_Read (s, &first, 1) != 1))
        return -1;

    event = (first & 0x80) ? first : tr->running_event;

    switch (event & 0xf0)
    {
        case 0xF0: /* System Exclusive */
            switch (event)
            {
                case 0xF0: /* System Specific start */
                case 0xF7: /* System Specific continuation */
                {
                    /* Variable length followed by SysEx event data */
                    int32_t len = ReadVarInt (s);
                    if (len == -1)
                        return -1;

                    block = vlc_stream_Block (s, len);
                    if (block == NULL)
                        return -1;
                    block = block_Realloc (block, 1, len);
                    if (block == NULL)
                        return -1;
                    block->p_buffer[0] = event;
                    goto send;
                }
                case 0xFF: /* SMF Meta Event */
                    if (HandleMeta (p_demux, tr))
                        return -1;
                    /* We MUST NOT pass this event forward. It would be
                     * confused as a MIDI Reset real-time event. */
                    goto skip;
                case 0xF1:
                case 0xF3:
                    datalen = 1;
                    break;
                case 0xF2:
                    datalen = 2;
                    break;
                case 0xF4:
                case 0xF5:
                    /* We cannot handle undefined "common" (non-real-time)
                     * events inside SMF, as we cannot differentiate a
                     * one byte delta-time (< 0x80) from event data. */
                default:
                    datalen = 0;
                    break;
            }
            break;
        case 0xC0:
        case 0xD0:
            datalen = 1;
            break;
        default:
            datalen = 2;
            break;
    }

    /* FIXME: one message per block is very inefficient */
    block = block_Alloc (1 + datalen);
    if (block == NULL)
        goto skip;

    block->p_buffer[0] = event;
    if (first & 0x80)
    {
        if (vlc_stream_Read(s, block->p_buffer + 1, datalen) < datalen)
            goto error;
    }
    else
    {
        if (datalen == 0)
        {   /* implicit running status requires non-empty payload */
            msg_Err (p_demux, "malformatted MIDI event");
            goto error;
        }

        block->p_buffer[1] = first;
        if (datalen > 1
         && vlc_stream_Read(s, block->p_buffer + 2, datalen - 1) < datalen - 1)
            goto error;
    }

send:
    block->i_dts = block->i_pts = date_Get(&sys->pts);
    if (out != NULL)
        es_out_Send(out, sys->es, block);
    else
        block_Release (block);

skip:
    if (event < 0xF8)
        /* If event is not real-time, update running status */
        tr->running_event = event;

    tr->offset = vlc_stream_Tell (s) - tr->start;
    return 0;

error:
    block_Release(block);
    return -1;
}

static int SeekSet0 (demux_t *demux)
{
    stream_t *stream = demux->s;
    demux_sys_t *sys = demux->p_sys;

    /* Default SMF tempo is 120BPM, i.e. half a second per quarter note */
    date_Init (&sys->pts, sys->ppqn * 2, 1);
    date_Set (&sys->pts, VLC_TICK_0);
    sys->pulse = 0;
    sys->tick = VLC_TICK_0;

    for (unsigned i = 0; i < sys->trackc; i++)
    {
        mtrk_t *tr = sys->trackv + i;

        tr->offset = 0;
        tr->next = 0;
        /* Why 0xF6 (Tuning Calibration)?
         * Because it has zero bytes of data, so the parser will detect the
         * error if the first event uses running status. */
        tr->running_event = 0xF6;

        if (vlc_stream_Seek (stream, tr->start)
         || ReadDeltaTime (stream, tr))
        {
            msg_Err (demux, "fatal parsing error");
            return -1;
        }
    }

    return 0;
}

static int ReadEvents (demux_t *demux, uint64_t *restrict pulse,
                       es_out_t *out)
{
    uint64_t cur_pulse = *pulse, next_pulse = UINT64_MAX;
    demux_sys_t *sys = demux->p_sys;

    for (unsigned i = 0; i < sys->trackc; i++)
    {
        mtrk_t *track = sys->trackv + i;

        while (track->next <= cur_pulse)
        {
            if (HandleMessage (demux, track, out)
             || ReadDeltaTime (demux->s, track))
            {
                msg_Err (demux, "fatal parsing error");
                return -1;
            }
        }

        if (next_pulse > track->next)
            next_pulse = track->next;
    }

    if (next_pulse != UINT64_MAX)
        date_Increment (&sys->pts, next_pulse - cur_pulse);
    *pulse = next_pulse;
    return 0;
}

#define TICK VLC_TICK_FROM_MS(10)

/*****************************************************************************
 * Demux: read chunks and send them to the synthesizer
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux (demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    /* MIDI Tick emulation (ping the decoder every 10ms) */
    if (sys->tick <= date_Get (&sys->pts))
    {
        block_t *tick = block_Alloc (1);
        if (unlikely(tick == NULL))
            return VLC_ENOMEM;

        tick->p_buffer[0] = 0xF9;
        tick->i_dts = tick->i_pts = sys->tick;

        es_out_Send (demux->out, sys->es, tick);
        es_out_SetPCR (demux->out, sys->tick);

        sys->tick += TICK;
        return VLC_DEMUXER_SUCCESS;
    }

    /* MIDI events in chronological order across all tracks */
    uint64_t pulse = sys->pulse;

    if (ReadEvents (demux, &pulse, demux->out))
        return VLC_DEMUXER_EGENERIC;

    if (pulse == UINT64_MAX)
        return VLC_DEMUXER_EOF; /* all tracks are done */

    sys->pulse = pulse;
    return VLC_DEMUXER_SUCCESS;
}

static int Seek (demux_t *demux, vlc_tick_t pts)
{
    demux_sys_t *sys = demux->p_sys;

    /* Rewind if needed */
    if (pts < date_Get (&sys->pts) && SeekSet0 (demux))
        return VLC_EGENERIC;

    /* Fast forward */
    uint64_t pulse = sys->pulse;

    while (pts > date_Get (&sys->pts))
    {
        if (pulse == UINT64_MAX)
            return VLC_SUCCESS; /* premature end */
        if (ReadEvents (demux, &pulse, NULL))
            return VLC_EGENERIC;
    }

    sys->pulse = pulse;
    sys->tick = ((date_Get (&sys->pts) - VLC_TICK_0) / TICK) * TICK + VLC_TICK_0;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control (demux_t *demux, int i_query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (i_query)
    {
        case DEMUX_CAN_SEEK:
            *va_arg (args, bool *) = true;
            break;
        case DEMUX_GET_POSITION:
            if (!sys->duration)
                return VLC_EGENERIC;
            *va_arg (args, double *) = (sys->tick - (double)VLC_TICK_0)
                                     / sys->duration;
            break;
        case DEMUX_SET_POSITION:
            return Seek (demux, va_arg (args, double) * sys->duration);
        case DEMUX_GET_LENGTH:
            *va_arg (args, vlc_tick_t *) = sys->duration;
            break;
        case DEMUX_GET_TIME:
            *va_arg (args, vlc_tick_t *) = sys->tick - VLC_TICK_0;
            break;
        case DEMUX_SET_TIME:
            return Seek (demux, va_arg (args, vlc_tick_t));

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( demux->s, 0, -1, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/**
 * Probes file format and starts demuxing.
 */
static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    stream_t *stream = demux->s;
    const uint8_t *peek;
    bool multitrack;

    /* (Try to) parse the SMF header */
    /* Header chunk always has 6 bytes payload */
    if (vlc_stream_Peek (stream, &peek, 14) < 14)
        return VLC_EGENERIC;

    /* Skip RIFF MIDI header if present */
    if (!memcmp (peek, "RIFF", 4) && !memcmp (peek + 8, "RMID", 4))
    {
        uint32_t riff_len = GetDWLE (peek + 4);

        msg_Dbg (demux, "detected RIFF MIDI file (%"PRIu32" bytes)", riff_len);
        if ((vlc_stream_Read (stream, NULL, 12) < 12))
            return VLC_EGENERIC;

        /* Look for the RIFF data chunk */
        for (;;)
        {
            char chnk_hdr[8];
            uint32_t chnk_len;

            if ((riff_len < 8)
             || (vlc_stream_Read (stream, chnk_hdr, 8) < 8))
                return VLC_EGENERIC;

            riff_len -= 8;
            chnk_len = GetDWLE (chnk_hdr + 4);
            if (riff_len < chnk_len)
                return VLC_EGENERIC;
            riff_len -= chnk_len;

            if (!memcmp (chnk_hdr, "data", 4))
                break; /* found! */

            if (vlc_stream_Read (stream, NULL, chnk_len) < (ssize_t)chnk_len)
                return VLC_EGENERIC;
        }

        /* Read real SMF header. Assume RIFF data chunk length is proper. */
        if (vlc_stream_Peek (stream, &peek, 14) < 14)
            return VLC_EGENERIC;
    }

    if (memcmp (peek, "MThd\x00\x00\x00\x06", 8))
        return VLC_EGENERIC;
    peek += 8;

    /* First word: SMF type */
    switch (GetWBE (peek))
    {
        case 0:
            multitrack = false;
            break;
        case 1:
            multitrack = true;
            break;
        default:
            /* We don't implement SMF2 (as do many) */
            msg_Err (demux, "unsupported SMF file type %u", GetWBE (peek));
            return VLC_EGENERIC;
    }
    peek += 2;

    /* Second word: number of tracks */
    unsigned tracks = GetWBE (peek);
    peek += 2;
    if (!multitrack && (tracks != 1))
    {
        msg_Err (demux, "invalid SMF type 0 file");
        return VLC_EGENERIC;
    }

    msg_Dbg (demux, "detected Standard MIDI File (type %u) with %u track(s)",
             multitrack, tracks);

    /* Third/last word: timing */
    unsigned ppqn = GetWBE (peek);
    if (ppqn & 0x8000)
    {   /* FIXME */
        msg_Err (demux, "SMPTE timestamps not implemented");
        return VLC_EGENERIC;
    }
    else
    {
        if (ppqn == 0)
        {
            msg_Err(demux, "invalid SMF file PPQN: %u", ppqn);
            return VLC_EGENERIC;
        }
        msg_Dbg (demux, " %u pulses per quarter note", ppqn);
    }

    demux_sys_t *sys = malloc (sizeof (*sys) + (sizeof (mtrk_t) * tracks));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* We've had a valid SMF header - now skip it*/
    if (vlc_stream_Read (stream, NULL, 14) < 14)
        goto error;

    demux->p_sys = sys;
    sys->duration = 0;
    sys->ppqn = ppqn;
    sys->trackc = tracks;

    /* Prefetch track offsets */
    for (unsigned i = 0; i < tracks; i++)
    {
        mtrk_t *tr = sys->trackv + i;
        uint8_t head[8];

        /* Seeking screws streaming up, but there is no way around this, as
         * SMF1 tracks are performed simultaneously.
         * Not a big deal as SMF1 are usually only a few kbytes anyway. */
        if (i > 0 && vlc_stream_Seek (stream, tr[-1].start + tr[-1].length))
        {
            msg_Err (demux, "cannot build SMF index (corrupted file?)");
            goto error;
        }

        for (;;)
        {
            if (vlc_stream_Read (stream, head, 8) < 8)
            {
                /* FIXME: don't give up if we have at least one valid track */
                msg_Err (demux, "incomplete SMF chunk, file is corrupted");
                goto error;
            }

            if (memcmp (head, "MTrk", 4) == 0)
                break;

            uint_fast32_t chunk_len = GetDWBE(head + 4);
            msg_Dbg(demux, "skipping unknown SMF chunk (%"PRIuFAST32" bytes)",
                    chunk_len);
            if (vlc_stream_Seek(stream, vlc_stream_Tell(stream) + chunk_len))
                goto error;
        }

        tr->start = vlc_stream_Tell (stream);
        tr->length = GetDWBE (head + 4);
    }

    bool b;
    if (vlc_stream_Control (stream, STREAM_CAN_FASTSEEK, &b) == 0 && b)
    {
        if (SeekSet0 (demux))
            goto error;

        for (uint64_t pulse = 0; pulse != UINT64_MAX;)
             if (ReadEvents (demux, &pulse, NULL))
                 break;

        sys->duration = date_Get (&sys->pts);
    }

    if (SeekSet0 (demux))
        goto error;

    es_format_t  fmt;
    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_MIDI);
    fmt.audio.i_channels = 2;
    fmt.audio.i_rate = 44100; /* dummy value */
    fmt.i_id = 0;
    sys->es = es_out_Add (demux->out, &fmt);

    demux->pf_demux = Demux;
    demux->pf_control = Control;
    return VLC_SUCCESS;

error:
    free (sys);
    return VLC_EGENERIC;
}

/**
 * Releases allocate resources.
 */
static void Close (vlc_object_t * p_this)
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free (p_sys);
}

vlc_module_begin ()
    set_description (N_("SMF demuxer"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_DEMUX)
    set_capability ("demux", 20)
    set_callbacks (Open, Close)
vlc_module_end ()
