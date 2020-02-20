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
#include <vlc_interrupt.h>
#include "libvlc.h"

#include <vlc_addons.h>

/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/

typedef struct addon_entry_owner
{
    addon_entry_t entry;
    vlc_atomic_rc_t rc;
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
        vlc_interrupt_t *p_interrupt;
        DECL_ARRAY(char*) uris;
        DECL_ARRAY(addon_entry_t*) entries;
    } finder;

    struct
    {
        vlc_thread_t thread;
        vlc_cond_t waitcond;
        bool b_live;
        vlc_mutex_t lock;
        vlc_interrupt_t *p_interrupt;
        DECL_ARRAY(addon_entry_t*) entries;
    } installer;
};

static void *FinderThread( void * );
static void LoadLocalStorage( addons_manager_t *p_manager );

/*****************************************************************************
 * Public functions
 *****************************************************************************/

addon_entry_t * addon_entry_New(void)
{
    addon_entry_owner_t *owner = calloc( 1, sizeof(addon_entry_owner_t) );
    if( unlikely(owner == NULL) )
        return NULL;

    vlc_atomic_rc_init( &owner->rc );

    addon_entry_t *p_entry = &owner->entry;
    vlc_mutex_init( &p_entry->lock );
    ARRAY_INIT( p_entry->files );
    return p_entry;
}

addon_entry_t * addon_entry_Hold( addon_entry_t * p_entry )
{
    addon_entry_owner_t *owner = (addon_entry_owner_t *) p_entry;

    vlc_atomic_rc_inc( &owner->rc );
    return p_entry;
}

void addon_entry_Release( addon_entry_t * p_entry )
{
    addon_entry_owner_t *owner = (addon_entry_owner_t *) p_entry;

    if( !vlc_atomic_rc_dec( &owner->rc ) )
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
    ARRAY_FOREACH( p_file, p_entry->files )
    {
        free( p_file->psz_filename );
        free( p_file->psz_download_uri );
        free( p_file );
    }
    ARRAY_RESET( p_entry->files );

    free( owner );
}

addons_manager_t *addons_manager_New( vlc_object_t *p_this,
    const struct addons_manager_owner *restrict owner )
{
    addons_manager_t *p_manager = malloc( sizeof(addons_manager_t) );
    if ( !p_manager ) return NULL;

    p_manager->p_priv = malloc( sizeof(addons_manager_private_t) );
    if ( !p_manager->p_priv )
    {
        free( p_manager );
        return NULL;
    }

    p_manager->owner = *owner;
    p_manager->p_priv->p_parent = p_this;

    p_manager->p_priv->finder.p_interrupt = vlc_interrupt_create();
    p_manager->p_priv->installer.p_interrupt = vlc_interrupt_create();
    if ( !p_manager->p_priv->finder.p_interrupt ||
         !p_manager->p_priv->installer.p_interrupt )
    {
        if( p_manager->p_priv->finder.p_interrupt )
            vlc_interrupt_destroy( p_manager->p_priv->finder.p_interrupt );
        if( p_manager->p_priv->installer.p_interrupt )
            vlc_interrupt_destroy( p_manager->p_priv->installer.p_interrupt );
        free( p_manager->p_priv );
        free( p_manager );
        return NULL;
    }

#define INIT_QUEUE( name ) \
    p_manager->p_priv->name.b_live = false;\
    vlc_mutex_init( &p_manager->p_priv->name.lock );\
    vlc_cond_init( &p_manager->p_priv->name.waitcond );\
    ARRAY_INIT( p_manager->p_priv->name.entries );

    INIT_QUEUE( finder )
    INIT_QUEUE( installer )
    ARRAY_INIT( p_manager->p_priv->finder.uris );

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
        vlc_interrupt_kill( p_manager->p_priv->finder.p_interrupt );
        vlc_join( p_manager->p_priv->finder.thread, NULL );
    }

    vlc_mutex_lock( &p_manager->p_priv->installer.lock );
    b_live = p_manager->p_priv->installer.b_live;
    vlc_mutex_unlock( &p_manager->p_priv->installer.lock );
    if ( b_live )
    {
        vlc_interrupt_kill( p_manager->p_priv->installer.p_interrupt );
        vlc_join( p_manager->p_priv->installer.thread, NULL );
    }

    addon_entry_t *p_entry;

#define FREE_QUEUE( name ) \
    ARRAY_FOREACH( p_entry, p_manager->p_priv->name.entries )\
        addon_entry_Release( p_entry );\
    ARRAY_RESET( p_manager->p_priv->name.entries );\
    vlc_interrupt_destroy( p_manager->p_priv->name.p_interrupt );

    FREE_QUEUE( finder )
    FREE_QUEUE( installer )

    char *psz_uri;
    ARRAY_FOREACH( psz_uri, p_manager->p_priv->finder.uris )
       free( psz_uri );
    ARRAY_RESET( p_manager->p_priv->finder.uris );

    free( p_manager->p_priv );
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
    addon_entry_t *p_entry;
    ARRAY_FOREACH( p_entry, p_manager->p_priv->finder.entries )
    {
        if ( !memcmp( p_entry->uuid, uuid, sizeof( addon_uuid_t ) ) )
        {
            p_return = p_entry;
            addon_entry_Hold( p_return );
            break;
        }
    }
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
            p_manager->owner.addon_found( p_manager, p_entry );
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
    p_finder->obj.no_interact = true;

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
    vlc_object_delete(p_finder);
}

