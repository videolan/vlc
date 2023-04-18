/*****************************************************************************
 * openurl.cpp: Open a MRL or clipboard content
 *****************************************************************************
 * Copyright © 2009 the VideoLAN team
 *
 * Authors: Jean-Philippe André <jpeg@videolan.org>
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

#include "dialogs/open/openurl.hpp"
#include "util/validators.hpp"

#include <QPushButton>
#include <QDialogButtonBox>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>

#include <cassert>

OpenUrlDialog::OpenUrlDialog( qt_intf_t *_p_intf,
                              bool _bClipboard ) :
        QVLCDialog( nullptr, _p_intf ), bClipboard( _bClipboard )
{
    setWindowTitle( qtr( "Open URL" ) );
    setWindowRole( "vlc-open-url" );

    /* Buttons */
    QPushButton *but;

    QDialogButtonBox *box = new QDialogButtonBox( this );
    but = box->addButton( qtr( "&Play" ), QDialogButtonBox::AcceptRole );
    connect( but, &QPushButton::clicked, this, &OpenUrlDialog::play );

    but = box->addButton( qtr( "&Enqueue" ), QDialogButtonBox::AcceptRole );
    connect( but, &QPushButton::clicked, this, &OpenUrlDialog::enqueue );

    but = box->addButton( qtr( "&Cancel" ) , QDialogButtonBox::RejectRole );
    connect( box, &QDialogButtonBox::rejected, this, &OpenUrlDialog::reject );

    /* Info label and line edit */
    edit = new QLineEdit( this );
    edit->setPlaceholderText( qtr( "Enter URL here..." ) );
    edit->setValidator( new UrlValidator( edit ) );

    QLabel *info = new QLabel( qtr( "Please enter the URL or path "
                                    "to the media you want to play."),
                               this );

    setToolTip( qtr( "If your clipboard contains a valid URL\n"
                     "or the path to a file on your computer,\n"
                     "it will be automatically selected." ) );

    /* Layout */
    QVBoxLayout *vlay = new QVBoxLayout( this );

    vlay->addWidget( info );
    vlay->addWidget( edit );
    vlay->addWidget( box );
}

void OpenUrlDialog::enqueue()
{
    bShouldEnqueue = true;
    lastUrl = edit->text().trimmed();
    accept();
}

void OpenUrlDialog::play()
{
    lastUrl = edit->text().trimmed();
    accept();
}

QString OpenUrlDialog::url() const
{
    return lastUrl;
}

bool OpenUrlDialog::shouldEnqueue() const
{
    return bShouldEnqueue;
}

/** Show Event:
 * When the dialog is shown, try to extract a URL from the clipboard
 * and paste it in the Edit box.
 * showEvent can happen not only on exec() but I think it's cool to
 * actualize the URL on showEvent (eg. change virtual desktop...)
 **/
void OpenUrlDialog::showEvent( QShowEvent* )
{
    bShouldEnqueue = false;
    edit->setFocus( Qt::OtherFocusReason );
    if( !lastUrl.isEmpty() && edit->text().isEmpty() )
    {
        /* The text should not have been changed, excepted if the user
        has clicked Cancel before */
        edit->setText( lastUrl );
    }
    else
        edit->clear();

    if( bClipboard )
    {
        QClipboard *clipboard = QApplication::clipboard();
        assert( clipboard != NULL );
        QString txt = clipboard->text( QClipboard::Selection ).trimmed();

        if( txt.isEmpty() || ( !txt.contains("://") && !QFile::exists(txt) ) )
            txt = clipboard->text( QClipboard::Clipboard ).trimmed();

        if( txt.contains( "://" ) || QFile::exists( txt ) )
            edit->setText( txt );
    }
}
