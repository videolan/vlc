/*
 * test.h - libvlc smoke test common definitions
 *
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  Copyright (C) 2008 Pierre d'Herbemont.                            *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#ifndef TEST_H
#define TEST_H

/*********************************************************************
 * Some useful common headers
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc/vlc.h>
#include <vlc_common.h>
#include <vlc_threads.h>

#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

/*********************************************************************
 * Some useful global var
 */

static const char * test_defaults_args[] = {
    "-v", "--vout=vdummy", "--aout=adummy", "--text-renderer=tdummy",
};

static const int test_defaults_nargs =
    sizeof (test_defaults_args) / sizeof (test_defaults_args[0]);

static const char test_default_sample[] = "mock://";

/*********************************************************************
 * Some useful common functions
 */

#define test_log( ... ) printf( "testapi: " __VA_ARGS__ );

static inline void on_timeout(int signum)
{
    assert(signum == SIGALRM);
    abort(); /* Cause a core dump */
}

static inline void test_init (void)
{
    (void)test_default_sample; /* This one may not be used */

    /* Make sure "make check" does not get stuck */
    /* Timeout of 10secs by default */
    unsigned alarm_timeout = 10;
    /* Valid timeout value are < 0, for infinite, and > 0, for the number of
     * seconds */
    char *alarm_timeout_str = getenv("VLC_TEST_TIMEOUT");
    if (alarm_timeout_str)
    {
        int val = atoi(alarm_timeout_str);
        if (val <= 0)
            alarm_timeout = 0; /* infinite */
        else
            alarm_timeout = val;
    }
    if (alarm_timeout != 0)
    {
        struct sigaction sig = {
            .sa_handler = on_timeout,
        };
        sigaction(SIGALRM, &sig, NULL);
        alarm (alarm_timeout);
    }

    setenv( "VLC_PLUGIN_PATH", "../modules", 1 );
}

static void
libvlc_media_parse_finished_event(const libvlc_event_t *p_ev, void *p_data)
{
    (void) p_ev;
    vlc_sem_t *p_sem = p_data;
    vlc_sem_post(p_sem);
}

static inline void
libvlc_media_parse_sync(libvlc_media_t *p_m, libvlc_media_parse_flag_t parse_flag,
                        int timeout)
{
    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    libvlc_event_manager_t *p_em = libvlc_media_event_manager(p_m);
    libvlc_event_attach(p_em, libvlc_MediaParsedChanged,
                        libvlc_media_parse_finished_event, &sem);

    int i_ret = libvlc_media_parse_with_options(p_m, parse_flag, timeout);
    assert(i_ret == 0);

    vlc_sem_wait (&sem);

    libvlc_event_detach(p_em, libvlc_MediaParsedChanged,
                        libvlc_media_parse_finished_event, &sem);
}

#endif /* TEST_H */
