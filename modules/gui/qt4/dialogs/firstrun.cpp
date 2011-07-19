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
#include <QDialogButtonBox>
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
#ifdef UPDATE_CHECK
    config_PutInt( p_intf,  "qt-updates-notif", checkbox2->isChecked() );
#endif
    config_PutInt( p_intf,  "qt-privacy-ask", 0 );

    /* FIXME Should not save here. This will not work as expected if another
     * plugin overwrote items of its own. */
#warning FIXME
    /* We have to save here because the user may not launch Prefs */
    config_SaveConfigFile( p_intf );
    close();
}

void FirstRun::buildPrivDialog()
{
    setWindowTitle( qtr( "Privacy and Network Access Policy" ) );
    setWindowRole( "vlc-privacy" );
    setWindowModality( Qt::ApplicationModal );
    setWindowFlags( Qt::Dialog );
    setAttribute( Qt::WA_DeleteOnClose );

    QGridLayout *gLayout = new QGridLayout( this );

    QGroupBox *blabla = new QGroupBox( qtr( "Privacy and Network Access Policy" ) );
    QGridLayout *blablaLayout = new QGridLayout( blabla );
    QLabel *text = new QLabel( qtr(
        "<p><i>VLC media player</i> does <b>not</b> send or collect any "
        "information, even anonymously, about your usage.</p>\n"
        "<p>However, it can connect to the Internet "
        "in order to display <b>medias information</b> "
#ifdef UPDATE_CHECK
        "or to check for available <b>updates</b>"
#endif
        ".</p>\n"
        "<p><i>VideoLAN</i> (the authors) requires you to express your consent "
        "before allowing this software to access the Internet.</p>\n"
        "<p>According to your choices, please check or uncheck the following options:</p>\n"
        ) );
    text->setWordWrap( true );
    text->setTextFormat( Qt::RichText );

    blablaLayout->addWidget( text, 0, 0 ) ;

    QGroupBox *options = new QGroupBox( qtr( "Network Access Policy" ) );
    QGridLayout *optionsLayout = new QGridLayout( options );

    gLayout->addWidget( blabla, 0, 0, 1, 3 );
    gLayout->addWidget( options, 1, 0, 1, 3 );
    int line = 0;

    checkbox = new QCheckBox( qtr( "Allow downloading media information" ) );
    checkbox->setChecked( true );
    optionsLayout->addWidget( checkbox, line++, 0 );

#ifdef UPDATE_CHECK
    checkbox2 = new QCheckBox( qtr( "Allow checking for VLC updates" ) );
    checkbox2->setChecked( true );
    optionsLayout->addWidget( checkbox2, line++, 0 );
#endif

    QDialogButtonBox *buttonsBox = new QDialogButtonBox( this );
    buttonsBox->addButton( qtr( "Save and Continue" ), QDialogButtonBox::AcceptRole );

    gLayout->addWidget( buttonsBox, 2, 0, 2, 3 );

    CONNECT( buttonsBox, accepted(), this, save() );
    buttonsBox->setFocus();
}

