/*****************************************************************************
 * Messages.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#include "dialogs/messages.hpp"
#include "dialogs_provider.hpp"

#include <QSpacerItem>
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

MessagesDialog *MessagesDialog::instance = NULL;

MessagesDialog::MessagesDialog( intf_thread_t *_p_intf) :  QVLCFrame( _p_intf )
{
    setWindowTitle( qtr( "Messages" ) );
    resize( 600, 450 );

    QGridLayout *layout = new QGridLayout( this );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    QPushButton *clearButton = new QPushButton( qtr( "&Clear" ) );
    QPushButton *saveLogButton = new QPushButton( qtr( "&Save as..." ) );
    closeButton->setDefault( true );

    verbosityBox = new QSpinBox();
    verbosityBox->setRange( 0, 2 );
    verbosityBox->setValue( config_GetInt( p_intf, "verbose" ) );
    verbosityBox->setWrapping( true );
    verbosityBox->setMaximumWidth( 50 );

    QLabel *verbosityLabel = new QLabel( qtr( "Verbosity Level" ) );

    messages = new QTextEdit();
    messages->setReadOnly( true );
    messages->setGeometry( 0, 0, 440, 600 );
    messages->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    layout->addWidget( messages, 0, 0, 1, 0 );
    layout->addWidget( verbosityLabel, 1, 0, 1, 1 );
    layout->addWidget( verbosityBox, 1, 1 );
    layout->addItem( new QSpacerItem( 20, 20, QSizePolicy::Expanding ), 1, 2 );
    layout->addWidget( saveLogButton, 1, 3 );
    layout->addWidget( clearButton, 1, 4 );
    layout->addWidget( closeButton, 1, 5 );

    BUTTONACT( closeButton, close() );
    BUTTONACT( clearButton, clear() );
    BUTTONACT( saveLogButton, save() );
    ON_TIMEOUT( updateLog() );
}

MessagesDialog::~MessagesDialog()
{
}

void MessagesDialog::updateLog()
{
    msg_subscription_t *p_sub = p_intf->p_sys->p_sub;
    int i_start;

    vlc_mutex_lock( p_sub->p_lock );
    int i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    if( p_sub->i_start != i_stop )
    {
        messages->textCursor().movePosition( QTextCursor::End );

        for( i_start = p_sub->i_start;
                i_start != i_stop;
                i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            if( p_sub->p_msg[i_start].i_type == VLC_MSG_INFO ||
                p_sub->p_msg[i_start].i_type == VLC_MSG_ERR ||
                p_sub->p_msg[i_start].i_type == VLC_MSG_WARN &&
                    verbosityBox->value() >= 1 ||
                p_sub->p_msg[i_start].i_type == VLC_MSG_DBG &&
                    verbosityBox->value() >= 2 )
            {
                messages->setFontItalic( true );
                messages->setTextColor( "darkBlue" );
                messages->insertPlainText( qfu( p_sub->p_msg[i_start].psz_module ) );
            }
            else
                continue;

            switch( p_sub->p_msg[i_start].i_type )
            {
                case VLC_MSG_INFO:
                    messages->setTextColor( "blue" );
                    messages->insertPlainText( " info: " );
                    break;
                case VLC_MSG_ERR:
                    messages->setTextColor( "red" );
                    messages->insertPlainText( " error: " );
                    break;
                case VLC_MSG_WARN:
                    messages->setTextColor( "green" );
                    messages->insertPlainText( " warning: " );
                    break;
                case VLC_MSG_DBG:
                default:
                    messages->setTextColor( "grey" );
                    messages->insertPlainText( " debug: " );
                    break;
            }

            /* Add message Regular black Font */
            messages->setFontItalic( false );
            messages->setTextColor( "black" );
            messages->insertPlainText( qfu(p_sub->p_msg[i_start].psz_msg) );
            messages->insertPlainText( "\n" );
        }
        messages->ensureCursorVisible();

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }
}

void MessagesDialog::close()
{
    hide();
}

void MessagesDialog::clear()
{
    messages->clear();
}

bool MessagesDialog::save()
{
    QString saveLogFileName = QFileDialog::getSaveFileName(
            this, qtr( "Choose a filename to save the logs under..." ),
            qfu( p_intf->p_libvlc->psz_homedir ),
            "Texts / Logs (*.log *.txt);; All (*.*) " );

    if( !saveLogFileName.isNull() )
    {
        QFile file( saveLogFileName );
        if ( !file.open( QFile::WriteOnly | QFile::Text ) ) {
            QMessageBox::warning( this, qtr( "Application" ),
                    qtr( "Cannot write file %1:\n%2." )
                    .arg( saveLogFileName )
                    .arg( file.errorString() ) );
            return false;
        }

        QTextStream out( &file );
        out << messages->toPlainText() << "\n";

        return true;
    }
    return false;
}
