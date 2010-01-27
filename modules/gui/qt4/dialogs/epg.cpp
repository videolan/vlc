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
#include <QSplitter>
#include <QLabel>
#include <QGroupBox>

EpgDialog::EpgDialog( intf_thread_t *_p_intf ): QVLCFrame( _p_intf )
{
    setWindowTitle( "Program Guide" );

    QHBoxLayout *layout = new QHBoxLayout( this );
    QSplitter *splitter = new QSplitter( this );
    EPGWidget *epg = new EPGWidget( this );
    splitter->addWidget( epg );
    splitter->setOrientation(Qt::Vertical);

    QGroupBox *descBox = new QGroupBox( qtr( "Description" ), this );

    QHBoxLayout *boxLayout = new QHBoxLayout( descBox );

    description = new QLabel( this );
    description->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel );
    description->setAutoFillBackground( true );

    QPalette palette;
    palette.setBrush(QPalette::Active, QPalette::Window, palette.brush( QPalette::Base ) );
    description->setPalette( palette );

    boxLayout->addWidget( description );

    splitter->addWidget( epg );
    splitter->addWidget( descBox );
    layout->addWidget( splitter );

    CONNECT( epg, descriptionChanged( EPGEvent *), this, showEvent( EPGEvent *) );
}

EpgDialog::~EpgDialog()
{
}

void EpgDialog::showEvent( EPGEvent *event )
{
    if( !event ) return;

    description->setText( event->description );
}
