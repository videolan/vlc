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
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope {
    id: root

    // when height/width is explicitly set (force size), implicit values will not be used.
    // when height/width is not explicitly set, IconToolButton will set its ...
    // height and width to these implicit counterparts because ...
    // height and width will be set to implicit values when they are not ...
    // explicitly set.
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    property alias model: list.model
    property string textRole
    property string criteriaRole

    property int popupAlignment: Qt.AlignRight | Qt.AlignBottom
    property int listWidth
    property alias currentIndex: list.currentIndex
    property alias focusPolicy: button.focusPolicy

    property VLCColors _colors: VLCStyle.colors

    // properties that should be handled by parent
    // if they are not updated, SortControl will behave as before
    property var sortKey : PlaylistControllerModel.SORT_KEY_NONE
    property var sortOrder : undefined

    property bool _intSortOrder : false

    property alias size: button.size

    signal sortSelected(var modelData)
    signal sortOrderSelected(var order)

    onFocusChanged: {
        if (!focus)
            popup.close()
    }

    onVisibleChanged: {
        if (!visible)
            popup.close()
    }

    Widgets.IconToolButton {
        id: button

        // set height and width to root height and width so that ...
        // we can forcefully set SortControl's width and height.
        height: root.height
        width: root.width

        size: VLCStyle.icon_normal
        iconText: VLCIcons.topbar_sort
        text: i18n.qtr("Sort")

        focus: true

        color: _colors.buttonText
        colorDisabled: _colors.textInactive

        onClicked: {
            if (popup.opened)
                popup.close()
            else
                popup.open()
        }

    }

    Popup {
        id: popup

        y: (popupAlignment & Qt.AlignBottom) ? (root.height + 1) : - (implicitHeight + 1)
        x: (popupAlignment & Qt.AlignRight) ? (button.width - width) :  0
        width: root.listWidth
        implicitHeight: contentItem.implicitHeight + padding * 2
        padding: 1

        onOpened: {
            updateBgRect()

            button.KeyNavigation.down = list
            button.highlighted = true

            list.forceActiveFocus()
        }

        onClosed: {
            button.KeyNavigation.down = null
            button.highlighted = false

            if (button.focusPolicy !== Qt.NoFocus)
                button.forceActiveFocus()
        }

        contentItem: ListView {
            id: list

            clip: true
            implicitHeight: contentHeight
            spacing: 0

            ScrollIndicator.vertical: ScrollIndicator { }

            highlight: Rectangle {
                color: _colors.accent
                opacity: 0.8
            }

            delegate: ItemDelegate {
                id: itemDelegate

                anchors.left: parent.left
                anchors.right: parent.right
                padding: 0

                background: Item {}
                contentItem: Item {
                    implicitHeight: itemRow.implicitHeight
                    width: itemDelegate.width

                    Rectangle {
                        anchors.fill: parent
                        color: _colors.accent
                        visible: mouseArea.containsMouse
                        opacity: 0.8
                    }

                    RowLayout {
                        id: itemRow
                        anchors.fill: parent

                        MenuCaption {
                            id: isActiveText
                            Layout.preferredHeight: itemText.implicitHeight
                            Layout.preferredWidth: tickMetric.width
                            Layout.leftMargin: VLCStyle.margin_xsmall

                            text: root.criteriaRole ? (Array.isArray(root.model) ? (modelData[root.criteriaRole] === sortKey ? "✓" : "")
                                                                                 : (model[root.criteriaRole] === sortKey ? "✓" : "")) : ""
                            color: _colors.buttonText

                            TextMetrics {
                                id: tickMetric
                                text: "✓"
                            }
                        }

                        MenuCaption {
                            Layout.fillWidth: true
                            Layout.topMargin: VLCStyle.margin_xxsmall
                            Layout.bottomMargin: VLCStyle.margin_xxsmall
                            Layout.leftMargin: VLCStyle.margin_xsmall

                            id: itemText
                            text: root.textRole ? (Array.isArray(root.model) ? modelData[root.textRole] : model[root.textRole]) : modelData

                            color: _colors.buttonText
                        }

                        MenuCaption {
                            Layout.preferredHeight: itemText.implicitHeight
                            Layout.rightMargin: VLCStyle.margin_xsmall

                            text: (isActiveText.text === "" ? "" : (sortOrder === PlaylistControllerModel.SORT_ORDER_ASC ? "↓" : "↑"))

                            color: _colors.buttonText
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: itemDelegate.clicked(mouse)
                    }
                }

                onClicked: {
                    root.currentIndex = index

                    if (root.sortOrder !== undefined) {
                        var _sortOrder = root.sortOrder
                        var _sortKey = root.sortKey
                    }

                    root.sortSelected(Array.isArray(root.model) ? modelData : model)

                    if (root.sortOrder !== undefined) {
                        if (root.sortKey !== _sortKey)
                            root._intSortOrder = false

                        if (root.sortOrder === _sortOrder) {
                            root.sortOrderSelected(root._intSortOrder ? PlaylistControllerModel.SORT_ORDER_DESC : PlaylistControllerModel.SORT_ORDER_ASC)
                            root._intSortOrder = !root._intSortOrder
                        }
                    }

                    popup.close()
                }
            }
        }

        function updateBgRect() {
            glassEffect.popupGlobalPos = mainInterfaceRect.mapFromItem(root, popup.x, popup.y)
        }

        background: Rectangle {
            border.width: VLCStyle.dp(1)
            border.color: _colors.accent

            Widgets.FrostedGlassEffect {
                id: glassEffect
                source: mainInterfaceRect

                anchors.fill: parent
                anchors.margins: VLCStyle.dp(1)

                property point popupGlobalPos
                sourceRect: Qt.rect(popupGlobalPos.x, popupGlobalPos.y, glassEffect.width, glassEffect.height)

                tint: _colors.bg
                tintStrength: 0.3
            }
        }

        Connections {
            target: mainInterfaceRect

            enabled: popup.visible

            onWidthChanged: {
                popup.updateBgRect()
            }

            onHeightChanged: {
                popup.updateBgRect()
            }
        }

        Connections {
            target: mainInterface

            enabled: popup.visible

            onIntfScaleFactorChanged: {
                popup.updateBgRect()
            }
        }

        Connections {
            target: playlistColumn

            onWidthChanged: {
                popup.updateBgRect()
            }
        }
    }
}
