/*****************************************************************************
 * Epg.cpp : Epg Viewer dialog
 ****************************************************************************
 * Copyright © 2010 VideoLAN and AUTHORS
 *
 * Authors:    Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/epg.hpp"

#include "components/epg/EPGWidget.hpp"

#include <QHBoxLayout>

EpgDialog::EpgDialog( intf_thread_t *_p_intf ): QVLCFrame( _p_intf )
{
    setTitle( "Program Guide" );

    QHBoxLayout *layout = new QHBoxLayout( this );
    EPGWidget *epg = new EPGWidget( this );

    layout->addWidget( epg );
}

EpgDialog::~EpgDialog()
{
}

