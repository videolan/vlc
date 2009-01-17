/*****************************************************************************
 * bonjour.c
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon@nanocrew.net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "bonjour.h"

#ifdef HAVE_AVAHI_CLIENT
#include <vlc_sout.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

/*****************************************************************************
 * Structures
 *****************************************************************************/
typedef struct poll_thread_t
{
    VLC_COMMON_MEMBERS

    AvahiSimplePoll     *simple_poll;
} poll_thread_t;

typedef struct bonjour_t
{
    vlc_object_t        *p_log;

    poll_thread_t       *poll_thread;
    AvahiSimplePoll     *simple_poll;
    AvahiEntryGroup     *group;
    AvahiClient         *client;
    char                *psz_stype;
    char                *psz_name;
    int                 i_port;
    char                *psz_txt;
} bonjour_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static int create_service( bonjour_t * );

/*****************************************************************************
 * entry_group_callback
 *****************************************************************************/
static void entry_group_callback( AvahiEntryGroup *g,
                                  AvahiEntryGroupState state,
                                  void *userdata )
{
    (void)g;
    bonjour_t *p_sys = (bonjour_t *)userdata;

    if( state == AVAHI_ENTRY_GROUP_ESTABLISHED )
    {
        msg_Dbg( p_sys->p_log, "service '%s' successfully established",
                 p_sys->psz_name );
    }
    else if( state == AVAHI_ENTRY_GROUP_COLLISION )
    {
        char *n;

        n = avahi_alternative_service_name( p_sys->psz_name );
        avahi_free( p_sys->psz_name );
        p_sys->psz_name = n;

        create_service( p_sys );
    }
}

/*****************************************************************************
 * create_service
 *****************************************************************************/
