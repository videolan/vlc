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
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Item {
    id: root

    property bool showTitleText: true
    property int titleCover_width: VLCStyle.trackListAlbumCover_width
    property int titleCover_height: VLCStyle.trackListAlbumCover_heigth
    property int titleCover_radius: VLCStyle.trackListAlbumCover_radius

    function titlecoverLabels(model) {
        // implement this function to show labels in title Cover
        return []
    }

    property Component titleDelegate: RowLayout {
        id: titleDel

        property var rowModel: parent.rowModel
        property var model: parent.colModel
        readonly property bool containsMouse: parent.containsMouse
        readonly property bool currentlyFocused: parent.currentlyFocused
        readonly property color foregroundColor: parent.foregroundColor

        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        Item {
            Layout.preferredHeight: root.titleCover_height
            Layout.preferredWidth: root.titleCover_width

            ListCoverShadow {
                anchors.fill: cover
            }

            Widgets.MediaCover {
                id: cover

                anchors.fill: parent

                source: {
                    var cover = null
                    if (!!titleDel.rowModel) {
                        if (root.showTitleText)
                            cover = titleDel.rowModel.cover
                        else
                            cover = titleDel.rowModel[titleDel.model.criteria]
                    }
                    return cover || titleDel.model.placeHolder || VLCStyle.noArtAlbumCover
                }
                playCoverVisible: (titleDel.currentlyFocused || titleDel.containsMouse)
                playIconSize: VLCStyle.play_cover_small
                onPlayIconClicked: g_mainDisplay.play(MediaLib, titleDel.rowModel.id)
                radius: root.titleCover_radius

                imageOverlay: Item {
                    width: cover.width
                    height: cover.height

                    Widgets.VideoQualityLabels {
                        anchors {
                            top: parent.top
                            right: parent.right
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxsmall
                            rightMargin: VLCStyle.margin_xxsmall
                        }

                        labels: root.titlecoverLabels(titleDel.rowModel)
                    }
                }
            }
        }

        Widgets.ScrollingText {
            id: textRect

            label: text
            forceScroll: parent.currentlyFocused
            clip: scrolling
            visible: root.showTitleText

            Layout.fillHeight: true
            Layout.fillWidth: true

            Widgets.ListLabel {
                id: text

                anchors.verticalCenter: parent.verticalCenter
                text: (titleDel.rowModel && root.showTitleText)
                      ? (titleDel.rowModel[titleDel.model.criteria] || I18n.qtr("Unknown Title"))
                      : ""
                color: titleDel.foregroundColor
            }
        }
    }

    property Component titleHeaderDelegate: Row {
        id: titleHeadDel
        property var model: parent.colModel

        spacing: VLCStyle.margin_normal

        Widgets.IconLabel {
            width: root.titleCover_width
            horizontalAlignment: Text.AlignHCenter
            text: VLCIcons.album_cover
            font.pixelSize: VLCStyle.icon_tableHeader
            color: VLCStyle.colors.caption
        }

        Widgets.CaptionLabel {
            text: titleHeadDel.model
                    ? titleHeadDel.model.text || ""
                    : ""
            visible: root.showTitleText
        }
    }

    property Component timeHeaderDelegate: Widgets.IconLabel {
        width: timeTextMetric.width
        horizontalAlignment: Text.AlignHCenter
        text: VLCIcons.time
        font.pixelSize: VLCStyle.icon_tableHeader
        color: VLCStyle.colors.caption
    }

    property Component timeColDelegate: Item {
        id: timeDel

        property var rowModel: parent.rowModel
        property var model: parent.colModel
        property color foregroundColor: parent.foregroundColor

        Widgets.ListLabel {
            width: timeTextMetric.width
            height: parent.height
            horizontalAlignment: Text.AlignHCenter
            text: (!timeDel.rowModel || !timeDel.rowModel[timeDel.model.criteria])
                ? ""
                : Helpers.msToString(timeDel.rowModel[timeDel.model.criteria], true)
            color: timeDel.foregroundColor
        }
    }

    TextMetrics {
        id: timeTextMetric

        font.pixelSize: VLCStyle.fontSize_normal
        text: "00h00"
    }

}
