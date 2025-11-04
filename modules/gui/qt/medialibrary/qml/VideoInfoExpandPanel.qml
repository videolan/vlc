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
import QtQuick.Window
import QtQuick.Controls
import QtQml.Models
import QtQuick.Layouts

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

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
                        Widgets.ImageExt {
                            id: expand_cover_id

                            anchors.fill: parent
                            source: model.thumbnail || VLCStyle.noArtVideoCover
                            sourceSize: Qt.size(width * eDPR, height * eDPR)
                            radius: VLCStyle.gridCover_radius
                            backgroundColor: theme.bg.primary

                            readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                            Widgets.DefaultShadow {
                                visible: (parent.status === Image.Ready)
                            }
                        }
                    }

                    Widgets.NavigableRow {
                        id: actionButtons

                        focus: true
                        spacing: VLCStyle.margin_large

                        Widgets.ActionButtonPrimary {
                            id: playActionBtn

                            iconTxt: VLCIcons.play
                            text: qsTr("Play")
                            onClicked: MediaLib.addAndPlay( model.id )
                        }

                        Widgets.ButtonExt {
                            id: enqueueActionBtn

                            iconTxt: VLCIcons.enqueue
                            text: qsTr("Enqueue")
                            onClicked: MediaLib.addToPlaylist( model.id )
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
                        text: model.title || qsTr("Unknown title")
                        color: theme.fg.primary

                        Layout.fillWidth: true
                    }

                    Widgets.IconToolButton {
                        id: closeButton

                        text: VLCIcons.close

                        description: qsTr("Close")

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
                    text: "<b>" + qsTr("File Name:") + "</b> " + root.model.fileName
                    width: parent.width
                    color: theme.fg.secondary
                    textFormat: Text.StyledText
                }

                Widgets.MenuCaption {

                    readonly property string folderMRL: MainCtx.folderMRL(root.model?.mrl ?? "")

                    text: {
                        if (!!folderMRL)
                            return "<b>%1</b> <a href='%2'>%3</a>"
                                        .arg(qsTr("Folder:"))
                                        .arg(folderMRL)
                                        .arg(MainCtx.displayMRL(folderMRL))

                        return "<b>" + qsTr("Path:") + "</b> " + root.model.display_mrl
                    }

                    linkColor: theme.fg.link
                    color: theme.fg.secondary
                    topPadding: VLCStyle.margin_xsmall
                    bottomPadding: VLCStyle.margin_large
                    width: parent.width
                    textFormat: Text.StyledText

                    onLinkActivated: function (link) {
                        Qt.openUrlExternally(link)
                    }
                }

                Widgets.ButtonExt {
                    id: showMoreButton

                    text: root._showMoreInfo ? qsTr("View Less") : qsTr("View More")
                    iconTxt: VLCIcons.expand
                    iconRotation: root._showMoreInfo ? -180 : 0
                    visible: (root.model.audioDesc?.length > 0)
                             || (root.model.videoDesc?.length > 0)

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

                    DescriptionList {
                        title: qsTr("Video track")

                        sourceModel: root.model.videoDesc

                        delegateModel:  [
                            {text: qsTr("Codec:"), role: "codec" },
                            {text: qsTr("Language:"), role: "language" },
                            {text: qsTr("FPS:"), role: "fps" }
                        ]
                    }

                    DescriptionList {
                        title: qsTr("Audio track")

                        sourceModel: root.model.videoDesc

                        delegateModel:  [
                            {text: qsTr("Codec:"), role: "codec" },
                            {text: qsTr("Language:"), role: "language" },
                            {text: qsTr("Channel:"), role: "nbchannels" }
                        ]
                    }

                    DescriptionList {
                        title: qsTr("Subtitle track")

                        sourceModel: [{"text": root.model.subtitleDesc?.map(desc => desc.language)
                                                    .filter(l => !!l).join(", ")}]

                        delegateModel:  [
                            {text: qsTr("Language:"), role: "text" }
                        ]
                    }
                }
            }
        }
    }

    component DescriptionList : Column {
        id: column

        property alias title: titleTxt.text

        property alias sourceModel: sourceRepeater.model

        // [["caption": <str>, "role": <str>].,.]
        property var delegateModel

        Widgets.MenuCaption {
            id: titleTxt

            color: theme.fg.secondary
            font.bold: true
            bottomPadding: VLCStyle.margin_small
        }

        Repeater {
            id: sourceRepeater

            Repeater {
                id: delegateRepeater

                model: column.delegateModel

                required property var modelData
                required property int index
                readonly property bool isFirst: (index === 0)

                Widgets.MenuCaption {
                    required property var modelData
                    required property int index

                    property string description: delegateRepeater.modelData[modelData.role] ?? ""

                    visible: !!description
                    text: modelData.text + " " + description
                    topPadding: (!delegateRepeater.isFirst) && (index === 0) ? VLCStyle.margin_small : 0
                    color: theme.fg.secondary
                    bottomPadding: VLCStyle.margin_xsmall
                }
            }
        }
    }
}
