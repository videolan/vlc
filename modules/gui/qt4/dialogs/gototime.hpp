/*****************************************************************************
 * GotoTime.hpp : GotoTime dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#ifndef QVLC_GOTOTIME_DIALOG_H_
#define QVLC_GOTOTIME_DIALOG_H_ 1

#include "util/qvlcframe.hpp"

class QTimeEdit;

class GotoTimeDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static GotoTimeDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new GotoTimeDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf );
        return instance;
    }
    virtual ~GotoTimeDialog();

private:
    GotoTimeDialog( QWidget *, intf_thread_t * );
    static GotoTimeDialog *instance;
    QTimeEdit *timeEdit;
private slots:
    void close();
    void cancel();
};

#endif
