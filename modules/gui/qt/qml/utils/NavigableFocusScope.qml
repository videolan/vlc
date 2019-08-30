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

/*
 * This class is designed to be inherited, It provide basic key handling to navigate between view
 * classes that inherits this class should provide something like
 * Keys.onPressed {
 *   //custom key handling
 *   defaultKeyAction(event, index)
 * }
 */
FocusScope {
    signal actionUp( int index )
    signal actionDown( int index )
    signal actionLeft( int index )
    signal actionRight( int index )
    signal actionCancel( int index )

    property var navigationParent: undefined
    property var navigationUp: defaultNavigationUp
    property var navigationDown: defaultNavigationDown
    property var navigationLeft: defaultNavigationLeft
    property var navigationRight: defaultNavigationRight
    property var navigationCancel: defaultNavigationCancel

    function defaultNavigationUp(index) {
        if (navigationParent) {
            navigationParent.navigationUp(index)
        } else {
            actionUp(index)
        }
    }

    function defaultNavigationDown(index) {
        if (navigationParent) {
            navigationParent.navigationDown(index)
        } else {
            actionDown(index)
        }
    }

    function defaultNavigationLeft(index) {
        if (navigationParent) {
            navigationParent.navigationLeft(index)
        } else {
            actionLeft(index)
        }
    }

    function defaultNavigationRight(index) {
        if (navigationParent) {
            navigationParent.navigationRight(index)
        } else {
            actionRight(index)
        }
    }

    function defaultNavigationCancel(index) {
        if (navigationParent) {
            navigationParent.navigationCancel(index)
        } else {
            actionCancel(index)
        }
    }

    function defaultKeyAction(event, index) {
        if (event.accepted)
            return
        if ( event.key === Qt.Key_Down || event.matches(StandardKey.MoveToNextLine) ||event.matches(StandardKey.SelectNextLine) ) {
            event.accepted = true
            navigationDown( index )
        } else if ( event.key === Qt.Key_Up || event.matches(StandardKey.MoveToPreviousLine) ||event.matches(StandardKey.SelectPreviousLine) ) {
            event.accepted = true
            navigationUp( index  )
        } else if (event.key === Qt.Key_Right || event.matches(StandardKey.MoveToNextChar) ) {
            event.accepted = true
            navigationRight( index )
        } else if (event.key === Qt.Key_Left || event.matches(StandardKey.MoveToPreviousChar) ) {
            event.accepted = true
            navigationLeft( index )
        } else if ( event.key === Qt.Key_Back || event.key === Qt.Key_Cancel || event.matches(StandardKey.Back) || event.matches(StandardKey.Cancel)) {
            event.accepted = true
            navigationCancel( index )
        }
    }
}
