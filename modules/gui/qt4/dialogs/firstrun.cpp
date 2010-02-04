/*****************************************************************************
 * firstrun : First Run dialogs
 ****************************************************************************
 * Copyright © 2009 VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "dialogs/firstrun.hpp"

#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

FirstRun::FirstRun( QWidget *_p, intf_thread_t *_p_intf  )
         : QWidget( _p ), p_intf( _p_intf )
{
#ifndef HAVE_MAEMO
    msg_Dbg( p_intf, "Boring first Run Wizard" );
    buildPrivDialog();
    setVisible( true );
#endif
}

#define ALBUM_ART_WHEN_ASKED 0
#define ALBUM_ART_ALL 2
void FirstRun::save()
{
    config_PutInt( p_intf,  "album-art", checkbox->isChecked() ? ALBUM_ART_ALL: ALBUM_ART_WHEN_ASKED );
    config_PutInt( p_intf,  "qt-updates-notif", checkbox2->isChecked() );
    config_PutInt( p_intf,  "qt-privacy-ask", 0 );

    /* We have to save here because the user may not launch Prefs */
    config_SaveConfigFile( p_intf, NULL );
    close();
}

void FirstRun::buildPrivDialog()
{
    setWindowTitle( qtr( "Privacy and Network Policies" ) );
    setWindowRole( "vlc-privacy" );
    setWindowModality( Qt::ApplicationModal );
    setWindowFlags( Qt::Dialog );
    setAttribute( Qt::WA_DeleteOnClose );

    QGridLayout *gLayout = new QGridLayout( this );

    QGroupBox *blabla = new QGroupBox( qtr( "Privacy and Network Warning" ) );
    QGridLayout *blablaLayout = new QGridLayout( blabla );
    QLabel *text = new QLabel( qtr(
        "<p><i>VideoLAN</i> prefers when applications request authorization "
        "before accessing Internet.</p>\n"
        "<p><b>VLC media player</b> can get information from the Internet "
        "in order to get <b>media informations</b> or to check for available <b>updates</b>.</p>\n"
        "<p><i>VLC media player</i> <b>doesn't</b> send or collect any "
        "information, even anonymously, about your usage.</p>\n" ) );
    text->setWordWrap( true );
    text->setTextFormat( Qt::RichText );

    blablaLayout->addWidget( text, 0, 0 ) ;

    QGroupBox *options = new QGroupBox( qtr( "Options" ) );
    QGridLayout *optionsLayout = new QGridLayout( options );

    gLayout->addWidget( blabla, 0, 0, 1, 3 );
    gLayout->addWidget( options, 1, 0, 1, 3 );
    int line = 0;

    checkbox = new QCheckBox( qtr( "Allow fetching media information from Internet" ) );
    checkbox->setChecked( true );
    optionsLayout->addWidget( checkbox, line++, 0 );

#ifdef UPDATE_CHECK
    checkbox2 = new QCheckBox( qtr( "Check for updates" ) );
    checkbox2->setChecked( true );
    optionsLayout->addWidget( checkbox2, line++, 0 );
#endif

    QPushButton *ok = new QPushButton( qtr( "OK" ) );

    gLayout->addWidget( ok, 2, 2 );

    CONNECT( ok, clicked(), this, save() );
}

