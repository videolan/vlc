/*****************************************************************************
 * addons.c: VLC addons manager
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_modules.h>
#include <vlc_arrays.h>
#include <vlc_events.h>
#include "libvlc.h"

#include <vlc_addons.h>

/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/

typedef struct addon_entry_owner
{
    addon_entry_t entry;
    atomic_uint refs;
} addon_entry_owner_t;

struct addons_manager_private_t
{
    vlc_object_t *p_parent;

    struct
    {
        vlc_thread_t thread;
        vlc_cond_t waitcond;
        bool b_live;
        vlc_mutex_t lock;
        DECL_ARRAY(char*) uris;
        DECL_ARRAY(addon_entry_t*) entries;
    } finder;

    struct
    {
        vlc_thread_t thread;
        vlc_cond_t waitcond;
        bool b_live;
        vlc_mutex_t lock;
        DECL_ARRAY(addon_entry_t*) entries;
    } installer;
};

static void *FinderThread( void * );
static void LoadLocalStorage( addons_manager_t *p_manager );

/*****************************************************************************
 * Public functions
 *****************************************************************************/

addon_entry_t * addon_entry_New()
{
    addon_entry_owner_t *owner = calloc( 1, sizeof(addon_entry_owner_t) );
    if( unlikely(owner == NULL) )
        return NULL;

    atomic_init( &owner->refs, 1 );

    addon_entry_t *p_entry = &owner->entry;
    vlc_mutex_init( &p_entry->lock );
    ARRAY_INIT( p_entry->files );
    return p_entry;
}

addon_entry_t * addon_entry_Hold( addon_entry_t * p_entry )
{
    addon_entry_owner_t *owner = (addon_entry_owner_t *) p_entry;

    atomic_fetch_add( &owner->refs, 1 );
    return p_entry;
}

void addon_entry_Release( addon_entry_t * p_entry )
{
    addon_entry_owner_t *owner = (addon_entry_owner_t *) p_entry;

    if( atomic_fetch_sub(&owner->refs, 1) != 1 )
        return;

    free( p_entry->psz_name );
    free( p_entry->psz_summary );
    free( p_entry->psz_description );
    free( p_entry->psz_archive_uri );
    free( p_entry->psz_author );
    free( p_entry->psz_source_uri );
    free( p_entry->psz_image_uri );
    free( p_entry->psz_image_data );
    free( p_entry->psz_source_module );
    free( p_entry->psz_version );
    free( p_entry->p_custom );

    addon_file_t *p_file;
    FOREACH_ARRAY( p_file, p_entry->files )
    free( p_file->psz_filename );
    free( p_file->psz_download_uri );
    FOREACH_END()
    ARRAY_RESET( p_entry->files );

    vlc_mutex_destroy( &p_entry->lock );
    free( owner );
}

addons_manager_t *addons_manager_New( vlc_object_t *p_this )
{
    addons_manager_t *p_manager = malloc( sizeof(addons_manager_t) );
    if ( !p_manager ) return NULL;

    p_manager->p_priv = malloc( sizeof(addons_manager_private_t) );
    if ( !p_manager->p_priv )
    {
        free( p_manager );
        return NULL;
    }

    p_manager->p_event_manager = malloc( sizeof(vlc_event_manager_t) );
    if ( !p_manager->p_event_manager )
    {
        free( p_manager->p_priv );
        free( p_manager );
        return NULL;
    }

    p_manager->p_priv->p_parent = p_this;

#define INIT_QUEUE( name ) \
    p_manager->p_priv->name.b_live = false;\
    vlc_mutex_init( &p_manager->p_priv->name.lock );\
    vlc_cond_init( &p_manager->p_priv->name.waitcond );\
    ARRAY_INIT( p_manager->p_priv->name.entries );

    INIT_QUEUE( finder )
    INIT_QUEUE( installer )
    ARRAY_INIT( p_manager->p_priv->finder.uris );

    vlc_event_manager_t *em = p_manager->p_event_manager;
    vlc_event_manager_init( em, p_manager );
    vlc_event_manager_register_event_type(em, vlc_AddonFound);
    vlc_event_manager_register_event_type(em, vlc_AddonsDiscoveryEnded);
    vlc_event_manager_register_event_type(em, vlc_AddonChanged);

    return p_manager;
}

