/*****************************************************************************
 * selector.hpp : Playlist source selector
 ****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf
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

#ifndef _PLSEL_H_
#define _PLSEL_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

#include <vlc_playlist.h>

#include "qt4.hpp"

class PlaylistWidget;
class QVLCIconLabel;

enum SelectorItemType {
    CATEGORY_TYPE,
    SD_TYPE,
    PL_ITEM_TYPE
};

enum SpecialData {
    IS_PODCAST = 1,
    IS_PL,
    IS_ML
};

enum {
    TYPE_ROLE = Qt::UserRole,
    NAME_ROLE, //QString
    LONGNAME_ROLE, //QString
    PL_ITEM_ROLE, //playlist_item_t*
    PL_ITEM_ID_ROLE, //playlist_item_t->i_id
    IN_ITEM_ROLE, //input_item_t->i_id
    SPECIAL_ROLE //SpecialData
};

enum ItemAction {
    ADD_ACTION,
    RM_ACTION
};

class PLSelItem : public QWidget
{
    Q_OBJECT;
public:
    PLSelItem( QTreeWidgetItem*, const QString& );
    void setText( const QString& );
    void addAction( ItemAction, const QString& toolTip = 0 );
    QTreeWidgetItem *treeItem() { return qitem; }
    QString text() { return lbl->text(); }
public slots:
    void showAction();
    void hideAction();
private slots:
    void triggerAction() { emit action( this ); }
signals:
    void action( PLSelItem* );
private:
    void enterEvent( QEvent* );
    void leaveEvent( QEvent* );
    QTreeWidgetItem* qitem;
    QVLCIconLabel *lblAction;
    QLabel *lbl;
    QHBoxLayout *layout;
};

Q_DECLARE_METATYPE( playlist_item_t *);
Q_DECLARE_METATYPE( input_item_t *);
class PLSelector: public QTreeWidget
{
    Q_OBJECT;
public:
    PLSelector( QWidget *p, intf_thread_t *_p_intf );
    virtual ~PLSelector();
protected:
    friend class PlaylistWidget;
private:
    QStringList mimeTypes () const;
    bool dropMimeData ( QTreeWidgetItem * parent, int index, const QMimeData * data, Qt::DropAction action );
    void createItems();
    PLSelItem * addItem (
        SelectorItemType type, const QString& str, bool drop,
        QTreeWidgetItem* parentItem = 0 );
    PLSelItem * addPodcastItem( playlist_item_t *p_item );
    inline PLSelItem * itemWidget( QTreeWidgetItem * );
    intf_thread_t *p_intf;
    QTreeWidgetItem *podcastsParent;
    int podcastsParentId;
private slots:
    void setSource( QTreeWidgetItem *item );
    void plItemAdded( int, int );
    void plItemRemoved( int );
    void inputItemUpdate( input_item_t * );
    void podcastAdd( PLSelItem* );
    void podcastRemove( PLSelItem* );

signals:
    void activated( playlist_item_t * );
};

#endif
