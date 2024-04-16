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
#include <QGuiApplication>
#include <QApplication>
#include <QQuickRenderTarget>
#include <QQuickGraphicsDevice>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>
#include <QThread>

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
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    format.setSwapInterval(0);

    // UI is renderred on offscreen, no need for double bufferring
    format.setSwapBehavior(QSurfaceFormat::SingleBuffer);

    // Check if this is XWayland:
    if (Q_UNLIKELY(QApplication::platformName() == QLatin1String("xcb") &&
                   qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")))
    {
        applyNvidiaWorkaround(format);
    }

    setFormat(format);

    m_context = new QOpenGLContext();
    m_context->setScreen(this->screen());
    m_context->setFormat(format);
    m_context->create();

    m_uiRenderControl = new CompositorX11RenderControl(window);

    m_uiWindow = new CompositorOffscreenWindow(m_uiRenderControl);
    m_uiWindow->setDefaultAlphaBuffer(true);
    m_uiWindow->setFormat(format);
    m_uiWindow->setColor(Qt::transparent);

    m_qmlEngine = new QQmlEngine();
    if (!m_qmlEngine->incubationController())
        m_qmlEngine->setIncubationController(m_uiWindow->incubationController());

    connect(m_uiWindow, &QQuickWindow::sceneGraphInitialized, this, &CompositorX11UISurface::createFbo);
    connect(m_uiWindow, &QQuickWindow::sceneGraphInvalidated, this, &CompositorX11UISurface::destroyFbo);
    connect(m_uiWindow, &QQuickWindow::beforeRendering, this, &CompositorX11UISurface::beforeRendering);
    connect(m_uiWindow, &QQuickWindow::afterRendering, this, &CompositorX11UISurface::afterRendering);

    connect(m_uiWindow, &QQuickWindow::focusObjectChanged, this, &CompositorX11UISurface::forwardFocusObjectChanged);

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

    if (m_textureId)
        m_context->functions()->glDeleteTextures(1, &m_textureId);

    if (m_fboId)
        m_context->functions()->glDeleteFramebuffers(1, &m_fboId);

    m_context->doneCurrent();

    delete m_context;
}


void CompositorX11UISurface::setContent(QQmlComponent*,  QQuickItem* rootItem)
{
    m_rootItem = rootItem;

    m_rootItem->setParentItem(m_uiWindow->contentItem());

    updateSizes();

    m_rootItem->forceActiveFocus();

    m_context->makeCurrent(this);
    m_uiWindow->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(m_context));
    m_uiRenderControl->initialize();

    initialized = true;
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
    // The scene graph has been initialized. It is now time to create an texture and associate
    // it with the QQuickWindow.
    m_dpr = devicePixelRatio();
    QSize fboSize = size() * devicePixelRatio();
    QOpenGLFunctions *f = m_context->functions();
    f->glGenTextures(1, &m_textureId);
    f->glBindTexture(GL_TEXTURE_2D, m_textureId);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fboSize.width(), fboSize.height(), 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    m_uiWindow->setRenderTarget(QQuickRenderTarget::fromOpenGLTexture(m_textureId, fboSize));

    f->glGenFramebuffers(1, &m_fboId);

    emit sizeChanged(fboSize);
}

void CompositorX11UISurface::destroyFbo()
{
    m_context->functions()->glDeleteTextures(1, &m_textureId);
    m_textureId = 0;
    m_context->functions()->glDeleteFramebuffers(1, &m_fboId);
    m_fboId = 0;
}

void CompositorX11UISurface::render()
{
    if (!isExposed())
        return;

    const bool current = m_context->makeCurrent(this);
    assert(current);

    m_uiRenderControl->beginFrame();
    m_uiRenderControl->polishItems();
    m_uiRenderControl->sync();

    // TODO: investigate multithreaded renderer
    m_uiRenderControl->render();

    m_uiRenderControl->endFrame();

    //m_uiWindow->resetOpenGLState();

    QOpenGLFramebufferObject::bindDefault();
    m_context->functions()->glFlush();

    const QSize fboSize = size() * devicePixelRatio();

    m_context->functions()->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboId);
    m_context->functions()->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureId, 0);
    m_context->extraFunctions()->glBlitFramebuffer(0, 0, fboSize.width(), fboSize.height(), 0, 0, fboSize.width(), fboSize.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
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
        QEnterEvent mappedEvent(enterEvent->position(), enterEvent->scenePosition(),
                                enterEvent->globalPosition());
        bool ret = QCoreApplication::sendEvent(m_uiWindow, &mappedEvent);
        event->setAccepted(mappedEvent.isAccepted());
        return ret;
    }

    case QEvent::FocusAboutToChange:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        return QCoreApplication::sendEvent(m_uiWindow, event);

    case QEvent::Show:
        m_uiWindow->setPseudoVisible(true);
        break;
    case QEvent::Hide:
        m_uiWindow->setPseudoVisible(false);
        break;
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
        QMouseEvent mappedEvent(mouseEvent->type(), mouseEvent->position(),
                                mouseEvent->position(), mouseEvent->globalPosition(),
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
    if (m_rootItem)
    {
        const bool current = m_context->makeCurrent(this);
        assert(current);

        m_context->functions()->glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
        m_context->functions()->glDeleteFramebuffers(1, &m_fboId);
        m_fboId = 0;
        createFbo();
        m_context->doneCurrent();
        updateSizes();
        render();
    }
}

void CompositorX11UISurface::applyNvidiaWorkaround(QSurfaceFormat &format)
{
    assert(QThread::currentThread() == qApp->thread());

    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();

    QOpenGLContext ctx;
    ctx.setFormat(format);
    if (ctx.create() && ctx.makeCurrent(&surface))
    {
        // Context needs to be created to access the functions
        if (QOpenGLFunctions * const func = ctx.functions())
        {
            if (const GLubyte* str = func->glGetString(GL_VENDOR))
            {
                if (!strcmp(reinterpret_cast<const char *>(str), "NVIDIA Corporation"))
                {
                    // for some reason SingleBuffer is not supported:
                    format.setSwapBehavior(QSurfaceFormat::DefaultSwapBehavior);
                }
            }
        }
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
        if (!initialized)
        {
            m_uiRenderControl->initialize();
        }
        requestUpdate();
    }
}

void CompositorX11UISurface::handleScreenChange()
{
   m_uiWindow->setGeometry(0, 0, width(), height());
   requestUpdate();
}

void CompositorX11UISurface::forwardFocusObjectChanged(QObject* object)
{
    m_renderWindow->focusObjectChanged(object);
}

QWindow* CompositorX11RenderControl::renderWindow(QPoint* offset)
{
    if (offset)
        *offset = QPoint(0, 0);
    return m_window;
}
