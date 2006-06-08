/*****************************************************************************
 * dialogs_provider.hpp : Dialogs provider
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

#ifndef _DIALOGS_PROVIDER_H_
#define _DIALOGS_PROVIDER_H_

#include <QObject>
#include <QTimer>
#include <QApplication>
#include <vlc/vlc.h>

class QEvent;

class DialogsProvider : public QObject
{
    Q_OBJECT;
public:
    static DialogsProvider *getInstance( intf_thread_t *p_intf )
    {
        if( !instance )
            instance = new DialogsProvider( p_intf );
        return instance;
    }
    virtual ~DialogsProvider();
    QTimer *idle_timer;
    QTimer *fixed_timer;
protected:
    void customEvent( QEvent *);
private:
    DialogsProvider( intf_thread_t *);
    intf_thread_t *p_intf;
    static DialogsProvider *instance;

public slots:
    void playlistDialog();
    void streaminfoDialog();
    void prefsDialog();
    void messagesDialog();
    void openDialog( int );
    void popupMenu( int );
};


#endif
