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

import QtQuick 2.11
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers


Widgets.StackViewExt {
    id: root

    /*
      A component for loading view subtypes depending on model or user preferences.
      It also handles common actions across sub views such as -
      restoring of initialIndex when view is reloaded

      Following are required inputs -
    */

    // components to load depending on MainCtx.gridView
    /* required */ property Component grid
    /* required */ property Component list

    // component to load when provided model is empty
    /* required */ property Component emptyLabel

    // view's model
    /* required */ property var model



    property var selectionModel: Util.SelectableDelegateModel {
        model: root.model
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    // the index to "go to" when the view is loaded
    property int initialIndex: -1

    // used in custom focus management for explicit "focusReason" transfer
    readonly property var setCurrentItemFocus: {
        return Helpers.get(currentItem, "setCurrentItemFocus", _setCurrentItemFocusDefault)
    }

    property var currentComponent: {
        if (typeof model === "undefined" || !model)
            return null // invalid state
        if (!model.ready && model.count === 0)
            return emptyLabel
        else if (MainCtx.gridView)
            return grid
        else
            return list
    }

    onCurrentComponentChanged: {
        _loadCurrentViewType()
    }

    onModelChanged: resetFocus()

    onInitialIndexChanged: resetFocus()

    Connections {
        target: model

        onCountChanged: {
            if (model.count === 0 || selectionModel.hasSelection)
                return

            resetFocus()
        }
    }

    function _setCurrentItemFocusDefault(reason) {
        if (currentItem)
            currentItem.forceActiveFocus(reason)
    }

    function _loadCurrentViewType() {
        if (typeof currentComponent === "undefined" || !currentComponent) {
            // invalid case, don't show anything
            clear()
            return
        }

        replace(null, currentComponent)
    }

    // makes the views currentIndex initial index and position view at that index
    function resetFocus() {
        if (!model || model.count === 0) return

        var initialIndex = root.initialIndex
        if (initialIndex >= model.count)
            initialIndex = 0

        selectionModel.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        if (currentItem && currentItem.hasOwnProperty("positionViewAtIndex")) {
            currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)

            // Table View require this for focus handling
            if (!MainCtx.gridView)
                currentItem.currentIndex = initialIndex
        }
    }
}
