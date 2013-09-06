/*****************************************************************************
 * QMenuView
 ****************************************************************************
 * Copyright © 2011 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#include "qt4.hpp"
#include "util/qmenuview.hpp"

#include "components/playlist/vlc_model.hpp" /* data( IsCurrentRole ) */

#include <QFont>
#include <QVariant>
#include <assert.h>

/***
 * This is a simple view, like a QListView, but displayed as a QMenu
 * So far, this is quite limited.
 *
 * This is now linked to VLC's models. It should be splittable in the future.
 *
 * TODO: - limit the width of the entries
 *       - deal with a root item
 ***/

Q_DECLARE_METATYPE(QPersistentModelIndex); // So we can store it in a QVariant

QMenuView::QMenuView( QWidget * parent, int _iMaxVisibleCount )
          : QMenu( parent ), iMaxVisibleCount( _iMaxVisibleCount )
{
    m_model = NULL;

    /* Rebuild the Menu just before showing it */
    CONNECT( this, aboutToShow(), this, rebuild() );

    /* */
    CONNECT( this, triggered(QAction*), this, activate(QAction*) );
}

/* */
void QMenuView::rebuild()
{
    if( !m_model )
        return;

    /* Clear all Items */
    clear();

    /* Rebuild from root */
    build( QModelIndex() );

    if( isEmpty() )
        addAction( qtr( "Empty" ) )->setDisabled( true );
}

/* */
void QMenuView::build( const QModelIndex &parent )
{
    int i_count = iMaxVisibleCount == 0 ? m_model->rowCount( parent )
                                         : __MIN( iMaxVisibleCount, m_model->rowCount( parent ) );
    for( int i = 0; i < i_count; i++ )
    {
        QModelIndex idx = m_model->index(i, 0, parent);
        if( m_model->hasChildren( idx ) )
        {
            build( idx );
        }
        else
        {
            addAction( createActionFromIndex( idx ) );
        }
    }
}

/* Create individual actions */
QAction* QMenuView::createActionFromIndex( QModelIndex index )
{
    QIcon icon = qvariant_cast<QIcon>( index.data( Qt::DecorationRole ) );
    QAction * action = new QAction( icon, index.data().toString(), this );

    /* Display in bold the active element */
    if( index.data( VLCModel::IsCurrentRole ).toBool() )
    {
        QFont font; font.setBold ( true );
        action->setFont( font );
    }

    /* Some items could be hypothetically disabled */
    action->setEnabled( index.flags().testFlag( Qt::ItemIsEnabled ) );

    /* */
    QVariant variant; variant.setValue( QPersistentModelIndex( index ) );
    action->setData( variant );

    return action;
}

/* QMenu action trigger */
void QMenuView::activate( QAction* action )
{
    assert( m_model );

    QVariant variant = action->data();
    if( variant.canConvert<QPersistentModelIndex>() )
    {
        emit activated( variant.value<QPersistentModelIndex>());
    }
}