void addons_manager_Delete( addons_manager_t *p_manager )
{
    bool b_live;

    vlc_mutex_lock( &p_manager->p_priv->finder.lock );
    b_live = p_manager->p_priv->finder.b_live;
    vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
    if ( b_live )
    {
        vlc_cancel( p_manager->p_priv->finder.thread );
        vlc_join( p_manager->p_priv->finder.thread, NULL );
    }

    vlc_mutex_lock( &p_manager->p_priv->installer.lock );
    b_live = p_manager->p_priv->installer.b_live;
    vlc_mutex_unlock( &p_manager->p_priv->installer.lock );
    if ( b_live )
    {
        vlc_cancel( p_manager->p_priv->installer.thread );
        vlc_join( p_manager->p_priv->installer.thread, NULL );
    }

    vlc_event_manager_fini( p_manager->p_event_manager );

#define FREE_QUEUE( name ) \
    vlc_mutex_lock( &p_manager->p_priv->name.lock );\
    FOREACH_ARRAY( addon_entry_t *p_entry, p_manager->p_priv->name.entries )\
        addon_entry_Release( p_entry );\
    FOREACH_END();\
    ARRAY_RESET( p_manager->p_priv->name.entries );\
    vlc_mutex_unlock( &p_manager->p_priv->name.lock );\
    vlc_mutex_destroy( &p_manager->p_priv->name.lock );\
    vlc_cond_destroy( &p_manager->p_priv->name.waitcond );

    FREE_QUEUE( finder )
    FREE_QUEUE( installer )
    FOREACH_ARRAY( char *psz_uri, p_manager->p_priv->finder.uris )
       free( psz_uri );
    FOREACH_END();
    ARRAY_RESET( p_manager->p_priv->finder.uris );

    free( p_manager->p_priv );
    free( p_manager->p_event_manager );
    free( p_manager );
}

void addons_manager_Gather( addons_manager_t *p_manager, const char *psz_uri )
{
    if ( !psz_uri )
        return;

    vlc_mutex_lock( &p_manager->p_priv->finder.lock );

    ARRAY_APPEND( p_manager->p_priv->finder.uris, strdup( psz_uri ) );

    if( !p_manager->p_priv->finder.b_live )
    {
        if( vlc_clone( &p_manager->p_priv->finder.thread, FinderThread, p_manager,
                       VLC_THREAD_PRIORITY_LOW ) )
        {
            msg_Err( p_manager->p_priv->p_parent,
                     "cannot spawn entries provider thread" );
            vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
            return;
        }
        p_manager->p_priv->finder.b_live = true;
    }

    vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
    vlc_cond_signal( &p_manager->p_priv->finder.waitcond );
}

/*****************************************************************************
 * Private functions
 *****************************************************************************/

static addon_entry_t * getHeldEntryByUUID( addons_manager_t *p_manager,
                                           const addon_uuid_t uuid )
{
    addon_entry_t *p_return = NULL;
    vlc_mutex_lock( &p_manager->p_priv->finder.lock );
    FOREACH_ARRAY( addon_entry_t *p_entry, p_manager->p_priv->finder.entries )
    if ( !memcmp( p_entry->uuid, uuid, sizeof( addon_uuid_t ) ) )
    {
        p_return = p_entry;
        addon_entry_Hold( p_return );
        break;
    }
    FOREACH_END()
    vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
    return p_return;
}

static void MergeSources( addons_manager_t *p_manager,
                          addon_entry_t **pp_addons, int i_count )
{
    addon_entry_t *p_entry, *p_manager_entry;
    addon_uuid_t zerouuid;
    memset( zerouuid, 0, sizeof( addon_uuid_t ) );
    for ( int i=0; i < i_count; i++ )
    {
        p_entry = pp_addons[i];
        vlc_mutex_lock( &p_entry->lock );
        if ( memcmp( p_entry->uuid, zerouuid, sizeof( addon_uuid_t ) ) )
            p_manager_entry = getHeldEntryByUUID( p_manager, p_entry->uuid );
        else
            p_manager_entry = NULL;
        if ( !p_manager_entry )
        {
            ARRAY_APPEND( p_manager->p_priv->finder.entries, p_entry );
            vlc_event_t event;
            event.type = vlc_AddonFound;
            event.u.addon_generic_event.p_entry = p_entry;
            vlc_event_send( p_manager->p_event_manager, &event );
        }
        else
        {
            vlc_mutex_lock( &p_manager_entry->lock );
            if ( ( p_manager_entry->psz_version && p_entry->psz_version )
                 && /* FIXME: better version comparison */
                 strcmp( p_manager_entry->psz_version, p_entry->psz_version )
                 )
            {
                p_manager_entry->e_flags |= ADDON_UPDATABLE;
            }
            vlc_mutex_unlock( &p_manager_entry->lock );
            addon_entry_Release( p_manager_entry );
        }
        vlc_mutex_unlock( &p_entry->lock );
    }
}

