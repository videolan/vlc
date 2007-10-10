/*****************************************************************************
 * smf.c : Standard MIDI File (.mid) demux module for vlc
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
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

#include <vlc/vlc.h>
#include <vlc_demux.h>
#include <vlc_aout.h>
#include <vlc_codecs.h>
#include <limits.h>

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ();
    set_description (_("SMF demuxer"));
    set_category (CAT_INPUT);
    set_subcategory (SUBCAT_INPUT_DEMUX);
    set_capability ("demux2", 20);
    set_callbacks (Open, Close);
vlc_module_end ();

static int Demux   (demux_t *);
static int Control (demux_t *, int i_query, va_list args);

typedef struct smf_track_t
{
    int64_t  offset; /* Read offset in the file (stream_Tell) */
    int64_t  end;    /* End offset in the file */
    uint64_t next;   /* Time of next message (in term of pulses) */
    uint8_t  running_event; /* Running (previous) event */
} mtrk_t;

static int ReadDeltaTime (stream_t *s, mtrk_t *track);

struct demux_sys_t
{
    es_out_id_t *es;
    date_t       pts;
    uint64_t     pulse; /* Pulses counter */

    unsigned     ppqn;   /* Pulses Per Quarter Note */
    /* by the way, "quarter note" is "noire" in French */

    unsigned     trackc; /* Number of tracks */
    mtrk_t       trackv[0]; /* Track states */
};

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open (vlc_object_t * p_this)
{
    demux_t       *p_demux = (demux_t *)p_this;
    stream_t      *stream = p_demux->s;
    demux_sys_t   *p_sys;
    const uint8_t *peek;
    unsigned       tracks, ppqn;
    vlc_bool_t     multitrack;

    /* (Try to) parse the SMF header */
    /* Header chunk always has 6 bytes payload */
    if (stream_Peek (stream, &peek, 14) < 14)
        return VLC_EGENERIC;

    if (memcmp (peek, "MThd\x00\x00\x00\x06", 8))
        return VLC_EGENERIC;
    peek += 8;

    /* First word: SMF type */
    switch (GetWBE (peek))
    {
        case 0:
            multitrack = VLC_FALSE;
            break;
        case 1:
            multitrack = VLC_TRUE;
            break;
        default:
            /* We don't implement SMF2 (as do many) */
            msg_Err (p_this, "unsupported SMF file type %u", GetWBE (peek));
            return VLC_EGENERIC;
    }
    peek += 2;

    /* Second word: number of tracks */
    tracks = GetWBE (peek);
    peek += 2;
    if (!multitrack && (tracks != 1))
    {
        msg_Err (p_this, "invalid SMF type 0 file");
        return VLC_EGENERIC;
    }

    msg_Dbg (p_this, "detected Standard MIDI File (type %u) with %u track(s)",
             multitrack, tracks);

    /* Third/last word: timing */
    ppqn = GetWBE (peek);
    if (ppqn & 0x8000)
    {
        /* FIXME */
        msg_Err (p_this, "SMPTE timestamps not implemented");
        return VLC_EGENERIC;
    }
    else
    {
        msg_Dbg (p_this, " %u pulses per quarter note", ppqn);
    }

    p_sys = malloc (sizeof (*p_sys) + (sizeof (mtrk_t) * tracks));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    /* We've had a valid SMF header - now skip it*/
    if (stream_Read (stream, NULL, 14) < 14)
        goto error;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys;

    /* SMF expresses tempo in Beats-Per-Minute, default is 120 */
    date_Init (&p_sys->pts, ppqn * 120, 60);
    p_sys->pulse        = 0;
    p_sys->ppqn         = ppqn;

    p_sys->trackc       = tracks;
    /* Prefetch track offsets */
    for (unsigned i = 0; i < tracks; i++)
    {
        uint8_t head[8];

        if (i > 0)
        {
            /* Seeking screws streaming up, but there is no way around this,
             * as SMF1 tracks are performed simultaneously.
             * Not a big deal as SMF1 are usually only a few kbytes anyway. */
            if (stream_Seek (stream,  p_sys->trackv[i-1].end))
            {
                msg_Err (p_this, "cannot build SMF index (corrupted file?)");
                goto error;
            }
        }

        for (;;)
        {
            stream_Read (stream, head, 8);
            if (memcmp (head, "MTrk", 4) == 0)
                break;

            msg_Dbg (p_this, "skipping unknown SMF chunk");
            stream_Read (stream, NULL, GetDWBE (head + 4));
        }

        p_sys->trackv[i].offset = stream_Tell (stream);
        p_sys->trackv[i].end = p_sys->trackv[i].offset + GetDWBE (head + 4);
        msg_Dbg (p_this, "track %u: 0x"I64Fx"-0x"I64Fx, i,
                 p_sys->trackv[i].offset, p_sys->trackv[i].end);
        p_sys->trackv[i].next = 0;
        ReadDeltaTime (stream, p_sys->trackv + i);
        p_sys->trackv[i].running_event = 0xF6;
        /* Why 0xF6 (Tuning Calibration)?
         * Because it has zero bytes of data, so the parser will detect the
         * error if the first event uses running status. */
    }

    es_format_t  fmt;
    es_format_Init (&fmt, AUDIO_ES, VLC_FOURCC('M', 'I', 'D', 'I'));
    p_sys->es = es_out_Add (p_demux->out, &fmt);

    return VLC_SUCCESS;

error:
    free (p_sys);
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
        if (stream_Read (s, &byte, 1) < 1)
            return -1;

        val = (val << 7) | (byte & 0x7f);
        if ((byte & 0x80) == 0)
            return val;
    }

    return -1;
}


