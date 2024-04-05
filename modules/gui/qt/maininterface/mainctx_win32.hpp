/*****************************************************************************
 * mainctx_win32.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MAIN_INTERFACE_WIN32_HPP
#define MAIN_INTERFACE_WIN32_HPP

#include "maininterface/mainctx.hpp"
#include "interface_window_handler.hpp"
#include <QAbstractNativeEventFilter>

class WinTaskbarWidget : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    WinTaskbarWidget( qt_intf_t *p_intf, QWindow* windowHandle, QObject* parent = nullptr);
    virtual ~WinTaskbarWidget();

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void createTaskBarButtons();

private slots:
    void changeThumbbarButtons(PlayerController::PlayingState );
    void playlistItemCountChanged( size_t itemId );
    virtual void onVideoFullscreenChanged( bool fs );

private:
    qt_intf_t* p_intf = nullptr;
    HIMAGELIST himl = nullptr;
    ITaskbarList3 *p_taskbl = nullptr;
    UINT taskbar_wmsg = 0;
    QWindow* m_window = nullptr;

};



class MainCtxWin32 : public MainCtx
{
    Q_OBJECT
public:
    explicit MainCtxWin32(qt_intf_t *);
    virtual ~MainCtxWin32() = default;

public slots:
    void reloadPrefs() override;
};

class InterfaceWindowHandlerWin32 : public InterfaceWindowHandler, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit InterfaceWindowHandlerWin32(qt_intf_t *_p_intf, MainCtx* mainCtx, QWindow* window, QObject *parent = nullptr);
    virtual ~InterfaceWindowHandlerWin32();
    void toggleWindowVisibility() override;

    bool eventFilter(QObject*, QEvent* event) override;

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
    void updateCSDWindowSettings() override;
    QObject *m_CSDWindowEventHandler {};
#endif
};

#endif // MAIN_INTERFACE_WIN32_HPP
