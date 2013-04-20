/*****************************************************************************
 * audiobargraph_a.c : audiobargraph audio plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id$
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

#include <vlc_network.h>
#include <math.h>

#define ADDRESS_TEXT N_("TCP address to use")
#define ADDRESS_LONGTEXT N_("TCP address to use to communicate with the video "\
                "part of the Bar Graph (default localhost). " \
                "In the case of bargraph incrustation, use localhost." )
#define PORT_TEXT N_("TCP port to use")
#define PORT_LONGTEXT N_("TCP port to use to communicate with the video "\
                "part of the Bar Graph (default 12345). " \
                "Use the same port as the one used in the rc interface." )
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
#define CONNECTION_RESET_TEXT N_("Force connection reset regularly" )
#define CONNECTION_RESET_LONGTEXT N_("Defines if the TCP connection should be reset. "\
                "This is to be used when using with audiobargraph_v (default 1)." )

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

    add_string( CFG_PREFIX "address", "localhost", ADDRESS_TEXT, ADDRESS_LONGTEXT, false )
    add_integer( CFG_PREFIX "port", 12345, PORT_TEXT, PORT_LONGTEXT, false )
    add_integer( CFG_PREFIX "bargraph", 1, BARGRAPH_TEXT, BARGRAPH_LONGTEXT, false )
    add_integer( CFG_PREFIX "bargraph_repetition", 4, BARGRAPH_REPETITION_TEXT, BARGRAPH_REPETITION_LONGTEXT, false )
    add_integer( CFG_PREFIX "silence", 1, SILENCE_TEXT, SILENCE_LONGTEXT, false )
    add_integer( CFG_PREFIX "time_window", 5000, TIME_WINDOW_TEXT, TIME_WINDOW_LONGTEXT, false )
    add_float( CFG_PREFIX "alarm_threshold", 0.1, ALARM_THRESHOLD_TEXT, ALARM_THRESHOLD_LONGTEXT, false )
    add_integer( CFG_PREFIX "repetition_time", 2000, REPETITION_TIME_TEXT, REPETITION_TIME_LONGTEXT, false )
    add_integer( CFG_PREFIX "connection_reset", 1, CONNECTION_RESET_TEXT, CONNECTION_RESET_LONGTEXT, false )

    set_callbacks( Open, Close )
vlc_module_end ()

typedef struct ValueDate_t {
    float value;
    mtime_t date;
    struct ValueDate_t* next;
} ValueDate_t;

struct filter_sys_t
{
    char*           address;
    int             port;
    int             bargraph;
    int             bargraph_repetition;
    int             silence;
    int             time_window;
    float           alarm_threshold;
    int             repetition_time;
    int             connection_reset;
    int             TCPconnection;
    int             counter;
    int             nbChannels;
    ValueDate_t*    first;
    ValueDate_t*    last;
    int             started;
    mtime_t         lastAlarm;
};

/*****************************************************************************
 * Open: open the visualizer
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t     *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->bargraph = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-bargraph" );
    p_sys->bargraph_repetition = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-bargraph_repetition" );
    p_sys->silence = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-silence" );
    p_sys->address = var_CreateGetStringCommand( p_filter, "audiobargraph_a-address" );
    p_sys->port = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-port" );
    p_sys->time_window = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-time_window" );
    p_sys->alarm_threshold = var_CreateGetFloatCommand( p_filter, "audiobargraph_a-alarm_threshold" );
    p_sys->repetition_time = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-repetition_time" );
    p_sys->connection_reset = var_CreateGetIntegerCommand( p_filter, "audiobargraph_a-connection_reset" );
    if ((p_sys->TCPconnection = net_ConnectTCP(p_this,p_sys->address,p_sys->port)) == -1) {
        free(p_sys);
        return VLC_EGENERIC;
    }
    p_sys->counter = 0;
    p_sys->nbChannels = 0;
    p_sys->first = NULL;
    p_sys->last = NULL;
    p_sys->started = 0;
    p_sys->lastAlarm = 0;

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork: treat an audio buffer
 ****************************************************************************/
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    int i, j;
    float *p_sample = (float *)p_in_buf->p_buffer;
    float *i_value = NULL;
    float ch;
    float max = 0.0;
    //char *message = (char*)malloc(255*sizeof(char));
    char message[255];
    int nbChannels = 0;
    ValueDate_t* new = NULL;
    ValueDate_t* current = NULL;
    float sum;
    int count = 0;

    nbChannels = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    p_sys->nbChannels = nbChannels;

    i_value = (float*)malloc(nbChannels * sizeof(float));

    for (i=0; i<nbChannels; i++) {
        i_value[i] = 0;
    }

    /* 1 - Compute the peack values */
    for ( i = 0 ; i < (int)(p_in_buf->i_nb_samples); i++ )
    {
        for (j=0; j<nbChannels; j++) {
            ch = (*p_sample++);
            if (ch > i_value[j])
                i_value[j] = ch;
            if (ch > max)
                max = ch;
        }
    }
    max = pow( max, 2 );

    if (p_sys->silence) {
        /* 2 - store the new value */
        new = (ValueDate_t*)malloc(sizeof(ValueDate_t));
        new->value = max;
        new->date = p_in_buf->i_pts;
        new->next = NULL;
        if (p_sys->last != NULL) {
            p_sys->last->next = new;
        }
        p_sys->last = new;
        if (p_sys->first == NULL) {
            p_sys->first = new;
        }

        /* 3 - delete too old values */
        while (p_sys->first->date < (new->date - (p_sys->time_window*1000))) {
            p_sys->started = 1; // we have enough values to compute a valid total
            current = p_sys->first;
            p_sys->first = p_sys->first->next;
            free(current);
        }

        /* If last message was sent enough time ago */
        if ((p_sys->started) && (p_in_buf->i_pts > p_sys->lastAlarm + (p_sys->repetition_time*1000))) {

            /* 4 - compute the RMS */
            current = p_sys->first;
            sum = 0.0;
            while (current != NULL) {
                sum += current->value;
                count ++;
                current = current->next;
            }
            sum = sum / count;
            sum = sqrt(sum);

            /* 5 - compare it to the threshold */
            if (sum < p_sys->alarm_threshold) {
                i=1;
            } else {
                i=0;
            }
            snprintf(message,255,"@audiobargraph_v audiobargraph_v-alarm %d\n",i);

            msg_Dbg( p_filter, "message alarm : %s", message );
            //TCPconnection = net_ConnectTCP(p_filter,p_sys->address,p_sys->port);
            net_Write(p_filter, p_sys->TCPconnection, NULL, message, strlen(message));
            //net_Close(TCPconnection);

            p_sys->lastAlarm = p_in_buf->i_pts;
        }
    }

    /*for (i=0; i<nbChannels; i++) {
        value[i] = abs(i_value[i]*100);
        if ( value[i] > p_sys->value[i] - 6 )
            p_sys->value[i] = value[i];
        else
            p_sys->value[i] = p_sys->value[i] - 6;
    }*/

    if (p_sys->bargraph) {
        /* 6 - sent the message with the values for the BarGraph */
        if ((nbChannels > 0) && (p_sys->counter%(p_sys->bargraph_repetition) == 0)) {
            j=snprintf(message,255,"@audiobargraph_v audiobargraph_v-i_values ");
            for (i=0; i<(nbChannels-1); i++) {
                j+=snprintf(message+j,255,"%f:", i_value[i]);
            }
            snprintf(message+j,255,"%f\n", i_value[nbChannels-1]);
            msg_Dbg( p_filter, "message values : %s", message );

            //test = send(p_sys->TCPconnection,message,strlen(message),0);
            //net_Write(p_filter, p_sys->TCPconnection, NULL, message, strlen(message));

            net_Write(p_filter, p_sys->TCPconnection, NULL, message, strlen(message));

        }
    }

    free(i_value);

    if (p_sys->counter > p_sys->bargraph_repetition*100) {
        if (p_sys->connection_reset) {
            net_Close(p_sys->TCPconnection);
            p_sys->TCPconnection = net_ConnectTCP(p_filter,p_sys->address,p_sys->port);
        }
        p_sys->counter = 0;
    }

    //free(message);
    p_sys->counter++;

    return p_in_buf;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    ValueDate_t* current;

    p_sys->last = NULL;
    while (p_sys->first != NULL) {
        current = p_sys->first;
        p_sys->first = p_sys->first->next;
        free(current);
    }
    net_Close(p_sys->TCPconnection);
    free(p_sys->address);
    //free(p_sys->value);
    free( p_filter->p_sys );
}
