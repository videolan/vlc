/**
 * @file gme.c
 * @brief Game Music Emu demux module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdarg.h>
#include <limits.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>

#include <gme/gme.h>

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname ("GME")
    set_description ("Game Music Emu")
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_DEMUX)
    set_capability ("demux", 10)
    set_callbacks (Open, Close)
vlc_module_end ()

#define RATE 48000

struct demux_sys_t
{
    Music_Emu   *emu;
    es_out_id_t *es;
    date_t       pts;
};


static int Demux (demux_t *);
static int Control (demux_t *, int, va_list);
static gme_err_t Reader (void *, void *, int);

static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    int64_t size = stream_Size (demux->s);
    if (size < 4 /* GME needs to know the file size */
     || size > LONG_MAX /* too big for GME */)
        return VLC_EGENERIC;

    const uint8_t *peek;
    if (stream_Peek (demux->s, &peek, 4) < 4)
        return VLC_EGENERIC;

    const char *type = gme_identify_header (peek);
    if (!*type)
        return VLC_EGENERIC;
    msg_Dbg (obj, "detected file type %s", type);

    demux_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->emu = gme_new_emu (gme_identify_extension (type), RATE);
    if (sys->emu == NULL)
    {
        free (sys);
        return VLC_ENOMEM;
    }
    gme_load_custom (sys->emu, Reader, size, demux->s);
    gme_start_track (sys->emu, 0);

    es_format_t fmt;
    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_S16N);
    fmt.audio.i_rate = RATE;
    fmt.audio.i_bytes_per_frame = 4;
    fmt.audio.i_frame_length = 4;
    fmt.audio.i_channels = 2;
    fmt.audio.i_blockalign = 4;
    fmt.audio.i_bitspersample = 16;
    fmt.i_bitrate = RATE * 4;

    sys->es = es_out_Add (demux->out, &fmt);
    date_Init (&sys->pts, RATE, 1);
    date_Set (&sys->pts, 0);

    demux->pf_demux = Demux;
    demux->pf_control = Control;
    demux->p_sys = sys;
    return VLC_SUCCESS;
}


static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    gme_delete (sys->emu);
    free (sys);
}


static gme_err_t Reader (void *data, void *buf, int length)
{
    stream_t *s = data;

    if (stream_Read (s, buf, length) < length)
        return "short read";
    return NULL;
}


#define SAMPLES (RATE / 10)

static int Demux (demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    if (gme_track_ended (sys->emu))
    {
        msg_Dbg (demux, "track ended");
        return 0;
    }

    block_t *block = block_Alloc (2 * 2 * SAMPLES);
    if (unlikely(block == NULL))
        return 0;

    gme_err_t ret = gme_play (sys->emu, 2 * SAMPLES, (void *)block->p_buffer);
    if (ret != NULL)
    {
        block_Release (block);
        msg_Err (demux, "%s", ret);
        return 0;
    }

    block->i_pts = block->i_dts = VLC_TS_0 + date_Get (&sys->pts);
    es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);
    es_out_Send (demux->out, sys->es, block);
    date_Increment (&sys->pts, SAMPLES);
    return 1;
}


static int Control (demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_POSITION: // TODO
        {
            double *pos = va_arg (args, double *);
            *pos = 0.;
            return VLC_SUCCESS;
        }

        //case DEMUX_SET_POSITION: TODO
        //case DEMUX_GET_LENGTH: TODO
        case DEMUX_GET_TIME:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = gme_tell (sys->emu) * INT64_C(1000);
            return VLC_SUCCESS;
        }

        case DEMUX_SET_TIME:
        {
            int64_t v = va_arg (args, int64_t);
            if (v > INT_MAX || gme_seek (sys->emu, v / 1000))
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}
