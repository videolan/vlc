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
import QtQml.Models 2.11

import org.videolan.medialib 0.1
import org.videolan.controls 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

FocusScope {
    id: root

    property var model

    property var headerFocusScope
    property var enqueueActionBtn
    property var playActionBtn

    property bool forcePlayActionBtnFocusOnce: false

    signal retract()

    implicitWidth: layout.implicitWidth

    implicitHeight: {
        var verticalMargins = layout.anchors.topMargin + layout.anchors.bottomMargin
        if (tracks.contentHeight < artAndControl.height)
            return artAndControl.height + verticalMargins
        return Math.min(tracks.contentHeight
                        , tracks.listView.headerItem.height + tracks.rowHeight * 6) // show a maximum of 6 rows
                + verticalMargins
    }

    // components should shrink with change of height, but it doesn't happen fast enough
    // causing expand and shrink animation bit laggy, so clip the delegate to fix it
    clip: true

    function setCurrentItemFocus(reason) {
        root.playActionBtn.forceActiveFocus(reason);
        if (VLCStyle.isScreenSmall)
            root.forcePlayActionBtnFocusOnce = true;
    }

    function _getStringTrack() {
        var count = Helpers.get(model, "nb_tracks", 0);

        if (count < 2)
            return I18n.qtr("%1 track").arg(count);
        else
            return I18n.qtr("%1 tracks").arg(count);
    }

    Rectangle {
        anchors.fill: parent
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
    }

    RowLayout {
        id: layout

        anchors.fill: parent
        anchors.leftMargin: VLCStyle.margin_normal
        anchors.topMargin: VLCStyle.margin_large
        anchors.rightMargin: VLCStyle.margin_small
        anchors.bottomMargin: VLCStyle.margin_xxsmall
        spacing: VLCStyle.margin_large

        Component {
            id: cover

            RoundImage {
                id: expand_cover_id

                property int cover_height: parent.cover_height
                property int cover_width: parent.cover_width

                height: cover_height
                width: cover_width
                radius: VLCStyle.expandCover_music_radius
                source: (root.model && root.model.cover && root.model.cover !== "")
                    ?  root.model.cover
                    : VLCStyle.noArtAlbumCover

                Widgets.ListCoverShadow {
                    anchors.fill: parent
                }
            }
        }

        Component {
            id: buttons

            Widgets.NavigableRow {
                id: actionButtons

                property alias enqueueActionBtn: _enqueueActionBtn
                property alias playActionBtn: _playActionBtn

                focus: true
                width: expand_cover_id.width
                spacing: VLCStyle.margin_small

                Layout.alignment: Qt.AlignCenter

                model: ObjectModel {
                    Widgets.ActionButtonPrimary {
                        id: _playActionBtn

                        iconTxt: VLCIcons.play_outline
                        text: I18n.qtr("Play")
                        onClicked: MediaLib.addAndPlay( root.model.id )

                        onActiveFocusChanged: {
                            // root.setCurrentItemFocus sets active focus to playActionBtn, but it gets stolen
                            // by the delegate of the first track at initial load when playActionBtn is in the
                            // header of tracks
                            if (VLCStyle.isScreenSmall && root.forcePlayActionBtnFocusOnce) {
                                root.forcePlayActionBtnFocusOnce = false
                                root.playActionBtn.forceActiveFocus(Qt.TabFocusReason)
                            }
                        }
                    }

                    Widgets.ButtonExt {
                        id: _enqueueActionBtn

                        iconTxt: VLCIcons.enqueue
                        text: I18n.qtr("Enqueue")
                        onClicked: MediaLib.addToPlaylist( root.model.id )
                    }
                }

                Navigation.parentItem: root
                Navigation.rightItem: VLCStyle.isScreenSmall ? root.headerFocusScope : tracks
                Navigation.upItem: VLCStyle.isScreenSmall ? root.headerFocusScope : null

                Navigation.downAction: function () {
                    if (!VLCStyle.isScreenSmall)
                        return

                    if (tracks.count > 0) {
                        tracks.setCurrentItemFocus(Qt.TabFocusReason)
                    } else {
                        root.Navigation.downAction()
                    }
                }
            }
        }

        Component {
            id: header_common

            FocusScope {
                id: headerFocusScope

                property int bottomPadding: parent.bottomPadding

                width: parent.width
                height: implicitHeight
                implicitHeight: col.implicitHeight

                focus: true

                Navigation.parentItem: root
                Navigation.leftItem: root.enqueueActionBtn
                Navigation.downAction: function () {
                    if (VLCStyle.isScreenSmall) {
                        root.enqueueActionBtn.forceActiveFocus(Qt.TabFocusReason);
                        return
                    }

                    if (tracks.count > 0) {
                        tracks.setCurrentItemFocus(Qt.TabFocusReason)
                    } else {
                        root.Navigation.downAction()
                    }
                }

                Column {
                    id: col

                    anchors.fill: parent
                    bottomPadding: headerFocusScope.bottomPadding

                    RowLayout {
                        width: parent.width

                        /* The title of the albums */
                        Widgets.SubtitleLabel {
                            id: expand_infos_title_id

                            text: Helpers.get(root.model, "title", I18n.qtr("Unknown title"))

                            Layout.fillWidth: true
                        }

                        Widgets.IconControlButton {
                            iconText: VLCIcons.close
                            focus: true

                            Navigation.parentItem: headerFocusScope
                            Layout.rightMargin: VLCStyle.margin_small

                            onClicked: root.retract()
                        }
                    }

                    Widgets.CaptionLabel {
                        id: expand_infos_subtitle_id

                        width: parent.width
                        text: I18n.qtr("%1 - %2 - %3 - %4")
                            .arg(Helpers.get(root.model, "main_artist", I18n.qtr("Unknown artist")))
                            .arg(Helpers.get(root.model, "release_year", ""))
                            .arg(_getStringTrack())
                            .arg((root.model && root.model.duration) ? root.model.duration.formatHMS() : 0)
                    }
                }
            }
        }

        Component {
            id: header_small

            RowLayout {
                id: row

                width: parent.width
                height: implicitHeight
                implicitHeight: col.implicitHeight

                Loader {
                    sourceComponent: cover
                    property int cover_height: VLCStyle.cover_small
                    property int cover_width: VLCStyle.cover_small

                    Layout.bottomMargin: VLCStyle.margin_large
                    Layout.rightMargin: VLCStyle.margin_xxsmall
                }

                Column {
                    id: col

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.bottomMargin: VLCStyle.margin_large

                    Loader {
                        sourceComponent: header_common
                        width: parent.width
                        property int bottomPadding: VLCStyle.margin_xsmall

                        onLoaded: {
                            root.headerFocusScope = item
                        }
                    }

                    Loader {
                        sourceComponent: buttons

                        onLoaded: {
                            root.enqueueActionBtn = item.enqueueActionBtn
                            root.playActionBtn = item.playActionBtn
                        }
                    }
                }
            }
        }

        FocusScope {
            id: artAndControl

            visible: !VLCStyle.isScreenSmall
            focus: !VLCStyle.isScreenSmall

            implicitHeight: artAndControlLayout.implicitHeight
            implicitWidth: artAndControlLayout.implicitWidth
            Layout.alignment: Qt.AlignTop

            Column {
                id: artAndControlLayout

                spacing: VLCStyle.margin_normal
                bottomPadding: VLCStyle.margin_large

                /* A bigger cover for the album */
                Loader {
                    sourceComponent: !VLCStyle.isScreenSmall ? cover : null
                    property int cover_height: VLCStyle.expandCover_music_height
                    property int cover_width: VLCStyle.expandCover_music_width
                }

                Loader {
                    sourceComponent: !VLCStyle.isScreenSmall ? buttons : null

                    onLoaded: {
                        root.playActionBtn = item.playActionBtn
                        root.enqueueActionBtn = item.enqueueActionBtn
                    }
                }
            }
        }

        /* The list of the tracks available */
        MusicTrackListDisplay {
            id: tracks

            backgroundColor: VLCStyle.colors.expandDelegate

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(tracks.availableRowWidth)

            property Component titleDelegate: RowLayout {
                property var rowModel: parent.rowModel

                anchors.fill: parent

                Widgets.ListLabel {
                    text: !!rowModel && !!rowModel.track_number ? rowModel.track_number : ""
                    color: foregroundColor
                    font.weight: Font.Normal

                    Layout.fillHeight: true
                    Layout.leftMargin: VLCStyle.margin_xxsmall
                    Layout.preferredWidth: VLCStyle.margin_large
                }

                Widgets.ListLabel {
                    text: !!rowModel && !!rowModel.title ? rowModel.title : ""
                    color: foregroundColor

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
            }

            property Component titleHeaderDelegate: Row {

                Widgets.CaptionLabel {
                    text: "#"
                    width: VLCStyle.margin_large
                }

                Widgets.CaptionLabel {
                    text: I18n.qtr("Title")
                }
            }

            header: Loader {
                sourceComponent: VLCStyle.isScreenSmall
                                 ? header_small
                                 : header_common
                width: tracks.width

                property int bottomPadding: VLCStyle.margin_large //used only by header_common
            }

            clip: true // content may overflow if not enough space is provided
            headerPositioning: ListView.InlineHeader
            section.property: ""

            Layout.fillWidth: true
            Layout.fillHeight: true

            rowHeight: VLCStyle.tableRow_height
            headerColor: VLCStyle.colors.expandDelegate

            parentId: Helpers.get(root.model, "id")
            onParentIdChanged: {
                currentIndex = 0
            }

            sortModel: [{
                size: Math.max(tracks._nbCols - 1, 1),

                model: {
                    criteria: "title",

                    visible: true,

                    text: I18n.qtr("Title"),

                    showSection: "",

                    colDelegate: titleDelegate,
                    headerDelegate: titleHeaderDelegate
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    visible: true,

                    showSection: "",

                    colDelegate: tableColumns.timeColDelegate,
                    headerDelegate: tableColumns.timeHeaderDelegate
                }
            }]

            Navigation.parentItem: root
            Navigation.leftItem: VLCStyle.isScreenSmall ? null : root.enqueueActionBtn
            Navigation.upItem: headerItem

            Widgets.TableColumns {
                id: tableColumns
            }
        }
    }


    Keys.priority:  Keys.AfterItem
    Keys.onPressed:  root.Navigation.defaultKeyAction(event)
}
