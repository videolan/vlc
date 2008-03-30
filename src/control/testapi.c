/*
 * testapi.c - libvlc smoke test
 *
 * $Id$
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc/libvlc.h>

#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static libvlc_exception_t ex;

#define log( ... ) printf( "testapi: " __VA_ARGS__ );

static void catch (void)
{
    if (libvlc_exception_raised (&ex))
    {
         fprintf (stderr, "Exception: %s\n",
                  libvlc_exception_get_message (&ex));
         abort ();
    }

    assert (libvlc_exception_get_message (&ex) == NULL);
    libvlc_exception_clear (&ex);
}

/* Test we have */
static void test_core (const char ** argv, int argc);
static void test_media_list (const char ** argv, int argc);
static void test_events (const char ** argv, int argc);
static void test_media_player_play_stop(const char** argv, int argc);

/* Tests implementations */
static void test_core (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    int id;

    log ("Testing core\n");

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    libvlc_playlist_clear (vlc, &ex);
    catch ();

    id = libvlc_playlist_add_extended (vlc, "/dev/null", "Test", 0, NULL,
                                       &ex);
    catch ();

    libvlc_playlist_clear (vlc, &ex);
    catch ();

    libvlc_retain (vlc);
    libvlc_release (vlc);
    libvlc_release (vlc);
}

static void test_media_list (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_descriptor_t *md;
    libvlc_media_list_t *ml;

    log ("Testing media_list\n");

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    ml = libvlc_media_list_new (vlc, &ex);
    catch ();

    md = libvlc_media_descriptor_new (vlc, "/dev/null", &ex);
    catch ();

    libvlc_media_list_add_media_descriptor (ml, md, &ex);
    catch ();
    libvlc_media_list_add_media_descriptor (ml, md, &ex);
    catch ();

    assert( libvlc_media_list_count (ml, &ex) == 2 );
    catch ();

    libvlc_media_descriptor_release (md);

    libvlc_media_list_release (ml);

    libvlc_release (vlc);
    catch ();
}

/* This one is an internal API. We use it here to run tests that
 * don't depends on playback, and only test the event framework */
extern void libvlc_event_send( libvlc_event_manager_t *, libvlc_event_t *);

static void test_events_dummy_callback( const libvlc_event_t * event, void * user_data)
{
    vlc_bool_t * callback_was_called = user_data;
    *callback_was_called = VLC_TRUE;
}

static void test_events_callback_and_detach( const libvlc_event_t * event, void * user_data)
{
    vlc_bool_t * callback_was_called = user_data;
    libvlc_event_manager_t *em;

    em = libvlc_media_instance_event_manager (event->p_obj, &ex);
    catch();

    libvlc_event_detach (em, event->type, test_events_callback_and_detach, user_data, &ex);
    *callback_was_called = VLC_TRUE;
}

static void test_event_type_reception( libvlc_event_manager_t * em, libvlc_event_type_t event_type, vlc_bool_t * callback_was_called )
{
    libvlc_event_t event;
    event.type = event_type;
    *callback_was_called = VLC_FALSE;
    libvlc_event_send (em, &event);
    assert (*callback_was_called);
}

static void test_events (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_instance_t *mi;
    libvlc_event_manager_t *em;
    vlc_bool_t callback_was_called;
    libvlc_exception_t ex;
    libvlc_event_type_t mi_events[] = {
        libvlc_MediaInstancePlayed,
        libvlc_MediaInstancePaused,
        libvlc_MediaInstanceReachedEnd,
        libvlc_MediaInstanceEncounteredError,
        libvlc_MediaInstanceTimeChanged,
        libvlc_MediaInstancePositionChanged,
    };
    int i, mi_events_len = sizeof(mi_events)/sizeof(*mi_events);
    
    log ("Testing events\n");

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    mi = libvlc_media_instance_new (vlc, &ex);
    catch ();

    em = libvlc_media_instance_event_manager (mi, &ex);

    log ("+ Testing attaching to Media Instance\n");

    for (i = 0; i < mi_events_len; i++) {
        libvlc_event_attach (em, mi_events[i], test_events_dummy_callback, &callback_was_called, &ex);
        catch ();
    }

    log ("+ Testing event reception\n");

    for (i = 0; i < mi_events_len; i++)
        test_event_type_reception (em, mi_events[i], &callback_was_called);

    log ("+ Testing event detaching while in the event callback\n");

    libvlc_event_t event;
    event.type = mi_events[mi_events_len-1];
    callback_was_called = VLC_FALSE;

    libvlc_event_detach (em, mi_events[mi_events_len-1], test_events_dummy_callback, &callback_was_called, &ex);
    catch ();

    libvlc_event_attach (em, mi_events[mi_events_len-1], test_events_callback_and_detach, &callback_was_called, &ex);
    catch ();

    libvlc_event_send (em, &event);
    assert( callback_was_called );

    callback_was_called = VLC_FALSE;
    libvlc_event_send (em, &event);
    assert( !callback_was_called );

    libvlc_event_detach (em, mi_events[mi_events_len-1], test_events_callback_and_detach, &callback_was_called, &ex);
    catch ();

    log ("+ Testing regular detach()\n");

    for (i = 0; i < mi_events_len - 1; i++) {
        libvlc_event_detach (em, mi_events[i], test_events_dummy_callback, &callback_was_called, &ex);
        catch ();
    }

    libvlc_media_instance_release (mi);
    catch ();

    libvlc_release (vlc);
    catch ();
}

static void test_media_player_play_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_descriptor_t *md;
    libvlc_media_instance_t *mi;
    const char * file = "file://../bindings/java/core/src/test/resources/raffa_voice.ogg";

    log ("Testing play and pause of %s\n", file);

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    md = libvlc_media_descriptor_new (vlc, file, &ex);
    catch ();

    mi = libvlc_media_instance_new_from_media_descriptor (md, &ex);
    catch ();
    
    libvlc_media_descriptor_release (md);

    libvlc_media_instance_play (mi, &ex);
    catch ();

    /* FIXME: Do something clever */
    sleep(1);

    assert( libvlc_media_instance_get_state (mi, &ex) != libvlc_Error );
    catch ();

    libvlc_media_instance_stop (mi, &ex);
    catch ();

    libvlc_media_instance_release (mi);
    catch ();

    libvlc_release (vlc);
    catch ();
}

int main (int argc, char *argv[])
{
    const char *args[argc + 5];
    int nlibvlc_args = sizeof (args) / sizeof (args[0]);

    alarm (50); /* Make sure "make check" does not get stuck */

    args[0] = "-vvv";
    args[1] = "-I";
    args[2] = "dummy";
    args[3] = "--plugin-path=../modules";
    args[4] = "--vout=dummy";
    args[5] = "--aout=dummy";
    for (int i = 1; i < argc; i++)
        args[i + 3] = argv[i];

    test_core (args, nlibvlc_args);

    test_events (args, nlibvlc_args);

    test_media_list (args, nlibvlc_args);

    test_media_player_play_stop(args, nlibvlc_args);

    return 0;
}
