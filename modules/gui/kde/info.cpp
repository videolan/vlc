/*****************************************************************************
 * info.cpp: the KInfoWindow class
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

    int i, j;

    vlc_mutex_lock( &p_input->p_item->lock );
    for ( i = 0; i < p_input->p_item->i_categories; i++ )
    {
        info_category_t *p_category =
           p_input->p_item->pp_categories[i];

        QFrame *page = addPage( QString(p_category->psz_name) );
        QVBoxLayout *toplayout = new QVBoxLayout( page);
        QVBox *category_table = new QVBox(page);
        toplayout->addWidget(category_table);
        toplayout->setResizeMode(QLayout::FreeResize);
        toplayout->addStretch(10);
        category_table->setSpacing(spacingHint());

        for ( j = 0; j < p_category->i_infos; j++ )
        {
            info_t *p_info = p_category->pp_infos[j];

            QHBox *hb = new QHBox( category_table );
            new QLabel( QString(p_info->psz_name) + ":", hb );
            new QLabel( p_info->psz_value, hb );
        }
    }
    vlc_mutex_unlock( &p_input->p_item->lock );
    resize(300,400);
    show();
}

KInfoWindow::~KInfoWindow()
{
    ;
}
