/*
 *  SPDX-FileCopyrightText: 2019 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

/*
 *  This file is part of the KDE Kirigami project.
 *  Upstream commit: 85ac5406
 *
 *  It is slightly modified to fit into the codebase here.
 *  Modifications are subject to the same license.
 *
 *  Origin: kirigami/src/wheelhandler.cpp
 */

#include "kirigamiwheelhandler.hpp"

#include <QQmlEngine>
#include <QQuickWindow>
#include <QWheelEvent>

using namespace Kirigami;

KirigamiWheelEvent::KirigamiWheelEvent(QObject *parent)
    : QObject(parent)
{
}

KirigamiWheelEvent::~KirigamiWheelEvent()
{
}

void KirigamiWheelEvent::initializeFromEvent(QWheelEvent *event)
{
    m_x = event->position().x();
    m_y = event->position().y();
    m_angleDelta = event->angleDelta();
    m_pixelDelta = event->pixelDelta();
    m_buttons = event->buttons();
    m_modifiers = event->modifiers();
    m_accepted = false;
    m_inverted = event->inverted();
}

qreal KirigamiWheelEvent::x() const
{
    return m_x;
}

qreal KirigamiWheelEvent::y() const
{
    return m_y;
}

QPointF KirigamiWheelEvent::angleDelta() const
{
    return m_angleDelta;
}

QPointF KirigamiWheelEvent::pixelDelta() const
{
    return m_pixelDelta;
}

int KirigamiWheelEvent::buttons() const
{
    return m_buttons;
}

int KirigamiWheelEvent::modifiers() const
{
    return m_modifiers;
}

bool KirigamiWheelEvent::inverted() const
{
    return m_inverted;
}

bool KirigamiWheelEvent::isAccepted()
{
    return m_accepted;
}

void KirigamiWheelEvent::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

///////////////////////////////

WheelFilterItem::WheelFilterItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setEnabled(false);
}

///////////////////////////////

WheelHandler::WheelHandler(QObject *parent)
    : DummyFlickableScrollHandler(parent)
    , m_filterItem(new WheelFilterItem(nullptr))
{
    m_filterItem->installEventFilter(this);

    m_wheelScrollingTimer.setSingleShot(true);
    m_wheelScrollingTimer.setInterval(m_wheelScrollingDuration);
    m_wheelScrollingTimer.callOnTimeout([this]() {
        setScrolling(false);
    });

    m_xScrollAnimation.setEasingCurve(QEasingCurve::OutCubic);
    m_yScrollAnimation.setEasingCurve(QEasingCurve::OutCubic);
    m_xInertiaScrollAnimation.setEasingCurve(QEasingCurve::OutQuad);
    m_yInertiaScrollAnimation.setEasingCurve(QEasingCurve::OutQuad);

    const auto adjustStepSizes = [this](int scrollLines) {
        m_defaultPixelStepSize = 20 * scrollLines * m_scaleFactor;
        if (!m_explicitVStepSize && m_verticalStepSize != m_defaultPixelStepSize) {
            m_verticalStepSize = m_defaultPixelStepSize;
            Q_EMIT verticalStepSizeChanged();
        }
        if (!m_explicitHStepSize && m_horizontalStepSize != m_defaultPixelStepSize) {
            m_horizontalStepSize = m_defaultPixelStepSize;
            Q_EMIT horizontalStepSizeChanged();
        }
    };

    connect(QGuiApplication::styleHints(), &QStyleHints::wheelScrollLinesChanged, this, adjustStepSizes);
    connect(this, &WheelHandler::scaleFactorChanged, this, [adjustStepSizes]() {
        assert(QGuiApplication::styleHints());
        const int scrollLines = QGuiApplication::styleHints()->wheelScrollLines();
        adjustStepSizes(scrollLines);
    });
}

WheelHandler::~WheelHandler()
{
    delete m_filterItem;
}

QQuickItem *WheelHandler::target() const
{
    return m_flickable;
}

