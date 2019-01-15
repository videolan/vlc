/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#ifndef QMLEVENTFILTER_HPP
#define QMLEVENTFILTER_HPP

#include <QQuickItem>

/**
 * @brief The QmlEventFilter class allows to register an event filter from QML
 * this might be usefull to process key press before they are processed (and
 * eventually accepted) by children components
 *
 * this is usable as
 *
 *  EventFilter {
 *      filterEnabled: true
 *      Keys.onPressed: {
 *          //do stuff
 *          event.accepted = true //to block the event
 *      }
 *  }
 *
 *  Component.onCompleted: {
 *      filter.source = rootWindow
 *  }
 *
 * Note that this solution doesn't work as-is for mouse events as Qml doesn't provide
 * the equivalent of Keys.onPressed.
 */
class QmlEventFilter : public QQuickItem
{
    Q_OBJECT
public:
    Q_PROPERTY(QObject * source READ getSource WRITE setSource)
    Q_PROPERTY(bool filterEnabled READ getFilterEnabled WRITE setFilterEnabled)

public:
    QmlEventFilter(QQuickItem *parent = nullptr);
    ~QmlEventFilter();

    void setSource(QObject *source);

    inline QObject * getSource() { return m_source; }
    inline void setFilterEnabled(bool value) { m_filterEnabled = value; }
    inline bool getFilterEnabled() { return m_filterEnabled; }

private:

    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QObject *m_source = nullptr;
    bool m_filterEnabled = true;
    bool m_qmlAccepted = false;
};
#endif // QMLEVENTFILTER_HPP
