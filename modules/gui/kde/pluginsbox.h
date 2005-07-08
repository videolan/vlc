/*****************************************************************************
 * pluginbox.h: includes for the pluginbox class
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
#ifndef _KDE_PLUGINBOX_H_
#define _KDE_PLUGINBOX_H_
#include <qgroupbox.h>
#include <klistview.h>
#include <qpushbutton.h>
#include <klineedit.h>
#include "preferences.h"
class KPluginsBox : public QGroupBox
{
    Q_OBJECT
 public:
    KPluginsBox(intf_thread_t *p_intf, QString title, QString value,
                QWidget *parent, int spacing, KPreferences *pref);
    ~KPluginsBox();

    QListView *getListView(void);

 private slots:
    void selectClicked(void);
    void selectionChanged( QListViewItem * );

 signals:
    void selectionChanged(const QString &text);
    
 private:
    intf_thread_t *p_intf;
    KListView *listView;
    QPushButton *selectButton;
    KLineEdit *line;
    KPreferences *owner;
};
#endif