void WheelHandler::setTarget(QQuickItem *target)
{
    if (m_flickable == target) {
        return;
    }

    if (target && !target->inherits("QQuickFlickable")) {
        qmlWarning(this) << "target must be a QQuickFlickable";
        return;
    }

    if (m_flickable) {
        m_flickable->removeEventFilter(this);
        disconnect(m_flickable, nullptr, m_filterItem, nullptr);
        disconnect(m_flickable, &QQuickItem::parentChanged, this, &WheelHandler::_k_rebindScrollBars);
    }

    m_flickable = target;
    m_filterItem->setParentItem(target);
    if (m_xScrollAnimation.targetObject()) {
        m_xScrollAnimation.stop();
    }
    m_xScrollAnimation.setTargetObject(target);
    if (m_yScrollAnimation.targetObject()) {
        m_yScrollAnimation.stop();
    }
    m_yScrollAnimation.setTargetObject(target);
    if (m_yInertiaScrollAnimation.targetObject()) {
        m_yInertiaScrollAnimation.stop();
    }
    m_yInertiaScrollAnimation.setTargetObject(target);
    if (m_xInertiaScrollAnimation.targetObject()) {
        m_xInertiaScrollAnimation.stop();
    }
    m_xInertiaScrollAnimation.setTargetObject(target);

    if (target) {
        target->installEventFilter(this);

        // Stack WheelFilterItem over the Flickable's scrollable content
        m_filterItem->stackAfter(target->property("contentItem").value<QQuickItem *>());
        // Make it fill the Flickable
        m_filterItem->setWidth(target->width());
        m_filterItem->setHeight(target->height());
        connect(target, &QQuickItem::widthChanged, m_filterItem, [this, target]() {
            m_filterItem->setWidth(target->width());
        });
        connect(target, &QQuickItem::heightChanged, m_filterItem, [this, target]() {
            m_filterItem->setHeight(target->height());
        });
    }

    _k_rebindScrollBars();

    Q_EMIT targetChanged();
}

void WheelHandler::_k_rebindScrollBars()
{
    struct ScrollBarAttached {
        QObject *attached = nullptr;
        QQuickItem *vertical = nullptr;
        QQuickItem *horizontal = nullptr;
    };

    ScrollBarAttached attachedToFlickable;
    ScrollBarAttached attachedToScrollView;

    if (m_flickable) {
        // Get ScrollBars so that we can filter them too, even if they're not
        // in the bounds of the Flickable
        const auto flickableChildren = m_flickable->children();
        for (const auto child : flickableChildren) {
            if (child->inherits("QQuickScrollBarAttached")) {
                attachedToFlickable.attached = child;
                attachedToFlickable.vertical = child->property("vertical").value<QQuickItem *>();
                attachedToFlickable.horizontal = child->property("horizontal").value<QQuickItem *>();
                break;
            }
        }

        // Check ScrollView if there are no scrollbars attached to the Flickable.
        // We need to check if the parent inherits QQuickScrollView in case the
        // parent is another Flickable that already has a Kirigami WheelHandler.
        auto flickableParent = m_flickable->parentItem();
        if (m_scrollView && m_scrollView != flickableParent) {
            m_scrollView->removeEventFilter(this);
        }
        if (flickableParent && flickableParent->inherits("QQuickScrollView")) {
            if (m_scrollView != flickableParent) {
                m_scrollView = flickableParent;
                m_scrollView->installEventFilter(this);
            }
            const auto siblings = m_scrollView->children();
            for (const auto child : siblings) {
                if (child->inherits("QQuickScrollBarAttached")) {
                    attachedToScrollView.attached = child;
                    attachedToScrollView.vertical = child->property("vertical").value<QQuickItem *>();
                    attachedToScrollView.horizontal = child->property("horizontal").value<QQuickItem *>();
                    break;
                }
            }
        }
    }

    // Dilemma: ScrollBars can be attached to both ScrollView and Flickable,
    // but only one of them should be shown anyway. Let's prefer Flickable.

    struct ChosenScrollBar {
        QObject *attached = nullptr;
        QQuickItem *scrollBar = nullptr;
    };

    ChosenScrollBar vertical;
    if (attachedToFlickable.vertical) {
        vertical.attached = attachedToFlickable.attached;
        vertical.scrollBar = attachedToFlickable.vertical;
    } else if (attachedToScrollView.vertical) {
        vertical.attached = attachedToScrollView.attached;
        vertical.scrollBar = attachedToScrollView.vertical;
    }

    ChosenScrollBar horizontal;
    if (attachedToFlickable.horizontal) {
        horizontal.attached = attachedToFlickable.attached;
        horizontal.scrollBar = attachedToFlickable.horizontal;
    } else if (attachedToScrollView.horizontal) {
        horizontal.attached = attachedToScrollView.attached;
        horizontal.scrollBar = attachedToScrollView.horizontal;
    }

    // Flickable may get re-parented to or out of a ScrollView, so we need to
    // redo the discovery process. This is especially important for
    // Kirigami.ScrollablePage component.
    if (m_flickable) {
        if (attachedToFlickable.horizontal && attachedToFlickable.vertical) {
            // But if both scrollbars are already those from the preferred
            // Flickable, there's no need for rediscovery.
            disconnect(m_flickable, &QQuickItem::parentChanged, this, &WheelHandler::_k_rebindScrollBars);
        } else {
            connect(m_flickable, &QQuickItem::parentChanged, this, &WheelHandler::_k_rebindScrollBars, Qt::UniqueConnection);
        }
    }

    if (m_verticalScrollBar != vertical.scrollBar) {
        if (m_verticalScrollBar) {
            m_verticalScrollBar->removeEventFilter(this);
            disconnect(m_verticalChangedConnection);
        }
        m_verticalScrollBar = vertical.scrollBar;
        if (vertical.scrollBar) {
            vertical.scrollBar->installEventFilter(this);
            m_verticalChangedConnection = connect(vertical.attached, SIGNAL(verticalChanged()), this, SLOT(_k_rebindScrollBars()));
        }
    }

    if (m_horizontalScrollBar != horizontal.scrollBar) {
        if (m_horizontalScrollBar) {
            m_horizontalScrollBar->removeEventFilter(this);
            disconnect(m_horizontalChangedConnection);
        }
        m_horizontalScrollBar = horizontal.scrollBar;
        if (horizontal.scrollBar) {
            horizontal.scrollBar->installEventFilter(this);
            m_horizontalChangedConnection = connect(horizontal.attached, SIGNAL(horizontalChanged()), this, SLOT(_k_rebindScrollBars()));
        }
    }
}

