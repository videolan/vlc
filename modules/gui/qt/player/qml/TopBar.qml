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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus

FocusScope{
    id: topFocusScope

    enum GroupAlignment {
        Horizontal,
        Vertical
    }

    /* required */ property int textWidth

    property string title
    property VLCColors colors: VLCStyle.nightColors
    property int groupAlignment: TopBar.GroupAlignment.Vertical
    property Item _currentTitleText: null

    signal togglePlaylistVisibility()
    signal requestLockUnlockAutoHide(bool lock, var source)

    implicitHeight: topcontrollerMouseArea.implicitHeight

    Component.onCompleted: {
        // if groupAlignment == Horizontal, then onGroupAlignment isn't called when Component is created
        if (groupAlignment === TopBar.GroupAlignment.Horizontal)
            _layout()
    }

    onGroupAlignmentChanged: _layout()

    function _layout() {
        if (topFocusScope._currentTitleText)
            topFocusScope._currentTitleText.destroy()

        switch (groupAlignment) {
            case TopBar.GroupAlignment.Horizontal:
                leftColumn.children = [menubar, logoGroup]

                _currentTitleText = centerTitleTextComponent.createObject(topcontrollerMouseArea)

                var rightRow = Qt.createQmlObject("import QtQuick 2.11; Row {}", rightColumn, "TopBar")
                rightRow.children = [playlistGroup, csdDecorations]
                playlistGroup.anchors.verticalCenter = rightRow.verticalCenter
                break;

            case TopBar.GroupAlignment.Vertical:
                _currentTitleText = leftTitleTextComponent.createObject()
                leftColumn.children = [menubar, logoGroup, _currentTitleText]
                playlistGroup.anchors.verticalCenter = undefined
                rightColumn.children = [csdDecorations, playlistGroup]
                playlistGroup.Layout.alignment = Qt.AlignRight
        }
    }

    // Main Content Container
    MouseArea {
        id: topcontrollerMouseArea

        hoverEnabled: true
        anchors.fill: parent
        implicitHeight: rowLayout.implicitHeight

        onContainsMouseChanged: topFocusScope.requestLockUnlockAutoHide(containsMouse, topFocusScope)

        //drag and dbl click the titlebar in CSD mode
        Loader {
            anchors.fill: parent
            active: MainCtx.clientSideDecoration
            source: "qrc:///widgets/CSDTitlebarTapNDrapHandler.qml"
        }

        RowLayout {
            id: rowLayout

            anchors.fill: parent

            ColumnLayout {
                id: leftColumn

                spacing: 0
                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                Layout.leftMargin: VLCStyle.margin_xxsmall
                Layout.topMargin: VLCStyle.margin_xxsmall
                Layout.bottomMargin: VLCStyle.margin_xxsmall
            }

            ColumnLayout {
                id: rightColumn

                spacing: 0
                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                // this column may contain CSD, don't apply margins directly
            }
        }
    }

    // Components -
    Menus.Menubar {
        id: menubar

        width: implicitWidth
        height: VLCStyle.icon_normal
        visible: MainCtx.hasToolbarMenu
        textColor: topFocusScope.colors.text
        highlightedBgColor: topFocusScope.colors.bgHover
        highlightedTextColor: topFocusScope.colors.bgHoverText
    }

    RowLayout {
        id: logoGroup

        spacing: VLCStyle.margin_xxsmall

        Widgets.IconControlButton {
            id: backBtn

            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

            objectName: "IconToolButton"
            size: VLCStyle.banner_icon_size
            iconText: VLCIcons.topbar_previous
            text: I18n.qtr("Back")
            focus: true
            colors: topFocusScope.colors

            Navigation.parentItem: topFocusScope
            Navigation.rightItem: menuSelector
            onClicked: {
                if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
                   mainPlaylistController.stop()
                }
                History.previous()
            }
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

    Component {
        id: centerTitleTextComponent

        T.Label {
            id: centerTitleText

            readonly property int _availableWidth: rightColumn.x - (leftColumn.x + leftColumn.width)
            readonly property int _centerX: ((topcontrollerMouseArea.width - centerTitleText.implicitWidth) / 2)
            readonly property bool _alignHCenter: _centerX > leftColumn.x + leftColumn.width
                                                  && _centerX + centerTitleText.implicitWidth < rightColumn.x

            y: leftColumn.y
            topPadding: VLCStyle.margin_xxsmall
            leftPadding: VLCStyle.margin_small
            rightPadding: VLCStyle.margin_small
            text: topFocusScope.title
            color: topFocusScope.colors.playerFg
            font.pixelSize: VLCStyle.dp(13, VLCStyle.scale)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            width: Math.min(centerTitleText._availableWidth, centerTitleText.implicitWidth)

            on_AlignHCenterChanged: _layout()
            Component.onCompleted: _layout()

            function _layout() {
                if (_alignHCenter) {
                    centerTitleText.x = 0
                    centerTitleText.anchors.horizontalCenter = topcontrollerMouseArea.horizontalCenter
                } else {
                    centerTitleText.anchors.horizontalCenter = undefined
                    centerTitleText.x = Qt.binding(function() { return leftColumn.x + leftColumn.width; })
                }
            }
        }
    }

    Component {
        id: leftTitleTextComponent

        //FIXME use the the right class
        T.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: topFocusScope.textWidth - VLCStyle.margin_normal

            text: topFocusScope.title
            horizontalAlignment: Text.AlignLeft
            topPadding: VLCStyle.margin_large
            leftPadding: logo.x
            color: topFocusScope.colors.playerFg
            font.weight: Font.DemiBold
            font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
            elide: Text.ElideRight
        }
    }

    Loader {
        id: csdDecorations

        focus: false
        height: VLCStyle.icon_normal
        active: MainCtx.clientSideDecoration
        enabled: MainCtx.clientSideDecoration
        visible: MainCtx.clientSideDecoration
        source: "qrc:///widgets/CSDWindowButtonSet.qml"
        onLoaded: {
            item.color = Qt.binding(function() { return topFocusScope.colors.playerFg })
            item.hoverColor = Qt.binding(function() { return topFocusScope.colors.windowCSDButtonDarkBg })
        }
    }

    Row {
        id: playlistGroup

        focus: true
        spacing: VLCStyle.margin_xxsmall
        topPadding: VLCStyle.margin_xxsmall
        rightPadding: VLCStyle.margin_xxsmall

        Widgets.IconControlButton {
            id: menuSelector

            focus: true
            size: VLCStyle.banner_icon_size
            iconText: VLCIcons.ellipsis
            text: I18n.qtr("Menu")
            colors: topFocusScope.colors

            Navigation.parentItem: topFocusScope
            Navigation.leftItem: backBtn
            Navigation.rightItem: playlistButton

            onClicked: contextMenu.popup(this.mapToGlobal(0, height))

            QmlGlobalMenu {
                id: contextMenu

                ctx: MainCtx

                onAboutToShow: topFocusScope.requestLockUnlockAutoHide(true, contextMenu)
                onAboutToHide: topFocusScope.requestLockUnlockAutoHide(false, contextMenu)
            }
        }

        Widgets.IconControlButton {
            id: playlistButton

            objectName: ControlListModel.PLAYLIST_BUTTON
            size: VLCStyle.banner_icon_size
            iconText: VLCIcons.playlist
            text: I18n.qtr("Playlist")
            colors: topFocusScope.colors
            focus: false

            Navigation.parentItem: topFocusScope
            Navigation.leftItem: menuSelector
            onClicked: togglePlaylistVisibility()
        }
    }
}
