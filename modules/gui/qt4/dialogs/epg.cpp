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

#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

EpgDialog::EpgDialog( intf_thread_t *_p_intf ): QVLCFrame( _p_intf )
{
    setWindowTitle( "Program Guide" );

    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setMargin( 0 );
    QSplitter *splitter = new QSplitter( this );
    EPGWidget *epg = new EPGWidget( this );
    splitter->addWidget( epg );
    splitter->setOrientation(Qt::Vertical);

    QGroupBox *descBox = new QGroupBox( qtr( "Description" ), this );

    QVBoxLayout *boxLayout = new QVBoxLayout( descBox );

    description = new QLabel( this );
    description->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel );
    description->setAutoFillBackground( true );

    QPalette palette;
    palette.setBrush(QPalette::Active, QPalette::Window, palette.brush( QPalette::Base ) );
    description->setPalette( palette );

    title = new QLabel( qtr( "Title" ), this );

    boxLayout->addWidget( title );
    boxLayout->addWidget( description, 10 );

    splitter->addWidget( epg );
    splitter->addWidget( descBox );
    layout->addWidget( splitter );

    CONNECT( epg, itemSelectionChanged( EPGEvent *), this, showEvent( EPGEvent *) );

    QPushButton *close = new QPushButton( qtr( "&Close" ) );
    layout->addWidget( close, 0, Qt::AlignRight );
    BUTTONACT( close, close() );

    resize( 650, 400 );
}

EpgDialog::~EpgDialog()
{
}

void EpgDialog::showEvent( EPGEvent *event )
{
    if( !event ) return;

    title->setText( event->name );
    description->setText( event->description );
}

