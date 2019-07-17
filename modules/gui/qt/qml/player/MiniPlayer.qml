import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {

    id: root

    Layout.fillWidth: true

    readonly property bool expanded: root.implicitHeight === root.childrenRect.height

    Component.onCompleted : {
        if (player.playingState === PlayerController.PLAYING_STATE_STOPPED)
            root.implicitHeight = 0;
        else
            root.implicitHeight = root.childrenRect.height;
    }

    Connections {
        target: player
        onPlayingStateChanged: {
            if (player.playingState === PlayerController.PLAYING_STATE_STOPPED)
                animateRetract.start()
            else if (player.playingState === PlayerController.PLAYING_STATE_PLAYING)
                animateExpand.start()
        }
    }

    PropertyAnimation {
        id: animateExpand;
        target: root;
        properties: "implicitHeight"
        duration: 250
        to: root.childrenRect.height
    }

    PropertyAnimation {
        id: animateRetract;
        target: root;
        properties: "implicitHeight"
        duration: 250
        to: 0
    }

    Rectangle {

        anchors.left: parent.left
        anchors.right: parent.right

        height: VLCStyle.miniPlayerHeight
        color: VLCStyle.colors.banner

        RowLayout {
            anchors.fill: parent

            Item {
                id: playingItemInfo
                Layout.fillHeight: true
                width: childrenRect.width

                Rectangle {
                    anchors.fill: parent
                    visible: parent.activeFocus
                    color: VLCStyle.colors.accent
                    border.width: 0
                    border.color: VLCStyle.colors.accent
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: history.push(["player"], History.Go)
                }

                Keys.onReleased: {
                    if (!event.accepted && (event.key === Qt.Key_Return || event.key === Qt.Key_Space))
                        history.push(["player"], History.Go)
                }

                Row {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    rightPadding: VLCStyle.margin_normal

                    Image {
                        id: cover
                        source: (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                                ? mainPlaylistController.currentItem.artwork
                                : VLCStyle.noArtAlbum
                        fillMode: Image.PreserveAspectFit

                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        leftPadding: VLCStyle.margin_normal

                        Text {
                            id: titleLabel
                            text: mainPlaylistController.currentItem.title
                            font.pixelSize: VLCStyle.fontSize_large
                            color: VLCStyle.colors.text
                        }

                        Text {
                            id: artistLabel
                            text: mainPlaylistController.currentItem.artist
                            font.pixelSize: VLCStyle.fontSize_normal
                            color: VLCStyle.colors.lightText
                        }
                    }
                }

                KeyNavigation.right: randomBtn
            }

            Item {
                Layout.fillWidth: true
            }

            Row {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                rightPadding: VLCStyle.margin_normal

                Utils.IconToolButton {
                    id: randomBtn
                    size: VLCStyle.icon_normal
                    checked: mainPlaylistController.random
                    text: VLCIcons.shuffle_on
                    onClicked: mainPlaylistController.toggleRandom()
                    KeyNavigation.right: prevBtn
                }

                Utils.IconToolButton {
                    id: prevBtn
                    size: VLCStyle.icon_normal
                    text: VLCIcons.previous
                    onClicked: mainPlaylistController.prev()
                    KeyNavigation.right: playBtn
                }

                Utils.IconToolButton {
                    id: playBtn
                    size: VLCStyle.icon_normal
                    text: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
                           && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                                 ? VLCIcons.pause
                                 : VLCIcons.play
                    onClicked: mainPlaylistController.togglePlayPause()
                    focus: true
                    KeyNavigation.right: nextBtn
                }

                Utils.IconToolButton {
                    id: nextBtn
                    size: VLCStyle.icon_normal
                    text: VLCIcons.next
                    onClicked: mainPlaylistController.next()
                    KeyNavigation.right: repeatBtn
                }

                Utils.IconToolButton {
                    id: repeatBtn
                    size: VLCStyle.icon_normal
                    checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
                    text: (mainPlaylistController.repeatMode === PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                                 ? VLCIcons.repeat_one
                                 : VLCIcons.repeat_all
                    onClicked: mainPlaylistController.toggleRepeatMode()
                }
            }
        }
    }

    Keys.onPressed: {
        if (!event.accepted)
            defaultKeyAction(event, 0)
    }
}
