/*****************************************************************************
 * main_interface_win32.cpp : Main interface
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

#include "maininterface/main_interface.hpp"
#include "interface_window_handler.hpp"
#include <QAbstractNativeEventFilter>

class WinTaskbarWidget : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    WinTaskbarWidget( intf_thread_t *p_intf, QWindow* windowHandle, QObject* parent = nullptr);
    virtual ~WinTaskbarWidget();

private:
    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
    void createTaskBarButtons();

private slots:
    void changeThumbbarButtons(PlayerController::PlayingState );
    void playlistItemCountChanged( size_t itemId );
    virtual void onVideoFullscreenChanged( bool fs );

private:
    intf_thread_t* p_intf = nullptr;
    HIMAGELIST himl = nullptr;
    ITaskbarList3 *p_taskbl = nullptr;
    UINT taskbar_wmsg = 0;
    QWindow* m_window = nullptr;

};



class MainInterfaceWin32 : public MainInterface
{
    Q_OBJECT
public:
    MainInterfaceWin32( intf_thread_t *, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    virtual ~MainInterfaceWin32() = default;

private:
    virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

public slots:
    virtual void reloadPrefs() override;

protected:
    void updateClientSideDecorations() Q_DECL_OVERRIDE;
};

class InterfaceWindowHandlerWin32 : public InterfaceWindowHandler
{
    Q_OBJECT
public:
    explicit InterfaceWindowHandlerWin32(intf_thread_t *_p_intf, MainInterface* mainInterface, QWindow* window, QObject *parent = nullptr);
    virtual ~InterfaceWindowHandlerWin32() = default;
    virtual void toggleWindowVisiblity() override;

    virtual bool eventFilter(QObject*, QEvent* event) override;
};

#endif // MAIN_INTERFACE_WIN32_HPP
