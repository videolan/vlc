/*****************************************************************************
 * views.hpp : Custom views
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: qvlcframe.hpp 16283 2006-08-17 18:16:09Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _QVLCVIEWS_H_
#define _QVLCVIEWS_H_

#include <QMouseEvent>
#include <QTreeView>
#include <QCursor>
#include <QPoint>
#include <QModelIndex>

class QVLCTreeView : public QTreeView
{
    Q_OBJECT;
public:
    QVLCTreeView( QWidget * parent ) : QTreeView( parent )
    {
    };
    virtual ~QVLCTreeView()   {};

    void mouseReleaseEvent(QMouseEvent* e )
    {
        if( e->button() & Qt::RightButton )
        {
            emit rightClicked( indexAt( QPoint( e->x(), e->y() ) ), QCursor::pos() );
        }
    }
signals:
    void rightClicked( QModelIndex, QPoint  );
};
#endif
