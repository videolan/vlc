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
    id: expandRect

    property int currentId: -1
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

    Rectangle{
        id: contentRect

        anchors.fill: parent
        implicitHeight: contentLayout.implicitHeight + ( VLCStyle.margin_normal * 2 )
        color: VLCStyle.colors.expandDelegate

        Rectangle {
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            color: VLCStyle.colors.border
            height: VLCStyle.expandDelegate_border
        }

        Rectangle {
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }
            color: VLCStyle.colors.border
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

                            Widgets.TabButtonExt {
                                id: enqueueActionBtn

                                iconTxt: VLCIcons.enqueue
                                text: I18n.qtr("Enqueue")
                                onClicked: MediaLib.addToPlaylist( model.id )
                            }
                        }

                        Navigation.parentItem: expandRect
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

                        Layout.fillWidth: true
                    }

                    Widgets.IconLabel {
                        text: VLCIcons.close

                        MouseArea {
                            anchors.fill: parent
                            onClicked: expandRect.retract()
                        }
                    }
                }

                Widgets.CaptionLabel {
                    text: model.duration.formatHMS()
                    color: VLCStyle.colors.text
                    width: parent.width
                }

                Widgets.MenuCaption {
                    topPadding: VLCStyle.margin_normal
                    text: "<b>" + I18n.qtr("File Name:") + "</b> " + expandRect.model.fileName
                    width: parent.width
                    textFormat: Text.StyledText
                }

                Widgets.MenuCaption {
                    text: "<b>" + I18n.qtr("Path:") + "</b> " + expandRect.model.display_mrl
                    topPadding: VLCStyle.margin_xsmall
                    width: parent.width
                    textFormat: Text.StyledText
                }

                MouseArea {
                    width: childrenRect.width
                    height: childrenRect.height

                    onClicked: _showMoreInfo = !_showMoreInfo

                    Row {
                        topPadding: VLCStyle.margin_large
                        spacing: VLCStyle.margin_xsmall

                        Widgets.IconLabel {
                            text: VLCIcons.expand
                            rotation: _showMoreInfo ? -180 : 0
                            font.pixelSize: VLCStyle.icon_normal

                            Behavior on rotation {
                                NumberAnimation {
                                    duration: VLCStyle.duration_short
                                }
                            }
                        }

                        Widgets.CaptionLabel {
                            text: _showMoreInfo ? I18n.qtr("View Less") : I18n.qtr("View More")
                            color: VLCStyle.colors.text
                        }
                    }
                }

                Row {
                    width: parent.width

                    topPadding: VLCStyle.margin_normal

                    spacing: VLCStyle.margin_xlarge

                    visible: _showMoreInfo

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

                            opacity: (visible) ? 1.0 : 0.0

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: VLCStyle.duration_short
                                }
                            }

                            Widgets.MenuCaption {
                                text: modelData.title
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
            model: expandRect.model.videoDesc

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
                    bottomPadding: VLCStyle.margin_xsmall
                }
            }
        }
    }

    ObjectModel {
        id: audioDescModel

        // TODO: use inline Component for Qt > 5.14
        Repeater {
            model: expandRect.model.audioDesc

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
                }
            }
        }
    }
}