static int create_service( bonjour_t *p_sys )
{
    int error;

    if( p_sys->group == NULL )
    {
        p_sys->group = avahi_entry_group_new( p_sys->client,
                                              entry_group_callback,
                                              p_sys );
        if( p_sys->group == NULL )
        {
            msg_Err( p_sys->p_log, "failed to create avahi entry group: %s",
                     avahi_strerror( avahi_client_errno( p_sys->client ) ) );
            return VLC_EGENERIC;
        }
    }

    error = avahi_entry_group_add_service( p_sys->group, AVAHI_IF_UNSPEC,
                                           AVAHI_PROTO_UNSPEC, 0, p_sys->psz_name,
                                           p_sys->psz_stype, NULL, NULL,
                                           p_sys->i_port,
                                           p_sys->psz_txt, NULL );
    if( error < 0 )
    {
        msg_Err( p_sys->p_log, "failed to add %s service: %s",
                 p_sys->psz_stype, avahi_strerror( error ) );
        return VLC_EGENERIC;
    }

    error = avahi_entry_group_commit( p_sys->group );
    if( error < 0 )
    {
        msg_Err( p_sys->p_log, "failed to commit entry group: %s",
                 avahi_strerror( error ) );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * client_callback
 *****************************************************************************/
static void client_callback( AvahiClient *c,
                             AvahiClientState state,
                             void * userdata )
{
    bonjour_t *p_sys = (bonjour_t *)userdata;

    if( state == AVAHI_CLIENT_S_RUNNING )
    {
        p_sys->client = c;
        create_service( p_sys );
    }
    else if( state == AVAHI_CLIENT_S_COLLISION )
    {
        if( p_sys->group != NULL )
            avahi_entry_group_reset( p_sys->group );
    }
    else if( state == AVAHI_CLIENT_FAILURE &&
              (avahi_client_errno(c) == AVAHI_ERR_DISCONNECTED) )
    {
        msg_Err( p_sys->p_log, "avahi client disconnected" );
        avahi_simple_poll_quit( p_sys->simple_poll );
    }
}

/*****************************************************************************
 * poll_iterate_thread
 *****************************************************************************/
static void* poll_iterate_thread( vlc_object_t *p_this )
{
    poll_thread_t *p_pt = (poll_thread_t*)p_this;
    int canc = vlc_savecancel ();

    while( vlc_object_alive (p_pt) )
        if( avahi_simple_poll_iterate( p_pt->simple_poll, 100 ) != 0 )
            break;

    vlc_restorecancel (canc);
    return NULL;
}

/*****************************************************************************
 * bonjour_start_service
 *****************************************************************************/
void *bonjour_start_service( vlc_object_t *p_log, const char *psz_stype,
                             const char *psz_name, int i_port, char *psz_txt )
{
    int err;

    bonjour_t* p_sys = calloc( 1, sizeof(*p_sys) );
    if( p_sys == NULL )
        return NULL;

    p_sys->p_log = p_log;
    p_sys->i_port = i_port;
    p_sys->psz_name = avahi_strdup( psz_name );
    p_sys->psz_stype = avahi_strdup( psz_stype );
    if( p_sys->psz_name == NULL || p_sys->psz_stype == NULL )
        goto error;

    if( psz_txt != NULL )
    {
        p_sys->psz_txt = avahi_strdup( psz_txt );
        if( p_sys->psz_txt == NULL )
            goto error;
    }

    p_sys->simple_poll = avahi_simple_poll_new();
    if( p_sys->simple_poll == NULL )
    {
        msg_Err( p_sys->p_log, "failed to create avahi simple pool" );
        goto error;
    }

    p_sys->client = avahi_client_new( avahi_simple_poll_get(p_sys->simple_poll),
                                      0,
                                      client_callback, p_sys, &err );
    if( p_sys->client == NULL )
    {
        msg_Err( p_sys->p_log, "failed to create avahi client: %s",
                 avahi_strerror( err ) );
        goto error;
    }

    p_sys->poll_thread = vlc_object_create( p_sys->p_log,
                                            sizeof(poll_thread_t) );
    if( p_sys->poll_thread == NULL )
        goto error;
    p_sys->poll_thread->simple_poll = p_sys->simple_poll;

    if( vlc_thread_create( p_sys->poll_thread, "Avahi Poll Iterate Thread",
                           poll_iterate_thread,
                           VLC_THREAD_PRIORITY_HIGHEST ) )
    {
        msg_Err( p_sys->p_log, "failed to create poll iterate thread" );
        goto error;
    }

    return (void *)p_sys;

error:
    if( p_sys->poll_thread != NULL )
        vlc_object_release( p_sys->poll_thread );
    if( p_sys->client != NULL )
        avahi_client_free( p_sys->client );
    if( p_sys->simple_poll != NULL )
        avahi_simple_poll_free( p_sys->simple_poll );
    if( p_sys->psz_stype != NULL )
        avahi_free( p_sys->psz_stype );
    if( p_sys->psz_name != NULL )
        avahi_free( p_sys->psz_name );
    if( p_sys->psz_txt != NULL )
        avahi_free( p_sys->psz_txt );

    free( p_sys );

    return NULL;
}

/*****************************************************************************
 * bonjour_stop_service
 *****************************************************************************/
void bonjour_stop_service( void *_p_sys )
{
    bonjour_t *p_sys = (bonjour_t *)_p_sys;

    vlc_object_kill( p_sys->poll_thread );
    vlc_thread_join( p_sys->poll_thread );
    vlc_object_release( p_sys->poll_thread );

    if( p_sys->group != NULL )
        avahi_entry_group_free( p_sys->group );

    avahi_client_free( p_sys->client );
    avahi_simple_poll_free( p_sys->simple_poll );

    if( p_sys->psz_name != NULL )
        avahi_free( p_sys->psz_name );

    if( p_sys->psz_txt != NULL )
        avahi_free( p_sys->psz_txt );

    avahi_free( p_sys->psz_stype );

    free( _p_sys );
}

#endif /* HAVE_AVAHI_CLIENT */
