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

class MainInterfaceWin32 : public MainInterface
{
    Q_OBJECT

public:
    MainInterfaceWin32( intf_thread_t *p_intf );
    virtual ~MainInterfaceWin32();

private:
    virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;
    virtual bool winEvent( MSG *, long * );
    virtual void toggleUpdateSystrayMenuWhenVisible() Q_DECL_OVERRIDE;

protected:
    virtual void resizeEvent( QResizeEvent *event ) Q_DECL_OVERRIDE;

private:
    HWND WinId( QWidget *);
    void createTaskBarButtons();

private:
    HIMAGELIST himl;
    ITaskbarList3 *p_taskbl;
    UINT taskbar_wmsg;

private slots:
    void changeThumbbarButtons(PlayerController::PlayingState );
    void playlistItemCountChanged( size_t itemId );
    virtual void reloadPrefs() Q_DECL_OVERRIDE;
    virtual void setVideoFullScreen( bool fs ) Q_DECL_OVERRIDE;
};

#endif // MAIN_INTERFACE_WIN32_HPP