static void LoadLocalStorage( addons_manager_t *p_manager )
{
    addons_finder_t *p_finder =
        vlc_custom_create( p_manager->p_priv->p_parent, sizeof( *p_finder ), "entries finder" );
    p_finder->i_flags |= OBJECT_FLAGS_NOINTERACT;

    module_t *p_module = module_need( p_finder, "addons finder",
                                      "addons.store.list", true );
    if( p_module )
    {
        ARRAY_INIT( p_finder->entries );
        p_finder->psz_uri = NULL;
        p_finder->pf_find( p_finder );
        module_unneed( p_finder, p_module );

        MergeSources( p_manager, p_finder->entries.p_elems, p_finder->entries.i_size );

        ARRAY_RESET( p_finder->entries );
    }
    vlc_object_release( p_finder );
}

static void *FinderThread( void *p_data )
{
    addons_manager_t *p_manager = p_data;
    int i_cancel;
    char *psz_uri;

    for( ;; )
    {
        vlc_mutex_lock( &p_manager->p_priv->finder.lock );
        mutex_cleanup_push( &p_manager->p_priv->finder.lock );
        while( p_manager->p_priv->finder.uris.i_size == 0 )
        {
            vlc_cond_wait( &p_manager->p_priv->finder.waitcond,
                           &p_manager->p_priv->finder.lock );
        }
        psz_uri = p_manager->p_priv->finder.uris.p_elems[0];
        ARRAY_REMOVE( p_manager->p_priv->finder.uris, 0 );
        vlc_cleanup_run();

        addons_finder_t *p_finder =
                vlc_custom_create( p_manager->p_priv->p_parent, sizeof( *p_finder ), "entries finder" );

        i_cancel = vlc_savecancel();
        if( p_finder != NULL )
        {
            p_finder->i_flags |= OBJECT_FLAGS_NOINTERACT;
            module_t *p_module;
            ARRAY_INIT( p_finder->entries );
            vlc_mutex_lock( &p_manager->p_priv->finder.lock );
            p_finder->psz_uri = psz_uri;
            vlc_mutex_unlock( &p_manager->p_priv->finder.lock );

            p_module = module_need( p_finder, "addons finder", NULL, false );
            if( p_module )
            {
                p_finder->pf_find( p_finder );
                module_unneed( p_finder, p_module );
                MergeSources( p_manager, p_finder->entries.p_elems, p_finder->entries.i_size );
            }
            ARRAY_RESET( p_finder->entries );
            free( psz_uri );
            vlc_object_release( p_finder );
        }

        vlc_event_t event;
        event.type = vlc_AddonsDiscoveryEnded;
        event.u.addon_generic_event.p_entry = NULL;
        vlc_event_send( p_manager->p_event_manager, &event );

        vlc_restorecancel( i_cancel );
        vlc_testcancel();
    }

    return NULL;
}

static int addons_manager_WriteCatalog( addons_manager_t *p_manager )
{
    int i_return = VLC_EGENERIC;

    addons_storage_t *p_storage =
        vlc_custom_create( p_manager->p_priv->p_parent, sizeof( *p_storage ), "entries storage" );
    p_storage->i_flags |= OBJECT_FLAGS_NOINTERACT;

    module_t *p_module = module_need( p_storage, "addons storage",
                                      "addons.store.install", true );
    if( p_module )
    {
        vlc_mutex_lock( &p_manager->p_priv->finder.lock );
        i_return = p_storage->pf_catalog( p_storage, p_manager->p_priv->finder.entries.p_elems,
                                          p_manager->p_priv->finder.entries.i_size );
        vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
        module_unneed( p_storage, p_module );
    }
    vlc_object_release( p_storage );

    return i_return;
}

int addons_manager_LoadCatalog( addons_manager_t *p_manager )
{
    LoadLocalStorage( p_manager );
    return VLC_SUCCESS;
}

static int installOrRemoveAddon( addons_manager_t *p_manager, addon_entry_t *p_entry, bool b_install )
{
    int i_return = VLC_EGENERIC;

    addons_storage_t *p_storage =
        vlc_custom_create( p_manager->p_priv->p_parent, sizeof( *p_storage ), "entries storage" );
    p_storage->i_flags |= OBJECT_FLAGS_NOINTERACT;

    module_t *p_module = module_need( p_storage, "addons storage",
                                      "addons.store.install", true );
    if( p_module )
    {
        if ( b_install )
            i_return = p_storage->pf_install( p_storage, p_entry );
        else
            i_return = p_storage->pf_remove( p_storage, p_entry );
        module_unneed( p_storage, p_module );
        msg_Dbg( p_manager->p_priv->p_parent, "InstallAddon returns %d", i_return );
        if ( i_return == VLC_SUCCESS )
        {
            /* Reset flags */
            vlc_mutex_lock( &p_entry->lock );
            p_entry->e_flags = ADDON_MANAGEABLE;
            vlc_mutex_unlock( &p_entry->lock );
        }
    }
    vlc_object_release( p_storage );

    return i_return;
}