qreal WheelHandler::verticalStepSize() const
{
    return m_verticalStepSize;
}

void WheelHandler::setVerticalStepSize(qreal stepSize)
{
    m_explicitVStepSize = true;
    if (qFuzzyCompare(m_verticalStepSize, stepSize)) {
        return;
    }
    // Mimic the behavior of QQuickScrollBar when stepSize is 0
    if (qFuzzyIsNull(stepSize)) {
        resetVerticalStepSize();
        return;
    }
    m_verticalStepSize = stepSize;
    Q_EMIT verticalStepSizeChanged();
}

void WheelHandler::resetVerticalStepSize()
{
    m_explicitVStepSize = false;
    if (qFuzzyCompare(m_verticalStepSize, m_defaultPixelStepSize)) {
        return;
    }
    m_verticalStepSize = m_defaultPixelStepSize;
    Q_EMIT verticalStepSizeChanged();
}

qreal WheelHandler::horizontalStepSize() const
{
    return m_horizontalStepSize;
}

void WheelHandler::setHorizontalStepSize(qreal stepSize)
{
    m_explicitHStepSize = true;
    if (qFuzzyCompare(m_horizontalStepSize, stepSize)) {
        return;
    }
    // Mimic the behavior of QQuickScrollBar when stepSize is 0
    if (qFuzzyIsNull(stepSize)) {
        resetHorizontalStepSize();
        return;
    }
    m_horizontalStepSize = stepSize;
    Q_EMIT horizontalStepSizeChanged();
}

void WheelHandler::resetHorizontalStepSize()
{
    m_explicitHStepSize = false;
    if (qFuzzyCompare(m_horizontalStepSize, m_defaultPixelStepSize)) {
        return;
    }
    m_horizontalStepSize = m_defaultPixelStepSize;
    Q_EMIT horizontalStepSizeChanged();
}

Qt::KeyboardModifiers WheelHandler::pageScrollModifiers() const
{
    return m_pageScrollModifiers;
}

