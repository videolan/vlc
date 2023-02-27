/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "flickable_scroll_handler.hpp"

#include <QApplication>
#include <QStyleHints>
#include <QtQml>
#include <QQmlProperty>

#define OVERRIDE_SCROLLBAR_STEPSIZE true
#define PAGE_SCROLL_SHIFT_OR_CTRL true

FlickableScrollHandler::FlickableScrollHandler(QObject *parent)
    : QObject(parent)
{
    connect(this, &FlickableScrollHandler::scaleFactorChanged, this, [this]() {
        m_effectiveScaleFactor = QApplication::styleHints()->wheelScrollLines() * 20 * m_scaleFactor;
        emit effectiveScaleFactorChanged();
    });

    setScaleFactor(1.0);
}

FlickableScrollHandler::~FlickableScrollHandler()
{
    detach();
}

void FlickableScrollHandler::classBegin()
{
}

void FlickableScrollHandler::componentComplete()
{
    assert(parent());

    m_target = qobject_cast<QQuickItem*>(parent());
    if (!m_target || !m_target->inherits("QQuickFlickable"))
    {
        qmlWarning(this) << "Parent is not QQuickFlickable!";
        return;
    }

    const auto qCtx = qmlContext(m_target);
    assert(qCtx);

    m_propertyContentX = QQmlProperty(m_target, "contentX", qCtx);
    m_propertyContentY = QQmlProperty(m_target, "contentY", qCtx);
    m_propertyContentHeight = QQmlProperty(m_target, "contentHeight", qCtx);
    m_propertyContentWidth = QQmlProperty(m_target, "contentWidth", qCtx);
    m_propertyHeight = QQmlProperty(m_target, "height", qCtx);
    m_propertyWidth = QQmlProperty(m_target, "width", qCtx);

    m_scrollBarV.scrollBar = QQmlProperty(m_target, "ScrollBar.vertical", qCtx);
    m_scrollBarH.scrollBar = QQmlProperty(m_target, "ScrollBar.horizontal", qCtx);

    adjustScrollBarV();
    adjustScrollBarH();

    m_scrollBarV.scrollBar.connectNotifySignal(this, SLOT(adjustScrollBarV()));
    m_scrollBarH.scrollBar.connectNotifySignal(this, SLOT(adjustScrollBarH()));

    if (enabled())
        attach();

    emit initialized();
}

