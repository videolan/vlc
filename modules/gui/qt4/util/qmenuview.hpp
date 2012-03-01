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

#ifndef _QMENU_VIEW_H_
#define _QMENU_VIEW_H_

#include <QMenu>
#include <QAbstractItemModel>

class QMenuView : public QMenu
{
    Q_OBJECT

public:
    QMenuView( QWidget * parent = 0, int iMaxVisibleCount = 0 );
    virtual ~QMenuView(){}

    /* Model */
    void setModel( QAbstractItemModel * model ) { m_model = model; }
    QAbstractItemModel * model() const { return m_model; }

    /* Size limit */
    void setMaximumItemCount( int count ) { iMaxVisibleCount = count; }

private:
    QAbstractItemModel *m_model;

    QAction *createActionFromIndex( QModelIndex index );

    void build( const QModelIndex &parent );

    int iMaxVisibleCount;

private slots:
    void rebuild();
    void activate(QAction*);

signals:
    void activated( const QModelIndex & index );
};

#endif
