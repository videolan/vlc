/*****************************************************************************
 * GotoTime.cpp : GotoTime and About dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: Messages.cpp 16024 2006-07-13 13:51:05Z xtophe $
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

#include "dialogs/gototime.hpp"

#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "qt4.hpp"

#include <QTextBrowser>
#include <QTabWidget>
#include <QFile>
#include <QLabel>

GotoTimeDialog *GotoTimeDialog::instance = NULL;

GotoTimeDialog::GotoTimeDialog( intf_thread_t *_p_intf) :  QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "GotoTime" ) );
    resize(600, 500);

    QGridLayout *layout = new QGridLayout(this);
    QPushButton *closeButton = new QPushButton(qtr("&Close"));

    layout->addWidget(closeButton, 1, 3);

    BUTTONACT( closeButton, close() );
}

GotoTimeDialog::~GotoTimeDialog()
{
}

void GotoTimeDialog::close()
{
    this->toggleVisible();
}
