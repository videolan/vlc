/*****************************************************************************
 * plugins.hpp : Plug-ins and extensions listing
 ****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
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

#ifndef QVLC_PLUGIN_DIALOG_H_
#define QVLC_PLUGIN_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include <QTreeWidget>
#include <QStringList>

class QTreeWidget;
class QLineEdit;

class SearchLineEdit;
class PluginDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static PluginDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new PluginDialog( p_intf );
        return instance;
    }
    static void killInstance()
    {
        delete instance;
        instance = NULL;
    }
private:
    PluginDialog( intf_thread_t * );
    virtual ~PluginDialog();
    static PluginDialog *instance;

    void FillTree();
    QTreeWidget *treePlugins;
    SearchLineEdit *edit;
private slots:
    void search( const QString& );
};

class PluginTreeItem : public QTreeWidgetItem
{
public:
    PluginTreeItem(QStringList &qs_item, int Type = QTreeWidgetItem::Type) : QTreeWidgetItem (qs_item, Type)
    { }
    virtual bool operator< ( const QTreeWidgetItem & other ) const;
};

#endif

