import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {

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
            anchors.leftMargin: VLCStyle.applicationHorizontalMargin
            anchors.rightMargin: VLCStyle.applicationHorizontalMargin
            anchors.bottomMargin: VLCStyle.applicationVerticalMargin

            Widgets.FocusBackground {
                id: playingItemInfo
                Layout.fillHeight: true
                Layout.preferredWidth: playingItemInfoRow.implicitWidth
                width: childrenRect.width
                focus: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: history.push(["player"])
                }

                Keys.onPressed: {
                    if (KeyHelper.matchOk(event) ) {
                        event.accepted = true
                    }
                }
                Keys.onReleased: {
                    if (!event.accepted && KeyHelper.matchOk(event))
                        history.push(["player"])
                }

                Row {
                    id: playingItemInfoRow
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
                            color: VLCStyle.colors.textInactive
                        }
                    }
                }

                KeyNavigation.right: buttonrow
            }

            Item {
                Layout.fillWidth: true
            }

            PlayerButtonsLayout {
                id: buttonrow

                model: miniPlayerModel
                defaultSize: VLCStyle.icon_normal

                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: VLCStyle.margin_normal
                Layout.preferredWidth: buttonrow.implicitWidth
                Layout.preferredHeight: buttonrow.implicitHeight

                navigationParent: root
                navigationLeftItem: playingItemInfo
            }
        }

        Connections{
            target: mainInterface
            onToolBarConfUpdated: {
                miniPlayerModel.reloadModel()
            }
        }

        PlayerControlBarModel {
            id: miniPlayerModel
            mainCtx: mainctx
            configName: "MiniPlayerToolbar"
        }

        ControlButtons {
            id: controlmodelbuttons
        }

        Keys.onPressed: {
            if (!event.accepted)
                defaultKeyAction(event, 0)
            if (!event.accepted)
                mainInterface.sendHotkey(event.key, event.modifiers);
        }

    }
}
