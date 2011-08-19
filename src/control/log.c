/*****************************************************************************
 * log.c: libvlc new API log functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Damien Fouilleul <damienf@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>

/* This API is terminally broken.
 * First, it does not implement any kind of notification.
 * Second, the iterating scheme is hermetic to any kind of thread-safety
 * owing to its constant pointer constraints.
 *  -- Courmisch
 *
 * "If you break your leg, don't run to me for sympathy"
 *   -- some character, Beneath a Steel Sky
 */

struct msg_cb_data_t
{
    vlc_spinlock_t lock;
    msg_item_t *items[VLC_MSG_QSIZE];
    unsigned    count;
    int         verbosity;
};

static void handler( msg_cb_data_t *d, const msg_item_t *p_item )
{
    if (p_item->i_type > d->verbosity)
        return;

    msg_item_t *msg = msg_Copy (p_item);

    vlc_spin_lock (&d->lock);
    if (d->count < VLC_MSG_QSIZE)
        d->items[d->count++] = msg;
    vlc_spin_unlock (&d->lock);
}

struct libvlc_log_t
{
    libvlc_instance_t  *p_instance;
    msg_subscription_t *p_messages;
    msg_cb_data_t       data;
};

struct libvlc_log_iterator_t
{
    msg_cb_data_t *p_data;
    unsigned i_pos;
    unsigned i_end;
};

unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance )
{
    assert( p_instance );
    return p_instance->verbosity;
}

void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level )
{
    assert( p_instance );
    p_instance->verbosity = level;
}

libvlc_log_t *libvlc_log_open( libvlc_instance_t *p_instance )
{
    struct libvlc_log_t *p_log = malloc(sizeof(*p_log));
    if (unlikely(p_log == NULL))
    {
        libvlc_printerr ("Not enough memory");
        return NULL;
    }

    p_log->p_instance = p_instance;
    vlc_spin_init( &p_log->data.lock );
    p_log->data.count = 0;
    p_log->data.verbosity = p_instance->verbosity;
    p_log->p_messages = vlc_Subscribe(handler, &p_log->data);

    if( !p_log->p_messages )
    {
        free( p_log );
        libvlc_printerr ("Not enough memory");
        return NULL;
    }

    libvlc_retain( p_instance );
    return p_log;
}

void libvlc_log_close( libvlc_log_t *p_log )
{
    if( !p_log )
        return;

    assert( p_log->p_messages );
    vlc_Unsubscribe(p_log->p_messages);
    libvlc_release( p_log->p_instance );
    libvlc_log_clear( p_log );
    vlc_spin_destroy( &p_log->data.lock );
    free(p_log);
}

unsigned libvlc_log_count( const libvlc_log_t *p_log )
{
    if( !p_log )
        return 0;

    msg_cb_data_t *data = &((libvlc_log_t *)p_log)->data;
    unsigned ret;

    /* We cannot lock due to constant pointer constraints. Break them.
     * Even then, this si not really thread safe anyway. */
    vlc_spin_lock (&data->lock);
    ret = data->count;
    vlc_spin_unlock (&data->lock);
    return ret;
}

void libvlc_log_clear( libvlc_log_t *p_log )
{
    if( !p_log )
        return;

    vlc_spin_lock (&p_log->data.lock);
    msg_item_t *tab[p_log->data.count];
    memcpy (tab, p_log->data.items, sizeof (tab));
    p_log->data.count = 0;
    vlc_spin_unlock (&p_log->data.lock);

    for (unsigned i = 0; i < sizeof (tab) / sizeof (tab[0]); i++)
         msg_Free (tab[i]);
}

libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *p_log )
{
    if (p_log == NULL)
        return NULL;

    struct libvlc_log_iterator_t *p_iter = malloc (sizeof (*p_iter));
    if (unlikely(p_iter == NULL))
    {
        libvlc_printerr ("Not enough memory");
        return NULL;
    }

    /* FIXME: break constant pointer constraints */
    msg_cb_data_t *data = &((libvlc_log_t *)p_log)->data;

    vlc_spin_lock (&data->lock);
    p_iter->p_data  = data;
    p_iter->i_pos   = 0;
    p_iter->i_end   = data->count;
    vlc_spin_unlock (&data->lock);
    return p_iter;
}

void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter )
{
    free( p_iter );
}

int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter )
{
    if( !p_iter )
        return 0;
    return p_iter->i_pos != p_iter->i_end;
}

libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                libvlc_log_message_t *buffer )
{
    unsigned i_pos;

    if( !p_iter )
        return NULL;
    assert (buffer != NULL);

    i_pos = p_iter->i_pos;
    if( i_pos != p_iter->i_end )
    {
        msg_item_t *msg;
        vlc_spin_lock (&p_iter->p_data->lock);
        msg = p_iter->p_data->items[i_pos];
        buffer->i_severity  = msg->i_type;
        buffer->psz_type    = msg->psz_object_type;
        buffer->psz_name    = msg->psz_module;
        buffer->psz_header  = msg->psz_header;
        buffer->psz_message = msg->psz_msg;
        vlc_spin_unlock (&p_iter->p_data->lock);
        p_iter->i_pos++;

        return buffer;
    }
    return NULL;
}
