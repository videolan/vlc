/*****************************************************************************
 * expert_view.hpp : Detailed preferences overview - view
 *****************************************************************************
 * Copyright (C) 2019-2022 VLC authors and VideoLAN
 *
 * Authors: Lyndon Brown <jnqnfe@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_EXPERT_PREFERENCES_VIEW_HPP_
#define VLC_QT_EXPERT_PREFERENCES_VIEW_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "expert_model.hpp"

#include <QTreeView>
#include <QDialog>

#include "qt.hpp"

class QVBoxLayout;
class QAction;
class QContextMenuEvent;

class ConfigControl;
class ExpertPrefsEditDialog;

class ExpertPrefsTable : public QTreeView
{
    Q_OBJECT

public:
    ExpertPrefsTable( QWidget *parent = nullptr );
    ExpertPrefsTableModel *myModel()
    {
        return static_cast<ExpertPrefsTableModel *>( model() );
    }
    void applyAll();
    void filter( const QString &text, bool );

protected:
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent *event) override;
#endif // QT_NO_CONTEXTMENU

private:
    void modifyItem( const QModelIndex & );
    void toggleItem( const QModelIndex & );
    QAction *reset_action;
    QAction *toggle_action;
    QAction *modify_action;
    QAction *copy_name_action;
    QAction *copy_value_action;
    ExpertPrefsEditDialog *expert_edit;

private slots:
    void resetItem();
    void toggleItem();
    void modifyItem();
    void copyItemName();
    void copyItemValue();
    void doubleClicked( const QModelIndex & );
};

class ExpertPrefsEditDialog : public QDialog
{
    Q_OBJECT
public:
    ExpertPrefsEditDialog( ExpertPrefsTable * );
    void setControl( ConfigControl *, ExpertPrefsTableItem * );

private:
    void clearControl();
    ExpertPrefsTable *table;
    ExpertPrefsTableItem *table_item;
    QVBoxLayout *layout;
    QWidget *control_widget;
    ConfigControl *control;

private slots:
    void accept();
    void reject();
};

#endif
