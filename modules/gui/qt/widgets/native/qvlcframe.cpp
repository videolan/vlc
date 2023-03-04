/*****************************************************************************
 * qvlcframe.cpp : A few helpers
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <QWindow>
#include <QScreen>
#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QStyle>

#include "qvlcframe.hpp"
#include "maininterface/compositor.hpp"

void QVLCTools::saveWidgetPosition(QSettings *settings, QWidget *widget)
{
    settings->setValue("geometry", widget->saveGeometry());
}

void QVLCTools::saveWindowPosition(QSettings *settings, QWindow *window)
{
    //no standard way to do it, mimic what qt is doing internally
    QByteArray array;
    QDataStream serialized(&array, QIODevice::WriteOnly);
    serialized.setVersion(QDataStream::Qt_4_0);

    //we use a different magic number than Qt
    const quint32 magicNumber = 0x1D94200;
    serialized << magicNumber
        << quint8(1) //version major
        << quint8(0) //version minor
        << window->screen()->name()
        << window->screen()->devicePixelRatio()
        << window->screen()->geometry()
        << window->position()
        << window->geometry()
        << quint8(window->windowStates() & Qt::WindowMaximized)
        << quint8(window->windowStates() & Qt::WindowFullScreen);
    settings->setValue("geometry", array);
}

static bool restoreWindowPositionImpl(QSettings *settings, QWindow *window)
{
    //no standard way to do it, mimic what qt is doing internally
    auto raw = settings->value("geometry").toByteArray();

    if (raw.length() < 4)
        return false;

    QDataStream serialized(raw);
    serialized.setVersion(QDataStream::Qt_4_0);
    quint32 magicNumber;
    quint8 versionMajor;
    quint8 versionMinor;

    serialized
        >> magicNumber
        >> versionMajor
        >> versionMinor;

    if (magicNumber != 0x1D94200 || versionMajor != 1)
        return false;

    QString screenName;
    qreal screenDRP;
    QRect screenGeometry;

    QPoint position;
    QRect geometry;
    quint8 maximized;
    quint8 fullscreen;

    serialized
        >> screenName
        >> screenDRP
        >> screenGeometry
        >> position
        >> geometry
        >> maximized
        >> fullscreen;

    if (screenName.isNull() || screenGeometry.isNull())
        return false;

    if (geometry.isNull())
        return false;

    bool screenFound = false;
    for (auto screen: QGuiApplication::screens())
    {
        if (screen->name() == screenName)
        {
            if (screen->geometry() != screenGeometry
                || screen->devicePixelRatio() != screenDRP)
            {
                //same screen but its property has changed, don't restore the position
                break;
            }
            window->setScreen(screen);
            screenFound = true;
            break;
        }
    }
    if (!screenFound)
        return false;

    window->setPosition(position);
    window->setGeometry(geometry);

    if (maximized || fullscreen)
    {
        Qt::WindowStates state = window->windowStates();
        if (maximized)
            state |= Qt::WindowMaximized;
        if (fullscreen)
            state |= Qt::WindowFullScreen;
        window->setWindowStates(state);
    }
    else
    {
        window->setWindowStates(window->windowStates() & ~(Qt::WindowMaximized | Qt::WindowFullScreen));
    }

    return true;
}

void QVLCTools::restoreWindowPosition(QSettings *settings, QWindow *window, QSize defSize, QPoint defPos)
{
    bool ret = restoreWindowPositionImpl(settings, window);
    if (ret)
        return;

    window->resize(defSize);
    if (defPos.isNull())
    {
        window->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, window->size(), QGuiApplication::primaryScreen()->availableGeometry()));
    }
    else
    {
        window->setPosition(defPos);
    }
}


void QVLCTools::saveWidgetPosition(qt_intf_t *p_intf,
                                   const QString& configName,
                                   QWidget *widget)
{
    getSettings()->beginGroup(configName);
    QVLCTools::saveWidgetPosition(getSettings(), widget);
    getSettings()->endGroup();
}

bool QVLCTools::restoreWidgetPosition(QSettings *settings, QWidget *widget,
                                      QSize defSize, QPoint defPos)
{
    if (!widget->restoreGeometry(settings->value("geometry").toByteArray()))
    {
        widget->move(defPos);
        widget->resize(defSize);

        if (defPos.x() == 0 && defPos.y()==0)
            widget->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, widget->size(), qApp->primaryScreen()->availableGeometry()));
        return true;
    }
    return false;
}

bool QVLCTools::restoreWidgetPosition(qt_intf_t *p_intf,
                                      const QString& configName,
                                      QWidget *widget, QSize defSize,
                                      QPoint defPos)
{
    getSettings()->beginGroup(configName);
    bool defaultUsed =
        QVLCTools::restoreWidgetPosition(getSettings(), widget, defSize, defPos);
    getSettings()->endGroup();

    return defaultUsed;
}

void QVLCFrame::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape)
    {
        this->cancel();
    }
    else if (keyEvent->key() == Qt::Key_Return ||
             keyEvent->key() == Qt::Key_Enter)
    {
        this->close();
    }
}

void QVLCDialog::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape)
    {
        this->cancel();
    }
    else if (keyEvent->key() == Qt::Key_Return ||
             keyEvent->key() == Qt::Key_Enter)
    {
        this->close();
    }
}

void QVLCDialog::setWindowTransientParent(QWidget* widget, QWindow* parent, qt_intf_t* p_intf)
{
    if (!parent && p_intf)
        parent = p_intf->p_compositor->interfaceMainWindow();
    if (!parent)
        return;

    widget->createWinId();
    QWindow* handle  = widget->windowHandle();
    handle->setTransientParent(parent);
}


QVLCDialog::QVLCDialog(QWindow *parent, qt_intf_t *_p_intf)
    : QDialog(),
      p_intf( _p_intf )
{
    setWindowFlags( Qt::Dialog|Qt::WindowMinMaxButtonsHint|
                    Qt::WindowSystemMenuHint|Qt::WindowCloseButtonHint );

    setWindowTransientParent(this, parent, p_intf);
}
