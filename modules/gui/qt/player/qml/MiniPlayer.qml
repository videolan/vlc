import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    readonly property bool expanded: root.implicitHeight === VLCStyle.miniPlayerHeight

    property var mainContent: undefined

    Component.onCompleted: {
        if (player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
            root.implicitHeight = Qt.binding(function() { return VLCStyle.miniPlayerHeight; })
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
        duration: 200
        easing.type: Easing.InSine
        to: VLCStyle.miniPlayerHeight
        onStopped: {
            root.implicitHeight = Qt.binding(function() { return VLCStyle.miniPlayerHeight; })
        }
    }

    PropertyAnimation {
        id: animateRetract;
        target: root;
        properties: "implicitHeight"
        duration: 200
        easing.type: Easing.OutSine
        to: 0
        onStopped: {
            root.implicitHeight = 0
        }
    }

    // this MouseArea prevents mouse events to be sent below miniplayer
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
    }

    Widgets.FrostedGlassEffect {
        anchors.fill: column

        source: mainContent
        sourceRect: Qt.rect(root.x, root.y, root.width, root.height)

        tint: VLCStyle.colors.blendColors(VLCStyle.colors.bg, VLCStyle.colors.banner, 0.85)
    }

    Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right

        SliderBar {
            id: progressBar
            value: player.position
            visible: progressBar.value >= 0.0 && progressBar.value <= 1.0

            focus: true

            isMiniplayer: true

            anchors {
                left: parent.left
                right: parent.right
            }

            Keys.onDownPressed: buttonsLayout.focus = true
            Keys.onUpPressed: root.navigationUpItem.focus = true
        }

        Item {
            id: mainRect

            anchors {
                left: parent.left
                right: parent.right
            }

            height: VLCStyle.miniPlayerHeight

            PlayerButtonsLayout {
                id: buttonsLayout

                anchors {
                    fill: parent
                    leftMargin: VLCStyle.applicationHorizontalMargin
                    rightMargin: VLCStyle.applicationHorizontalMargin
                    bottomMargin: VLCStyle.applicationVerticalMargin
                }

                models: [miniPlayerModel_left, miniPlayerModel_center, miniPlayerModel_right]

                navigationUpItem: progressBar.enabled ? progressBar : root.navigationUpItem

                isMiniplayer: true
            }

            PlayerControlBarModel {
                id: miniPlayerModel_left
                mainCtx: mainctx
                configName: "MiniPlayerToolbar-left"
            }

            PlayerControlBarModel {
                id: miniPlayerModel_center
                mainCtx: mainctx
                configName: "MiniPlayerToolbar-center"
            }

            PlayerControlBarModel {
                id: miniPlayerModel_right
                mainCtx: mainctx
                configName: "MiniPlayerToolbar-right"
            }

            Keys.onPressed: {
                if (!event.accepted)
                    defaultKeyAction(event, 0)
                if (!event.accepted)
                    mainInterface.sendHotkey(event.key, event.modifiers);
            }
        }
    }
}
