/*****************************************************************************
 * info.cpp: the KInfoWindow class
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: info.cpp,v 1.2 2003/12/22 14:23:14 sam Exp $
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

#include "info.h"
#include "common.h"
#include <qtextview.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qvbox.h>

KInfoWindow::KInfoWindow( intf_thread_t * p_intf,  input_thread_t *p_input ) :
    KDialogBase( Tabbed, _( "Messages" ), Ok, Ok, 0, 0, false)
{
//    clearWFlags(~0);
//    setWFlags(WType_TopLevel);
    setSizeGripEnabled(true);
    vlc_mutex_lock( &p_input->stream.stream_lock );
    input_info_category_t *p_category = p_input->stream.p_info;
    while ( p_category )
    {
        QFrame *page = addPage( QString(p_category->psz_name) );
        QVBoxLayout *toplayout = new QVBoxLayout( page);
        QVBox *category_table = new QVBox(page);
        toplayout->addWidget(category_table);
        toplayout->setResizeMode(QLayout::FreeResize);
        toplayout->addStretch(10);
        category_table->setSpacing(spacingHint());
        input_info_t *p_info = p_category->p_info;
        while ( p_info )
        {
            QHBox *hb = new QHBox( category_table );
            new QLabel( QString(p_info->psz_name) + ":", hb );
            new QLabel( p_info->psz_value, hb );
            p_info = p_info->p_next;
        }
        p_category = p_category->p_next;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    resize(300,400);
    show();
}

KInfoWindow::~KInfoWindow()
{
    ;
}
