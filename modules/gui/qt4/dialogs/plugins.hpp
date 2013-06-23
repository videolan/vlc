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
#include "util/singleton.hpp"

#include <vlc_extensions.h>

#include <QStringList>
#include <QTreeWidgetItem>
#include <QAbstractListModel>
#include <QStyledItemDelegate>

class QLabel;
class QTabWidget;
class QComboBox;
class QTreeWidget;
class QLineEdit;
class QTextBrowser;
class QListView;
class QStyleOptionViewItem;
class QPainter;
class QKeyEvent;
class PluginTab;
class ExtensionTab;
class ExtensionListItem;
class SearchLineEdit;
class ExtensionCopy;


class PluginDialog : public QVLCFrame, public Singleton<PluginDialog>
{
    Q_OBJECT

private:
    PluginDialog( intf_thread_t * );
    virtual ~PluginDialog();

    QTabWidget *tabs;
    PluginTab *pluginTab;
    ExtensionTab *extensionTab;

    friend class Singleton<PluginDialog>;
};

class PluginTab : public QVLCFrame
{
    Q_OBJECT
public:
    enum
    {
        NAME = 0,
        CAPABILITY,
        SCORE
    };

protected:
    virtual void keyPressEvent( QKeyEvent *keyEvent );

private:
    PluginTab( intf_thread_t *p_intf );
    virtual ~PluginTab();

    void FillTree();
    QTreeWidget *treePlugins;
    SearchLineEdit *edit;

private slots:
    void search( const QString& );

    friend class PluginDialog;
};

class ExtensionTab : public QVLCFrame
{
    Q_OBJECT

protected:
    virtual void keyPressEvent( QKeyEvent *keyEvent );

private:
    ExtensionTab( intf_thread_t *p_intf );
    virtual ~ExtensionTab();

private slots:
    void moreInformation();
    void updateButtons();

private:
    QListView *extList;
    QPushButton *butMoreInfo;

    friend class PluginDialog;
};

class PluginTreeItem : public QTreeWidgetItem
{
public:
    PluginTreeItem(QStringList &qs_item, int Type = QTreeWidgetItem::Type)
            : QTreeWidgetItem (qs_item, Type) {}
    virtual ~PluginTreeItem() {}

    virtual bool operator< ( const QTreeWidgetItem & other ) const;
};

class ExtensionListModel : public QAbstractListModel
{

    Q_OBJECT

public:
    /* Safe copy of the extension_t struct */
    class ExtensionCopy
    {

    public:
        ExtensionCopy( extension_t * );
        ~ExtensionCopy();
        QVariant data( int role ) const;

    private:
        QString name, title, description, shortdesc, author, version, url;
        QPixmap *icon;
    };

    ExtensionListModel( QListView *view, intf_thread_t *p_intf );
    virtual ~ExtensionListModel();

    enum
    {
        DescriptionRole = Qt::UserRole,
        VersionRole,
        AuthorRole,
        LinkRole,
        NameRole
    };

    virtual QVariant data( const QModelIndex& index, int role ) const;
    virtual QModelIndex index( int row, int column = 0,
                               const QModelIndex& = QModelIndex() ) const;
    virtual int rowCount( const QModelIndex& = QModelIndex() ) const;

private slots:
    void updateList();

private:

    intf_thread_t *p_intf;
    QList<ExtensionCopy*> extensions;
};

class ExtensionItemDelegate : public QStyledItemDelegate
{
public:
    ExtensionItemDelegate( intf_thread_t *p_intf, QListView *view );
    virtual ~ExtensionItemDelegate();

    virtual void paint( QPainter *painter,
                        const QStyleOptionViewItem &option,
                        const QModelIndex &index ) const;
    virtual QSize sizeHint( const QStyleOptionViewItem &option,
                            const QModelIndex &index ) const;

private:
    QListView *view;
    intf_thread_t *p_intf;
};

class ExtensionInfoDialog : public QVLCDialog
{
public:
    ExtensionInfoDialog( const QModelIndex &index,
                         intf_thread_t *p_intf, QWidget *parent );
};

#endif

