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

namespace vlc {

CompositorDummy::CompositorDummy(intf_thread_t *p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
}

MainInterface* CompositorDummy::makeMainInterface()
{
    m_rootWindow = new MainInterface(m_intf);
    m_rootWindow->show();
    QQuickWidget* centralWidget = new QQuickWidget(m_rootWindow);
    centralWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    MainUI* m_ui = new MainUI(m_intf, m_rootWindow, this);
    m_ui->setup(centralWidget->engine());
    centralWidget->setContent(QUrl(), m_ui->getComponent(), m_ui->createRootItem());

    m_rootWindow->setCentralWidget(centralWidget);
    return m_rootWindow;
}

void CompositorDummy::destroyMainInterface()
{
    if (m_rootWindow)
    {
        delete m_rootWindow;
        m_rootWindow = nullptr;
    }
}

bool CompositorDummy::setupVoutWindow(vout_window_t*)
{
    //dummy compositor doesn't handle window intergration
    return false;
}

}
