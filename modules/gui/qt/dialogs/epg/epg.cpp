/*****************************************************************************
 * epg.cpp : Epg Viewer dialog
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

#include "epg.hpp"

#include "EPGWidget.hpp"
#include "EPGItem.hpp"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDateTime>
#include <QLocale>

#include "qt.hpp"
#include "player/player_controller.hpp"

EpgDialog::EpgDialog( qt_intf_t *_p_intf ): QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Program Guide" ) );

    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setContentsMargins(0, 0, 0, 0);
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

    connect( epg, &EPGWidget::itemSelectionChanged, this, &EpgDialog::displayEvent );
    connect( epg, &EPGWidget::programActivated, THEMIM, &PlayerController::changeProgram );
    connect( THEMIM, &PlayerController::epgChanged, this, &EpgDialog::scheduleUpdate );
    connect( THEMIM, &PlayerController::inputChanged, this, &EpgDialog::inputChanged );

    QDialogButtonBox *buttonsBox = new QDialogButtonBox( this );

#if 0
    QPushButton *update = new QPushButton( qtr( "Update" ) ); // Temporary to test
    buttonsBox->addButton( update, QDialogButtonBox::ActionRole );
    BUTTONACT( update, &EpgDialog::updateInfos );
#endif

    buttonsBox->addButton( new QPushButton( qtr( "&Close" ) ),
                           QDialogButtonBox::RejectRole );
    boxLayout->addWidget( buttonsBox );
    connect( buttonsBox, &QDialogButtonBox::rejected, this, &EpgDialog::close );

    timer = new QTimer( this );
    timer->setSingleShot( true );
    timer->setInterval( 5000 );
    connect( timer, &QTimer::timeout, this, &EpgDialog::timeout );

    updateInfos();
    restoreWidgetPosition( "EPGDialog", QSize( 650, 450 ) );
}

EpgDialog::~EpgDialog()
{
    saveWidgetPosition( "EPGDialog" );
}

void EpgDialog::showEvent(QShowEvent *)
{
    scheduleUpdate();
}

void EpgDialog::timeout()
{
    if( !isVisible() )
        scheduleUpdate();
    else
        updateInfos();
}

void EpgDialog::inputChanged()
{
    epg->reset();
    timeout();
}

void EpgDialog::scheduleUpdate()
{
    if( !timer->isActive() )
        timer->start( 5000 );
}

void EpgDialog::displayEvent( EPGItem *epgItem )
{
    if( !epgItem )
    {
        title->clear();
        description->clear();
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QDateTime enddate = epgItem->start().addSecs( epgItem->duration() );

    QString start, end;
    QLocale locale;
    if( epgItem->start().daysTo(now) != 0 )
        start = locale.toString(epgItem->start());
    else
        start = locale.toString(epgItem->start().time(), QLocale::ShortFormat);

    end = locale.toString(enddate.time(), QLocale::ShortFormat);

    title->setText( QString("%1 - %2 : %3%4")
                   .arg( start )
                   .arg( end )
                   .arg( epgItem->name() )
                   .arg( epgItem->rating() ?
                             qtr(" (%1+ rated)").arg( epgItem->rating() ) :
                             QString() )
                   );
    description->setText( epgItem->description() );
    const QList<QPair<QString, QString>> items = epgItem->descriptionItems();
    QList<QPair<QString, QString>>::const_iterator it;
    for( it=items.begin(); it != items.end(); ++it )
    {
        description->append(QString("\n<b>%1:</b> %2")
                              .arg((*it).first)
                              .arg((*it).second));
    }
    description->verticalScrollBar()->setValue(0);
}

void EpgDialog::updateInfos()
{
    input_item_t *media = NULL;
    {
        vlc_player_locker lock{ p_intf->p_player };
        media = vlc_player_HoldCurrentMedia( p_intf->p_player ); /* w/hold */
    }

    if( media )
    {
        epg->updateEPG( media );
        input_item_Release( media );
    }
    else
        epg->reset();
}
