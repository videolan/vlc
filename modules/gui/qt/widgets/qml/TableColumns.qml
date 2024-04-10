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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Item {
    id: root

    // Properties

    property bool showTitleText: true
    property bool showCriterias: false

    // NOTE: That's useful when we want to enforce a cover criteria for the titleDelegate.
    property string criteriaCover: "cover"

    property int titleCover_width: VLCStyle.trackListAlbumCover_width
    property int titleCover_height: VLCStyle.trackListAlbumCover_heigth
    property int titleCover_radius: VLCStyle.trackListAlbumCover_radius

    // function (model) -> [string...]
    // implement this function to show labels in title Cover
    property var titlecoverLabels: function(model) {
        return []
    }

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

            // NOTE: We can't use 'instanceof' because VLCTick is uncreatable.
            if (value.toString().indexOf("VLCTick(") === 0) {

                string += value.formatShort()
            } else if (criteria === "nb_tracks") {

                string += qsTr("%1 tracks").arg(value)
            } else {
                string += value
            }
        }

        return string
    }

    // Components

    property Component titleDelegate: RowLayout {
        id: titleDel

        property var rowModel: parent.rowModel
        property var model: parent.colModel

        readonly property bool containsMouse: parent.containsMouse
        readonly property bool currentlyFocused: parent.currentlyFocused
        readonly property ColorContext colorContext: parent.colorContext
        readonly property bool selected: parent.selected

        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        Widgets.MediaCover {
            id: cover

            Layout.preferredHeight: root.titleCover_height
            Layout.preferredWidth: root.titleCover_width

            source: titleDel.rowModel?.[root.criteriaCover] ?? ""

            fallbackImageSource: titleDel.model.placeHolder || VLCStyle.noArtAlbumCover

            playCoverVisible: (titleDel.currentlyFocused || titleDel.containsMouse)
            playIconSize: VLCStyle.play_cover_small
            onPlayIconClicked: {
                MediaLib.addAndPlay(titleDel.rowModel.id)
                History.push(["player"])
            }
            radius: root.titleCover_radius
            color: titleDel.colorContext.bg.secondary

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
                anchors.centerIn: parent

                sourceItem: parent
            }
        }

        Column {
            Layout.fillHeight: true
            Layout.fillWidth: true

            Layout.topMargin: VLCStyle.margin_xxsmall
            Layout.bottomMargin: VLCStyle.margin_xxsmall

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

                forceScroll: titleDel.currentlyFocused

                Widgets.ListLabel {
                    id: text

                    anchors.verticalCenter: parent.verticalCenter
                    text: (titleDel.rowModel && root.showTitleText)
                          ? (titleDel.rowModel[titleDel.model.criteria] || qsTr("Unknown Title"))
                          : ""

                    color: titleDel.selected
                        ? titleDel.colorContext.fg.highlight
                        : titleDel.colorContext.fg.primary

                }
            }

            Widgets.MenuCaption {
                anchors.left: parent.left
                anchors.right: parent.right

                height: textRect.height

                visible: root.showCriterias
                enabled: visible

                text: (visible) ? root.getCriterias(titleDel.model, titleDel.rowModel) : ""

                color: titleDel.selected
                    ? titleDel.colorContext.fg.highlight
                    : titleDel.colorContext.fg.secondary
            }
        }
    }

    property Component titleHeaderDelegate: Row {
        id: titleHeadDel
        property var model: parent.colModel
        readonly property ColorContext colorContext: parent.colorContext

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

        Widgets.CaptionLabel {
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            height: parent.height

            color: titleHeadDel.colorContext.fg.secondary

            text: titleHeadDel.model
                    ? titleHeadDel.model.text || ""
                    : ""
            visible: root.showTitleText

            Accessible.ignored: true
        }
    }

    property Component timeHeaderDelegate: Widgets.IconLabel {
        width: timeTextMetric.width
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: VLCIcons.time
        font.pixelSize: VLCStyle.icon_tableHeader
        color: parent.colorContext.fg.secondary
    }

    property Component timeColDelegate: Item {
        id: timeDel

        property var rowModel: parent.rowModel
        property var model: parent.colModel
        readonly property bool selected: parent.selected
        readonly property ColorContext colorContext: parent.colorContext

        Widgets.ListLabel {
            width: timeTextMetric.width
            height: parent.height
            horizontalAlignment: Text.AlignHCenter
            text: (!timeDel.rowModel || !timeDel.rowModel[timeDel.model.criteria])
                ? ""
                : timeDel.rowModel[timeDel.model.criteria].formatShort()
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

}