static void finder_thread_interrupted( void* p_data )
{
    addons_manager_t *p_manager = p_data;
    vlc_mutex_lock( &p_manager->p_priv->finder.lock );
    p_manager->p_priv->finder.b_live = false;
    vlc_cond_signal( &p_manager->p_priv->finder.waitcond );
    vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
}

static void *FinderThread( void *p_data )
{
    addons_manager_t *p_manager = p_data;
    int i_cancel = vlc_savecancel();
    vlc_interrupt_set( p_manager->p_priv->finder.p_interrupt );

    vlc_mutex_lock( &p_manager->p_priv->finder.lock );
    while( p_manager->p_priv->finder.b_live )
    {
        char *psz_uri;

        vlc_interrupt_register( finder_thread_interrupted, p_data );
        while( p_manager->p_priv->finder.uris.i_size == 0 &&
               p_manager->p_priv->finder.b_live )
        {
            vlc_cond_wait( &p_manager->p_priv->finder.waitcond,
                           &p_manager->p_priv->finder.lock );
        }
        vlc_interrupt_unregister();
        if( !p_manager->p_priv->finder.b_live )
            break;
        psz_uri = p_manager->p_priv->finder.uris.p_elems[0];
        ARRAY_REMOVE( p_manager->p_priv->finder.uris, 0 );

        vlc_mutex_unlock( &p_manager->p_priv->finder.lock );

        addons_finder_t *p_finder =
                vlc_custom_create( p_manager->p_priv->p_parent, sizeof( *p_finder ), "entries finder" );

        if( p_finder != NULL )
        {
            p_finder->obj.no_interact = true;
            module_t *p_module;
            ARRAY_INIT( p_finder->entries );
            p_finder->psz_uri = psz_uri;

            p_module = module_need( p_finder, "addons finder", NULL, false );
            if( p_module )
            {
                p_finder->pf_find( p_finder );
                module_unneed( p_finder, p_module );
                MergeSources( p_manager, p_finder->entries.p_elems, p_finder->entries.i_size );
            }
            ARRAY_RESET( p_finder->entries );
            free( psz_uri );
            vlc_object_delete(p_finder);
        }

        p_manager->owner.discovery_ended( p_manager );
        vlc_mutex_lock( &p_manager->p_priv->finder.lock );
    }

    vlc_mutex_unlock( &p_manager->p_priv->finder.lock );
    vlc_restorecancel( i_cancel );
    return NULL;
}

static int addons_manager_WriteCatalog( addons_manager_t *p_manager )
{
    int i_return = VLC_EGENERIC;

    addons_storage_t *p_storage =
        vlc_custom_create( p_manager->p_priv->p_parent, sizeof( *p_storage ), "entries storage" );
    p_storage->obj.no_interact = true;

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
    vlc_object_delete(p_storage);

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
    p_storage->obj.no_interact = true;

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
    vlc_object_delete(p_storage);

    return i_return;
}

static void installer_thread_interrupted( void* p_data )
{
    addons_manager_t *p_manager = p_data;
    vlc_mutex_lock( &p_manager->p_priv->installer.lock );
    p_manager->p_priv->installer.b_live = false;
    vlc_cond_signal( &p_manager->p_priv->installer.waitcond );
    vlc_mutex_unlock( &p_manager->p_priv->installer.lock );
}

static void *InstallerThread( void *p_data )
{
    addons_manager_t *p_manager = p_data;
    int i_cancel = vlc_savecancel();
    vlc_interrupt_set( p_manager->p_priv->installer.p_interrupt );
    int i_ret;

    vlc_mutex_lock( &p_manager->p_priv->installer.lock );
    while( p_manager->p_priv->installer.b_live )
    {
        vlc_interrupt_register( installer_thread_interrupted, p_data );
        while ( !p_manager->p_priv->installer.entries.i_size &&
                p_manager->p_priv->installer.b_live )
        {
            /* No queued addons */
            vlc_cond_wait( &p_manager->p_priv->installer.waitcond,
                           &p_manager->p_priv->installer.lock );
        }
        vlc_interrupt_unregister();
        if( !p_manager->p_priv->installer.b_live )
            break;

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
            p_manager->owner.addon_changed( p_manager, p_entry );

            i_ret = installOrRemoveAddon( p_manager, p_entry, false );

            vlc_mutex_lock( &p_entry->lock );
            p_entry->e_state = ( i_ret == VLC_SUCCESS ) ? ADDON_NOTINSTALLED
                                                        : ADDON_INSTALLED;
        }
        else if ( p_entry->e_state == ADDON_NOTINSTALLED )
        {
            p_entry->e_state = ADDON_INSTALLING;
            vlc_mutex_unlock( &p_entry->lock );

            /* notify */
            p_manager->owner.addon_changed( p_manager, p_entry );

            i_ret = installOrRemoveAddon( p_manager, p_entry, true );

            vlc_mutex_lock( &p_entry->lock );
            p_entry->e_state = ( i_ret == VLC_SUCCESS ) ? ADDON_INSTALLED
                                                        : ADDON_NOTINSTALLED;
        }
        vlc_mutex_unlock( &p_entry->lock );
        /* !DO WORK */

        p_manager->owner.addon_changed( p_manager, p_entry );

        addon_entry_Release( p_entry );

        addons_manager_WriteCatalog( p_manager );
        vlc_mutex_lock( &p_manager->p_priv->installer.lock );
    }
    vlc_mutex_unlock( &p_manager->p_priv->installer.lock );
    vlc_restorecancel( i_cancel );
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