void WheelHandler::setPageScrollModifiers(Qt::KeyboardModifiers modifiers)
{
    if (m_pageScrollModifiers == modifiers) {
        return;
    }
    m_pageScrollModifiers = modifiers;
    Q_EMIT pageScrollModifiersChanged();
}

void WheelHandler::resetPageScrollModifiers()
{
    setPageScrollModifiers(m_defaultPageScrollModifiers);
}

bool WheelHandler::filterMouseEvents() const
{
    return m_filterMouseEvents;
}

void WheelHandler::setFilterMouseEvents(bool enabled)
{
    if (m_filterMouseEvents == enabled) {
        return;
    }
    m_filterMouseEvents = enabled;
    Q_EMIT filterMouseEventsChanged();
}

bool WheelHandler::keyNavigationEnabled() const
{
    return m_keyNavigationEnabled;
}

void WheelHandler::setKeyNavigationEnabled(bool enabled)
{
    if (m_keyNavigationEnabled == enabled) {
        return;
    }
    m_keyNavigationEnabled = enabled;
    Q_EMIT keyNavigationEnabledChanged();
}

void WheelHandler::classBegin()
{
    // Initializes smooth scrolling
    m_engine = qmlEngine(this);
}

void WheelHandler::componentComplete()
{
    const auto parentItem = qobject_cast<QQuickItem*>(parent());
    if (!target() && parentItem && parentItem->inherits("QQuickFlickable"))
        setTarget(parentItem);
}

void WheelHandler::setScrolling(bool scrolling)
{
    if (m_wheelScrolling == scrolling) {
        if (m_wheelScrolling) {
            m_wheelScrollingTimer.start();
        }
        return;
    }
    m_wheelScrolling = scrolling;
    m_filterItem->setEnabled(m_wheelScrolling);
}

void WheelHandler::startInertiaScrolling()
{
    const qreal width = m_flickable->width();
    const qreal height = m_flickable->height();
    const qreal contentWidth = m_flickable->property("contentWidth").toReal();
    const qreal contentHeight = m_flickable->property("contentHeight").toReal();
    const qreal topMargin = m_flickable->property("topMargin").toReal();
    const qreal bottomMargin = m_flickable->property("bottomMargin").toReal();
    const qreal leftMargin = m_flickable->property("leftMargin").toReal();
    const qreal rightMargin = m_flickable->property("rightMargin").toReal();
    const qreal originX = m_flickable->property("originX").toReal();
    const qreal originY = m_flickable->property("originY").toReal();
    const qreal contentX = m_flickable->property("contentX").toReal();
    const qreal contentY = m_flickable->property("contentY").toReal();

    QPointF minExtent = QPointF(leftMargin, topMargin) - QPointF(originX, originY);
    QPointF maxExtent = QPointF(width, height) - (QPointF(contentWidth, contentHeight) + QPointF(rightMargin, bottomMargin) + QPointF(originX, originY));

    QPointF totalDelta(0, 0);
    for (const QPoint delta : m_wheelEvents) {
        totalDelta += delta;
    }
    const uint64_t elapsed = std::max<uint64_t>(m_timestamps.last() - m_timestamps.first(), 1);

    // The inertia is more natural if we multiply
    // the actual scrolling speed by some factor,
    // chosen manually here to be 2.5. Otherwise, the
    // scrolling will appear to be too slow.
    const qreal speedFactor = 2.5;

    // We get the velocity in px/s by calculating
    // displacement / elapsed time; we multiply by
    // 1000 since the elapsed time is in ms.
    QPointF vel = -totalDelta * 1000 / elapsed * speedFactor;
    QPointF startValue = QPointF(contentX, contentY);

    // We decelerate at 4000px/s^2, chosen by manual test
    // to be natural.
    const qreal deceleration = 4000 * speedFactor;

    // We use constant deceleration formulas to find:
    // time = |velocity / deceleration|
    // distance_traveled = time * velocity / 2
    QPointF time = QPointF(qAbs(vel.x() / deceleration), qAbs(vel.y() / deceleration));
    QPointF endValue = QPointF(startValue.x() + time.x() * vel.x() / 2, startValue.y() + time.y() * vel.y() / 2);

    // We bound the end value so that we don't animate
    // beyond the scrollable amount.
    QPointF boundedEndValue =
        QPointF(std::max(std::min(endValue.x(), -maxExtent.x()), -minExtent.x()), std::max(std::min(endValue.y(), -maxExtent.y()), -minExtent.y()));

    // If we did bound the end value, we check how much
    // (from 0 to 1) of the animation is actually played,
    // and we adjust the time required for it accordingly.
    QPointF progressFactor = QPointF((boundedEndValue.x() - startValue.x()) / (endValue.x() - startValue.x()),
                                     (boundedEndValue.y() - startValue.y()) / (endValue.y() - startValue.y()));
    // The formula here is:
    // partial_time = complete_time * (1 - sqrt(1 - partial_progress_factor)),
    // with partial_progress_factor being between 0 and 1.
    // It can be obtained by inverting the OutQad easing formula,
    // which is f(t) = t(2 - t).
    // We also convert back from seconds to milliseconds.
    QPointF realTime = QPointF(time.x() * (1 - std::sqrt(1 - progressFactor.x())), time.y() * (1 - std::sqrt(1 - progressFactor.y()))) * 1000;
    m_wheelEvents.clear();
    m_timestamps.clear();

    m_xScrollAnimation.stop();
    m_yScrollAnimation.stop();
    if (realTime.x() > 0) {
        m_xInertiaScrollAnimation.setStartValue(startValue.x());
        m_xInertiaScrollAnimation.setEndValue(boundedEndValue.x());
        m_xInertiaScrollAnimation.setDuration(realTime.x());
        m_xInertiaScrollAnimation.start(QAbstractAnimation::KeepWhenStopped);
    }
    if (realTime.y() > 0) {
        m_yInertiaScrollAnimation.setStartValue(startValue.y());
        m_yInertiaScrollAnimation.setEndValue(boundedEndValue.y());
        m_yInertiaScrollAnimation.setDuration(realTime.y());
        m_yInertiaScrollAnimation.start(QAbstractAnimation::KeepWhenStopped);
    }
}

