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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus

FocusScope{
    id: root

    /* required */ property int textWidth

    property string title
    property VLCColors colors: VLCStyle.nightColors

    property bool showCSD: false
    property bool showToolbar: false
    property bool pinControls: false

    property int reservedHeight: 0

    property int _sideMargin: VLCStyle.margin_small

    signal togglePlaylistVisibility()
    signal requestLockUnlockAutoHide(bool lock)
    signal backRequested()

    Component.onCompleted:  root._layout()

    onShowCSDChanged: root._layout()
    onPinControlsChanged: root._layout()
    onShowToolbarChanged: root._layout()

    function _layoutLine(c1, c2, offset)
    {
        var lineHeight =  Math.max(c1 !== undefined ? c1.implicitHeight : 0, c2 !== undefined ? c2.implicitHeight : 0)

        if (c1) {
            c1.height = lineHeight
            c1.anchors.topMargin = offset
        }

        if (c2) {
            c2.height = lineHeight
            c2.anchors.topMargin = offset
        }
        return lineHeight
    }

    function _layout() {
        var offset = 0

        if (root.pinControls && !root.showToolbar && root.showCSD) {
            //place everything on one line
            var lineHeight = Math.max(logoGroup.implicitHeight, playlistGroup.implicitHeight, csdDecorations.implicitHeight)

            centerTitleText.y = 0
            centerTitleText.height = lineHeight

            csdDecorations.height  = lineHeight

            logoGroup.height =  lineHeight

            playlistGroup.height = lineHeight
            playlistGroup.anchors.topMargin = 0
            playlistGroup.extraRightMargin = Qt.binding(function() { return root.width - csdDecorations.x })


            root.implicitHeight = lineHeight
            offset += lineHeight

        } else {
            playlistGroup.extraRightMargin = 0

            var left = undefined
            var right = undefined
            var logoPlaced = false

            if (root.showToolbar) {
                left = menubar
            }

            if (root.showCSD) {
                right = csdDecorations
                if (!left) {
                    left = logoGroup
                    logoPlaced = true
                }
            }

            if (!!left || !!right) {
                offset += root._layoutLine(left, right, offset)

                if (root.showCSD) {
                    tapNDrag.height = offset
                }
            }

            if (!logoPlaced) {
                left = logoGroup
            } else {
                left = undefined
            }

            right = playlistGroup

            var secondLineOffset = offset
            var secondLineHeight = root._layoutLine(left, right, offset)

            offset += secondLineHeight

            if (root.pinControls) {
                centerTitleText.y = secondLineOffset
                centerTitleText.height = secondLineHeight
            }

        }

        root.implicitHeight = offset
        reservedHeight = offset
    }

    //drag and dbl click the titlebar in CSD mode
    Loader {
        id: tapNDrag

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        active: root.showCSD
        source: "qrc:///widgets/CSDTitlebarTapNDrapHandler.qml"
    }

    // Components -
    Menus.Menubar {
        id: menubar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: root._sideMargin

        width: implicitWidth

        visible: root.showToolbar
        textColor: root.colors.text
        highlightedBgColor: root.colors.bgHover
        highlightedTextColor: root.colors.bgHoverText

        onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
        onMenuOpenedChanged: root.requestLockUnlockAutoHide(menuOpened)
    }

    RowLayout {
        id: logoGroup

        spacing: VLCStyle.margin_xxsmall

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin:  root._sideMargin

        Widgets.IconControlButton {
            id: backBtn

            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

            objectName: "IconToolButton"
            size: VLCStyle.banner_icon_size
            iconText: VLCIcons.topbar_previous
            text: I18n.qtr("Back")
            focus: true
            colors: root.colors

            Navigation.parentItem: root
            Navigation.rightItem: menuSelector
            onClicked: root.backRequested()

            onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
        }

        Image {
            id: logo

            Layout.alignment: Qt.AlignVCenter

            sourceSize.width: VLCStyle.icon_small
            sourceSize.height: VLCStyle.icon_small
            source: "qrc:///logo/cone.svg"
            enabled: false
        }
    }

    //FIXME use the the right class
    T.Label {
        id: centerTitleText

        readonly property int _leftLimit: logoGroup.x + logoGroup.width
        readonly property int _rightLimit: playlistGroup.x
        readonly property int _availableWidth: _rightLimit - _leftLimit
        readonly property int _centerX: ((root.width - centerTitleText.implicitWidth) / 2)
        readonly property bool _alignHCenter: _centerX > _leftLimit
                                              && _centerX + centerTitleText.implicitWidth < _rightLimit

        visible: root.pinControls

        width: Math.min(centerTitleText._availableWidth, centerTitleText.implicitWidth)

        leftPadding: VLCStyle.margin_small
        rightPadding: VLCStyle.margin_small

        text: root.title
        color: root.colors.playerFg
        font.pixelSize: VLCStyle.dp(13, VLCStyle.scale)
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter

        on_AlignHCenterChanged: _layout()
        Component.onCompleted: _layout()

        function _layout() {
            if (_alignHCenter) {
                centerTitleText.x = 0
                centerTitleText.anchors.horizontalCenter = root.horizontalCenter
            } else {
                centerTitleText.anchors.horizontalCenter = undefined
                centerTitleText.x = Qt.binding(function() { return centerTitleText._leftLimit })
            }
        }
    }

    //FIXME use the the right class
    T.Label {
        id: leftTitleText

        anchors.left: parent.left
        anchors.top: logoGroup.bottom
        anchors.leftMargin: root._sideMargin

        width: root.textWidth - VLCStyle.margin_normal

        visible: !root.pinControls

        topPadding: VLCStyle.margin_large
        leftPadding: logo.x

        text: root.title
        horizontalAlignment: Text.AlignLeft
        color: root.colors.playerFg
        font.weight: Font.DemiBold
        font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
        elide: Text.ElideRight
    }

    Loader {
        id: csdDecorations

        anchors.top: parent.top
        anchors.right: parent.right

        focus: false
        height: VLCStyle.icon_normal
        active: root.showCSD
        enabled: root.showCSD
        visible: root.showCSD
        source: "qrc:///widgets/CSDWindowButtonSet.qml"
        onLoaded: {
            item.color = Qt.binding(function() { return root.colors.playerFg })
            item.hoverColor = Qt.binding(function() { return root.colors.windowCSDButtonDarkBg })
        }

        Connections {
            target: csdDecorations.item
            enabled: csdDecorations.loaded
            onHoveredChanged: root.requestLockUnlockAutoHide(csdDecorations.item.hovered)
        }
    }

    Row {
        id: playlistGroup

        property int extraRightMargin: 0

        focus: true
        spacing: VLCStyle.margin_xxsmall

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: root._sideMargin + extraRightMargin

        Widgets.IconControlButton {
            id: menuSelector

            visible: !root.showToolbar
            enabled: visible
            focus: visible
            size: VLCStyle.banner_icon_size

            width: VLCStyle.bannerButton_width
            height: VLCStyle.bannerButton_height

            iconText: VLCIcons.ellipsis
            text: I18n.qtr("Menu")
            colors: root.colors

            Navigation.parentItem: root
            Navigation.leftItem: backBtn
            Navigation.rightItem: playlistButton

            onClicked: contextMenu.popup(this.mapToGlobal(0, height))

            onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

            QmlGlobalMenu {
                id: contextMenu

                ctx: MainCtx

                onAboutToShow: root.requestLockUnlockAutoHide(true)
                onAboutToHide: root.requestLockUnlockAutoHide(false)
            }
        }

        Widgets.IconControlButton {
            id: playlistButton

            objectName: ControlListModel.PLAYLIST_BUTTON
            size: VLCStyle.banner_icon_size
            iconText: VLCIcons.playlist
            text: I18n.qtr("Playlist")
            colors: root.colors
            focus: root.showToolbar

            width: VLCStyle.bannerButton_width
            height: VLCStyle.bannerButton_height

            Navigation.parentItem: root
            Navigation.leftItem: menuSelector.visible ? menuSelector : backBtn
            onClicked: togglePlaylistVisibility()

            onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
        }
    }
}