static void *InstallerThread( void *p_data )
{
    addons_manager_t *p_manager = p_data;
    int i_ret, i_cancel;
    vlc_event_t event;
    event.type = vlc_AddonChanged;

    for( ;; )
    {
        vlc_mutex_lock( &p_manager->p_priv->installer.lock );
        mutex_cleanup_push( &p_manager->p_priv->installer.lock );
        while ( !p_manager->p_priv->installer.entries.i_size )
        {
            /* No queued addons */
            vlc_cond_wait( &p_manager->p_priv->installer.waitcond,
                           &p_manager->p_priv->installer.lock );
        }
        vlc_cleanup_pop();

        addon_entry_t *p_entry = p_manager->p_priv->installer.entries.p_elems[0];
        ARRAY_REMOVE( p_manager->p_priv->installer.entries, 0 );
        addon_entry_Hold( p_entry );
        vlc_mutex_unlock( &p_manager->p_priv->installer.lock );

        vlc_mutex_lock( &p_entry->lock );
        /* DO WORK */
        if ( p_entry->e_state == ADDON_INSTALLED )
        {
            p_entry->e_state = ADDON_UNINSTALLING;
            vlc_mutex_unlock( &p_entry->lock );

            /* notify */
            i_cancel = vlc_savecancel();
            event.u.addon_generic_event.p_entry = p_entry;
            vlc_event_send( p_manager->p_event_manager, &event );

            i_ret = installOrRemoveAddon( p_manager, p_entry, false );
            vlc_restorecancel( i_cancel );

            vlc_mutex_lock( &p_entry->lock );
            p_entry->e_state = ( i_ret == VLC_SUCCESS ) ? ADDON_NOTINSTALLED
                                                        : ADDON_INSTALLED;
            vlc_mutex_unlock( &p_entry->lock );
        }
        else if ( p_entry->e_state == ADDON_NOTINSTALLED )
        {
            p_entry->e_state = ADDON_INSTALLING;
            vlc_mutex_unlock( &p_entry->lock );

            /* notify */
            i_cancel = vlc_savecancel();
            event.u.addon_generic_event.p_entry = p_entry;
            vlc_event_send( p_manager->p_event_manager, &event );

            i_ret = installOrRemoveAddon( p_manager, p_entry, true );
            vlc_restorecancel( i_cancel );

            vlc_mutex_lock( &p_entry->lock );
            p_entry->e_state = ( i_ret == VLC_SUCCESS ) ? ADDON_INSTALLED
                                                        : ADDON_NOTINSTALLED;
            vlc_mutex_unlock( &p_entry->lock );
        }
        else
            vlc_mutex_unlock( &p_entry->lock );
        /* !DO WORK */

        i_cancel = vlc_savecancel();
        event.u.addon_generic_event.p_entry = p_entry;
        vlc_event_send( p_manager->p_event_manager, &event );
        vlc_restorecancel( i_cancel );

        addon_entry_Release( p_entry );

        i_cancel = vlc_savecancel();
        addons_manager_WriteCatalog( p_manager );
        vlc_restorecancel( i_cancel );
    }

    return NULL;
}

static int InstallEntry( addons_manager_t *p_manager, addon_entry_t *p_entry )
{
    if ( p_entry->e_type == ADDON_UNKNOWN ||
         p_entry->e_type == ADDON_PLUGIN ||
         p_entry->e_type == ADDON_OTHER )
        return VLC_EBADVAR;

    vlc_mutex_lock( &p_manager->p_priv->installer.lock );
    ARRAY_APPEND( p_manager->p_priv->installer.entries, p_entry );
    if( !p_manager->p_priv->installer.b_live )
    {
        if( vlc_clone( &p_manager->p_priv->installer.thread, InstallerThread, p_manager,
                       VLC_THREAD_PRIORITY_LOW ) )
        {
            msg_Err( p_manager->p_priv->p_parent,
                     "cannot spawn addons installer thread" );
            vlc_mutex_unlock( &p_manager->p_priv->installer.lock );
            return VLC_EGENERIC;
        }
        else
            p_manager->p_priv->installer.b_live = true;
    }
    vlc_mutex_unlock( &p_manager->p_priv->installer.lock );
    vlc_cond_signal( &p_manager->p_priv->installer.waitcond );
    return VLC_SUCCESS;
}

int addons_manager_Install( addons_manager_t *p_manager, const addon_uuid_t uuid )
{
    addon_entry_t *p_install_entry = getHeldEntryByUUID( p_manager, uuid );
    if ( ! p_install_entry ) return VLC_EGENERIC;
    int i_ret = InstallEntry( p_manager, p_install_entry );
    addon_entry_Release( p_install_entry );
    return i_ret;
}

int addons_manager_Remove( addons_manager_t *p_manager, const addon_uuid_t uuid )
{
    return addons_manager_Install( p_manager, uuid );
}
