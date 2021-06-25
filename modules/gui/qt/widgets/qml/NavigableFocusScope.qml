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
import QtQuick 2.11
import QtQuick.Controls 2.4
import "qrc:///util/KeyHelper.js" as KeyHelper

/*
 * This class is designed to be inherited, It provide basic key handling to navigate between view
 * classes that inherits this class should provide something like
 * Keys.onPressed {
 *   //custom key handling
 *   defaultKeyAction(event)
 * }
 */
FocusScope {
    signal actionUp( )
    signal actionDown( )
    signal actionLeft( )
    signal actionRight( )
    signal actionCancel( )

    property bool navigable: true

    property var navigationParent: undefined
    property var navigationUp: defaultNavigationUp
    property var navigationDown: defaultNavigationDown
    property var navigationLeft: defaultNavigationLeft
    property var navigationRight: defaultNavigationRight
    property var navigationCancel: defaultNavigationCancel

    property var navigationUpItem: undefined
    property var navigationDownItem: undefined
    property var navigationLeftItem: undefined
    property var navigationRightItem: undefined
    property var navigationCancelItem: undefined


    function defaultNavigationUp() {
        if (navigationUpItem) {
            if (navigationUpItem.visible
                && navigationUpItem.enabled
                && (navigationUpItem.navigable === undefined || navigationUpItem.navigable)) {
                navigationUpItem.forceActiveFocus()
            } else {
                navigationUpItem.navigationUp()
            }
        } else if (navigationParent) {
            navigationParent.navigationUp()
        } else {
            actionUp()
        }
    }

    function defaultNavigationDown() {
        if (navigationDownItem) {
            var item = navigationDownItem
            if (item.visible
                && item.enabled
                && (item.navigable === undefined || item.navigable)) {
                item.forceActiveFocus()
            } else {
                item.navigationDown()
            }
        } else if (navigationParent) {
            navigationParent.navigationDown()
        } else {
            actionDown()
        }
    }

    function defaultNavigationLeft() {
        if (navigationLeftItem) {
            var item = navigationLeftItem
            if (item.visible
                && item.enabled
                && (item.navigable === undefined || item.navigable)) {
                item.forceActiveFocus()
            } else {
                item.navigationLeft()
            }
        } else if (navigationParent) {
            navigationParent.navigationLeft()
        } else {
            actionLeft()
        }
    }

    function defaultNavigationRight() {
        if (navigationRightItem) {
            var item = navigationRightItem
            if (item.visible
                && item.enabled
                && (item.navigable === undefined || item.navigable)) {
                item.forceActiveFocus()
            } else {
                item.navigationRight()
            }
        } else if (navigationParent) {
            navigationParent.navigationRight()
        } else {
            actionRight()
        }
    }

    function defaultNavigationCancel() {
        if (navigationCancelItem) {
            var item = navigationCancelItem
            if (item.visible
                && item.enabled
                && (item.navigable === undefined || item.navigable)) {
                item.forceActiveFocus()
            } else {
                item.navigationCancel()
            }
        } else if (navigationParent) {
            navigationParent.navigationCancel()
        } else {
            actionCancel()
        }
    }

    function defaultKeyAction(event) {
        if (event.accepted)
            return
        if ( KeyHelper.matchDown(event) ) {
            event.accepted = true
            navigationDown()
        } else if ( KeyHelper.matchUp(event) ) {
            event.accepted = true
            navigationUp()
        } else if ( KeyHelper.matchRight(event) ) {
            event.accepted = true
            navigationRight()
        } else if ( KeyHelper.matchLeft(event) ) {
            event.accepted = true
            navigationLeft()
        } else if ( KeyHelper.matchCancel(event) ) {
            event.accepted = true
            navigationCancel()
        }
    }

    function defaultKeyReleaseAction(event) {
        if (event.accepted)
            return

        if ( KeyHelper.matchLeft(event)
                || KeyHelper.matchRight(event)
                || KeyHelper.matchUp(event)
                || KeyHelper.matchDown(event)
                || KeyHelper.matchCancel(event) )
        {
            event.accepted = true
        }
    }
}
