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
#include <QActionGroup>
#include "medialibrary/mlrecentmediamodel.hpp"

class QAbstractListModel;

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
     * @param grouping whether the menu should use an ActionGroup or not
     * @param parent QObject parent
     */
    CheckableListMenu(QString title, QAbstractListModel* model, QActionGroup::ExclusionPolicy grouping = QActionGroup::ExclusionPolicy::None, QWidget *parent = nullptr);

protected:
    QAbstractListModel* m_model = nullptr;
};

class QQuickImageResponse;

// NOTE: This class is a helper to populate and maintain a QMenu from an QAbstractListModel.
class ListMenuHelper : public QObject
{
    Q_OBJECT

public:
    // NOTE: The model actions will be inserted before 'before' or at the end if it's NULL.
    ListMenuHelper(QMenu * menu, QAbstractListModel * model, QAction * before = nullptr,
                   QObject * parent = nullptr);
    virtual ~ListMenuHelper();

public: // Interface
    int count() const;

    QActionGroup* getActionGroup() const;

private slots:
    void onRowsInserted(const QModelIndex & parent, int first, int last);
    void onRowsRemoved (const QModelIndex & parent, int first, int last);

    void onDataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight,
                       const QVector<int> & roles = QVector<int>());

    void onModelReset();

    void onTriggered(bool checked);

signals:
    void select(int index, bool checked);

    void countChanged(int count);

private:
    void setIcon(QAction* action,  const QUrl& iconUrl);

    QMenu * m_menu = nullptr;

    QActionGroup * m_group = nullptr;

    QAbstractListModel * m_model = nullptr;

    QList<QAction *> m_actions;

    QAction * m_before = nullptr;

    std::unique_ptr<QQuickImageResponse> m_iconLoader;
};

/**
 * @brief The BooleanPropertyAction class allows to bind a boolean Q_PROPERTY to a QAction
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

class RendererManager;
class QProgressBar;
class QLabel;
class PlayerController;
class RendererMenu : public QMenu
{
    Q_OBJECT

public:
    RendererMenu( QMenu* parent, qt_intf_t* intf, PlayerController* playerController );
    virtual ~RendererMenu();

private slots:
    void updateStatus();

protected:
    QProgressBar* m_statusProgressBar;
    QLabel* m_statusLabel;

    QAction *m_statusAction;
    QActionGroup *group;
    qt_intf_t *p_intf;
    RendererManager* m_renderManager = nullptr;
};


class RecentMenu : public QMenu
{
    Q_OBJECT
public:
    RecentMenu(MLRecentMediaModel* model, MediaLib* ml, QWidget *parent = nullptr);

private:
    MLRecentMediaModel* m_model = nullptr;
    MediaLib* m_ml = nullptr;
};

class BookmarkMenu : public QMenu
{
    Q_OBJECT

public:
    BookmarkMenu(MediaLib * mediaLib, vlc_player_t * player, QWidget * parent = nullptr);
};

#endif // CUSTOM_MENUS_HPP
