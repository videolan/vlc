/*****************************************************************************
 * gototime.cpp : GotoTime and About dialogs
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/gototime.hpp"

#include "input_manager.hpp"

#include <QTabWidget>
#include <QLabel>
#include <QTimeEdit>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>

GotoTimeDialog::GotoTimeDialog( intf_thread_t *_p_intf)
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    setWindowFlags( Qt::Tool );
    setWindowTitle( qtr( "Go to Time" ) );
    setWindowRole( "vlc-goto-time" );

    QGridLayout *mainLayout = new QGridLayout( this );
    mainLayout->setSizeConstraint( QLayout::SetFixedSize );

    QPushButton *gotoButton = new QPushButton( qtr( "&Go" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );
    QDialogButtonBox *buttonBox = new QDialogButtonBox;

    gotoButton->setDefault( true );
    buttonBox->addButton( gotoButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    QLabel *timeIntro = new QLabel( qtr( "Go to time" ) + ":" );
    timeIntro->setWordWrap( true );
    timeIntro->setAlignment( Qt::AlignCenter );

    timeEdit = new QTimeEdit();
    timeEdit->setDisplayFormat( "HH'H':mm'm':ss's'" );
    timeEdit->setAlignment( Qt::AlignRight );
    timeEdit->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );

    QPushButton *resetButton = new QPushButton( QIcon(":/update"), "" );
    resetButton->setToolTip( qtr("Reset") );

    mainLayout->addWidget( timeIntro, 0, 0, 1, 1 );
    mainLayout->addWidget( timeEdit, 0, 1, 1, 1 );
    mainLayout->addWidget( resetButton, 0, 2, 1, 1 );

    mainLayout->addWidget( buttonBox, 1, 0, 1, 3 );

    BUTTONACT( gotoButton, close() );
    BUTTONACT( cancelButton, cancel() );
    BUTTONACT( resetButton, reset() );

    QVLCTools::restoreWidgetPosition( p_intf, "gototimedialog", this );
}

GotoTimeDialog::~GotoTimeDialog()
{
    QVLCTools::saveWidgetPosition( p_intf, "gototimedialog", this );
}

void GotoTimeDialog::toggleVisible()
{
    reset();
    if ( !isVisible() && THEMIM->getIM()->hasInput() )
    {
        int64_t i_time = var_GetTime( THEMIM->getInput(), "time" );
        timeEdit->setTime( timeEdit->time().addSecs( i_time / 1000000 ) );
    }
    QVLCDialog::toggleVisible();
    activateWindow ();
}

void GotoTimeDialog::cancel()
{
    reset();
    toggleVisible();
}

void GotoTimeDialog::close()
{
    if ( THEMIM->getIM()->hasInput() )
    {
        int64_t i_time = (int64_t)
            ( QTime( 0, 0, 0 ).msecsTo( timeEdit->time() ) ) * 1000;
        var_SetTime( THEMIM->getInput(), "time", i_time );
    }
    toggleVisible();
}

void GotoTimeDialog::reset()
{
    timeEdit->setTime( QTime( 0, 0, 0) );
}
