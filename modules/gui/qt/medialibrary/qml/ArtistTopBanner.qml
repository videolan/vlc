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
import QtQml.Models 2.11
import QtGraphicalEffects 1.0

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    property var artist: ({})

    height: col.implicitHeight


    Image {
        id: background

        anchors.fill: parent

        source: artist.cover || VLCStyle.noArtArtist

        fillMode: Image.PreserveAspectCrop
        sourceSize.width: parent.width
        sourceSize.height: parent.height

        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: LinearGradient {
                id: mask
                width: background.width
                height: background.height
                gradient: Gradient {
                    GradientStop { position: 0.2; color: "#88000000" }
                    GradientStop { position: 0.9; color: "transparent" }
                }
            }
        }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_small
            Layout.rightMargin: VLCStyle.margin_small

            Widgets.RoundImage {
                source: artist.cover || VLCStyle.noArtArtist
                height: VLCStyle.cover_xsmall
                width: VLCStyle.cover_xsmall
                radius: VLCStyle.cover_xsmall

                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: VLCStyle.cover_small
                Layout.preferredWidth: VLCStyle.cover_small
            }

            Text {
                id: artistName
                text: artist.name || i18n.qtr("No artist")

                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: VLCStyle.margin_small
                Layout.rightMargin: VLCStyle.margin_small

                font.pixelSize: VLCStyle.fontSize_xxlarge
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: VLCStyle.colors.text
            }

            Item {
                Layout.fillWidth: true
            }
        }

        Widgets.NavigableRow {
            id: actionButtons

            Layout.fillWidth: true

            focus: true
            navigationParent: root

            model: ObjectModel {
                Widgets.TabButtonExt {
                    id: playActionBtn
                    iconTxt: VLCIcons.play
                    text: i18n.qtr("Play all")
                    onClicked: medialib.addAndPlay( artist.id )
                }

                Widgets.TabButtonExt {
                    id: enqueueActionBtn
                    iconTxt: VLCIcons.add
                    text: i18n.qtr("Enqueue all")
                    onClicked: medialib.addToPlaylist( artist.id )
                }
            }
        }

    }

}
