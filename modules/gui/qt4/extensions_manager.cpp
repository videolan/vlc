/*****************************************************************************
 * extensions_manager.cpp: Extensions manager for Qt
 ****************************************************************************
 * Copyright (C) 2009-2010 VideoLAN and authors
 * $Id$
 *
 * Authors: Jean-Philippe André < jpeg # videolan.org >
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

#include "extensions_manager.hpp"
#include "input_manager.hpp"
#include "dialogs/extensions.hpp"

#include <vlc_modules.h>
#include "assert.h"

#include <QMenu>
#include <QAction>
#include <QSignalMapper>
#include <QIcon>

#define MENU_MAP(a,e) ((uint32_t)( (((uint16_t)a) << 16) | ((uint16_t)e) ))
#define MENU_GET_ACTION(a) ( (uint16_t)( ((uint32_t)a) >> 16 ) )
#define MENU_GET_EXTENSION(a) ( (uint16_t)( ((uint32_t)a) & 0xFFFF ) )

ExtensionsManager* ExtensionsManager::instance = NULL;

ExtensionsManager::ExtensionsManager( intf_thread_t *_p_intf, QObject *parent )
        : QObject( parent ), p_intf( _p_intf ), p_extensions_manager( NULL )
        , p_edp( NULL )
{
    assert( ExtensionsManager::instance == NULL );
    instance = this;

    menuMapper = new QSignalMapper( this );
    CONNECT( menuMapper, mapped( int ), this, triggerMenu( int ) );
    CONNECT( THEMIM->getIM(), playingStatusChanged( int ), this, playingChanged( int ) );
    DCONNECT( THEMIM, inputChanged( input_thread_t* ),
              this, inputChanged( input_thread_t* ) );
    CONNECT( THEMIM->getIM(), metaChanged( input_item_t* ),
             this, metaChanged( input_item_t* ) );
    b_unloading = false;
    b_failed = false;
}

ExtensionsManager::~ExtensionsManager()
{
    msg_Dbg( p_intf, "Killing extension dialog provider" );
    ExtensionsDialogProvider::killInstance();
    if( p_extensions_manager )
    {
        module_unneed( p_extensions_manager, p_extensions_manager->p_module );
        vlc_object_release( p_extensions_manager );
    }
}

bool ExtensionsManager::loadExtensions()
{
    if( !p_extensions_manager )
    {
        p_extensions_manager = ( extensions_manager_t* )
                    vlc_object_create( p_intf, sizeof( extensions_manager_t ) );
        if( !p_extensions_manager )
        {
            b_failed = true;
            emit extensionsUpdated();
            return false;
        }

        p_extensions_manager->p_module =
                module_need( p_extensions_manager, "extension", NULL, false );

        if( !p_extensions_manager->p_module )
        {
            msg_Err( p_intf, "Unable to load extensions module" );
            vlc_object_release( p_extensions_manager );
            p_extensions_manager = NULL;
            b_failed = true;
            emit extensionsUpdated();
            return false;
        }

        /* Initialize dialog provider */
        p_edp = ExtensionsDialogProvider::getInstance( p_intf,
                                                       p_extensions_manager );
        if( !p_edp )
        {
            msg_Err( p_intf, "Unable to create dialogs provider for extensions" );
            module_unneed( p_extensions_manager,
                           p_extensions_manager->p_module );
            vlc_object_release( p_extensions_manager );
            p_extensions_manager = NULL;
            b_failed = true;
            emit extensionsUpdated();
            return false;
        }
        b_unloading = false;
    }
    b_failed = false;
    emit extensionsUpdated();
    return true;
}

void ExtensionsManager::unloadExtensions()
{
    if( !p_extensions_manager )
        return;
    b_unloading = true;
    ExtensionsDialogProvider::killInstance();
    module_unneed( p_extensions_manager, p_extensions_manager->p_module );
    vlc_object_release( p_extensions_manager );
    p_extensions_manager = NULL;
}

void ExtensionsManager::reloadExtensions()
{
    unloadExtensions();
    loadExtensions();
    emit extensionsUpdated();
}

