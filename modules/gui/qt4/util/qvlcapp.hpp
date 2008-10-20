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

class QVLCApp : public QApplication
{
public:
    QVLCApp( int & argc, char ** argv, bool GUIenabled ) : QApplication( argc,
            argv, GUIenabled ) {}

#if defined(Q_WS_WIN)
protected:
    virtual bool winEventFilter( MSG *msg, long *result )
    {
        switch( msg->message )
        {
            case 0x0319: /* WM_APPCOMMAND 0x0319 */
                DefWindowProc( msg->hwnd, msg->message,
                               msg->wParam, msg->lParam );
                break;
        }
        return false;
    }
#endif
};

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
