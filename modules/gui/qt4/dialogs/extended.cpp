/*****************************************************************************
 * extended.cpp : Extended controls - Undocked
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: streaminfo.cpp 16687 2006-09-17 12:15:42Z jb $
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

#include <QTabWidget>
#include <QBoxLayout>

#include "dialogs/extended.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "components/extended_panels.hpp"
#include "qt4.hpp"

ExtendedDialog *ExtendedDialog::instance = NULL;

ExtendedDialog::ExtendedDialog( intf_thread_t *_p_intf ): QVLCFrame( _p_intf )
{
    QHBoxLayout *l = new QHBoxLayout( this );
    QTabWidget *tab = new QTabWidget( this );

    l->addWidget( tab );

    setWindowTitle( qtr( "Extended controls" ) );
    Equalizer *foo = new Equalizer( p_intf, this );

    tab->addTab( foo, qtr( "Equalizer" ) );
}

ExtendedDialog::~ExtendedDialog()
{
}