/**
 * Reads (delta) time from the next event of a given track.
 * @param s stream to read data from (must be positioned at the right offset)
 */
static int ReadDeltaTime (stream_t *s, mtrk_t *track)
{
    int32_t delta_time;

    assert (stream_Tell (s) == track->offset);

    if (track->offset >= track->end)
    {
        /* This track is done */
        track->next = UINT64_MAX;
        return 0;
    }

    delta_time = ReadVarInt (s);
    if (delta_time < 0)
        return -1;

    track->next += delta_time;
    track->offset = stream_Tell (s);
    return 0;
}


static
int HandleMessage (demux_t *p_demux, mtrk_t *tr)
{
    stream_t *s = p_demux->s;
    block_t *block;
    uint8_t first, event;
    unsigned datalen;

    if (stream_Seek (s, tr->offset)
     || (stream_Read (s, &first, 1) != 1))
        return -1;

    event = (first & 0x80) ? first : tr->running_event;
    msg_Dbg (p_demux, "MIDI event 0x%02X", event);

    switch (event & 0xf0)
    {
        case 0xF0: /* System Exclusive */
            switch (event)
            {
                case 0xF0: /* System Specific */
                {
                    /* TODO: don't skip these */
                    stream_Read (s, NULL, 1); /* Manuf ID */
                    for (;;)
                    {
                        uint8_t c;
                        if (stream_Read (s, &c, 1) != 1)
                            return -1;
                        if (c == 0xF7)
                            goto skip;
                    }
                }
                case 0xFF: /* SMF Meta Event */
                {
                    uint8_t type;
                    int32_t length;

                    if (stream_Read (s, &type, 1) != 1)
                        return -1;
                    length = ReadVarInt (s);
                    if (length < 0)
                        return -1;

                    msg_Warn (p_demux,
                              "unknown SMF Meta Event type 0x%02X (%d bytes)",
                              type, length);
                    /* TODO: parse these */
                    if (stream_Read (s, NULL, length) != length)
                        return -1;

                    /* We MUST NOT pass this event to forward. It would be
                     * confused as a MIDI Reset real-time event. */
                    goto skip;
                }
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
                case 0xF7: /* End of sysex -> should never happen(?) */
                    msg_Err (p_demux, "unknown MIDI event 0x%02X", event);
                    return -1; /* undefined events */
                default:
                    datalen = 0;
                    break;
            }
        case 0xC0:
        case 0xD0:
            datalen = 1;
            break;
        default:
            datalen = 2;
            break;
    }

    /* FIXME: one message per block is very inefficient */
    block = block_New (p_demux, 1 + datalen);
    if (block == NULL)
        goto skip;

    block->p_buffer[0] = event;
    if (first & 0x80)
    {
        stream_Read (s, block->p_buffer + 1, datalen);
    }
    else
    {
        if (datalen == 0)
        {
            msg_Err (p_demux, "malformatted MIDI event");
            return -1; /* can't use implicit running status with empty payload! */
        }

        block->p_buffer[1] = first;
        if (datalen > 1)
            stream_Read (s, block->p_buffer + 2, datalen - 1);
    }

    block->i_dts = block->i_pts = date_Get (&p_demux->p_sys->pts);
    es_out_Send (p_demux->out, p_demux->p_sys->es, block);

skip:
    if (event < 0xF8)
        /* If event is not real-time, update running status */
        tr->running_event = event;

    tr->offset = stream_Tell (s);
    return 0;
}

/*****************************************************************************
 * Demux: read chunks and send them to the synthetizer
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux (demux_t *p_demux)
{
    stream_t *s = p_demux->s;
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t     pulse = p_sys->pulse, next_pulse = UINT64_MAX;

    if (pulse == UINT64_MAX)
        return 0; /* all tracks are done */

    es_out_Control (p_demux->out, ES_OUT_SET_PCR, date_Get (&p_sys->pts));

    msg_Dbg (p_demux, "pulse %llu", pulse);
    for (unsigned i = 0; i < p_sys->trackc; i++)
    {
        mtrk_t *track = p_sys->trackv + i;

        while (track->next == pulse)
        {
            msg_Dbg (p_demux, "event on track %u", i);
            if (HandleMessage (p_demux, track)
             || ReadDeltaTime (s, track))
            {
                msg_Err (p_demux, "fatal parsing error");
                return VLC_EGENERIC;
            }
        }

        if (track->next < next_pulse)
            next_pulse = track->next;
    }

    if (next_pulse != UINT64_MAX)
        date_Increment (&p_sys->pts, next_pulse - pulse);
    p_sys->pulse = next_pulse;

    return 1;
}


/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control (demux_t *p_demux, int i_query, va_list args)
{
    return demux2_vaControlHelper (p_demux->s, 0, -1, 0, 1, i_query, args);
}
