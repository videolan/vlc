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
#ifndef ITEMKEYEVENTFILTER_HPP
#define ITEMKEYEVENTFILTER_HPP

#include <QQuickItem>

/**
 * @brief The ItemKeyEventFilter class allows to register an event filter,
 * which forwards the filtered key events to itself. This might be useful to
 * process key press before they are processed (and eventually accepted) by
 * children components
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
 */
class ItemKeyEventFilter : public QQuickItem
{
    Q_OBJECT
public:
    Q_PROPERTY(QObject * source READ getSource WRITE setSource FINAL)
    Q_PROPERTY(bool filterEnabled READ getFilterEnabled WRITE setFilterEnabled FINAL)

public:
    ItemKeyEventFilter(QQuickItem *parent = nullptr);
    ~ItemKeyEventFilter();

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
#endif // ITEMKEYEVENTFILTER_HPP