void ExtensionsManager::menu( QMenu *current )
{
    assert( current != NULL );
    if( !isLoaded() )
    {
        // This case can happen: do nothing
        return;
    }

    vlc_mutex_lock( &p_extensions_manager->lock );

    QAction *action;
    extension_t *p_ext = NULL;
    int i_ext = 0;
    FOREACH_ARRAY( p_ext, p_extensions_manager->extensions )
    {
        bool b_Active = extension_IsActivated( p_extensions_manager, p_ext );

        if( b_Active && extension_HasMenu( p_extensions_manager, p_ext ) )
        {
            QMenu *submenu = new QMenu( qfu( p_ext->psz_title ), current );
            char **ppsz_titles = NULL;
            uint16_t *pi_ids = NULL;
            size_t i_num = 0;
            action = current->addMenu( submenu );

            action->setCheckable( true );
            action->setChecked( true );

            if( extension_GetMenu( p_extensions_manager, p_ext,
                                   &ppsz_titles, &pi_ids ) == VLC_SUCCESS )
            {
                for( int i = 0; ppsz_titles[i] != NULL; ++i )
                {
                    ++i_num;
                    action = submenu->addAction( qfu( ppsz_titles[i] ) );
                    menuMapper->setMapping( action,
                                            MENU_MAP( pi_ids[i], i_ext ) );
                    CONNECT( action, triggered(), menuMapper, map() );
                    free( ppsz_titles[i] );
                }
                if( !i_num )
                {
                    action = submenu->addAction( qtr( "Empty" ) );
                    action->setEnabled( false );
                }
                free( ppsz_titles );
                free( pi_ids );
            }
            else
            {
                msg_Warn( p_intf, "Could not get menu for extension '%s'",
                          p_ext->psz_title );
                action = submenu->addAction( qtr( "Empty" ) );
                action->setEnabled( false );
            }

            submenu->addSeparator();
            action = submenu->addAction( QIcon( ":/menu/quit" ),
                                         qtr( "Deactivate" ) );
            menuMapper->setMapping( action, MENU_MAP( 0, i_ext ) );
            CONNECT( action, triggered(), menuMapper, map() );
        }
        else
        {
            action = current->addAction( qfu( p_ext->psz_title ) );
            menuMapper->setMapping( action, MENU_MAP( 0, i_ext ) );
            CONNECT( action, triggered(), menuMapper, map() );

            if( !extension_TriggerOnly( p_extensions_manager, p_ext ) )
            {
                action->setCheckable( true );
                action->setChecked( b_Active );
            }
        }
        i_ext++;
    }
    FOREACH_END()

    vlc_mutex_unlock( &p_extensions_manager->lock );
}

void ExtensionsManager::triggerMenu( int id )
{
    uint16_t i_ext = MENU_GET_EXTENSION( id );
    uint16_t i_action = MENU_GET_ACTION( id );

    vlc_mutex_lock( &p_extensions_manager->lock );

    if( (int) i_ext > p_extensions_manager->extensions.i_size )
    {
        msg_Dbg( p_intf, "can't trigger extension with wrong id %d",
                 (int) i_ext );
        vlc_mutex_unlock( &p_extensions_manager->lock );
        return;
    }

    extension_t *p_ext = ARRAY_VAL( p_extensions_manager->extensions, i_ext );
    assert( p_ext != NULL);

    vlc_mutex_unlock( &p_extensions_manager->lock );

    if( i_action == 0 )
    {
        msg_Dbg( p_intf, "activating or triggering extension '%s'",
                 p_ext->psz_title );

        if( extension_TriggerOnly( p_extensions_manager, p_ext ) )
        {
            extension_Trigger( p_extensions_manager, p_ext );
        }
        else
        {
            if( !extension_IsActivated( p_extensions_manager, p_ext ) )
                extension_Activate( p_extensions_manager, p_ext );
            else
                extension_Deactivate( p_extensions_manager, p_ext );
        }
    }
    else
    {
        msg_Dbg( p_intf, "triggering extension '%s', on menu with id = 0x%x",
                 p_ext->psz_title, i_action );

        extension_TriggerMenu( p_extensions_manager, p_ext, i_action );
    }
}

void ExtensionsManager::inputChanged( input_thread_t* p_input )
{
    //This is unlikely, but can happen if no extension modules can be loaded.
    if ( p_extensions_manager == NULL )
        return ;
    vlc_mutex_lock( &p_extensions_manager->lock );

    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_extensions_manager->extensions )
    {
        if( extension_IsActivated( p_extensions_manager, p_ext ) )
        {
            extension_SetInput( p_extensions_manager, p_ext, p_input );
        }
    }
    FOREACH_END()

    vlc_mutex_unlock( &p_extensions_manager->lock );
}

void ExtensionsManager::playingChanged( int state )
{
    //This is unlikely, but can happen if no extension modules can be loaded.
    if ( p_extensions_manager == NULL )
        return ;
    vlc_mutex_lock( &p_extensions_manager->lock );

    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_extensions_manager->extensions )
    {
        if( extension_IsActivated( p_extensions_manager, p_ext ) )
        {
            extension_PlayingChanged( p_extensions_manager, p_ext, state );
        }
    }
    FOREACH_END()

    vlc_mutex_unlock( &p_extensions_manager->lock );
}

void ExtensionsManager::metaChanged( input_item_t* )
{
    //This is unlikely, but can happen if no extension modules can be loaded.
    if ( p_extensions_manager == NULL )
        return ;
    vlc_mutex_lock( &p_extensions_manager->lock );
    extension_t *p_ext;
    FOREACH_ARRAY( p_ext, p_extensions_manager->extensions )
    {
        if( extension_IsActivated( p_extensions_manager, p_ext ) )
        {
            extension_MetaChanged( p_extensions_manager, p_ext );
        }
    }
    FOREACH_END()
    vlc_mutex_unlock( &p_extensions_manager->lock );
}
