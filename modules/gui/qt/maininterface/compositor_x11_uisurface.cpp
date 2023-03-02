/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QOffscreenSurface>

#include "compositor_x11_uisurface.hpp"
#include "compositor_common.hpp"

using namespace vlc;

CompositorX11UISurface::CompositorX11UISurface(QWindow* window, QScreen* screen)
    : QWindow(screen)
    , m_renderWindow(window)
{
    setSurfaceType(QWindow::OpenGLSurface);

    m_renderWindow->installEventFilter(this);

    QSurfaceFormat format;
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(8);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    format.setSwapInterval(0);

    // UI is renderred on offscreen, no need for double bufferring
    format.setSwapBehavior(QSurfaceFormat::SingleBuffer);

    setFormat(format);

    m_context = new QOpenGLContext();
    m_context->setScreen(this->screen());
    m_context->setFormat(format);
    m_context->create();

    m_uiRenderControl = new CompositorX11RenderControl(window);

    m_uiWindow = new QQuickWindow(m_uiRenderControl);
    m_uiWindow->setDefaultAlphaBuffer(true);
    m_uiWindow->setFormat(format);
    m_uiWindow->setColor(Qt::transparent);
    m_uiWindow->setClearBeforeRendering(true);

    m_qmlEngine = new QQmlEngine();
    if (!m_qmlEngine->incubationController())
        m_qmlEngine->setIncubationController(m_uiWindow->incubationController());

    connect(m_uiWindow, &QQuickWindow::sceneGraphInitialized, this, &CompositorX11UISurface::createFbo);
    connect(m_uiWindow, &QQuickWindow::sceneGraphInvalidated, this, &CompositorX11UISurface::destroyFbo);
    connect(m_uiWindow, &QQuickWindow::beforeRendering, this, &CompositorX11UISurface::beforeRendering);
    connect(m_uiWindow, &QQuickWindow::afterRendering, this, &CompositorX11UISurface::afterRendering);

    connect(m_uiRenderControl, &QQuickRenderControl::renderRequested, this, &CompositorX11UISurface::requestUpdate);
    connect(m_uiRenderControl, &QQuickRenderControl::sceneChanged, this, &CompositorX11UISurface::requestUpdate);
}

CompositorX11UISurface::~CompositorX11UISurface()
{
    m_renderWindow->removeEventFilter(this);

    auto surface = new QOffscreenSurface();
    surface->setFormat(m_context->format());
    surface->create();

    // Make sure the context is current while doing cleanup. Note that we use the
    // offscreen surface here because passing 'this' at this point is not safe: the
    // underlying platform window may already be destroyed. To avoid all the trouble, use
    // another surface that is valid for sure.
    m_context->makeCurrent(surface);

    delete m_rootItem;
    delete m_uiRenderControl;
    delete m_uiWindow;
    delete m_qmlEngine;

    m_context->doneCurrent();

    delete surface;
    delete m_context;
}


void CompositorX11UISurface::setContent(QQmlComponent*,  QQuickItem* rootItem)
{
    m_rootItem = rootItem;

    QQuickItem* contentItem  = m_uiWindow->contentItem();

    m_rootItem->setParentItem(contentItem);

    updateSizes();
}

QQuickItem * CompositorX11UISurface::activeFocusItem() const /* override */
{
    return m_uiWindow->activeFocusItem();
}

QQuickWindow* CompositorX11UISurface::getOffscreenWindow() const
{
    return m_uiWindow;
}

void CompositorX11UISurface::createFbo()
{
    //write to the immediate context
    QSize fboSize = size() * devicePixelRatio();
    m_uiWindow->setRenderTarget(0, fboSize);
    emit sizeChanged(fboSize);
}

void CompositorX11UISurface::destroyFbo()
{
}

void CompositorX11UISurface::render()
{
    if (!isExposed())
        return;

    m_context->makeCurrent(this);

    m_uiRenderControl->polishItems();
    m_uiRenderControl->sync();

    // TODO: investigate multithreaded renderer
    m_uiRenderControl->render();

    m_uiWindow->resetOpenGLState();

    m_context->functions()->glFlush();
    m_context->swapBuffers(this);

    emit updated();
}

void CompositorX11UISurface::updateSizes()
{
    qreal dpr = devicePixelRatio();
    QSize windowSize = size();

    m_onscreenSize = windowSize * dpr;

    // Behave like SizeRootObjectToView.
    m_rootItem->setSize(windowSize);
    m_uiWindow->resize(windowSize);
}

