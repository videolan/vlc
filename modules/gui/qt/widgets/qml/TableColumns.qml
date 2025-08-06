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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

Item {
    id: root

    // Properties

    property bool showTitleText: true
    property bool showCriterias: false

    // NOTE: That's useful when we want to enforce a cover criteria for the titleDelegate.
    property string criteriaCover: "cover"

    property int fillMode: Image.PreserveAspectFit

    property int titleCover_width: VLCStyle.trackListAlbumCover_width
    property int titleCover_height: VLCStyle.trackListAlbumCover_heigth
    property int titleCover_radius: VLCStyle.trackListAlbumCover_radius

    // function (model) -> [string...]
    // implement this function to show labels in title Cover
    property var titlecoverLabels: function(model) {
        return []
    }

    // function (rowModel, colModel) -> string
    // implement this function for providing custom text for title column
    property var _titleTextProvider: function (rowModel, colModel) {
        const text = rowModel?.[colModel.criteria]
        return text || qsTr("Unknown Title")
    }

    // this is called in reponse to user request to play
    // model is associated row data of delegate
    signal playClicked(var model)

    function getCriterias(colModel, rowModel) {
        if (colModel === null || rowModel === null)
            return ""

        const criterias = colModel.subCriterias

        if (criterias === undefined || criterias.length === 0)
            return ""

        let string = ""

        for (let i = 0; i < criterias.length; i++) {
            if (i) string += " • "

            const criteria = criterias[i]

            const value = rowModel[criteria]

            if (value.formatShort) {

                string += value.formatShort()
            } else if (criteria === "nb_tracks") {

                string += qsTr("%1 tracks").arg(value)
            } else {
                string += value
            }
        }

        return string
    }

    property Component titleDelegate: TableRowDelegate {
        id: titleDel

        property alias artworkTextureProvider: cover.textureProvider

        RowLayout {
            anchors.fill: parent
            spacing: VLCStyle.margin_normal

            Widgets.MediaCover {
                id: cover

                Layout.preferredHeight: root.titleCover_height
                Layout.preferredWidth: root.titleCover_width
                
                pictureWidth: width
                pictureHeight: height

                source: titleDel.rowModel?.[root.criteriaCover] ?? ""

                fallbackImageSource: titleDel.colModel.placeHolder || VLCStyle.noArtAlbumCover

                fillMode: root.fillMode

                playCoverVisible: (titleDel.currentlyFocused || titleDel.containsMouse)
                playIconSize: VLCStyle.play_cover_small
                radius: root.titleCover_radius
                color: titleDel.colorContext.bg.secondary

                Component.onCompleted: {
                    console.assert(titleDel.delegate)
                    titleDel.delegate.ListView.reused.connect(cover.reinitialize)
                    titleDel.delegate.ListView.pooled.connect(cover.releaseResources)
                }

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

                DefaultShadow {

                }

                onPlayIconClicked: root.playClicked(titleDel.rowModel)
            }

            TitleText {
                Layout.fillHeight: true
                Layout.fillWidth: true

                Layout.topMargin: VLCStyle.margin_xxsmall
                Layout.bottomMargin: VLCStyle.margin_xxsmall

                source: titleDel
            }
        }
    }

    property Component titleHeaderDelegate: TableHeaderDelegate {
        id: titleHeadDel
        Row {
            anchors.fill: parent

            spacing: VLCStyle.margin_normal

            Widgets.IconLabel {
                width: root.titleCover_width
                height: parent.height
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: VLCStyle.icon_tableHeader

                text: VLCIcons.album_cover
                color: titleHeadDel.colorContext.fg.secondary
            }

            TitleHeaderText {
                height: parent.height

                color: titleHeadDel.colorContext.fg.secondary

                text: titleHeadDel.colModel.text ?? ""
            }
        }
    }

    property Component timeHeaderDelegate: TableHeaderDelegate {
        Widgets.IconLabel {
            width: timeTextMetric.width
            height: parent.height

            anchors.centerIn: parent

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            text: VLCIcons.time
            font.pixelSize: VLCStyle.icon_tableHeader
            color: parent.colorContext.fg.secondary
        }
    }

    property Component titleTextDelegate: TableRowDelegate {
        id: titleTextdel

        TitleText {
            anchors.fill: parent

            source: titleTextdel
        }
    }

    property Component titleTextHeaderDelegate: TableHeaderDelegate {
        id: titleHeaderTextDel

        TitleHeaderText {
            height: parent.height

            color: titleHeaderTextDel.colorContext.fg.secondary

            text: titleHeaderTextDel.colModel.text ?? ""
        }
    }

    property Component timeColDelegate: TableRowDelegate {
        id: timeDel

        Widgets.ListLabel {
            width: timeTextMetric.width
            height: parent.height

            anchors.centerIn: parent

            horizontalAlignment: Text.AlignHCenter
            text: timeDel.rowModel?.[timeDel.colModel.criteria]?.formatShort() ?? ""
            color: timeDel.selected
                ? timeDel.colorContext.fg.highlight
                : timeDel.colorContext.fg.primary
        }
    }

    // Children

    TextMetrics {
        id: timeTextMetric

        font.pixelSize: VLCStyle.fontSize_normal
        text: "000h00"
    }

    component TitleHeaderText: Widgets.CaptionLabel {
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        visible: root.showTitleText

        Accessible.ignored: true
    }

    component TitleText: Column {
        required property var source

        Widgets.TextAutoScroller {
            id: textRect

            anchors.left: parent.left
            anchors.right: parent.right

            height: (root.showCriterias) ? Math.round(parent.height / 2)
                                         : parent.height

            visible: root.showTitleText
            enabled: visible

            clip: scrolling

            label: text

            forceScroll: source.currentlyFocused

            Widgets.ListLabel {
                id: text

                anchors.verticalCenter: parent.verticalCenter
                text: (source.rowModel && root.showTitleText)
                      ? root._titleTextProvider(source.rowModel, source.colModel)
                      : ""

                color: source.selected
                    ? source.colorContext.fg.highlight
                    : source.colorContext.fg.primary

            }
        }

        Widgets.MenuCaption {
            anchors.left: parent.left
            anchors.right: parent.right

            height: textRect.height

            visible: root.showCriterias
            enabled: visible

            text: (visible) ? root.getCriterias(source.colModel, source.rowModel) : ""

            color: source.selected
                ? source.colorContext.fg.highlight
                : source.colorContext.fg.secondary
        }
    }

}
