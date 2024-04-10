/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Author: Prince Gupta <guptaprince8832@gmail.com>
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

import QtQuick
import QtQml.Models

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers


Widgets.StackViewExt {
    id: root

    /*
      A component for loading view subtypes depending on model or user preferences.
      It also handles common actions across sub views such as -
      restoring of initialIndex when view is reloaded and navigation cancel.

      Following are required inputs -
    */

    // components to load depending on MainCtx.gridView
    /* required */ property Component grid
    /* required */ property Component list

    // component to load when provided model is empty
    /* required */ property Component emptyLabel

    // view's model
    /* required */ property var model

    // behave like a Page
    property var pagePrefix: []

    // optional, loaded when isLoading is true
    property Component loadingComponent: null

    // NOTE: Sometimes the model has no 'loading' property.
    readonly property bool isLoading: model.loading ?? false

    readonly property int count: model.count

    readonly property bool hasGridListMode: !!grid && !!list

    property bool isSearchable: false

    property var sortModel: []

    property ItemSelectionModel selectionModel: ListSelectionModel {
        model: root.model
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    // the index to "go to" when the view is loaded
    property int initialIndex: -1

    // used in custom focus management for explicit "focusReason" transfer
    property var setCurrentItemFocus: {
        return Helpers.get(currentItem, "setCurrentItemFocus", setCurrentItemFocusDefault)
    }

    // NOTE: We have to use a Component here. When using a var the onCurrentComponentChanged event
    //       gets called multiple times even when the currentComponent stays the same.
    property Component currentComponent: {
        if (isLoading) {
            if (loadingComponent)
                return loadingComponent
        } else if (count === 0)
            return emptyLabel

        if (MainCtx.gridView)
            return grid
        else
            return list
    }

    // Navigation

    // handle cancelAction, if currentIndex is set reset it to 0
    // otherwise perform default Navigation action
    Navigation.cancelAction: function () {
        if (isLoading || count === 0 || currentItem === null || currentItem.currentIndex === 0)
            return false // transfer cancel action to parent

        if (currentItem.hasOwnProperty("positionViewAtIndex"))
            currentItem.positionViewAtIndex(0, ItemView.Contain)

        currentItem.setCurrentItem(0)

        return true
    }

    // Events

    Component.onCompleted: {
        _updateView()

        // NOTE: This call is useful to avoid a binding loop on currentComponent.
        currentComponentChanged.connect(function() { _updateView() })
    }

    onModelChanged: resetFocus()

    onInitialIndexChanged: resetFocus()

    Connections {
        target: model

        function onCountChanged() {
            if (selectionModel.hasSelection)
                return

            resetFocus()
        }
    }

    // makes the views currentIndex initial index and position view at that index
    function resetFocus() {
        if (isLoading || count === 0 || initialIndex === -1) return

        var index

        if (initialIndex < count)
            index = initialIndex
        else
            index = 0

        selectionModel.select(model.index(index, 0), ItemSelectionModel.ClearAndSelect)

        if (currentItem.hasOwnProperty("positionViewAtIndex"))
            currentItem.positionViewAtIndex(index, ItemView.Contain)

        currentItem.setCurrentItem(index)
    }

    function setCurrentItemFocusDefault(reason) {
        if (currentItem) {
            if (currentItem.setCurrentItemFocus)
                currentItem.setCurrentItemFocus(reason)
            else
                currentItem.forceActiveFocus(reason)
        } else
            Helpers.enforceFocus(root, reason)
    }

    function _updateView() {
        // NOTE: When the currentItem is null we default to the StackView focusReason.
        if (currentItem && currentItem.activeFocus)
            _applyView(currentItem.focusReason)
        else if (activeFocus)
            _applyView(focusReason)
        else
            replace(null, currentComponent)
    }

    function _applyView(reason) {
        replace(null, currentComponent)

        setCurrentItemFocus(reason)
    }
}
