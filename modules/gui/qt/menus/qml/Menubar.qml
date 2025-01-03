/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Controls
import QtQuick.Controls.impl 
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Menus


Item {
    id: root

    implicitHeight: menubarLayout.implicitHeight
    implicitWidth: menubarLayout.implicitWidth

    property bool hovered: _countHovered !== 0
    property bool menuOpened: _menuIndex !== -1

    readonly property ColorContext colorContext: ColorContext {
        colorSet: ColorContext.MenuBar
    }

    readonly property var toolbarModel: [
        QmlMenuBar.MEDIA,
        QmlMenuBar.PLAYBACK,
        QmlMenuBar.VIDEO,
        QmlMenuBar.AUDIO,
        QmlMenuBar.SUBTITLE,
        QmlMenuBar.TOOL,
        QmlMenuBar.VIEW,
        QmlMenuBar.HELP
    ]

    property int _menuIndex: -1
    property int _countHovered: 0

    //will contain the visible action (after _updateContentModel)
    property var _visibleActionsModel: []
    //will contain the actions acessible through the '>' menu (after _updateContentModel)
    property var _extraActionsModel: []


    signal _requestMenuPopup(int menuId)

    // Accessible
    Accessible.role: Accessible.MenuBar

    onWidthChanged: _updateContentModel()

    function _openMenu(item, index) {
        menubar.popupMenuEntry(item, toolbarModel[index])
        root._menuIndex = index
    }

    function _updateHover(item, index, hovered ) {
        root._countHovered += hovered ? 1 : -1

        if (hovered && menubar.openMenuOnHover) {
            _openMenu(item, index)
        }
    }

    //set up the model to show as many as possible menu entries
    //remaining menus are placed in the ">" menu
    function _updateContentModel() {
        const visibleActions = []
        const extraActions = []
        const extraPadding = VLCStyle.margin_xsmall * 2
        //keep enough place for the > menu
        let availableWidth = root.width - _buttonWidth(root._extraActionLabel)
        let i = 0
        for (; i < root.toolbarModel.length; ++i) {
            const textWidth = _buttonWidth(toolbarModel[i])
            if (textWidth < availableWidth) {
                visibleActions.push(i)
            } else {
                break
            }
            availableWidth -= textWidth;
        }
        for (; i < toolbarModel.length; ++i) {
            extraActions.push(i)
        }

        root._visibleActionsModel = visibleActions
        root._extraActionsModel = extraActions
    }

    function _buttonWidth(buttonId) {
        return fontMetrics.advanceWidth(
                    menubar.menuEntryTitle(buttonId).replace("&", "") //beware of mnemonics
                ) + (VLCStyle.margin_xsmall * 2)
    }

    QmlMenuBar {
        id: menubar
        ctx: MainCtx
        menubar: menubarLayout
        playerViewVisible: History.match(History.viewPath, ["player"])

        onMenuClosed: _menuIndex = -1
        onNavigateMenu: (direction) => {
            const i =  (root._menuIndex + root.toolbarModel.length + direction) % root.toolbarModel.length
            root._requestMenuPopup(i)
        }
    }

    FontMetrics {
        id: fontMetrics
        font.pixelSize: VLCStyle.fontSize_normal
    }

    Row {
        id: menubarLayout
        spacing: 0

        Repeater {
            model: root._visibleActionsModel

            MenubarButton {
                id: visibleButton

                text: menubar.menuEntryTitle(toolbarModel[modelData])
                focused: modelData === root._menuIndex

                onClicked: root._openMenu(visibleButton, modelData)
                onHoveredChanged: root._updateHover(visibleButton, modelData, hovered)

                Connections {
                    target: root
                    function on_RequestMenuPopup(menuId) {
                        if (menuId === modelData) {
                            root._openMenu(visibleButton, modelData)
                        }
                    }
                }
            }
        }

        Repeater {
            model: root._extraActionsModel

            Item {
                id: hiddenButton

                width: _buttonWidth(toolbarModel[modelData])
                height: 1 //item with 0 height are discarded
                anchors.bottom: menubarLayout.bottom

                T.MenuItem {
                    text: menubar.menuEntryTitle(toolbarModel[modelData])

                    width: 0
                    height: 0

                    onClicked: root._openMenu(hiddenButton, modelData)
                    Accessible.onPressAction: root._openMenu(hiddenButton, modelData)

                    contentItem: null
                    background: null
                }

                Connections {
                    target: root
                    function on_RequestMenuPopup(menuId) {
                        if (menuId === modelData) {
                            root._openMenu(hiddenButton, modelData)
                        }
                    }
                }
            }
        }
    }

    MenubarButton {
        id: extraActionButton

        anchors.top:  parent.top
        anchors.right: parent.right

        text: ">"

        Accessible.ignored: true

        visible: _extraActionsModel.length > 0
        enabled: visible

        onClicked: menubar.popupExtraActionsMenu(extraActionButton, root._extraActionsModel)
    }

    component MenubarButton: T.MenuItem {
        id: control

        property bool focused: false
        property bool textVisible: true

        font.pixelSize: VLCStyle.fontSize_normal

        padding: 0

        implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
        implicitHeight: contentItem.implicitHeight + VLCStyle.margin_xxxsmall * 2

        leftPadding: VLCStyle.margin_xsmall
        rightPadding: VLCStyle.margin_xsmall

        Accessible.onPressAction: control.clicked()

        ColorContext {
            id: theme
            colorSet: root.colorContext.colorSet

            enabled: control.enabled
            focused: control.focused
            hovered: control.hovered
        }

        contentItem: IconLabel {
            text: control.text
            font: control.font
            opacity: enabled ? 1.0 : 0.3
            color: theme.fg.primary
            visible: control.textVisible
        }

        background: Rectangle {
            color: theme.bg.primary
        }
    }
}
