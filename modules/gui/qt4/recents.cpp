/*****************************************************************************
 * recents.cpp : Recents MRL (menu)
 *****************************************************************************
 * Copyright © 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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


#include "recents.hpp"
#include "dialogs_provider.hpp"

#include <QList>
#include <QString>
#include <QAction>
#include <QSettings>
#include <QRegExp>
#include <QSignalMapper>

RecentsMRL* RecentsMRL::instance = NULL;

RecentsMRL::RecentsMRL( intf_thread_t *_p_intf ) : p_intf( _p_intf )
{
    stack = new QList<QString>;
    signalMapper = new QSignalMapper(this);
    CONNECT( signalMapper,
            mapped(const QString & ),
            DialogsProvider::getInstance( p_intf ),
            playMRL( const QString & ) );

    isActive = config_GetInt( p_intf, "qt-recentplay" );
    filter = new QRegExp(
            qfu( config_GetPsz( p_intf, "qt-recentplay-filter" ) ),
            Qt::CaseInsensitive );

    load();
    if ( !isActive ) clear();
}

RecentsMRL::~RecentsMRL()
{
    delete stack;
}

void RecentsMRL::addRecent( const QString &mrl )
{
    if ( !isActive || filter->indexIn( mrl ) >= 0 )
        return;

    msg_Dbg( p_intf, "Adding a new MRL to recent ones: %s", qtu( mrl ) );
    int i_index = stack->indexOf( mrl );
    if( 0 <= i_index )
    {
        /* move to the front */
        stack->move( i_index, 0 );
    }
    else
    {
        stack->prepend( mrl );
        if( stack->size() > RECENTS_LIST_SIZE )
            stack->takeLast();
    }
    emit updated();
    save();
}

void RecentsMRL::clear()
{
    if ( stack->isEmpty() )
        return;
    stack->clear();
    emit updated();
    save();
}

QList<QString> RecentsMRL::recents()
{
    return QList<QString>(*stack);
}

void RecentsMRL::load()
{
    QStringList list = getSettings()->value( "RecentsMRL/list" ).toStringList();

    for( int i = 0; i < list.size(); ++i )
    {
        if (filter->indexIn( list.at(i) ) == -1)
            stack->append( list.at(i) );
    }
}

void RecentsMRL::save()
{
    QStringList list;

    for( int i = 0; i < stack->size(); ++i )
        list << stack->at(i);

    getSettings()->setValue( "RecentsMRL/list", list );
}

