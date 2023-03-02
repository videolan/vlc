/*****************************************************************************
 * Copyright (C) 2023 the VideoLAN team
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

#include <QWindow>
#include <QGuiApplication>
#include <QQuickItem>

#if !defined(QT_NO_ACCESSIBILITY) && defined(QT5_DECLARATIVE_PRIVATE)

#include <QAccessibleObject>

#include <private/qquickitem_p.h>

#include "compositor_accessibility.hpp"
#include "compositor.hpp"

#ifdef HAVE_DCOMP_H
#  include "compositor_dcomp_uisurface.hpp"
#endif

namespace vlc {

/*
 * we could have use QAccessibleQuickWindow like in QAccessibleQuickWidget
 * but QAccessibleQuickWindow is not publicly exposed in the library, so we
 * mimic the behavior of QAccessibleQuickWindow directly
 */


static void unignoredChildren(QQuickItem *item, QList<QQuickItem *> *items)
{
    const QList<QQuickItem*> childItems = item->childItems();
    for (QQuickItem *child : childItems)
    {
        if (QQuickItemPrivate::get(child)->isAccessible)
            items->append(child);
        else
            unignoredChildren(child, items);
    }
}

static QList<QQuickItem *> accessibleUnignoredChildren(QQuickItem *item)
{
    QList<QQuickItem *> items;
    unignoredChildren(item, &items);
    return items;
}


class QAccessibleRenderWindow: public QAccessibleObject
{
public:
    QAccessibleRenderWindow(QWindow* window, AccessibleRenderWindow* renderWindow)
        : QAccessibleObject(window)
        , m_window(renderWindow->getOffscreenWindow())
    {
    }

    QAccessibleInterface* parent() const override
    {
        // we assume to be a top level window...
        return QAccessible::queryAccessibleInterface(qApp);
    }

    QList<QQuickItem *> rootItems() const
    {
        if (QQuickItem *ci = m_window->contentItem())
            return accessibleUnignoredChildren(ci);
        return QList<QQuickItem *>();
    }


    QAccessibleInterface* child(int index) const override
    {
        const QList<QQuickItem*> &kids = rootItems();
        if (index >= 0 && index < kids.count())
            return QAccessible::queryAccessibleInterface(kids.at(index));
        return nullptr;
    }

    int childCount() const override
    {
        return rootItems().count();
    }

    int indexOfChild(const QAccessibleInterface *iface) const override
    {
        int i = -1;
        if (iface)
        {
            const QList<QQuickItem *> &roots = rootItems();
            i = roots.count() - 1;
            while (i >= 0)
            {
                if (iface->object() == roots.at(i))
                    break;
                --i;
            }
        }
        return i;
    }

    QAccessibleInterface *childAt(int x, int y) const override
    {
        for (int i = childCount() - 1; i >= 0; --i)
        {
            QAccessibleInterface *childIface = child(i);
            if (childIface && !childIface->state().invisible)
            {
                if (QAccessibleInterface *iface = childIface->childAt(x, y))
                    return iface;
                if (childIface->rect().contains(x, y))
                    return childIface;
            }
        }
        return nullptr;
    }

    QString text(QAccessible::Text) const override
    {
        return window()->title();
    }

    QRect rect() const override
    {
        return QRect(window()->x(), window()->y(), window()->width(), window()->height());
    }

    QAccessible::Role role() const override
    {
        return QAccessible::Window;
    }

    QAccessible::State state() const override
    {

        QAccessible::State st;
        if (window() == QGuiApplication::focusWindow())
            st.active = true;
        if (!window()->isVisible())
            st.invisible = true;
        return st;
    }

    QWindow* window() const override
    {
        return static_cast<QWindow*>(object());
    }

private:
    QQuickWindow* m_window;
};


/*
 * DCompOffscreenWindow is a top window, mark it as unacessible so it won't
 * be introspected by a11y, the QML scene is actually accessible though QAccessibleRenderWindow
 */
class QAccessibleOffscreenWindow: public QAccessibleObject
{
public:
    QAccessibleOffscreenWindow(QQuickWindow *window)
        : QAccessibleObject(window)
    {
    }

    QAccessibleInterface* parent() const override
    {
        // we assume to be a top level window...
        return QAccessible::queryAccessibleInterface(qApp);
    }

    QAccessibleInterface* child(int) const override
    {
        return nullptr;
    }

    int childCount() const override
    {
        return 0;
    }

    int indexOfChild(const QAccessibleInterface *) const override
    {
        return -1;
    }

    QAccessibleInterface *childAt(int, int) const override
    {
        return nullptr;
    }

    QAccessibleInterface *focusChild() const override
    {
        return nullptr;
    }

    QString text(QAccessible::Text) const override
    {
        return window()->title();
    }

    QRect rect() const override
    {
        return QRect(window()->x(), window()->y(), window()->width(), window()->height());
    }

    QAccessible::Role role() const override
    {
        return QAccessible::Window;
    }

    QAccessible::State state() const override
    {

        QAccessible::State st;
        if (window() == QGuiApplication::focusWindow())
            st.active = true;
        if (!window()->isVisible())
            st.invisible = true;
        return st;
    }

private:
    QWindow* window() const override
    {
        return static_cast<QWindow*>(object());
    }
};

QAccessibleInterface* compositionAccessibleFactory(const QString &classname, QObject *object)
{
#ifdef HAVE_DCOMP_H
    if (classname == QLatin1String("vlc::DCompRenderWindow"))
    {

        DCompRenderWindow* renderWindow =  qobject_cast<DCompRenderWindow *>(object);
        assert(renderWindow);
        return new QAccessibleRenderWindow(renderWindow, renderWindow);
    }
#endif

    if (classname == QLatin1String("vlc::CompositorOffscreenWindow")
        || (classname == QLatin1String("vlc::DCompOffscreenWindow")) )
    {
        return new QAccessibleOffscreenWindow(qobject_cast<QQuickWindow *>(object));
    }

    return nullptr;
}

}

 #endif