bool WheelHandler::scrollFlickable(QPointF pixelDelta, QPointF angleDelta, Qt::KeyboardModifiers modifiers)
{
    if (!m_flickable || (pixelDelta.isNull() && angleDelta.isNull())) {
        return false;
    }

    const qreal width = m_flickable->width();
    const qreal height = m_flickable->height();
    const qreal contentWidth = m_flickable->property("contentWidth").toReal();
    const qreal contentHeight = m_flickable->property("contentHeight").toReal();
    const qreal contentX = m_flickable->property("contentX").toReal();
    const qreal contentY = m_flickable->property("contentY").toReal();
    const qreal topMargin = m_flickable->property("topMargin").toReal();
    const qreal bottomMargin = m_flickable->property("bottomMargin").toReal();
    const qreal leftMargin = m_flickable->property("leftMargin").toReal();
    const qreal rightMargin = m_flickable->property("rightMargin").toReal();
    const qreal originX = m_flickable->property("originX").toReal();
    const qreal originY = m_flickable->property("originY").toReal();
    const qreal pageWidth = width - leftMargin - rightMargin;
    const qreal pageHeight = height - topMargin - bottomMargin;
    const auto window = m_flickable->window();
    const auto screen = window ? window->screen() : nullptr;
    const qreal devicePixelRatio = window != nullptr ? window->devicePixelRatio() : qGuiApp->devicePixelRatio();
    const qreal refreshRate = screen ? screen->refreshRate() : 0;
    const bool pixelAligned = m_flickable->property("pixelAligned").toBool();

    // HACK: Only transpose deltas when not using xcb in order to not conflict with xcb's own delta transposing
    if (modifiers & m_defaultHorizontalScrollModifiers && qGuiApp->platformName() != QLatin1String("xcb")) {
        angleDelta = angleDelta.transposed();
        pixelDelta = pixelDelta.transposed();
    }

    const qreal xTicks = angleDelta.x() / 120;
    const qreal yTicks = angleDelta.y() / 120;
    bool scrolled = false;

    auto getChange = [pageScrollModifiers = modifiers & m_pageScrollModifiers](qreal ticks, qreal pixelDelta, qreal stepSize, qreal pageSize) {
        // Use page size with pageScrollModifiers. Matches QScrollBar, which uses QAbstractSlider behavior.
        if (pageScrollModifiers) {
            return qBound(-pageSize, ticks * pageSize, pageSize);
        } else if (pixelDelta != 0) {
            return pixelDelta;
        } else {
            return ticks * stepSize;
        }
    };

    auto getPosition = [devicePixelRatio, pixelAligned](qreal size,
                                                        qreal contentSize,
                                                        qreal contentPos,
                                                        qreal originPos,
                                                        qreal pageSize,
                                                        qreal leadingMargin,
                                                        qreal trailingMargin,
                                                        qreal change,
                                                        const QPropertyAnimation &animation) {
        if (contentSize <= pageSize) {
            return contentPos;
        }

        // contentX and contentY use reversed signs from what x and y would normally use, so flip the signs

        qreal minExtent = leadingMargin - originPos;
        qreal maxExtent = size - (contentSize + trailingMargin + originPos);
        qreal newContentPos = (animation.state() == QPropertyAnimation::Running ? animation.endValue().toReal() : contentPos) - change;
        // bound the values without asserts
        newContentPos = std::max(-minExtent, std::min(newContentPos, -maxExtent));

        // Flickable::pixelAligned rounds the position, so round to mimic that behavior.
        // Rounding prevents fractional positioning from causing text to be
        // clipped off on the top and bottom.
        // Multiply by devicePixelRatio before rounding and divide by devicePixelRatio
        // after to make position match pixels on the screen more closely.
        if (pixelAligned)
            return std::round(newContentPos * devicePixelRatio) / devicePixelRatio;
        else
            return (newContentPos * devicePixelRatio) / devicePixelRatio;
    };

    auto setPosition = [this, devicePixelRatio, refreshRate](qreal oldPos, qreal newPos, qreal stepSize, const char *property, QPropertyAnimation &animation) {
        animation.stop();
        if (oldPos == newPos) {
            return false;
        }
        if (!m_smoothScroll || !m_engine || refreshRate <= 0) {
            animation.setDuration(0);
            m_flickable->setProperty(property, newPos);
            return true;
        }

        // Can't use wheelEvent->deviceType() to determine device type since
        // on Wayland mouse is always regarded as touchpad:
        // https://invent.kde.org/qt/qt/qtwayland/-/blob/e695a39519a7629c1549275a148cfb9ab99a07a9/src/client/qwaylandinputdevice.cpp#L445
        // Mouse wheel can generate angle delta like 240, 360 and so on when
        // scrolling very fast on some mice such as the Logitech M150.
        // Mice with hi-res mouse wheels such as the Logitech MX Master 3 can
        // generate angle deltas as small as 16.
        // On X11, trackpads can also generate very fine angle deltas.

        // Duration is based on the duration and movement for 120 angle delta.
        // Shorten duration for smaller movements, limit duration for big movements.
        // We don't want fine deltas to feel extra slow and fast scrolling should still feel fast.
        // Minimum 3 frames for a 60hz display if delta > 2 physical pixels
        // (start already rendered -> 1/3 rendered -> 2/3 rendered -> end rendered).
        // Skip animation if <= 2 real frames for low refresh rate screens.
        // Otherwise, we don't scale the duration based on refresh rate or
        // device pixel ratio to avoid making the animation unexpectedly
        // longer or shorter on different screens.

        qreal absPixelDelta = std::abs(newPos - oldPos);
        int duration = absPixelDelta * devicePixelRatio > 2 //
            ? std::max(qCeil(1000.0 / 60.0 * 3), std::min(qRound(absPixelDelta * m_duration / stepSize), m_duration))
            : 0;
        animation.setDuration(duration <= qCeil(1000.0 / refreshRate * 2) ? 0 : duration);
        if (animation.duration() > 0) {
            animation.setStartValue(oldPos);
            animation.setEndValue(newPos);
            animation.start(QAbstractAnimation::KeepWhenStopped);
        } else {
            m_flickable->setProperty(property, newPos);
        }
        return true;
    };

    qreal xChange = getChange(xTicks, pixelDelta.x(), m_horizontalStepSize, pageWidth);
    qreal newContentX = getPosition(width, contentWidth, contentX, originX, pageWidth, leftMargin, rightMargin, xChange, m_xScrollAnimation);

    qreal yChange = getChange(yTicks, pixelDelta.y(), m_verticalStepSize, pageHeight);
    qreal newContentY = getPosition(height, contentHeight, contentY, originY, pageHeight, topMargin, bottomMargin, yChange, m_yScrollAnimation);

    // Don't use `||` because we need the position to be set for contentX and contentY.
    scrolled |= setPosition(contentX, newContentX, m_horizontalStepSize, "contentX", m_xScrollAnimation);
    scrolled |= setPosition(contentY, newContentY, m_verticalStepSize, "contentY", m_yScrollAnimation);

    return scrolled;
}

