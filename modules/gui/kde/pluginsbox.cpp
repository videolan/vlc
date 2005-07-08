/*****************************************************************************
 * pluginbox.cpp: the pluginbox class
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no> Mon Aug 12 2002
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
#include "pluginsbox.h"
#include "preferences.h"

#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qvbox.h>
#include <klistview.h>
#include <kbuttonbox.h>

KPluginsBox::KPluginsBox(intf_thread_t *p_intf,
                         QString text, QString value, QWidget *parent,
                         int spacing, KPreferences *pref) :
    QGroupBox( 1, Vertical, text, parent )
{
    owner = pref;
    this->p_intf = p_intf;
    QVBox *item_vbox = new QVBox( this );
    item_vbox->setSpacing(spacing);
    
    listView = new KListView(item_vbox);
    listView->setAllColumnsShowFocus(true);
    listView->addColumn(_("Name"));
    listView->addColumn(_("Description"));
    KButtonBox *item_bbox = new KButtonBox(item_vbox);
    selectButton = item_bbox->addButton( _("Select") );
    QHBox *item_hbox = new QHBox(item_vbox);
    item_hbox->setSpacing(spacing);
    new QLabel( _("Selected:"), item_hbox );
    line = new KLineEdit( value, item_hbox );
    connect(selectButton, SIGNAL(clicked()), this, SLOT(selectClicked()));
    connect(listView, SIGNAL(selectionChanged( QListViewItem *)),
            this, SLOT( selectionChanged( QListViewItem *)));
}

KPluginsBox::~KPluginsBox()
{
    ;
}

QListView* KPluginsBox::getListView()
{
    return listView;
}

void KPluginsBox::selectClicked()
{
    if (listView->selectedItem()) {
        line->setText(listView->selectedItem()->text(0));
        emit selectionChanged(listView->selectedItem()->text(0));
    }
}

void KPluginsBox::selectionChanged( QListViewItem *item )
{
    selectButton->setEnabled(true);
}
