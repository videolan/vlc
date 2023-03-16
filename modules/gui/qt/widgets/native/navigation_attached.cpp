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
#include "navigation_attached.hpp"
#include "util/keyhelper.hpp"

NavigationAttached::NavigationAttached(QObject *parent)
    : QObject(parent)
    , m_navigable(true)
    , m_parentItem(nullptr)
    , m_upItem(nullptr)
    , m_downItem(nullptr)
    , m_leftItem(nullptr)
    , m_rightItem(nullptr)
    , m_cancelItem(nullptr)
{
}


void NavigationAttached::defaultNavigationGeneric(QJSValue& jsCallback, QQuickItem* directionItem,
                                        void (NavigationAttached::* defaultNavFn)(void),
                                        Qt::FocusReason reason)
{
    if (jsCallback.isCallable()) {
        const auto ret = jsCallback.call();

        // if the function returns nothing or true, the action has been handled, stop the traversal
        // if the function returns a false explictly, continue traversal
        if (!ret.isBool() || ret.toBool())
            return;
    }


    if (directionItem)
    {
        NavigationAttached* nextItem = qobject_cast<NavigationAttached*>(qmlAttachedPropertiesObject<NavigationAttached>(directionItem));
        if (directionItem->isVisible()
            && directionItem->isEnabled()
            && (!nextItem || nextItem->getnavigable()))
        {
            directionItem->forceActiveFocus(reason);
        }
        else if (nextItem)
        {
            (nextItem->*defaultNavFn)();
        }
    }
    else if (m_parentItem)
    {
        NavigationAttached* parentNav = qobject_cast<NavigationAttached*>(qmlAttachedPropertiesObject<NavigationAttached>(m_parentItem));
        (parentNav->*defaultNavFn)();
    }
}

void NavigationAttached::defaultNavigationUp()
{
    defaultNavigationGeneric(m_upAction, m_upItem,
                             &NavigationAttached::defaultNavigationUp,
                             Qt::BacktabFocusReason);
}


void NavigationAttached::defaultNavigationDown()
{
    defaultNavigationGeneric(m_downAction, m_downItem,
                             &NavigationAttached::defaultNavigationDown,
                             Qt::TabFocusReason);
}

void NavigationAttached::defaultNavigationLeft()
{
    defaultNavigationGeneric(m_leftAction, m_leftItem,
                             &NavigationAttached::defaultNavigationLeft,
                             Qt::BacktabFocusReason);
}


void NavigationAttached::defaultNavigationRight()
{
    defaultNavigationGeneric(m_rightAction, m_rightItem,
                             &NavigationAttached::defaultNavigationRight,
                             Qt::TabFocusReason);
}

void NavigationAttached::defaultNavigationCancel()
{
    defaultNavigationGeneric(m_cancelAction, m_cancelItem,
                             &NavigationAttached::defaultNavigationCancel,
                             Qt::OtherFocusReason);
}

void NavigationAttached::defaultKeyAction(QObject* event)
{
    bool accepted = event->property("accepted").toBool();
    if (accepted)
        return;

    QKeyEvent fakeEvent(QKeyEvent::KeyPress,
                        event->property("key").toInt(),
                        static_cast<Qt::KeyboardModifiers>(event->property("modifiers").toInt()),
                        event->property("text").toString(),
                        event->property("isAutoRepeat").toBool(),
                        event->property("count").toInt()
                        );


    if ( KeyHelper::matchDown(&fakeEvent) ) {
        event->setProperty("accepted", true);
        defaultNavigationDown();
    } else if ( KeyHelper::matchUp(&fakeEvent) ) {
        event->setProperty("accepted", true);
        defaultNavigationUp();
    } else if ( KeyHelper::matchRight(&fakeEvent) ) {
        event->setProperty("accepted", true);
        defaultNavigationRight();
    } else if ( KeyHelper::matchLeft(&fakeEvent) ) {
        event->setProperty("accepted", true);
        defaultNavigationLeft();
    } else if ( KeyHelper::matchCancel(&fakeEvent) ) {
        event->setProperty("accepted", true);
        defaultNavigationCancel();
    }
}

void NavigationAttached::defaultKeyReleaseAction(QObject* event)
{
    bool accepted = event->property("accepted").toBool();

    if (accepted)
        return;

    QKeyEvent fakeEvent(QKeyEvent::KeyRelease,
                        event->property("key").toInt(),
                        static_cast<Qt::KeyboardModifiers>(event->property("modifiers").toInt()),
                        event->property("text").toString(),
                        event->property("isAutoRepeat").toBool(),
                        event->property("count").toInt()
                        );

     if ( KeyHelper::matchLeft(&fakeEvent)
             || KeyHelper::matchRight(&fakeEvent)
             || KeyHelper::matchUp(&fakeEvent)
             || KeyHelper::matchDown(&fakeEvent) )
     {
         event->setProperty("accepted", true);
     }
}
