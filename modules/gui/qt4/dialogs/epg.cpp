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

    CONNECT( epg, itemSelectionChanged( EPGEvent *), this, showEvent( EPGEvent *) );
    CONNECT( THEMIM->getIM(), epgChanged(), this, updateInfos() );

#if 0
    QPushButton *update = new QPushButton( qtr( "Update" ) ); // Temporary to test
    boxLayout->addWidget( update, 0, Qt::AlignRight );
    BUTTONACT( update, updateInfos() );
#endif

    QPushButton *close = new QPushButton( qtr( "&Close" ) );
    boxLayout->addWidget( close, 0, Qt::AlignRight );
    BUTTONACT( close, close() );

    updateInfos();
    readSettings( "EPGDialog", QSize( 650, 450 ) );
}

EpgDialog::~EpgDialog()
{
    writeSettings( "EPGDialog" );
}

void EpgDialog::showEvent( EPGEvent *event )
{
    if( !event ) return;

    QString titleDescription, textDescription;
    if( event->description.isEmpty() )
        textDescription = event->shortDescription;
    else
    {
        textDescription = event->description;
        if( !event->shortDescription.isEmpty() )
            titleDescription = " - " + event->shortDescription;
    }

    QDateTime end = event->start.addSecs( event->duration );
    title->setText( event->start.toString( "hh:mm" ) + " - "
                    + end.toString( "hh:mm" ) + " : "
                    + event->name + titleDescription );

    description->setText( textDescription );
}

void EpgDialog::updateInfos()
{
    if( !THEMIM->getInput() ) return;

    msg_Dbg( p_intf, "Found %i EPG items", input_GetItem( THEMIM->getInput())->i_epg);
    epg->updateEPG( input_GetItem( THEMIM->getInput())->pp_epg, input_GetItem( THEMIM->getInput())->i_epg );

}