bool CompositorX11UISurface::event(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::UpdateRequest:
        render();
        return true;
    default:
        return QWindow::event(event);
    }
}


static void remapInputMethodQueryEvent(QObject *object, QInputMethodQueryEvent *e)
{
    auto item = qobject_cast<QQuickItem *>(object);
    if (!item)
        return;
    // Remap all QRectF values.
    for (auto query : {Qt::ImCursorRectangle, Qt::ImAnchorRectangle, Qt::ImInputItemClipRectangle})
    {
        if (e->queries() & query)
        {
            auto value = e->value(query);
            if (value.canConvert<QRectF>())
                e->setValue(query, item->mapRectToScene(value.toRectF()));
        }
    }
    // Remap all QPointF values.
    if (e->queries() & Qt::ImCursorPosition)
    {
        auto value = e->value(Qt::ImCursorPosition);
        if (value.canConvert<QPointF>())
            e->setValue(Qt::ImCursorPosition, item->mapToScene(value.toPointF()));
    }
}

bool CompositorX11UISurface::eventFilter(QObject*, QEvent *event)
{
    switch (event->type())
    {

    case QEvent::Move:
    {
        QPoint windowPosition = mapToGlobal(QPoint(0,0));
        if (m_uiWindow->position() != windowPosition)
            m_uiWindow->setPosition(windowPosition);
        break;
    }

    case QEvent::Resize:
    {
        QResizeEvent* resizeEvent = static_cast<QResizeEvent*>(event);
        m_uiWindow->resize(resizeEvent->size());
        resize( resizeEvent->size() );
        resizeFbo();
        break;
    }

    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::Leave:
    {
        return QCoreApplication::sendEvent(m_uiWindow, event);
    }

    case QEvent::Enter:
    {
        QEnterEvent *enterEvent = static_cast<QEnterEvent *>(event);
        QEnterEvent mappedEvent(enterEvent->localPos(), enterEvent->windowPos(),
                                enterEvent->screenPos());
        bool ret = QCoreApplication::sendEvent(m_uiWindow, &mappedEvent);
        event->setAccepted(mappedEvent.isAccepted());
        return ret;
    }

    case QEvent::InputMethod:
        return QCoreApplication::sendEvent(m_uiWindow->focusObject(), event);

    case QEvent::InputMethodQuery:
    {
        bool eventResult = QCoreApplication::sendEvent(m_uiWindow->focusObject(), event);
        // The result in focusObject are based on offscreenWindow. But
        // the inputMethodTransform won't get updated because the focus
        // is on QQuickWidget. We need to remap the value based on the
        // widget.
        remapInputMethodQueryEvent(m_uiWindow->focusObject(), static_cast<QInputMethodQueryEvent *>(event));
        return eventResult;
    }

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QMouseEvent mappedEvent(mouseEvent->type(), mouseEvent->localPos(),
                                mouseEvent->localPos(), mouseEvent->screenPos(),
                                mouseEvent->button(), mouseEvent->buttons(),
                                mouseEvent->modifiers(), mouseEvent->source());
        QCoreApplication::sendEvent(m_uiWindow, &mappedEvent);
        return true;
    }

    case QEvent::Wheel:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
    case QEvent::DragResponse:
    case QEvent::Drop:
        return QCoreApplication::sendEvent(m_uiWindow, event);

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        return QCoreApplication::sendEvent(m_uiWindow, event);
    }

    case QEvent::ScreenChangeInternal:
        m_uiWindow->setScreen(screen());
        break;

    default:
        break;
    }
    return false;
}

void CompositorX11UISurface::resizeFbo()
{
    if (m_rootItem && m_context->makeCurrent(this))
    {
        createFbo();
        m_context->doneCurrent();
        updateSizes();
    }
}

void CompositorX11UISurface::resizeEvent(QResizeEvent *)
{
    if (m_onscreenSize != size() * devicePixelRatio())
        resizeFbo();
}

void CompositorX11UISurface::exposeEvent(QExposeEvent *)
{
    if (isExposed())
    {
        if (!m_uiWindow->openglContext())
        {
            m_context->makeCurrent(this);
            m_uiRenderControl->initialize(m_context);
            m_context->doneCurrent();
        }
        requestUpdate();
    }
}

void CompositorX11UISurface::handleScreenChange()
{
    m_uiWindow->setGeometry(0, 0, width(), height());
    requestUpdate();
}

QWindow* CompositorX11RenderControl::renderWindow(QPoint* offset)
{
    if (offset)
        *offset = QPoint(0, 0);
    return m_window;
}
