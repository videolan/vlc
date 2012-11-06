/**
 * @file gme.c
 * @brief Game Music Emu demux module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <stdarg.h>
#include <limits.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_demux.h>
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
    unsigned     track_id;

    es_out_id_t *es;
    date_t       pts;

    input_title_t **titlev;
    unsigned        titlec;
};


static int Demux (demux_t *);
static int Control (demux_t *, int, va_list);
static gme_err_t ReaderStream (void *, void *, int);
static gme_err_t ReaderBlock (void *, void *, int);

static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    int64_t size = stream_Size (demux->s);
    if (size > LONG_MAX /* too big for GME */)
        return VLC_EGENERIC;

    /* Auto detection */
    const uint8_t *peek;
    if (stream_Peek (demux->s, &peek, 4) < 4)
        return VLC_EGENERIC;

    const char *type = gme_identify_header (peek);
    if (!*type)
        return VLC_EGENERIC;
    msg_Dbg (obj, "detected file type %s", type);

    block_t *data = NULL;
    if (size <= 0)
    {
        data = stream_BlockRemaining (demux->s, 100000000);
        if (!data )
            return VLC_EGENERIC;
    }

    /* Initialization */
    demux_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->emu = gme_new_emu (gme_identify_extension (type), RATE);
    if (sys->emu == NULL)
    {
        free (sys);
        return VLC_ENOMEM;
    }
    if (data)
    {
        gme_load_custom (sys->emu, ReaderBlock, data->i_buffer, data);
        block_Release(data);
    }
    else
    {
        gme_load_custom (sys->emu, ReaderStream, size, demux->s);
    }
    gme_start_track (sys->emu, sys->track_id = 0);

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

    /* Titles */
    unsigned n = gme_track_count (sys->emu);
    sys->titlev = malloc (n * sizeof (*sys->titlev));
    if (unlikely(sys->titlev == NULL))
        n = 0;
    sys->titlec = n;
    for (unsigned i = 0; i < n; i++)
    {
         input_title_t *title = vlc_input_title_New ();
         sys->titlev[i] = title;
         if (unlikely(title == NULL))
             continue;

         gme_info_t *infos;
         if (gme_track_info (sys->emu, &infos, i))
             continue;
         msg_Dbg (obj, "track %u: %s %d ms", i, infos->song, infos->length);
         if (infos->length != -1)
             title->i_length = infos->length * INT64_C(1000);
         if (infos->song[0])
             title->psz_name = strdup (infos->song);
         gme_free_info (infos);
    }

    /* Callbacks */
    demux->pf_demux = Demux;
    demux->pf_control = Control;
    demux->p_sys = sys;
    return VLC_SUCCESS;
}


static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    for (unsigned i = 0, n = sys->titlec; i < n; i++)
        vlc_input_title_Delete (sys->titlev[i]);
    free (sys->titlev);
    gme_delete (sys->emu);
    free (sys);
}


static gme_err_t ReaderStream (void *data, void *buf, int length)
{
    stream_t *s = data;

    if (stream_Read (s, buf, length) < length)
        return "short read";
    return NULL;
}
static gme_err_t ReaderBlock (void *data, void *buf, int length)
{
    block_t *block = data;

    int max = __MIN (length, (int)block->i_buffer);
    memcpy (buf, block->p_buffer, max);
    block->i_buffer -= max;
    block->p_buffer += max;
    if (max != length)
        return "short read";
    return NULL;
}

#define SAMPLES (RATE / 10)

static int Demux (demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    /* Next track */
    if (gme_track_ended (sys->emu))
    {
        msg_Dbg (demux, "track %u ended", sys->track_id);
        if (++sys->track_id >= (unsigned)gme_track_count (sys->emu))
            return 0;

        demux->info.i_update |= INPUT_UPDATE_TITLE;
        demux->info.i_title = sys->track_id;
        gme_start_track (sys->emu, sys->track_id);
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
        case DEMUX_GET_POSITION:
        {
            double *pos = va_arg (args, double *);

            if (unlikely(sys->track_id >= sys->titlec)
             || (sys->titlev[sys->track_id]->i_length == 0))
                *pos = 0.;
            else
                *pos = (double)(gme_tell (sys->emu))
                    / (double)(sys->titlev[sys->track_id]->i_length / 1000);
            return VLC_SUCCESS;
        }

        case DEMUX_SET_POSITION:
        {
            double pos = va_arg (args, double);

            if (unlikely(sys->track_id >= sys->titlec)
             || (sys->titlev[sys->track_id]->i_length == 0))
                break;

            int seek = (sys->titlev[sys->track_id]->i_length / 1000) * pos;
            if (gme_seek (sys->emu, seek))
                break;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_LENGTH:
        {
            int64_t *v = va_arg (args, int64_t *);

            if (unlikely(sys->track_id >= sys->titlec)
             || (sys->titlev[sys->track_id]->i_length == 0))
                break;
            *v = sys->titlev[sys->track_id]->i_length;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TIME:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = gme_tell (sys->emu) * INT64_C(1000);
            return VLC_SUCCESS;
        }

        case DEMUX_SET_TIME:
        {
            int64_t v = va_arg (args, int64_t) / 1000;
            if (v > INT_MAX || gme_seek (sys->emu, v))
                break;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***titlev = va_arg (args, input_title_t ***);
            int *titlec = va_arg (args, int *);
            *(va_arg (args, int *)) = 0; /* Title offset */
            *(va_arg (args, int *)) = 0; /* Chapter offset */

            unsigned n = sys->titlec;
            *titlev = malloc (sizeof (**titlev) * n);
            if (unlikely(*titlev == NULL))
                n = 0;
            *titlec = n;
            for (unsigned i = 0; i < n; i++)
                (*titlev)[i] = vlc_input_title_Duplicate (sys->titlev[i]);
            return VLC_SUCCESS;
        }

        case DEMUX_SET_TITLE:
        {
            int track_id = va_arg (args, int);
            if (track_id >= gme_track_count (sys->emu))
                break;
            gme_start_track (sys->emu, track_id);
            demux->info.i_update |= INPUT_UPDATE_TITLE;
            demux->info.i_title = track_id;
            sys->track_id = track_id;
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}
