/*****************************************************************************
 * Copyright (C) 2020 VideoLAN and AUTHORS
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
#ifndef INTERFACEWINDOWHANDLER_H
#define INTERFACEWINDOWHANDLER_H

#include "qt.hpp"

#include <QObject>
#include <QWindow>

class MainInterface;
class InterfaceWindowHandler : public QObject
{
    Q_OBJECT
public:
    explicit InterfaceWindowHandler(intf_thread_t *_p_intf, MainInterface* mainInterface, QWindow* window, QObject *parent = nullptr);
    virtual ~InterfaceWindowHandler();

public slots:
    virtual void onVideoEmbedChanged( bool embed );

protected slots:
    virtual void setFullScreen( bool fs );
    virtual void setInterfaceFullScreen( bool fs );
    virtual void setInterfaceAlwaysOnTop( bool on_top );
    virtual void toggleWindowVisiblity();
    virtual void setInterfaceVisible(bool);


    virtual void setRaise();
    virtual void setBoss();

    virtual bool eventFilter(QObject*, QEvent* event) override;

signals:
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
    void interfaceAlwaysOnTopChanged(bool);
    void interfaceFullScreenChanged(bool);
    void incrementIntfUserScaleFactor(bool increment);

private:
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    bool CSDSetCursor(QMouseEvent* mouseEvent);
    bool CSDHandleClick(QMouseEvent* mouseEvent);
#endif

protected:
    intf_thread_t* p_intf = nullptr;
    QWindow* m_window = nullptr;
    MainInterface* m_mainInterface = nullptr;

    bool m_hasPausedWhenMinimized = false;

    bool m_isWindowTiled = false;

    bool m_pauseOnMinimize ;
    bool m_maximizedView = false;
    bool m_hideAfterCreation  = false; // --qt-start-minimized

    bool m_hasResizeCursor = false;

    QRect m_interfaceGeometry;
};

#endif // INTERFACEWINDOWHANDLER_H