bool WheelHandler::scrollUp(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_verticalStepSize;
    }
    // contentY uses reversed sign
    return scrollFlickable(QPointF(0, stepSize));
}

bool WheelHandler::scrollDown(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_verticalStepSize;
    }
    // contentY uses reversed sign
    return scrollFlickable(QPointF(0, -stepSize));
}

bool WheelHandler::scrollLeft(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_horizontalStepSize;
    }
    // contentX uses reversed sign
    return scrollFlickable(QPoint(stepSize, 0));
}

bool WheelHandler::scrollRight(qreal stepSize)
{
    if (qFuzzyIsNull(stepSize)) {
        return false;
    } else if (stepSize < 0) {
        stepSize = m_horizontalStepSize;
    }
    // contentX uses reversed sign
    return scrollFlickable(QPoint(-stepSize, 0));
}

bool WheelHandler::eventFilter(QObject *watched, QEvent *event)
{
    auto item = qobject_cast<QQuickItem *>(watched);
    if (!item || !item->isEnabled()) {
        return false;
    }

    // We only process keyboard events for QQuickScrollView.
    const auto eventType = event->type();
    if (item == m_scrollView && eventType != QEvent::KeyPress && eventType != QEvent::KeyRelease) {
        return false;
    }

    qreal contentWidth = 0;
    qreal contentHeight = 0;
    qreal pageWidth = 0;
    qreal pageHeight = 0;
    if (m_flickable) {
        contentWidth = m_flickable->property("contentWidth").toReal();
        contentHeight = m_flickable->property("contentHeight").toReal();
        pageWidth = m_flickable->width() - m_flickable->property("leftMargin").toReal() - m_flickable->property("rightMargin").toReal();
        pageHeight = m_flickable->height() - m_flickable->property("topMargin").toReal() - m_flickable->property("bottomMargin").toReal();
    }

    // The code handling touch, mouse and hover events is mostly copied/adapted from QQuickScrollView::childMouseEventFilter()
    switch (eventType) {
    case QEvent::Wheel: {
        // QQuickScrollBar::interactive handling Matches behavior in QQuickScrollView::eventFilter()
        if (m_filterMouseEvents) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
        }
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

        // NOTE: On X11 with libinput, pixelDelta is identical to angleDelta when using a mouse that shouldn't use pixelDelta.
        // If faulty pixelDelta, reset pixelDelta to (0,0).
        if (wheelEvent->pixelDelta() == wheelEvent->angleDelta()) {
            // In order to change any of the data, we have to create a whole new QWheelEvent from its constructor.
            QWheelEvent newWheelEvent(wheelEvent->position(),
                                      wheelEvent->globalPosition(),
                                      QPoint(0, 0), // pixelDelta
                                      wheelEvent->angleDelta(),
                                      wheelEvent->buttons(),
                                      wheelEvent->modifiers(),
                                      wheelEvent->phase(),
                                      wheelEvent->inverted(),
                                      wheelEvent->source());
            m_kirigamiWheelEvent.initializeFromEvent(&newWheelEvent);
        } else {
            m_kirigamiWheelEvent.initializeFromEvent(wheelEvent);
        }

        Q_EMIT wheel(&m_kirigamiWheelEvent);

        if (m_wheelEvents.count() > 6) {
            m_wheelEvents.dequeue();
            m_timestamps.dequeue();
        }
        if (m_wheelEvents.count() > 2 && wheelEvent->isEndEvent()) {
            startInertiaScrolling();
        } else {
            m_wheelEvents.enqueue(wheelEvent->pixelDelta());
            m_timestamps.enqueue(wheelEvent->timestamp());
        }

        if (m_kirigamiWheelEvent.isAccepted()) {
            return true;
        }

        bool scrolled = false;
        if (m_scrollFlickableTarget || (contentHeight <= pageHeight && contentWidth <= pageWidth)) {
            // Don't use pixelDelta from the event unless angleDelta is not available
            // because scrolling by pixelDelta is too slow on Wayland with libinput.
            QPointF pixelDelta = m_kirigamiWheelEvent.angleDelta().isNull() ? m_kirigamiWheelEvent.pixelDelta() : QPoint(0, 0);
            scrolled = scrollFlickable(pixelDelta, m_kirigamiWheelEvent.angleDelta(), Qt::KeyboardModifiers(m_kirigamiWheelEvent.modifiers()));
        }
        setScrolling(scrolled);

        // NOTE: Wheel events created by touchpad gestures with pixel deltas will cause scrolling to jump back
        // to where scrolling started unless the event is always accepted before it reaches the Flickable.
        bool flickableWillUseGestureScrolling = !(wheelEvent->source() == Qt::MouseEventNotSynthesized || wheelEvent->pixelDelta().isNull());
        return scrolled || m_blockTargetWheel || flickableWillUseGestureScrolling;
    }

    case QEvent::TouchBegin: {
        m_wasTouched = true;
        if (!m_filterMouseEvents) {
            break;
        }
        if (m_verticalScrollBar) {
            m_verticalScrollBar->setProperty("interactive", false);
        }
        if (m_horizontalScrollBar) {
            m_horizontalScrollBar->setProperty("interactive", false);
        }
        break;
    }

    case QEvent::TouchEnd: {
        m_wasTouched = false;
        break;
    }

    case QEvent::MouseButtonPress: {
        // NOTE: Flickable does not handle touch events, only synthesized mouse events
        m_wasTouched = static_cast<QMouseEvent *>(event)->source() != Qt::MouseEventNotSynthesized;
        if (!m_filterMouseEvents) {
            break;
        }
        if (!m_wasTouched) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
            break;
        }
        return !m_wasTouched && item == m_flickable;
    }

    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease: {
        setScrolling(false);
        if (!m_filterMouseEvents) {
            break;
        }
        if (static_cast<QMouseEvent *>(event)->source() == Qt::MouseEventNotSynthesized && item == m_flickable) {
            return true;
        }
        break;
    }

    case QEvent::HoverEnter:
    case QEvent::HoverMove: {
        if (!m_filterMouseEvents) {
            break;
        }
        if (m_wasTouched && (item == m_verticalScrollBar || item == m_horizontalScrollBar)) {
            if (m_verticalScrollBar) {
                m_verticalScrollBar->setProperty("interactive", true);
            }
            if (m_horizontalScrollBar) {
                m_horizontalScrollBar->setProperty("interactive", true);
            }
        }
        break;
    }

    case QEvent::KeyPress: {
        if (!m_keyNavigationEnabled) {
            break;
        }
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        bool horizontalScroll = keyEvent->modifiers() & m_defaultHorizontalScrollModifiers;
        switch (keyEvent->key()) {
        case Qt::Key_Up:
            return scrollUp();
        case Qt::Key_Down:
            return scrollDown();
        case Qt::Key_Left:
            return scrollLeft();
        case Qt::Key_Right:
            return scrollRight();
        case Qt::Key_PageUp:
            return horizontalScroll ? scrollLeft(pageWidth) : scrollUp(pageHeight);
        case Qt::Key_PageDown:
            return horizontalScroll ? scrollRight(pageWidth) : scrollDown(pageHeight);
        case Qt::Key_Home:
            return horizontalScroll ? scrollLeft(contentWidth) : scrollUp(contentHeight);
        case Qt::Key_End:
            return horizontalScroll ? scrollRight(contentWidth) : scrollDown(contentHeight);
        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    return false;
}
