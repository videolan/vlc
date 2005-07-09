/*****************************************************************************
 * messages.cpp: the KMessagesWindow class
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * $Id$
 *
 * Author: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "messages.h"
#include <qtextview.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qvbox.h>

KMessagesWindow::KMessagesWindow( intf_thread_t * p_intf,  msg_subscription_t *p_msg ) :
    KDialogBase( Plain, _( "Messages" ), Ok, Ok, 0, 0, false)
{
//    clearWFlags(~0);
//    setWFlags(WType_TopLevel);
    setSizeGripEnabled(true);
    this->p_intf = p_intf;
    this->p_msg = p_msg;
    QFrame *page = plainPage();
    QVBoxLayout *toplayout = new QVBoxLayout( page);
//    QScrollView *sv = new QScrollView(page);
//    sv->setResizePolicy(QScrollView::AutoOneFit);
//    sv->setFrameStyle(QScrollView::NoFrame);
//    toplayout->addWidget(sv);
//    QVBox *category_table = new QVBox(sv->viewport());
//    sv->addChild(category_table);
//    toplayout->addStretch(10);
    QVBox *category_table = new QVBox(page);
    toplayout->addWidget(category_table);
    toplayout->setResizeMode(QLayout::FreeResize);
    category_table->setSpacing(spacingHint());
    resize(300,400);
    new QLabel( _("Messages:"), category_table );
    text = new QTextView( category_table );
    text->setPaper( QBrush( black ) );
//    clearWFlags(WStyle_DialogBorder|WStyle_NoBorder);
//    setWFlags(WStyle_NormalBorder|WStyle_Customize);
//    connect(this, SIGNAL(okClicked()), this, SLOT(accept()));
}

KMessagesWindow::~KMessagesWindow()
{
    ;
}

void KMessagesWindow::update()
{
    int i_stop, i_start;
    /* Update the log window */
    vlc_mutex_lock( p_msg->p_lock );
    i_stop = *p_msg->pi_stop;
    vlc_mutex_unlock( p_msg->p_lock );

    if( p_msg->i_start != i_stop )
    {
        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ",
                                             " debug: " };
        static const char * ppsz_color[4] = {
            "<font color=white>",
            "<font color=red>",
            "<font color=yellow>",
            "<font color=gray>"
        };
        for( i_start = p_msg->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            text->append( QString("<font color=white>") +
                          p_msg->p_msg[i_start].psz_module +
                          ppsz_type[p_msg->p_msg[i_start].i_type] +
                          "</font>" +
                          ppsz_color[p_msg->p_msg[i_start].i_type] +
                          p_msg->p_msg[i_start].psz_msg + "</font>" );
        }

        vlc_mutex_lock( p_msg->p_lock );
        p_msg->i_start = i_start;
        vlc_mutex_unlock( p_msg->p_lock );

    }
}
