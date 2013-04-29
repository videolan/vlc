/**
 * @file timecode.c
 * @brief Time code sub-picture generator for VLC media player
 */
/*****************************************************************************
 * Copyright © 2013 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>

#define FPS_TEXT N_("Frame rate")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

static const char *const fps_values[] = { "24/1", "25/1", "30000/1001", "30/1" };
static const char *const fps_texts[] = { "24", "25", "29.97", "30" };

vlc_module_begin ()
    set_shortname (N_("Time code"))
    set_description (N_("Time code subpicture elementary stream generator"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_capability ("access_demux", 0)
    set_callbacks (Open, Close)

    add_string ("timecode-fps", "25/1", FPS_TEXT, FPS_TEXT, false)
        change_string_list (fps_values, fps_texts)
        change_safe ()
vlc_module_end ()

struct demux_sys_t
{
    es_out_id_t *es;
    date_t date;
    mtime_t next_time;
};

static int DemuxOnce (demux_t *demux, bool master)
{
    demux_sys_t *sys = demux->p_sys;
    mtime_t pts = date_Get (&sys->date);
    lldiv_t d;
    unsigned h, m, s, f;

    d = lldiv (pts, CLOCK_FREQ);
    f = d.rem * sys->date.i_divider_num / sys->date.i_divider_den / CLOCK_FREQ;
    d = lldiv (d.quot, 60);
    s = d.rem;
    d = lldiv (d.quot, 60);
    m = d.rem;
    h = d.quot;

    char *str;
    int len = asprintf (&str, "%02u:%02u:%02u:%02u", h, m, s, f);
    if (len == -1)
        return -1;

    block_t *block = block_heap_Alloc (str, len + 1);
    if (unlikely(block == NULL))
        return -1;

    block->i_buffer = len;
    assert(str[len] == '\0');

    block->i_pts = block->i_dts = pts;
    block->i_length = date_Increment (&sys->date, 1) - pts;
    es_out_Send (demux->out, sys->es, block);
    if (master)
        es_out_Control (demux->out, ES_OUT_SET_PCR, pts);
    return 1;
}

static int Demux (demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    if (sys->next_time == VLC_TS_INVALID) /* Master mode */
        return DemuxOnce (demux, true);

    /* Slave mode */
    while (sys->next_time > date_Get (&sys->date))
    {
        int val = DemuxOnce (demux, false);
        if (val <= 0)
            return val;
    }
    return 1;
}

static int Control (demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_POSITION:
            *va_arg (args, float *) = 0.f;
            break;

        case DEMUX_GET_LENGTH:
            *va_arg (args, int64_t *) = INT64_C(0);
            break;

        case DEMUX_GET_TIME:
            *va_arg (args, int64_t *) = date_Get (&sys->date);
            break;

        case DEMUX_SET_TIME:
            date_Set (&sys->date, va_arg (args, int64_t));
            break;

        case DEMUX_SET_NEXT_DEMUX_TIME:
        {
            const mtime_t pts = va_arg (args, int64_t );

            if (sys->next_time == VLC_TS_INVALID) /* first invocation? */
            {
                date_Set (&sys->date, pts);
                date_Decrement (&sys->date, 1);
            }
            sys->next_time = pts;
            break;
        }

        case DEMUX_GET_PTS_DELAY:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = INT64_C(1000) * var_InheritInteger (demux, "live-caching");
            break;
        }

        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_SEEK:
            *va_arg (args, bool *) = true;
            break;

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = malloc (sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    es_format_t fmt;
    es_format_Init (&fmt, SPU_ES, VLC_CODEC_ITU_T140);
    sys->es = es_out_Add (demux->out, &fmt);

    unsigned num, den;
    if (var_InheritURational (demux, &num, &den, "timecode-fps")
     || !num || !den)
    {
        msg_Err (demux, "invalid frame rate");
        free (sys);
        return VLC_EGENERIC;
    }

    date_Init (&sys->date, num, den);
    date_Set (&sys->date, VLC_TS_0);
    sys->next_time = VLC_TS_INVALID;

    demux->p_sys = sys;
    demux->pf_demux   = Demux;
    demux->pf_control = Control;
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    free (sys);
}
