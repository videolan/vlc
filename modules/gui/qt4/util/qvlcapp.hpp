/*****************************************************************************
 * qvlcapp.hpp : A few helpers
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
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


#ifndef _QVLC_APP_H_
#define _QVLC_APP_H_

#include <QApplication>
#include <QEvent>

#if defined(Q_WS_WIN)
#   include "qt4.hpp"
#   include <windows.h>
#   include "input_manager.hpp"
#endif

class QVLCApp : public QApplication
{
    Q_OBJECT

private slots:
    void doQuit()
    {
        closeAllWindows();
        quit();
    }

public:
    QVLCApp( int & argc, char ** argv ) : QApplication( argc, argv, true )
    {
        connect( this, SIGNAL(quitSignal()), this, SLOT(doQuit()) );
    }

    static void triggerQuit()
    {
        QVLCApp *app = qobject_cast<QVLCApp*>( instance() );
        if( app )
            emit app->quitSignal();
    }

signals:
    void quitSignal();

};
#endif
