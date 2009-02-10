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

static void handler( msg_cb_data_t *d, msg_item_t *p_item, unsigned i_drop )
{
    if (p_item->i_type > d->verbosity)
        return;

    vlc_spin_lock (&d->lock);
    if (d->count < VLC_MSG_QSIZE)
    {
        d->items[d->count++] = p_item;
        msg_Hold (p_item);
    }
    vlc_spin_unlock (&d->lock);
    (void)i_drop;
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

unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    assert( p_instance );
    (void)p_e;
    return p_instance->verbosity;
}

void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level, libvlc_exception_t *p_e )
{
    assert( p_instance );
    (void)p_e;
    p_instance->verbosity = level;
}

libvlc_log_t *libvlc_log_open( libvlc_instance_t *p_instance, libvlc_exception_t *p_e )
{
    struct libvlc_log_t *p_log =
        (struct libvlc_log_t *)malloc(sizeof(struct libvlc_log_t));

    if( !p_log ) RAISENULL( "Out of memory" );

    p_log->p_instance = p_instance;
    vlc_spin_init( &p_log->data.lock );
    p_log->data.count = 0;
    p_log->data.verbosity = p_instance->verbosity;
    p_log->p_messages = msg_Subscribe(p_instance->p_libvlc_int, handler, &p_log->data);

    if( !p_log->p_messages )
    {
        free( p_log );
        RAISENULL( "Out of memory" );
    }

    libvlc_retain( p_instance );
    return p_log;
}

void libvlc_log_close( libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log )
    {
        assert( p_log->p_messages );
        msg_Unsubscribe(p_log->p_messages);
        libvlc_release( p_log->p_instance );
        libvlc_log_clear( p_log, p_e );
        vlc_spin_destroy( &p_log->data.lock );
        free(p_log);
    }
    else
        RAISEVOID("Invalid log object!");
}

unsigned libvlc_log_count( const libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log )
    {
        msg_cb_data_t *data = &((libvlc_log_t *)p_log)->data;
        unsigned ret;

        /* We cannot lock due to constant pointer constraints. Break them.
         * Even then, this si not really thread safe anyway. */
        vlc_spin_lock (&data->lock);
        ret = data->count;
        vlc_spin_unlock (&data->lock);
        return ret;
    }
    RAISEZERO("Invalid log object!");
}

void libvlc_log_clear( libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log )
    {
        vlc_spin_lock (&p_log->data.lock);
        msg_item_t *tab[p_log->data.count];
        memcpy (tab, p_log->data.items, sizeof (tab));
        p_log->data.count = 0;
        vlc_spin_unlock (&p_log->data.lock);

        for (unsigned i = 0; i < sizeof (tab) / sizeof (tab[0]); i++)
            msg_Release (tab[i]);
    }
    else
        RAISEVOID("Invalid log object!");
}

libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *p_log, libvlc_exception_t *p_e )
{
    if( p_log )
    {
        struct libvlc_log_iterator_t *p_iter =
            (struct libvlc_log_iterator_t *)malloc(sizeof(struct libvlc_log_iterator_t));
        /* FIXME: break constant pointer constraints */
        msg_cb_data_t *data = &((libvlc_log_t *)p_log)->data;

        if( !p_iter ) RAISENULL( "Out of memory" );

        vlc_spin_lock (&data->lock);
        p_iter->p_data  = data;
        p_iter->i_pos   = 0;
        p_iter->i_end   = data->count;
        vlc_spin_unlock (&data->lock);

        return p_iter;
    }
    RAISENULL("Invalid log object!");
}

void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e )
{
    if( p_iter )
    {
        free(p_iter);
    }
    else
        RAISEVOID("Invalid log iterator!");
}

int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e )
{
    if( p_iter )
    {
        return p_iter->i_pos != p_iter->i_end;
    }
    RAISEZERO("Invalid log iterator!");
}

libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                libvlc_log_message_t *buffer,
                                                libvlc_exception_t *p_e )
{
    unsigned i_pos;

    if( !p_iter )
        RAISENULL("Invalid log iterator!");
    if( !buffer )
        RAISENULL("Invalid message buffer!");

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
    RAISENULL("No more messages");
}
