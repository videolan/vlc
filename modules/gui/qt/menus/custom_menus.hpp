/*****************************************************************************
 * custom_menus.hpp : Qt custom menus classes
 *****************************************************************************
 * Copyright © 2006-2018 VideoLAN authors
 *                  2018 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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
#ifndef CUSTOM_MENUS_HPP
#define CUSTOM_MENUS_HPP

#include "qt.hpp"

#include <QMenu>
#include <QAbstractListModel>

class RendererAction : public QAction
{
    Q_OBJECT

    public:
        RendererAction( vlc_renderer_item_t * );
        ~RendererAction();
        vlc_renderer_item_t *getItem();

    private:
        vlc_renderer_item_t *p_item;
};

class RendererMenu : public QMenu
{
    Q_OBJECT

public:
    RendererMenu( QMenu *, intf_thread_t * );
    virtual ~RendererMenu();
    void reset();

private slots:
    void addRendererItem( vlc_renderer_item_t * );
    void removeRendererItem( vlc_renderer_item_t * );
    void updateStatus( int );
    void RendererSelected( QAction* );

private:
    void addRendererAction( QAction * );
    void removeRendererAction( QAction * );
    static vlc_renderer_item_t* getMatchingRenderer( const QVariant &,
                                                     vlc_renderer_item_t* );
    QAction *status;
    QActionGroup *group;
    intf_thread_t *p_intf;
};


/*
 * Construct a menu from a QAbstractListModel with Qt::DisplayRole and Qt::CheckStateRole
 */
class CheckableListMenu : public QMenu
{
    Q_OBJECT
public:
    /**
     * @brief CheckableListMenu
     * @param title the title of the menu
     * @param model the model to observe, the model should provide at least Qt::DisplayRole and Qt::CheckStateRole
     * @param parent QObject parent
     */
    CheckableListMenu(QString title, QAbstractListModel* model , QWidget *parent = nullptr);

private slots:
    void onRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onRowInserted(const QModelIndex &parent, int first, int last);
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());
    void onModelAboutToBeReset();
    void onModelReset();

private:
    QAbstractListModel* m_model;
    QMenu * m_submenu;
};


/**
 * @brief The BooleanPropertyAction class allows to bind a boolean Q_PROPRERTY to a QAction
 */
class BooleanPropertyAction: public QAction
{
    Q_OBJECT
public:
    /**
     * @brief BooleanPropertyAction
     * @param title the title of the menu
     * @param model the object to observe
     * @param propertyName the name of the property on the @a model
     * @param parent QObject parent
     */
    BooleanPropertyAction(QString title, QObject* model , QString propertyName, QWidget *parent = nullptr);

private slots:
    void setModelChecked(bool checked);
private:
    QObject* m_model;
    QString m_propertyName;
};

#endif // CUSTOM_MENUS_HPP
