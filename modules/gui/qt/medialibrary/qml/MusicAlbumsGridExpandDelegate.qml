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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    property variant model: MLAlbumModel{}
    implicitHeight: layout.implicitHeight
    implicitWidth: layout.implicitWidth

    clip: true

    property int currentItemY
    property int currentItemHeight

    RowLayout {
        id: layout

        anchors.fill: parent
        implicitHeight: Math.max(expand_infos_id.height, artAndControl.height)

        spacing: VLCStyle.margin_xsmall

        FocusScope {
            id: artAndControl

            Layout.preferredHeight: artAndControlLayout.implicitHeight
            Layout.preferredWidth: artAndControlLayout.implicitWidth
            Layout.alignment: Qt.AlignTop

            focus: true

            Column {
                id: artAndControlLayout

                anchors.margins: VLCStyle.margin_small

                spacing: VLCStyle.margin_small

                /* A bigger cover for the album */
                Image {
                    id: expand_cover_id
                    height: VLCStyle.cover_large
                    width: VLCStyle.cover_large
                    source: model.cover || VLCStyle.noArtAlbum
                    sourceSize: Qt.size(width, height)
                }

                Widgets.NavigableCol {
                    id: actionButtons

                    focus: true

                    width: expand_cover_id.width

                    Layout.alignment: Qt.AlignCenter

                    model: ObjectModel {
                        Widgets.TabButtonExt {
                            id: playActionBtn

                            width: actionButtons.width

                            iconTxt: VLCIcons.play
                            text: i18n.qtr("Play album")
                            onClicked: medialib.addAndPlay( model.id )
                        }

                        Widgets.TabButtonExt {
                            id: enqueueActionBtn

                            width: actionButtons.width

                            iconTxt: VLCIcons.add
                            text: i18n.qtr("Enqueue album")
                            onClicked: medialib.addToPlaylist( model.id )
                        }
                    }

                    navigationParent: root
                    navigationRightItem: expand_track_id
                }

            }
        }


        ColumnLayout {
            id: expand_infos_id

            Layout.fillWidth: true
            Layout.fillHeight: true

            spacing: VLCStyle.margin_xsmall

            /* The title of the albums */
            Text {
                id: expand_infos_title_id

                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight

                text: "<b>"+(model.title || i18n.qtr("Unknown title") )+"</b>"
                font.pixelSize: VLCStyle.fontSize_xxlarge
                color: VLCStyle.colors.text
            }

            Text {
                id: expand_infos_subtitle_id

                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight

                text: i18n.qtr("By %1 - %2 - %3")
                    .arg(model.main_artist || i18n.qtr("Unknown title"))
                    .arg(model.release_year || "")
                    .arg(model.duration || "")
                font.pixelSize: VLCStyle.fontSize_large
                color: VLCStyle.colors.text
            }

            /* The list of the tracks available */
            MusicTrackListDisplay {
                id: expand_track_id

                readonly property int _nbCols: VLCStyle.gridColumnsForWidth(expand_track_id.availableRowWidth)

                section.property: ""

                Layout.fillWidth: true
                Layout.fillHeight: true

                headerColor: VLCStyle.colors.bgAlt

                parentId : root.model.id
                onParentIdChanged: {
                    currentIndex = 0
                }

                onCurrentItemChanged: {
                    if (currentItem != undefined) {
                        root.currentItemHeight = currentItem.height
                    }
                }

                sortModel: [
                    { criteria: "track_number",           width: VLCStyle.colWidth(1), visible: true, text: i18n.qtr("#"), showSection: "" },
                    { isPrimary: true, criteria: "title", width: VLCStyle.colWidth(Math.max(expand_track_id._nbCols - 2, 1)), visible: true, text: i18n.qtr("Title"), showSection: "" },
                    { criteria: "durationShort",          width: VLCStyle.colWidth(1), visible: true, showSection: "", colDelegate: tableColumns.timeColDelegate, headerDelegate: tableColumns.timeHeaderDelegate },
                ]

                navigationParent: root
                navigationLeftItem: actionButtons

                Widgets.TableColumns {
                    id: tableColumns
                }
            }
        }
    }


    Keys.priority:  KeyNavigation.AfterItem
    Keys.onPressed:  defaultKeyAction(event, 0)
}
