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
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QTimer>

#include "qt4.hpp"
#include "input_manager.hpp"

EpgDialog::EpgDialog( intf_thread_t *_p_intf ): QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Program Guide" ) );

    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setMargin( 0 );
    epg = new EPGWidget( this );

    QGroupBox *descBox = new QGroupBox( qtr( "Description" ), this );

    QVBoxLayout *boxLayout = new QVBoxLayout( descBox );

    description = new QTextEdit( this );
    description->setReadOnly( true );
    description->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel );
    description->setAutoFillBackground( true );
    description->setAlignment( Qt::AlignLeft | Qt::AlignTop );
    description->setFixedHeight( 100 );

    QPalette palette;
    palette.setBrush(QPalette::Active, QPalette::Window, palette.brush( QPalette::Base ) );
    description->setPalette( palette );

    title = new QLabel( qtr( "Title" ), this );
    title->setWordWrap( true );

    boxLayout->addWidget( title );
    boxLayout->addWidget( description );

    layout->addWidget( epg, 10 );
    layout->addWidget( descBox );

    CONNECT( epg, itemSelectionChanged( EPGItem *), this, showEvent( EPGItem *) );
    CONNECT( THEMIM->getIM(), epgChanged(), this, updateInfos() );
    CONNECT( THEMIM, inputChanged( input_thread_t * ), this, updateInfos() );

    QDialogButtonBox *buttonsBox = new QDialogButtonBox( this );

#if 0
    QPushButton *update = new QPushButton( qtr( "Update" ) ); // Temporary to test
    buttonsBox->addButton( update, QDialogButtonBox::ActionRole );
    BUTTONACT( update, updateInfos() );
#endif

    buttonsBox->addButton( new QPushButton( qtr( "&Close" ) ),
                           QDialogButtonBox::RejectRole );
    boxLayout->addWidget( buttonsBox );
    CONNECT( buttonsBox, rejected(), this, close() );

    timer = new QTimer( this );
    timer->setSingleShot( true );
    timer->setInterval( 1000 * 60 );
    CONNECT( timer, timeout(), this, updateInfos() );

    updateInfos();
    readSettings( "EPGDialog", QSize( 650, 450 ) );
}

EpgDialog::~EpgDialog()
{
    writeSettings( "EPGDialog" );
}

void EpgDialog::showEvent( EPGItem *epgItem )
{
    if( !epgItem ) return;

    QDateTime end = epgItem->start().addSecs( epgItem->duration() );
    title->setText( QString("%1 - %2 : %3")
                   .arg( epgItem->start().toString( "hh:mm" ) )
                   .arg( end.toString( "hh:mm" ) )
                   .arg( epgItem->name() )
                   );
    description->setText( epgItem->description() );
}

void EpgDialog::updateInfos()
{
    timer->stop();
    if( !THEMIM->getInput() ) return;
    epg->updateEPG( input_GetItem( THEMIM->getInput() ) );
    if ( isVisible() ) timer->start();
}
