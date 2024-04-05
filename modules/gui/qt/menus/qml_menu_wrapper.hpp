/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef QMLMENUWRAPPER_HPP
#define QMLMENUWRAPPER_HPP

#include "qt.hpp"

#include <QObject>
#include <QPoint>
#include <QQuickItem>
#include "menus.hpp"
#include "maininterface/mainctx.hpp"
#include "util/list_selection_model.hpp"

Q_MOC_INCLUDE("playlist/playlist_controller.hpp")
Q_MOC_INCLUDE("playlist/playlist_model.hpp")
Q_MOC_INCLUDE("network/networkdevicemodel.hpp")
Q_MOC_INCLUDE("network/networkmediamodel.hpp")
Q_MOC_INCLUDE("medialibrary/mlplaylistlistmodel.hpp")
Q_MOC_INCLUDE("medialibrary/mlplaylistmodel.hpp")

class MediaLib;
class MLPlaylistListModel;
class MLPlaylistModel;
class NetworkDeviceModel;
class NetworkMediaModel;
class MainCtx;
namespace vlc {
namespace playlist {
class PlaylistController;
class PlaylistListModel;
}
}

#define SIMPLE_MENU_PROPERTY(type, name, defaultValue) \
    Q_PROPERTY(type name READ get##name WRITE set##name FINAL) \
    public: \
    inline void set##name( type data) { m_##name = data; } \
    inline type get##name() const { return m_##name; } \
    private: \
    type m_##name = defaultValue;


class StringListMenu : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void popup(const QPoint &point, const QVariantList &stringList);

signals:
    void selected(int index, const QString &str);
};


class SortMenu : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool shown READ isShown NOTIFY shownChanged FINAL)

public:
    using QObject::QObject;

    Q_INVOKABLE void popup(const QPoint &point, bool popupAbovePoint, const QVariantList &model);

    Q_INVOKABLE void close();

    bool isShown() const { return m_shown; };

protected:
    virtual void onPopup(QMenu * menu);

signals:
    void selected(int index);
    void shownChanged();

private:
    std::unique_ptr<QMenu> m_menu;
    bool m_shown = false;
};

class SortMenuVideo : public SortMenu
{
    Q_OBJECT

    SIMPLE_MENU_PROPERTY(MainCtx *, ctx, nullptr)

protected: // SortMenu reimplementation
    void onPopup(QMenu * menu) override;

signals:
    void grouping(MainCtx::Grouping grouping);
};

//inherit VLCMenuBar so we can access menu creation functions
class QmlGlobalMenu : public VLCMenuBar
{
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MainCtx*, ctx, nullptr)

    Q_PROPERTY(bool shown READ isShown NOTIFY shownChanged FINAL)

public:
    explicit QmlGlobalMenu(QObject *parent = nullptr);

    bool isShown() const { return m_shown; };

signals:
    void aboutToShow();
    void aboutToHide();
    void shownChanged();

public slots:
    void popup( QPoint pos );
private:
    std::unique_ptr<QMenu> m_menu;
    bool m_shown = false;
};

//inherit VLCMenuBar so we can access menu creation functions
class QmlMenuBarMenu;
class QmlMenuBar : public VLCMenuBar
{
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MainCtx*, ctx, nullptr)
    SIMPLE_MENU_PROPERTY(QQuickItem*, menubar, nullptr)
    SIMPLE_MENU_PROPERTY(bool, openMenuOnHover, false)

public:
    explicit QmlMenuBar(QObject *parent = nullptr);

signals:
    //navigate to the left(-1)/right(1) menu
    void navigateMenu(int direction);

    void menuClosed();

public slots:
    void popupMediaMenu( QQuickItem* button);
    void popupPlaybackMenu( QQuickItem* button);
    void popupAudioMenu( QQuickItem* button );
    void popupVideoMenu( QQuickItem* button );
    void popupSubtitleMenu( QQuickItem* button );
    void popupToolsMenu( QQuickItem* button );
    void popupViewMenu( QQuickItem* button );
    void popupHelpMenu( QQuickItem* button );

private slots:
    void onMenuClosed();

private:
    typedef QMenu* (*CreateMenuFunc)();
    void popupMenuCommon( QQuickItem* button, std::function<void(QMenu*)> createMenuFunc);
    std::unique_ptr<QMenu> m_menu;
    QQuickItem* m_button = nullptr;
    friend class QmlMenuBarMenu;
};

//specialized QMenu for QmlMenuBar
class QmlMenuBarMenu : public QMenu
{
    Q_OBJECT
public:
    QmlMenuBarMenu(QmlMenuBar* menubar, QWidget* parent = nullptr);
    ~QmlMenuBarMenu();
protected:
    void mouseMoveEvent(QMouseEvent* mouseEvent) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
private:
    QmlMenuBar* m_menubar = nullptr;
};

