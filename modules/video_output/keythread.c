/*****************************************************************************
 * keythread.c: Asynchronous threads to emit key events
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include <vlc_common.h>
#include <assert.h>

#include "keythread.h"

struct key_thread
{
    vlc_object_t *libvlc;
    int          value;
    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    vlc_thread_t thread;
};

static void *KeyThread (void *data)
{
    key_thread_t *keys = data;

    mutex_cleanup_push (&keys->lock);
    for (;;)
    {
        int value;

        vlc_mutex_lock (&keys->lock);
        /* Note: Key strokes may be lost. A chained list should be used. */
        while (!(value = keys->value))
            vlc_cond_wait (&keys->wait, &keys->lock);
        keys->value = 0;
        vlc_mutex_unlock (&keys->lock);

        int canc = vlc_savecancel ();
        var_SetInteger (keys->libvlc, "key-pressed", value);
        vlc_restorecancel (canc);
    }

    vlc_cleanup_pop ();
    assert (0);
}


#undef vlc_CreateKeyThread
/**
 * Create an asynchronous key event thread.
 *
 * Normally, key events are received by the interface thread (e.g. Qt4), or
 * the window provider (e.g. XCB). However, some legacy video output plugins
 * do not use window providers, neither do they run their own event threads.
 * Instead, those lame video outputs insist on receiving key events from their
 * Manage() function.
 *
 * Some key event handlers are quite slow so they should not be triggered from
 * the video output thread. Worse yet, some handlers (such as snapshot) would
 * deadlock if triggered from the video output thread.
 */
key_thread_t *vlc_CreateKeyThread (vlc_object_t *obj)
{
    key_thread_t *keys = malloc (sizeof (*keys));
    if (unlikely(keys == NULL))
        return NULL;

    keys->libvlc = VLC_OBJECT(obj->p_libvlc);
    keys->value = 0;
    vlc_mutex_init (&keys->lock);
    vlc_cond_init (&keys->wait);
    if (vlc_clone (&keys->thread, KeyThread, keys, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_cond_destroy (&keys->wait);
        vlc_mutex_destroy (&keys->lock);
        free (keys);
        return NULL;
    }
    return keys;
}

void vlc_DestroyKeyThread (key_thread_t *keys)
{
    if (keys == NULL)
        return;

    vlc_cancel (keys->thread);
    vlc_join(keys->thread, NULL);
    vlc_cond_destroy(&keys->wait);
    vlc_mutex_destroy(&keys->lock);
    free (keys);
}

void vlc_EmitKey (key_thread_t *keys, int value)
{
    if (keys == NULL)
        return;

    vlc_mutex_lock(&keys->lock);
    keys->value = value;
    vlc_cond_signal(&keys->wait);
    vlc_mutex_unlock(&keys->lock);
}
