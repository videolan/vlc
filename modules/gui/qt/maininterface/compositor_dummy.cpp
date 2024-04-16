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

#include "maininterface/mainctx.hpp"
#include "maininterface/mainui.hpp"
#include "maininterface/interface_window_handler.hpp"

namespace vlc {

CompositorDummy::CompositorDummy(qt_intf_t *p_intf, QObject* parent)
    : QObject(parent)
    , m_intf(p_intf)
{
}

CompositorDummy::~CompositorDummy()
{
}

bool CompositorDummy::preInit(qt_intf_t *)
{
    return true;
}

bool CompositorDummy::init()
{
    return true;
}

bool CompositorDummy::makeMainInterface(MainCtx* mainCtx)
{
    m_mainCtx = mainCtx;

    QQuickWindow::setDefaultAlphaBuffer(false);

    m_qmlWidget = std::make_unique<QQuickView>();
    if (m_mainCtx->useClientSideDecoration())
        m_qmlWidget->setFlag(Qt::FramelessWindowHint);
    m_qmlWidget->setResizeMode(QQuickView::SizeRootObjectToView);

    m_intfWindowHandler = std::make_unique<InterfaceWindowHandler>(m_intf, m_mainCtx, m_qmlWidget.get(), nullptr);

    MainUI* ui = new MainUI(m_intf, m_mainCtx, m_qmlWidget.get(), m_qmlWidget.get());
    ui->setup(m_qmlWidget->engine());
    m_qmlWidget->setContent(QUrl(), ui->getComponent(), ui->createRootItem());

    if (m_qmlWidget->status() != QQuickView::Ready)
        return false;

    m_qmlWidget->show();

    return true;
}

QWindow* CompositorDummy::interfaceMainWindow() const
{
    return m_qmlWidget.get();
}

void CompositorDummy::destroyMainInterface()
{
    unloadGUI();
}

void CompositorDummy::unloadGUI()
{
    m_intfWindowHandler.reset();
    m_qmlWidget.reset();
}

bool CompositorDummy::setupVoutWindow(vlc_window_t*, VoutDestroyCb)
{
    //dummy compositor doesn't handle window integration
    return false;
}

Compositor::Type CompositorDummy::type() const
{
    return Compositor::DummyCompositor;
}

QQuickItem * CompositorDummy::activeFocusItem() const /* override */
{
    return m_qmlWidget->activeFocusItem();
}

}
