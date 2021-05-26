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

    property alias model: listView.model
    property string textRole
    property string criteriaRole
    // provided for convenience:
    property alias titleRole: root.textRole
    property alias keyRole: root.criteriaRole

    property int popupAlignment: Qt.AlignRight | Qt.AlignBottom
    property real listWidth: VLCStyle.widthSortBox
    property alias focusPolicy: button.focusPolicy
    property alias iconSize: button.size

    property VLCColors colors: VLCStyle.colors

    // properties that should be handled by parent
    // if they are not updated, tick mark and order mark will not be shown
    property var sortKey: undefined
    property var sortOrder: undefined

    // sortSelected is triggered with new sorting key when a different sorting key is selected
    // sortOrderSelected is triggered with Qt.AscendingOrder when different sorting key is selected
    // sortOrderSelected is triggered with Qt.AscendingOrder or Qt.DescendingOrder when the same sorting key is selected
    signal sortSelected(var type)
    signal sortOrderSelected(var type)

    onVisibleChanged: {
        if (!visible)
            popup._close()
    }

    onEnabledChanged: {
        if (!enabled)
            popup._close()
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

        onClicked: {
            if (popup.visible && !closeAnimation.running)
                popup._close()
            else
                popup._open()
        }
    }

    Popup {
        id: popup

        closePolicy: Popup.NoAutoClose
        y: (popupAlignment & Qt.AlignBottom) ? (root.height) : -(height)
        x: (popupAlignment & Qt.AlignRight) ? (button.width - width) : 0

        width: listWidth

        padding: bgRect.border.width

        clip: true

        height: 0

        NumberAnimation {
            id: openAnimation
            target: popup
            property: "height"
            duration: 125
            easing.type: Easing.InOutSine
            to: popup.implicitHeight

            onStarted: closeAnimation.stop()
        }

        NumberAnimation {
            id: closeAnimation
            target: popup
            property: "height"
            duration: 125
            easing.type: Easing.InOutSine
            to: 0

            onStarted: openAnimation.stop()
            onStopped: if (!openAnimation.running) popup.close()
        }

        function _open() {
            if (!popup.visible)
                popup.open()
            openAnimation.start()
        }

        function _close() {
            closeAnimation.start()
        }

        onOpened: {
            button.highlighted = true
            listView.forceActiveFocus()
        }

        onClosed: {
            popup.height = 0

            button.highlighted = false

            if (button.focusPolicy !== Qt.NoFocus)
                button.forceActiveFocus()
        }

        contentItem: ListView {
            id: listView

            implicitHeight: contentHeight

            onActiveFocusChanged: {
                // since Popup.CloseOnReleaseOutside closePolicy is limited to
                // modal popups, this is an alternative way of closing the popup
                // when the focus is lost
                if (!activeFocus && !button.activeFocus)
                    popup._close()
            }

            ScrollIndicator.vertical: ScrollIndicator { }

            property bool containsMouse: false

            delegate: ItemDelegate {
                id: itemDelegate

                width: parent.width

                readonly property var delegateSortKey: modelData[root.criteriaRole]

                readonly property bool isActive: (delegateSortKey === sortKey)

                background: FocusBackground {
                    active: (closeAnimation.running === false && itemDelegate.hovered)

                    // NOTE: We don't want animations here, because it looks sluggish.
                    durationAnimation: 0

                    backgroundColor: VLCStyle.colors.dropDown
                }

                onHoveredChanged: {
                    listView.containsMouse = hovered
                    itemDelegate.forceActiveFocus()
                }

                contentItem: Item {
                    implicitHeight: itemRow.height
                    width: itemDelegate.width

                    RowLayout {
                        id: itemRow

                        anchors.left: parent.left
                        anchors.right: parent.right

                        anchors {
                            leftMargin: VLCStyle.margin_xxsmall
                            rightMargin: VLCStyle.margin_xxsmall
                        }

                        MenuCaption {
                            Layout.preferredHeight: itemText.implicitHeight
                            Layout.preferredWidth: tickMetric.width

                            horizontalAlignment: Text.AlignHCenter

                            text: isActive ? tickMetric.text : ""

                            color: colors.buttonText

                            TextMetrics {
                                id: tickMetric
                                text: "✓"
                            }
                        }

                        MenuCaption {
                            Layout.fillWidth: true
                            Layout.leftMargin: VLCStyle.margin_xxsmall

                            id: itemText
                            text: modelData[root.textRole]

                            color: colors.buttonText
                        }

                        MenuCaption {
                            Layout.preferredHeight: itemText.implicitHeight

                            text: (sortOrder === Qt.AscendingOrder ? "↓" : "↑")
                            visible: isActive

                            color: colors.buttonText
                        }
                    }
                }

                onClicked: {
                    if (root.sortKey !== delegateSortKey) {
                        root.sortSelected(delegateSortKey)
                        root.sortOrderSelected(Qt.AscendingOrder)
                    }
                    else {
                        root.sortOrderSelected(root.sortOrder === Qt.AscendingOrder ? Qt.DescendingOrder : Qt.AscendingOrder)
                    }

                    popup.close()
                }

                BackgroundFocus {
                    anchors.fill: parent

                    visible: itemDelegate.activeFocus
                }
            }
        }

        background: Rectangle {
            id: bgRect

            border.width: VLCStyle.dp(1)
            border.color: colors.dropDownBorder

            Loader {
                id: effectLoader

                anchors.fill: parent
                anchors.margins: VLCStyle.dp(1)

                asynchronous: true

                Component {
                    id: frostedGlassEffect

                    Widgets.FrostedGlassEffect {
                        source: g_root

                        // since Popup is not an Item, we can not directly map its position
                        // to the source item. Instead, we can use root because popup's
                        // position is relative to root's position.
                        // This method unfortunately causes issues when source item is resized.
                        // But in that case, we reload the effectLoader to redraw the effect.
                        property point popupMappedPos: g_root.mapFromItem(root, popup.x, popup.y)
                        sourceRect: Qt.rect(popupMappedPos.x, popupMappedPos.y, width, height)

                        tint: colors.bg
                        tintStrength: 0.3
                    }
                }

                sourceComponent: frostedGlassEffect

                function reload() {
                    if (status != Loader.Ready)
                        return

                    sourceComponent = undefined
                    sourceComponent = frostedGlassEffect
                }
            }
        }

        Connections {
            target: g_root

            enabled: popup.visible

            onWidthChanged: {
                effectLoader.reload()
            }

            onHeightChanged: {
                effectLoader.reload()
            }
        }
    }
}