class QmlMenuPositioner : public QObject
{
public:
    explicit QmlMenuPositioner(QObject * parent = nullptr);

public: // Interface
    void popup(QMenu * menu, const QPoint & position, bool above);

public: // Events
    bool eventFilter(QObject * object, QEvent * event);

private:
    QPoint m_position;
};

class QmlBookmarkMenu : public QObject
{
    Q_OBJECT

    SIMPLE_MENU_PROPERTY(MainCtx *, ctx, nullptr)

    SIMPLE_MENU_PROPERTY(PlayerController *, player, nullptr)

public:
    explicit QmlBookmarkMenu(QObject * parent = nullptr);

public: // Interface
    Q_INVOKABLE void popup(const QPoint & position, bool above = false);

signals:
    void aboutToHide();
    void aboutToShow();

private:
    QmlMenuPositioner m_positioner;

    std::unique_ptr<QMenu> m_menu;
};

class QmlProgramMenu : public QObject
{
    Q_OBJECT

    SIMPLE_MENU_PROPERTY(PlayerController *, player, nullptr)

public:
    explicit QmlProgramMenu(QObject * parent = nullptr);

public: // Interface
    Q_INVOKABLE void popup(const QPoint & position, bool above = false);

signals:
    void aboutToHide();
    void aboutToShow();

private:
    QmlMenuPositioner m_positioner;

    std::unique_ptr<QMenu> m_menu;
};

class QmlRendererMenu : public QObject
{
    Q_OBJECT

    SIMPLE_MENU_PROPERTY(MainCtx *, ctx, nullptr)

public:
    explicit QmlRendererMenu(QObject * parent = nullptr);

public: // Interface
    Q_INVOKABLE void popup(const QPoint & position, bool above = false);

signals:
    void aboutToHide();
    void aboutToShow();

private:
    QmlMenuPositioner m_positioner;

    std::unique_ptr<RendererMenu> m_menu;
};

// Tracks

class QmlTrackMenu : public QObject
{
    Q_OBJECT

public: // Enums
    enum Action
    {
        Open,
        Synchronize,
        Download
    };

    Q_ENUM(Action)

public:
    QmlTrackMenu(QObject * parent = nullptr);

public: // Interface
    Q_INVOKABLE void popup(const QPoint & position);

protected: // Abstract functions
    virtual void beforePopup(QMenu * menu) = 0;

signals:
    void triggered(Action action);

private:
    std::unique_ptr<QMenu> m_menu;
};

class QmlSubtitleMenu : public QmlTrackMenu
{
    Q_OBJECT

    SIMPLE_MENU_PROPERTY(PlayerController *, player, nullptr)

public:
    QmlSubtitleMenu(QObject * parent = nullptr);

protected: // QmlTrackMenu implementation
    void beforePopup(QMenu * menu) override;
};

class QmlAudioMenu : public QmlTrackMenu
{
    Q_OBJECT

public:
    QmlAudioMenu(QObject * parent = nullptr);

protected: // QmlTrackMenu implementation
    void beforePopup(QMenu * menu) override;
};

class PlaylistListContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLPlaylistListModel *, model, nullptr)
public:
    PlaylistListContextMenu(QObject * parent = nullptr);

public slots:
    void popup(const QModelIndexList & selected, QPoint pos, QVariantMap options = {});
private:
    std::unique_ptr<QMenu> m_menu;
};

class PlaylistMediaContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(MLPlaylistModel *, model, nullptr)
public:
    PlaylistMediaContextMenu(QObject * parent = nullptr);

public slots:
    void popup(const QModelIndexList & selected, QPoint pos, QVariantMap options = {});
signals:
    void showMediaInformation(int index);
private:
    std::unique_ptr<QMenu> m_menu;
};

class NetworkMediaContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(NetworkMediaModel*, model, nullptr)
public:
    NetworkMediaContextMenu(QObject* parent = nullptr);

public slots:
    void popup(const QModelIndexList& selected, QPoint pos );
private:
    std::unique_ptr<QMenu> m_menu;
};

class NetworkDeviceContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(NetworkDeviceModel*, model, nullptr)
public:
    NetworkDeviceContextMenu(QObject* parent = nullptr);
public slots:
    void popup(const QModelIndexList& selected, QPoint pos );
private:
    std::unique_ptr<QMenu> m_menu;
};

class PlaylistContextMenu : public QObject {
    Q_OBJECT
    SIMPLE_MENU_PROPERTY(vlc::playlist::PlaylistListModel*, model, nullptr)
    SIMPLE_MENU_PROPERTY(vlc::playlist::PlaylistController*, controler, nullptr)
    SIMPLE_MENU_PROPERTY(ListSelectionModel*, selectionModel, nullptr)
public:
    PlaylistContextMenu(QObject* parent = nullptr);

signals:
    void jumpToCurrentPlaying();

public slots:
    void popup(int currentIndex, QPoint pos );
private:
    std::unique_ptr<QMenu> m_menu;
};

#undef SIMPLE_MENU_PROPERTY

#endif // QMLMENUWRAPPER_HPP
