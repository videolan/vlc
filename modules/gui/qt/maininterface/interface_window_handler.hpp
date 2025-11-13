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
#include <QPointer>
#include "util/vlchotkeyconverter.hpp"

class MainCtx;
class InterfaceWindowHandler : public QObject
{
    Q_OBJECT
public:
    explicit InterfaceWindowHandler(qt_intf_t *_p_intf, MainCtx* mainCtx, QWindow* window, QObject *parent = nullptr);
    virtual ~InterfaceWindowHandler();

public slots:
    virtual void onVideoEmbedChanged( bool embed );

protected slots:
    virtual void setFullScreen( bool fs );
    virtual void setInterfaceFullScreen( bool fs );
    virtual void setInterfaceAlwaysOnTop( bool on_top );
    virtual void toggleWindowVisibility();
    virtual void setInterfaceVisible(bool);
    virtual void setInterfaceHiden();
    virtual void setInterfaceShown();
    virtual void setInterfaceMinimized();
    virtual void setInterfaceMaximized();
    virtual void setInterfaceNormal();

    virtual void setRaise();
    virtual void setBoss();

    void requestActivate();

    bool eventFilter(QObject*, QEvent* event) override;

signals:
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
    void interfaceAlwaysOnTopChanged(bool);
    void interfaceFullScreenChanged(bool);
    void incrementIntfUserScaleFactor(bool increment);
    void kc_pressed();

private:
    bool applyKeyEvent(QKeyEvent * event) const;

protected:
    virtual void updateCSDWindowSettings();

protected:
    qt_intf_t* p_intf = nullptr;
    QPointer<QWindow> m_window;

    MainCtx* m_mainCtx = nullptr;

    bool m_hasPausedWhenMinimized = false;

    bool m_isWindowTiled = false;

    bool m_pauseOnMinimize ;
    bool m_maximizedView = false;
    bool m_hideAfterCreation  = false; // --qt-start-minimized

    bool m_hasResizeCursor = false;

    QRect m_interfaceGeometry;

    WheelToVLCConverter m_wheelAccumulator;

    static const Qt::Key kc[10]; /* easter eggs */
    int i_kc_offset = 0;
};

#endif // INTERFACEWINDOWHANDLER_H
