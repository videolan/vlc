/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include "compositor_dummy.hpp"

#include "maininterface/main_interface.hpp"
#include "maininterface/mainui.hpp"
#include "maininterface/interface_window_handler.hpp"

namespace vlc {

CompositorDummy::CompositorDummy(qt_intf_t *p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
}

MainInterface* CompositorDummy::makeMainInterface()
{
    m_rootWindow = new MainInterface(m_intf);
    if (m_rootWindow->useClientSideDecoration())
        m_rootWindow->setWindowFlag(Qt::FramelessWindowHint);
    m_rootWindow->show();
    m_qmlWidget = new QQuickWidget(m_rootWindow);
    m_qmlWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    new InterfaceWindowHandler(m_intf, m_rootWindow, m_rootWindow->windowHandle(), m_rootWindow);

    MainUI* m_ui = new MainUI(m_intf, m_rootWindow, m_rootWindow->windowHandle(), this);
    m_ui->setup(m_qmlWidget->engine());
    m_qmlWidget->setContent(QUrl(), m_ui->getComponent(), m_ui->createRootItem());

    m_rootWindow->setCentralWidget(m_qmlWidget);

    connect(m_rootWindow, &MainInterface::requestInterfaceMaximized,
            m_rootWindow, &MainInterface::showMaximized);
    connect(m_rootWindow, &MainInterface::requestInterfaceNormal,
            m_rootWindow, &MainInterface::showNormal);

    return m_rootWindow;
}

void CompositorDummy::destroyMainInterface()
{
    if (m_qmlWidget)
    {
        delete m_qmlWidget;
        m_qmlWidget = nullptr;
    }
    if (m_rootWindow)
    {
        delete m_rootWindow;
        m_rootWindow = nullptr;
    }
}

bool CompositorDummy::setupVoutWindow(vout_window_t*, VoutDestroyCb)
{
    //dummy compositor doesn't handle window intergration
    return false;
}

Compositor::Type CompositorDummy::type() const
{
    return Compositor::DummyCompositor;
}

}
