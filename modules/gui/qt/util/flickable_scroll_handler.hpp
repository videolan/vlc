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
#ifndef FLICKABLE_SCROLL_HANDLER_HPP
#define FLICKABLE_SCROLL_HANDLER_HPP

#include <QObject>
#include <QQmlProperty>
#include <QPointer>
#include <QQuickItem>
#include <QQmlParserStatus>

class DummyFlickableScrollHandler : public QObject
{
    Q_OBJECT

    // These properties may be overridden if necessary, they are not marked as final.
    Q_PROPERTY(qreal scaleFactor MEMBER m_scaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(bool smoothScroll MEMBER m_smoothScroll NOTIFY smoothScrollChanged)
    Q_PROPERTY(int duration MEMBER m_duration NOTIFY durationChanged)

public:
    explicit DummyFlickableScrollHandler(QObject *parent = nullptr) : QObject(parent) { }

signals:
    void scaleFactorChanged();
    void smoothScrollChanged();
    void durationChanged();

protected:
    qreal m_scaleFactor = 1.;
    int m_duration = 200;
    bool m_smoothScroll = true;
};

class FlickableScrollHandler : public DummyFlickableScrollHandler, public QQmlParserStatus
{
    Q_OBJECT

    Q_PROPERTY(QObject* parent READ parent NOTIFY initialized FINAL)

    Q_PROPERTY(qreal effectiveScaleFactor READ effectiveScaleFactor NOTIFY effectiveScaleFactorChanged FINAL)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool fallbackScroll MEMBER m_fallbackScroll NOTIFY fallbackScrollChanged FINAL)
    Q_PROPERTY(bool handleOnlyPixelDelta MEMBER m_handleOnlyPixelDelta NOTIFY handleOnlyPixelDeltaChanged FINAL)

    Q_INTERFACES(QQmlParserStatus)

public:
    explicit FlickableScrollHandler(QObject *parent = nullptr);
    ~FlickableScrollHandler();

    qreal effectiveScaleFactor() const;
    bool enabled() const;

    void setEnabled(bool newEnabled);

    void classBegin() override;
    void componentComplete() override;

signals:
    void initialized();

    void enabledChanged();
    void effectiveScaleFactorChanged();
    void fallbackScrollChanged();

    void handleOnlyPixelDeltaChanged();

private slots:
    void adjustScrollBarV();
    void adjustScrollBarH();

private:
    void attach();
    void detach();

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPointer<QQuickItem> m_target = nullptr;
    qreal m_effectiveScaleFactor;
    bool m_enabled = true;
    bool m_fallbackScroll = false;

    QQmlProperty m_propertyContentX, m_propertyContentY;
    QQmlProperty m_propertyContentHeight, m_propertyContentWidth;
    QQmlProperty m_propertyHeight, m_propertyWidth;

    struct ScrollBar {
        QQmlProperty scrollBar;
        QQmlProperty stepSize;
        QMetaMethod increaseMethod;
        QMetaMethod decreaseMethod;
        QQuickItem* item = nullptr;
        bool valid() const { return item != nullptr; }
    } m_scrollBarV, m_scrollBarH;

    void adjustScrollBar(ScrollBar& scrollBar);
    bool m_handleOnlyPixelDelta = false;
};

#endif // FLICKABLE_SCROLL_HANDLER_HPP
