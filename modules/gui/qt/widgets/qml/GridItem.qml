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
import QtQml.Models 2.2
import QtGraphicalEffects 1.0
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item {
    id: root

    property alias image: picture.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias playCoverBorder: picture.playCoverBorder
    property bool selected: false

    property alias progress: picture.progress
    property alias labels: picture.labels
    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleMargin: VLCStyle.margin_xsmall

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent)

    Keys.onMenuPressed: root.contextMenuButtonClicked(picture)

    Accessible.role: Accessible.Cell
    Accessible.name: title

    implicitWidth: mouseArea.implicitWidth
    implicitHeight: mouseArea.implicitHeight

    readonly property bool _highlighted: mouseArea.containsMouse || content.activeFocus

    readonly property int _selectedBorderWidth: VLCStyle.column_margin_width - ( VLCStyle.margin_small * 2 )

    MouseArea {
        id: mouseArea
        hoverEnabled: true

        anchors.fill: parent
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight

        acceptedButtons: Qt.RightButton | Qt.LeftButton
        Keys.onMenuPressed: root.contextMenuButtonClicked(picture)

        onClicked: {
            if (mouse.button === Qt.RightButton)
                contextMenuButtonClicked(picture);
            else {
                root.itemClicked(picture, mouse.button, mouse.modifiers);
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton)
                root.itemDoubleClicked(picture,mouse.buttons, mouse.modifiers)
        }

        FocusScope {
            id: content

            anchors.fill: parent
            implicitWidth: layout.implicitWidth
            implicitHeight: layout.implicitHeight
            focus: root.activeFocus

            /* background visible when selected */
            Rectangle {
                x: - root._selectedBorderWidth
                y: - root._selectedBorderWidth
                width: layout.implicitWidth + ( root._selectedBorderWidth * 2 )
                height:  layout.implicitHeight + ( root._selectedBorderWidth * 2 )
                color: VLCStyle.colors.bgAlt
                visible: root.selected || root._highlighted
            }

            ColumnLayout {
                id: layout

                anchors.fill: parent
                spacing: 0

                Widgets.MediaCover {
                    id: picture

                    width: pictureWidth
                    height: pictureHeight
                    playCoverVisible: root._highlighted
                    onPlayIconClicked: root.playClicked()

                    Layout.preferredWidth: pictureWidth
                    Layout.preferredHeight: pictureHeight
                }

                Widgets.ScrollingText {
                    id: titleTextRect

                    label: titleLabel
                    scroll: _highlighted

                    Layout.preferredHeight: titleLabel.contentHeight
                    Layout.topMargin: VLCStyle.margin_xxsmall
                    Layout.fillWidth: true
                    Layout.maximumWidth: pictureWidth

                    Widgets.MenuLabel {
                        id: titleLabel

                        elide: Text.ElideNone
                        width: pictureWidth
                    }
                }

                Widgets.MenuCaption {
                    id: subtitleTxt

                    visible: text !== ""
                    text: root.subtitle

                    Layout.preferredHeight: implicitHeight
                    Layout.maximumWidth: pictureWidth
                    Layout.fillWidth: true
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
