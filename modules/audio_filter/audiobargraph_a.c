/*****************************************************************************
 * audiobargraph_a.c : audiobargraph audio plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 *
 * Authors: Clement CHESNIN <clement.chesnin@gmail.com>
 *          Philippe COENT <philippe.coent@tdf.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

#include <math.h>

#define BARGRAPH_TEXT N_("Defines if BarGraph information should be sent")
#define BARGRAPH_LONGTEXT N_("Defines if BarGraph information should be sent. "\
                "1 if the information should be sent, 0 otherwise (default 1)." )
#define BARGRAPH_REPETITION_TEXT N_("Sends the barGraph information every n audio packets")
#define BARGRAPH_REPETITION_LONGTEXT N_("Defines how often the barGraph information should be sent. "\
                "Sends the barGraph information every n audio packets (default 4)." )
#define SILENCE_TEXT N_("Defines if silence alarm information should be sent")
#define SILENCE_LONGTEXT N_("Defines if silence alarm information should be sent. "\
                "1 if the information should be sent, 0 otherwise (default 1)." )
#define TIME_WINDOW_TEXT N_("Time window to use in ms")
#define TIME_WINDOW_LONGTEXT N_("Time Window during when the audio level is measured in ms for silence detection. "\
                "If the audio level is under the threshold during this time, "\
                "an alarm is sent (default 5000)." )
#define ALARM_THRESHOLD_TEXT N_("Minimum Audio level to raise the alarm")
#define ALARM_THRESHOLD_LONGTEXT N_("Threshold to be attained to raise an alarm. "\
                "If the audio level is under the threshold during this time, "\
                "an alarm is sent (default 0.1)." )
#define REPETITION_TIME_TEXT N_("Time between two alarm messages in ms" )
#define REPETITION_TIME_LONGTEXT N_("Time between two alarm messages in ms. "\
                "This value is used to avoid alarm saturation (default 2000)." )

#define CFG_PREFIX "audiobargraph_a-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static block_t *DoWork( filter_t *, block_t * );

vlc_module_begin ()
    set_description( N_("Audio part of the BarGraph function") )
    set_shortname( N_("Audiobar Graph") )
    set_capability( "audio filter", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )

    add_obsolete_string( CFG_PREFIX "address" )
    add_obsolete_integer( CFG_PREFIX "port" )
    add_integer( CFG_PREFIX "bargraph", 1, BARGRAPH_TEXT, BARGRAPH_LONGTEXT, false ) // FIXME: this is a bool
    add_integer( CFG_PREFIX "bargraph_repetition", 4, BARGRAPH_REPETITION_TEXT, BARGRAPH_REPETITION_LONGTEXT, false )
    add_integer( CFG_PREFIX "silence", 1, SILENCE_TEXT, SILENCE_LONGTEXT, false ) // FIXME: this is a bool
    add_integer( CFG_PREFIX "time_window", 5000, TIME_WINDOW_TEXT, TIME_WINDOW_LONGTEXT, false )
    add_float( CFG_PREFIX "alarm_threshold", 0.02, ALARM_THRESHOLD_TEXT, ALARM_THRESHOLD_LONGTEXT, false )
    add_integer( CFG_PREFIX "repetition_time", 2000, REPETITION_TIME_TEXT, REPETITION_TIME_LONGTEXT, false )
    add_obsolete_integer( CFG_PREFIX "connection_reset" )

    set_callbacks( Open, Close )
vlc_module_end ()

typedef struct ValueDate_t {
    float value;
    vlc_tick_t date;
    struct ValueDate_t* next;
} ValueDate_t;

typedef struct
{
    bool            bargraph;
    int             bargraph_repetition;
    bool            silence;
    vlc_tick_t      time_window;
    float           alarm_threshold;
    vlc_tick_t      repetition_time;
    int             counter;
    ValueDate_t*    first;
    ValueDate_t*    last;
    int             started;
    vlc_tick_t      lastAlarm;
} filter_sys_t;

/*****************************************************************************
 * Open: open the visualizer
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys = malloc(sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    static const char *const options[] = {
        "bargraph", "bargraph_repetition", "silence", "time_window",
        "alarm_threshold", "repetition_time", NULL
    };
    config_ChainParse(p_filter, CFG_PREFIX, options, p_filter->p_cfg);

    p_sys->bargraph = !!var_CreateGetInteger(p_filter, CFG_PREFIX "bargraph");
    p_sys->bargraph_repetition = var_CreateGetInteger(p_filter, CFG_PREFIX "bargraph_repetition");
    p_sys->silence = !!var_CreateGetInteger(p_filter, CFG_PREFIX "silence");
    p_sys->time_window = VLC_TICK_FROM_MS( var_CreateGetInteger(p_filter, CFG_PREFIX "time_window") );
    p_sys->alarm_threshold = var_CreateGetFloat(p_filter, CFG_PREFIX "alarm_threshold");
    p_sys->repetition_time = VLC_TICK_FROM_MS( var_CreateGetInteger(p_filter, CFG_PREFIX "repetition_time") );
    p_sys->counter = 0;
    p_sys->first = NULL;
    p_sys->last = NULL;
    p_sys->started = 0;
    p_sys->lastAlarm = 0;

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&p_filter->fmt_in.audio);
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(p_filter));

    var_Create(vlc, "audiobargraph_v-alarm", VLC_VAR_BOOL);
    var_Create(vlc, "audiobargraph_v-i_values", VLC_VAR_STRING);

    return VLC_SUCCESS;
}

static void SendValues(filter_t *p_filter, float *value, int nbChannels)
{
    char msg[256];
    size_t len = 0;

    for (int i = 0; i < nbChannels; i++) {
        if (len >= sizeof (msg))
            break;
        len += snprintf(msg + len, sizeof (msg) - len, "%f:", value[i]);
    }

    //msg_Dbg(p_filter, "values: %s", msg);
    var_SetString(vlc_object_instance(p_filter), "audiobargraph_v-i_values",
                  msg);
}

/*****************************************************************************
 * DoWork: treat an audio buffer
 ****************************************************************************/
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    float *p_sample = (float *)p_in_buf->p_buffer;
    float i_value[AOUT_CHAN_MAX];

    int nbChannels = aout_FormatNbChannels(&p_filter->fmt_in.audio);

    for (int i = 0; i < nbChannels; i++)
        i_value[i] = 0.;

    /* 1 - Compute the peak values */
    for (size_t i = 0; i < p_in_buf->i_nb_samples; i++)
        for (int j = 0; j<nbChannels; j++) {
            float ch = *p_sample++;
            if (ch > i_value[j])
                i_value[j] = ch;
        }

    if (p_sys->silence) {
        /* 2 - store the new value */
        ValueDate_t *new = xmalloc(sizeof(*new));
        new->value = 0.0;
        for (int j = 0; j<nbChannels; j++) {
            float ch = i_value[j];
            if (ch > new->value)
                new->value = ch;
        }
        new->value *= new->value;
        new->date = p_in_buf->i_pts;
        new->next = NULL;
        if (p_sys->last != NULL)
            p_sys->last->next = new;
        p_sys->last = new;
        if (p_sys->first == NULL)
            p_sys->first = new;

        /* 3 - delete too old values */
        while (p_sys->first->date < new->date - p_sys->time_window) {
            p_sys->started = 1; // we have enough values to compute a valid total
            ValueDate_t *current = p_sys->first;
            p_sys->first = p_sys->first->next;
            free(current);
        }

        /* If last message was sent enough time ago */
        if (p_sys->started && p_in_buf->i_pts > p_sys->lastAlarm + p_sys->repetition_time) {

            /* 4 - compute the RMS */
            ValueDate_t *current = p_sys->first;
            float sum = 0.0;
            int count = 0;
            while (current != NULL) {
                sum += current->value;
                count++;
                current = current->next;
            }
            sum /= count;
            sum = sqrtf(sum);

            /* 5 - compare it to the threshold */
            var_SetBool(vlc_object_instance(p_filter), "audiobargraph_v-alarm",
                        sum < p_sys->alarm_threshold);

            p_sys->lastAlarm = p_in_buf->i_pts;
        }
    }

    if (p_sys->bargraph && nbChannels > 0 && p_sys->counter++ > p_sys->bargraph_repetition) {
        SendValues(p_filter, i_value, nbChannels);
        p_sys->counter = 0;
    }

    return p_in_buf;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(p_filter));

    var_Destroy(vlc, "audiobargraph_v-i_values");
    var_Destroy(vlc, "audiobargraph_v-alarm");

    while (p_sys->first != NULL) {
        ValueDate_t *current = p_sys->first;
        p_sys->first = p_sys->first->next;
        free(current);
    }
    free(p_sys);
}
