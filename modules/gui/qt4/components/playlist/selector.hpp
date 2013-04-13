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

#include "qt4.hpp"
#include "util/customwidgets.hpp" /* QFramelessButton */

#include <QTreeWidget>

class QHBoxLayout;
class QPainter;
class QTreeWidgetItem;
class PlaylistWidget;
class QLabel;

enum SelectorItemType {
    CATEGORY_TYPE,
    SD_TYPE,
    PL_ITEM_TYPE,
    SQL_ML_TYPE,
};

enum SpecialData {
    IS_PODCAST = 1,
    IS_PL,
    IS_ML
};

enum {
    TYPE_ROLE = Qt::UserRole + 1,
    NAME_ROLE,           //QString
    LONGNAME_ROLE,       //QString
    PL_ITEM_ROLE,        //playlist_item_t*
    PL_ITEM_ID_ROLE,     //playlist_item_t->i_id
    IN_ITEM_ROLE,        //input_item_t->i_id
    SPECIAL_ROLE,        //SpecialData
    CAP_SEARCH_ROLE,
    SD_CATEGORY_ROLE,
};

enum ItemAction {
    ADD_ACTION,
    RM_ACTION
};


class SelectorActionButton : public QFramelessButton
{
protected:
    virtual void paintEvent( QPaintEvent * );
};

class PLSelItem : public QWidget
{
    Q_OBJECT
public:
    PLSelItem( QTreeWidgetItem*, const QString& );

    void setText( const QString& text ) { lbl->setText( text ); }
    QString text() const { return lbl->text(); }

    void addAction( ItemAction, const QString& toolTip = 0 );
    QTreeWidgetItem *treeItem() { return qitem; }

public slots:
    void showAction() { if( lblAction ) lblAction->show();  }
    void hideAction() { if( lblAction ) lblAction->hide(); }

private slots:
    void triggerAction() { emit action( this ); }

signals:
    void action( PLSelItem* );

private:
    inline void enterEvent( QEvent* ){ showAction(); }
    inline void leaveEvent( QEvent* ){ hideAction(); }

    QTreeWidgetItem*     qitem;
    QFramelessButton* lblAction;
    QLabel*              lbl;
    QHBoxLayout*         layout;
};

Q_DECLARE_METATYPE( playlist_item_t *);
Q_DECLARE_METATYPE( input_item_t *);
class PLSelector: public QTreeWidget
{
    Q_OBJECT
public:
    PLSelector( QWidget *p, intf_thread_t *_p_intf );
    virtual ~PLSelector();

    void getCurrentItemInfos( int *type, bool *delayedSearch, QString *name );
    int getCurrentItemCategory();

protected:
    virtual void drawBranches ( QPainter *, const QRect &, const QModelIndex & ) const;
    virtual void dragMoveEvent ( QDragMoveEvent * event );
    virtual bool dropMimeData ( QTreeWidgetItem *, int, const QMimeData *, Qt::DropAction );
    virtual QStringList mimeTypes () const;
    virtual void wheelEvent(QWheelEvent *e);

private:
    void createItems();
    PLSelItem * addItem ( SelectorItemType type, const char* str,
            bool drop = false, bool bold = false, QTreeWidgetItem* parentItem = 0 );
    PLSelItem * addPodcastItem( playlist_item_t *p_item );

    PLSelItem* playlistItem;

    void updateTotalDuration(PLSelItem*, const char*);

    inline PLSelItem * itemWidget( QTreeWidgetItem * );

    intf_thread_t    *p_intf;
    QTreeWidgetItem  *podcastsParent;
    int               podcastsParentId;
    QTreeWidgetItem  *curItem;

private slots:
    void setSource( QTreeWidgetItem *item );
    void plItemAdded( int, int );
    void plItemRemoved( int );
    void inputItemUpdate( input_item_t * );
    void podcastAdd( PLSelItem* );
    void podcastRemove( PLSelItem* );

signals:
    void categoryActivated( playlist_item_t *, bool );
    void SDCategorySelected( bool );
};

#endif
