/*****************************************************************************
 * qt4.hpp : QT4 interface
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

#ifndef _QVLC_H_
#define _QVLC_H_

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <QEvent>

class QApplication;
class MainInterface;
class DialogsProvider;

struct intf_sys_t
{
    QApplication *p_app;
    MainInterface *p_mi;

    msg_subscription_t *p_sub; ///< Subscription to the message bank
};

static int DialogEvent_Type = QEvent::User + 1;

class DialogEvent : public QEvent
{
public:
    DialogEvent( int _i_dialog, int _i_arg, intf_dialog_args_t *_p_arg ) :
                 QEvent( (QEvent::Type)(DialogEvent_Type) )
    {
        i_dialog = _i_dialog;
        i_arg = _i_arg;
        p_arg = _p_arg;
    };
    virtual ~DialogEvent() {};

    int i_arg, i_dialog;
    intf_dialog_args_t *p_arg;
};

#endif
