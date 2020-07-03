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
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: expandRect

    property int currentId: -1
    property var model : ({})
    property bool _showMoreInfo: false
    signal retract()

    implicitHeight: contentRect.implicitHeight

    Rectangle{
        id: contentRect

        implicitHeight: contentLayout.implicitHeight + ( VLCStyle.margin_normal * 2 )
        width: parent.width
        clip: true
        color: VLCStyle.colors.bgAlt

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

                    /* A bigger cover for the album */
                    Image {
                        id: expand_cover_id

                        height: VLCStyle.gridCover_video_height
                        width: VLCStyle.gridCover_video_width
                        source: model.thumbnail || VLCStyle.noArtCover
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }

                    Widgets.NavigableRow {
                        id: actionButtons

                        focus: true
                        spacing: VLCStyle.margin_large

                        model: ObjectModel {
                            Widgets.TabButtonExt {
                                id: playActionBtn

                                iconTxt: VLCIcons.play_outline
                                text: i18n.qtr("Play")
                                onClicked: medialib.addAndPlay( model.id )
                            }

                            Widgets.TabButtonExt {
                                id: enqueueActionBtn

                                iconTxt: VLCIcons.enqueue
                                text: i18n.qtr("Enqueue")
                                onClicked: medialib.addToPlaylist( model.id )
                            }
                        }

                        navigationParent: expandRect
                        navigationRightItem: infoPannelScrollView
                    }
                }
            }


            Column {
                id: expand_infos_id

                spacing: 0
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop

                Widgets.SubtitleLabel {
                    text: model.title || i18n.qtr("Unknown title")
                    width: parent.width
                }

                Widgets.CaptionLabel {
                    text: model.duration
                    color: VLCStyle.colors.text
                    topPadding: VLCStyle.margin_xxsmall
                    width: parent.width
                }

                Row {
                    width: parent.width
                    topPadding: VLCStyle.margin_normal
                    spacing: VLCStyle.margin_xlarge

                    Column {
                        width: audioTrackInfo.visible ? expand_infos_id.width / 2 : expand_infos_id.width

                        Widgets.MenuCaption {
                            text: "<b>" + i18n.qtr("File Name:") + "</b> " + expandRect.model.title
                            width: parent.width
                        }

                        Widgets.MenuCaption {
                            text: "<b>" + i18n.qtr("Path:") + "</b> " + expandRect.model.display_mrl
                            topPadding: VLCStyle.margin_xsmall
                            width: parent.width
                        }

                        MouseArea {
                            width: childrenRect.width
                            height: childrenRect.height

                            onClicked: _showMoreInfo = !_showMoreInfo

                            Row {
                                topPadding: VLCStyle.margin_large
                                spacing: VLCStyle.margin_xsmall

                                Widgets.IconLabel {
                                    text: VLCIcons.back
                                    rotation: _showMoreInfo ? 270 : 90

                                    Behavior on rotation {
                                        NumberAnimation {
                                            duration: 100
                                        }
                                    }
                                }

                                Widgets.CaptionLabel {
                                    text: _showMoreInfo ? i18n.qtr("View Less") : i18n.qtr("View More")
                                    color: VLCStyle.colors.text
                                }
                            }
                        }

                        Column {
                            topPadding: VLCStyle.margin_xxlarge
                            visible: _showMoreInfo && expandRect.model.videoDesc.length > 0
                            opacity: visible ? 1 : 0

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 100
                                }
                            }

                            Widgets.MenuCaption {
                                text: i18n.qtr("Video track:")
                                font.bold: true
                                bottomPadding: VLCStyle.margin_small
                            }

                            Repeater {
                                model: expandRect.model.videoDesc

                                delegate: Repeater {
                                    model: [
                                        {text: i18n.qtr("Codec:"), data: modelData.codec },
                                        {text: i18n.qtr("Language:"), data: modelData.language },
                                        {text: i18n.qtr("FPS:"), data: modelData.fps }
                                    ]

                                    delegate: Widgets.MenuCaption {
                                        text: modelData.text + " " + modelData.data
                                        bottomPadding: VLCStyle.margin_xsmall
                                    }

                                }
                            }
                        }
                    }

                    Column {
                        id: audioTrackInfo

                        visible: _showMoreInfo && expandRect.model.audioDesc.length > 0
                        opacity: visible ? 1 : 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 100
                            }
                        }

                        Widgets.MenuCaption {
                            text: i18n.qtr("Audio track:")
                            font.bold: true
                            bottomPadding: VLCStyle.margin_small
                        }

                        Repeater {
                            model: expandRect.model.audioDesc

                            delegate: Repeater {
                                model: [
                                    {text: i18n.qtr("Codec:"), data: modelData.codec },
                                    {text: i18n.qtr("Language:"), data: modelData.language },
                                    {text: i18n.qtr("Channel:"), data: modelData.nbchannels }
                                ]

                                delegate: Widgets.MenuCaption {
                                    text: modelData.text + " " + modelData.data
                                    bottomPadding: VLCStyle.margin_xsmall
                                }
                            }
                        }

                    }
                }
            }

            Widgets.IconLabel {
                text: VLCIcons.close
                color: VLCStyle.colors.caption

                Layout.alignment: Qt.AlignTop

                MouseArea {
                    anchors.fill: parent
                    onClicked: expandRect.retract()
                }
            }
        }
    }
}