bool FlickableScrollHandler::eventFilter(QObject *watched, QEvent *event)
{
    assert (event);
    assert (watched == m_target);

    if (event->type() != QEvent::Wheel)
        return QObject::eventFilter(watched, event);

    const auto wheel = static_cast<QWheelEvent *>(event);

    struct {
        QPoint delta;
        enum class Type {
            Pixel,
            Degree
        } type;
    } ev;

    using Type = decltype(ev)::Type;

    if (!wheel->pixelDelta().isNull() && (wheel->pixelDelta().manhattanLength() % 120))
    {
        ev.delta = wheel->pixelDelta();
        ev.type = Type::Pixel;
    }
    else if (!m_handleOnlyPixelDelta && !wheel->angleDelta().isNull())
    {
        ev.delta = wheel->angleDelta() / 8 / 15;
        ev.type = Type::Degree;
    }
    else
        return false;

    if (wheel->inverted())
    {
        ev.delta = -ev.delta;
    }

    const auto handler = [this, wheel, ev](Qt::Orientation orientation, bool fallback = false) {
        const auto vertical = (orientation == Qt::Vertical);

        const auto handler = [this, vertical](qreal delta, Type type) {
            const auto contentSize = vertical ? m_propertyContentHeight.read().toReal()
                                              : m_propertyContentWidth.read().toReal();
            const auto pos = vertical ? m_propertyContentY.read().toReal()
                                      : m_propertyContentX.read().toReal();
            const auto size = vertical ? m_propertyHeight.read().toReal()
                                       : m_propertyWidth.read().toReal();

            if (contentSize < size || pos >= contentSize)
                return false;

            const auto& scrollBar = vertical ? m_scrollBarV : m_scrollBarH;
            if (scrollBar.valid())
            {
                // Attached ScrollBar is available, use it to scroll
#if !OVERRIDE_SCROLLBAR_STEPSIZE
                const auto _stepSize = scrollBar.stepSize.read().toReal();
#endif
                qreal newStepSize = delta;
                if (type == Type::Degree)
                    newStepSize *= (m_effectiveScaleFactor / contentSize);
                else
                    newStepSize *= (1.0 / contentSize);

                scrollBar.stepSize.write(qBound<qreal>(0, qAbs(newStepSize), 1));

                if (newStepSize > 0)
                    scrollBar.decreaseMethod.invoke(scrollBar.item);
                else
                    scrollBar.increaseMethod.invoke(scrollBar.item);

#if !OVERRIDE_SCROLLBAR_STEPSIZE
                scrollBar.stepSize.write(_stepSize);
#endif
            }
            else
            {
                qreal newPos = pos;
                if (type == Type::Degree)
                    newPos -= (m_effectiveScaleFactor * delta);
                else
                    newPos -= delta;

                newPos = qBound<qreal>(0, newPos, contentSize - size);

                if (vertical)
                    m_propertyContentY.write(newPos);
                else
                    m_propertyContentX.write(newPos);
            }

            return (vertical ? (!qFuzzyCompare(m_propertyContentY.read().toReal(), pos))
                             : (!qFuzzyCompare(m_propertyContentX.read().toReal(), pos)));
        };

        const bool _vertical = (fallback ? !vertical : vertical);
        qreal _delta = _vertical ? ev.delta.y() : ev.delta.x();
        auto _type = ev.type;

#if PAGE_SCROLL_SHIFT_OR_CTRL
        if (_delta != 0 && (wheel->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
        {
            _type = Type::Pixel;
            _delta = (_vertical ? m_propertyHeight.read().toReal()
                                : m_propertyWidth.read().toReal()) * (_delta > 0 ? 1 : -1);
        }
#endif

        if (_delta != 0 && handler(_delta, _type))
        {
            wheel->accept();
            return true;
        }

        return false;
    };

    bool rV = handler(Qt::Vertical);
    bool rH = handler(Qt::Horizontal);

    if (m_fallbackScroll && (rV == false && rH == false))
    {
        if (ev.delta.y() != 0)
            rH = handler(Qt::Horizontal, true);
        else if (ev.delta.x() != 0)
            rV = handler(Qt::Vertical, true);
    }

    return (rV || rH) || QObject::eventFilter(watched, event);
}

void FlickableScrollHandler::attach()
{
    if (m_target)
    {
        m_target->installEventFilter(this);
    }
}

void FlickableScrollHandler::detach()
{
    if (m_target)
    {
        m_target->removeEventFilter(this);
    }
}

void FlickableScrollHandler::adjustScrollBar(ScrollBar& scrollBar)
{
    const auto item = scrollBar.scrollBar.read().value<QQuickItem *>();

    scrollBar.item = item;

    if (item)
    {
        scrollBar.stepSize = QQmlProperty(item, "stepSize", qmlContext(item));
        scrollBar.decreaseMethod = item->metaObject()->method(item->metaObject()->indexOfMethod("decrease()"));
        scrollBar.increaseMethod = item->metaObject()->method(item->metaObject()->indexOfMethod("increase()"));
    }
}

qreal FlickableScrollHandler::scaleFactor() const
{
    return m_scaleFactor;
}

void FlickableScrollHandler::setScaleFactor(qreal newScaleFactor)
{
    if (qFuzzyCompare(m_scaleFactor, newScaleFactor))
        return;
    m_scaleFactor = newScaleFactor;
    emit scaleFactorChanged();
}

bool FlickableScrollHandler::enabled() const
{
    return m_enabled;
}

void FlickableScrollHandler::setEnabled(bool newEnabled)
{
    if (m_enabled == newEnabled)
        return;

    if (newEnabled)
        attach();
    else
        detach();

    m_enabled = newEnabled;
    emit enabledChanged();
}

void FlickableScrollHandler::adjustScrollBarV()
{
    adjustScrollBar(m_scrollBarV);
}

void FlickableScrollHandler::adjustScrollBarH()
{
    adjustScrollBar(m_scrollBarH);
}

qreal FlickableScrollHandler::effectiveScaleFactor() const
{
    return m_effectiveScaleFactor;
}
