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
import QtQml.Models 2.11
import QtQuick.Layouts 1.11

import org.videolan.medialib 0.1
import org.videolan.controls 0.1

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

FocusScope {
    id: root

    property var model : ({})
    property bool _showMoreInfo: false
    signal retract()

    implicitHeight: contentRect.implicitHeight

    // otherwise produces artefacts on retract animation
    clip: true

    focus: true

    function setCurrentItemFocus(reason) {
        playActionBtn.forceActiveFocus(reason);
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }


    Rectangle{
        id: contentRect

        anchors.fill: parent
        implicitHeight: contentLayout.implicitHeight + ( VLCStyle.margin_normal * 2 )
        color: theme.bg.secondary

        Rectangle {
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            color: theme.border
            height: VLCStyle.expandDelegate_border
        }

        Rectangle {
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }
            color: theme.border
            height: VLCStyle.expandDelegate_border
        }

        RowLayout {
            id: contentLayout

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal
            implicitHeight: artAndControl.implicitHeight
            spacing: VLCStyle.margin_normal

            FocusScope {
                id: artAndControl

                focus: true

                implicitHeight: artAndControlLayout.implicitHeight
                implicitWidth: artAndControlLayout.implicitWidth

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                Layout.alignment: Qt.AlignTop

                Column {
                    id: artAndControlLayout

                    spacing: VLCStyle.margin_normal

                    Item {
                        height: VLCStyle.gridCover_video_height
                        width: VLCStyle.gridCover_video_width

                        /* A bigger cover for the album */
                        RoundImage {
                            id: expand_cover_id

                            anchors.fill: parent
                            source: model.thumbnail || VLCStyle.noArtVideoCover
                            radius: VLCStyle.gridCover_radius
                        }

                        Widgets.ListCoverShadow {
                            anchors.fill: expand_cover_id
                        }
                    }

                    Widgets.NavigableRow {
                        id: actionButtons

                        focus: true
                        spacing: VLCStyle.margin_large

                        model: ObjectModel {
                            Widgets.ActionButtonPrimary {
                                id: playActionBtn

                                iconTxt: VLCIcons.play_outline
                                text: I18n.qtr("Play")
                                onClicked: MediaLib.addAndPlay( model.id )
                            }

                            Widgets.ButtonExt {
                                id: enqueueActionBtn

                                iconTxt: VLCIcons.enqueue
                                text: I18n.qtr("Enqueue")
                                onClicked: MediaLib.addToPlaylist( model.id )
                            }
                        }

                        Navigation.parentItem: root
                        Navigation.rightItem: showMoreButton
                    }
                }
            }

            Column {
                id: expand_infos_id

                spacing: 0
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop

                RowLayout {
                    width: parent.width

                    Widgets.SubtitleLabel {
                        text: model.title || I18n.qtr("Unknown title")
                        color: theme.fg.primary

                        Layout.fillWidth: true
                    }

                    Widgets.IconToolButton {
                        id: closeButton

                        iconText: VLCIcons.close

                        onClicked: root.retract()

                        Navigation.parentItem: root
                        Navigation.leftItem: showMoreButton
                    }
                }

                Widgets.CaptionLabel {
                    text: (model && model.duration) ? model.duration.formatHMS() : ""
                    color: theme.fg.primary
                    width: parent.width
                }

                Widgets.MenuCaption {
                    topPadding: VLCStyle.margin_normal
                    text: "<b>" + I18n.qtr("File Name:") + "</b> " + root.model.fileName
                    width: parent.width
                    color: theme.fg.secondary
                    textFormat: Text.StyledText
                }

                Widgets.MenuCaption {
                    text: "<b>" + I18n.qtr("Path:") + "</b> " + root.model.display_mrl
                    color: theme.fg.secondary
                    topPadding: VLCStyle.margin_xsmall
                    bottomPadding: VLCStyle.margin_large
                    width: parent.width
                    textFormat: Text.StyledText
                }

                Widgets.ButtonExt {
                    id: showMoreButton

                    text: root._showMoreInfo ? I18n.qtr("View Less") : I18n.qtr("View More")
                    iconTxt: VLCIcons.expand
                    iconRotation: root._showMoreInfo ? -180 : 0

                    Behavior on iconRotation {
                        NumberAnimation {
                            duration: VLCStyle.duration_short
                        }
                    }

                    onClicked: root._showMoreInfo = !root._showMoreInfo

                    Navigation.parentItem: root
                    Navigation.leftItem: enqueueActionBtn
                    Navigation.rightItem: closeButton
                }

                Row {
                    width: parent.width

                    topPadding: VLCStyle.margin_normal

                    spacing: VLCStyle.margin_xlarge

                    visible: root._showMoreInfo

                    opacity: visible ? 1.0 : 0.0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: VLCStyle.duration_long
                        }
                    }

                    Repeater {
                        model: [
                            {
                                "title": I18n.qtr("Video track"),
                                "model": videoDescModel
                            },
                            {
                                "title": I18n.qtr("Audio track"),
                                "model": audioDescModel
                            }
                        ]

                        delegate: Column {
                            visible: delgateRepeater.count > 0

                            Widgets.MenuCaption {
                                text: modelData.title
                                color: theme.fg.secondary
                                font.bold: true
                                bottomPadding: VLCStyle.margin_small
                            }

                            Repeater {
                                id: delgateRepeater

                                model: modelData.model
                            }
                        }
                    }
                }
            }
        }
    }

    ObjectModel {
        id: videoDescModel

        Repeater {
            model: root.model.videoDesc

            // TODO: use inline Component for Qt > 5.14
            delegate: Repeater {
                id: videoDescRepeaterDelegate

                readonly property bool isFirst: (index === 0)

                model: [
                    {text: I18n.qtr("Codec:"), data: modelData.codec },
                    {text: I18n.qtr("Language:"), data: modelData.language },
                    {text: I18n.qtr("FPS:"), data: modelData.fps }
                ]

                delegate: Widgets.MenuCaption {
                    topPadding: (!videoDescRepeaterDelegate.isFirst) && (index === 0) ? VLCStyle.margin_small : 0
                    text: modelData.text + " " + modelData.data
                    color: theme.fg.secondary
                    bottomPadding: VLCStyle.margin_xsmall
                }
            }
        }
    }

    ObjectModel {
        id: audioDescModel

        // TODO: use inline Component for Qt > 5.14
        Repeater {
            model: root.model.audioDesc

            delegate: Repeater {
                id: audioDescRepeaterDelegate

                readonly property bool isFirst: (index === 0)

                model: [
                    {text: I18n.qtr("Codec:"), data: modelData.codec },
                    {text: I18n.qtr("Language:"), data: modelData.language },
                    {text: I18n.qtr("Channel:"), data: modelData.nbchannels }
                ]

                delegate: Widgets.MenuCaption {
                    topPadding: (!audioDescRepeaterDelegate.isFirst) && (index === 0) ? VLCStyle.margin_small : 0
                    text: modelData.text + " " + modelData.data
                    bottomPadding: VLCStyle.margin_xsmall
                    color: theme.fg.secondary
                }
            }
        }
    }
}
