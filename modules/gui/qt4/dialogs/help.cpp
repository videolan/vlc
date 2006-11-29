/*****************************************************************************
 * Help.cpp : Help and About dialogs
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

#include "dialogs/help.hpp"

#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "qt4.hpp"

HelpDialog *HelpDialog::instance = NULL;

HelpDialog::HelpDialog( intf_thread_t *_p_intf) :  QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Help" ) );
    resize(600, 400);

    QGridLayout *layout = new QGridLayout(this);
    QPushButton *closeButton = new QPushButton(qtr("&Close"));

    BUTTONACT( closeButton, close() );
}

HelpDialog::~HelpDialog()
{
}
void HelpDialog::close()
{
    this->toggleVisible();
}

AboutDialog *AboutDialog::instance = NULL;

AboutDialog::AboutDialog( intf_thread_t *_p_intf) :  QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "About" ) );
    resize(600, 400);

    QGridLayout *layout = new QGridLayout(this);
    QPushButton *closeButton = new QPushButton(qtr("&Close"));

    BUTTONACT( closeButton, close() );
}

AboutDialog::~AboutDialog()
{
}
void AboutDialog::close()
{
    this->toggleVisible();
}
