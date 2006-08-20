/*****************************************************************************
 * Messages.cpp : Information about an item
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: Messages.cpp 16024 2006-07-13 13:51:05Z xtophe $
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

#include "input_manager.hpp"
#include "dialogs/messages.hpp"
#include "dialogs_provider.hpp"
#include "util/qvlcframe.hpp"
#include "qt4.hpp"

MessagesDialog *MessagesDialog::instance = NULL;

MessagesDialog::MessagesDialog( intf_thread_t *_p_intf, bool _main_input ) :
                              QVLCFrame( _p_intf ), main_input( _main_input )
{
    setWindowTitle( _("Messages" ) );
    resize(600, 400);

    QGridLayout *layout = new QGridLayout(this);
    QPushButton *closeButton = new QPushButton(qtr("&Close"));
    QPushButton *clearButton = new QPushButton(qtr("&Clear"));
    QPushButton *saveLogButton = new QPushButton(qtr("&Save as..."));
    QSpinBox *verbosityBox = new QSpinBox();
    verbosityBox->setRange(1, 3);
    verbosityBox->setWrapping(true);
    QLabel *verbosityLabel = new QLabel(qtr("Verbosity Level"));
    messages = new QTextEdit();
    messages->setReadOnly(true);
    messages->setGeometry(0, 0, 440, 600);
    messages->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    layout->addWidget(messages, 0, 0, 1, 0);
    layout->addWidget(verbosityLabel, 1, 0, 1, 1);
    layout->addWidget(verbosityBox, 1, 2);
    layout->addWidget(saveLogButton, 2, 0);
    layout->addWidget(clearButton, 2, 1);
    layout->addWidget(closeButton, 2, 2);

    connect( closeButton, SIGNAL( clicked() ) ,
           this, SLOT( onCloseButton()));
    connect( clearButton, SIGNAL( clicked() ) ,
           this, SLOT( onClearButton()));
    connect( saveLogButton, SIGNAL( clicked() ) ,
           this, SLOT( onSaveButton()));
    connect( verbosityBox, SIGNAL( valueChanged(int) ),
           this, SLOT( onVerbosityChanged(int)));
    connect( DialogsProvider::getInstance(NULL)->fixed_timer,
             SIGNAL( timeout() ), this, SLOT(updateLog() ) );

    p_input = NULL;
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
        for( i_start = p_sub->i_start;
                i_start != i_stop;
                i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
          // [FIXME] Does not work as the old one
          // Outputs too much data ?
          // if (p_sub->p_msg[i_start].i_type = VLC_MSG_ERR)
          //          continue;
          //  if( !b_verbose &&
          //         VLC_MSG_ERR != p_sub->p_msg[i_start].i_type )
          //                continue;

            /* Append all messages to log window */


            messages->setFontItalic(true);
            messages->setTextColor("darkBlue");
            messages->insertPlainText(p_sub->p_msg[i_start].psz_module);

            switch( p_sub->p_msg[i_start].i_type )
            {
                case VLC_MSG_INFO:
                    messages->setTextColor("blue");
                    messages->insertPlainText(": ");
                    break;
                case VLC_MSG_ERR:
                    messages->setTextColor("red");
                    messages->insertPlainText(" error: ");
                    break;
                case VLC_MSG_WARN:
                    messages->setTextColor("green");
                    messages->insertPlainText(" warning: ");
                    break;
                case VLC_MSG_DBG:
                default:
                    messages->setTextColor("grey");
                    messages->insertPlainText(" debug: ");
                    break;
            }

            /* Add message Regular black Font */
            messages->setFontItalic(false);
            messages->setTextColor("black");
            messages->insertPlainText( p_sub->p_msg[i_start].psz_msg );
            messages->insertPlainText( "\n" );
        }
        messages->ensureCursorVisible();

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }
}

void MessagesDialog::onCloseButton()
{
    this->toggleVisible();
}

void MessagesDialog::onClearButton()
{
    messages->clear();
}

bool MessagesDialog::onSaveButton()
{
    QString saveLogFileName = QFileDialog::getSaveFileName(
            this,
            "Choose a filename to save the logs under...",
            p_intf->p_vlc->psz_homedir,
            "Texts / Logs (*.log *.txt);; All (*.*) ");

    if (saveLogFileName != NULL)
    {
        QFile file(saveLogFileName);
        if (!file.open(QFile::WriteOnly | QFile::Text)) {
            QMessageBox::warning(this, qtr("Application"),
                    qtr("Cannot write file %1:\n%2.")
                    .arg(saveLogFileName)
                    .arg(file.errorString()));
            return false;
        }

        QTextStream out(&file);
        out << messages->toPlainText() << "\n";

        return true;
    }
    return false;
}

void MessagesDialog::onVerbosityChanged(int verbosityLevel)
{
    //FIXME: Does not seems to work.
    vlc_value_t  val;
    val.i_int = verbosityLevel - 1;
    var_Set( p_intf->p_vlc, "verbose", val );
}

