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
#include "dialogs/extensions.hpp"

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
    b_unloading = false;
}

ExtensionsManager::~ExtensionsManager()
{
    if( p_extensions_manager )
    {
        module_unneed( p_extensions_manager, p_extensions_manager->p_module );
        vlc_object_release( p_extensions_manager );
    }
}

void ExtensionsManager::loadExtensions()
{
    if( !p_extensions_manager )
    {
        p_extensions_manager = ( extensions_manager_t* )
                    vlc_object_create( p_intf, sizeof( extensions_manager_t ) );
        if( !p_extensions_manager )
            return;
        vlc_object_attach( p_extensions_manager, p_intf );

        p_extensions_manager->p_module =
                module_need( p_extensions_manager, "extension", NULL, false );

        if( !p_extensions_manager->p_module )
        {
            msg_Err( p_intf, "Unable to load extensions module" );
            return;
        }

        /* Initialize dialog provider */
        p_edp = ExtensionsDialogProvider::getInstance( p_intf,
                                                       p_extensions_manager );
        if( !p_edp )
        {
            msg_Err( p_intf, "Unable to create dialogs provider for extensions" );
            return;
        }
        b_unloading = false;
    }
}

void ExtensionsManager::unloadExtensions()
{
    if( !p_extensions_manager )
        return;
    b_unloading = true;
    module_unneed( p_extensions_manager, p_extensions_manager->p_module );
    vlc_object_release( p_extensions_manager );
    p_extensions_manager = NULL;
    ExtensionsDialogProvider::killInstance();
}

void ExtensionsManager::menu( QMenu *current )
{
    QAction *action;
    assert( current != NULL );
    if( !isLoaded() )
    {
        // This case should not happen
        action = current->addAction( qtr( "Extensions not loaded" ) );
        action->setEnabled( false );
        return;
    }

    /* Some useless message */
    action = current->addAction( p_extensions_manager->extensions.i_size
                                 ? qtr( "Extensions found:" )
                                 : qtr( "No extensions found" ) );
    action->setEnabled( false );
    current->addSeparator();

    extension_t *p_ext = NULL;
    int i_ext = 0;
    FOREACH_ARRAY( p_ext, p_extensions_manager->extensions )
    {
        bool b_Active = extension_IsActivated( p_extensions_manager, p_ext );

        if( b_Active && extension_HasMenu( p_extensions_manager, p_ext ) )
        {
            QMenu *submenu = new QMenu( qfu( p_ext->psz_title ) );
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
                }
                if( !i_num )
                {
                    action = submenu->addAction( qtr( "Empty" ) );
                    action->setEnabled( false );
                }
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

    /* Possibility to unload the module */
    current->addSeparator();
    current->addAction( QIcon( ":/menu/quit" ), qtr( "Unload extensions" ),
                        this, SLOT( unloadExtensions() ) );
}

void ExtensionsManager::triggerMenu( int id )
{
    uint16_t i_ext = MENU_GET_EXTENSION( id );
    uint16_t i_action = MENU_GET_ACTION( id );

    if( (int) i_ext > p_extensions_manager->extensions.i_size )
    {
        msg_Dbg( p_intf, "can't trigger extension with wrong id %d",
                 (int) i_ext );
        return;
    }

    extension_t *p_ext = ARRAY_VAL( p_extensions_manager->extensions, i_ext );
    assert( p_ext != NULL);

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
