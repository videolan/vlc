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
        anchors.fill: controlBar

        source: mainContent
        sourceRect: Qt.rect(root.x, root.y, root.width, root.height)

        tint: VLCStyle.colors.blendColors(VLCStyle.colors.bg, VLCStyle.colors.banner, 0.85)
    }

    ControlBar {
        id: controlBar

        anchors.left: parent.left
        anchors.right: parent.right
        focus: true
        colors: VLCStyle.colors
        height: VLCStyle.miniPlayerHeight
        textPosition: ControlBar.TimeTextPosition.Hide
        sliderHeight: VLCStyle.dp(3, VLCStyle.scale)
        sliderBackgroundColor: colors.sliderBarMiniplayerBgColor
        sliderProgressColor: colors.accent
        identifier: "MiniPlayer"
        navigationParent: root

        Keys.onPressed: {
            if (!event.accepted)
                defaultKeyAction(event, 0)
            if (!event.accepted)
                mainInterface.sendHotkey(event.key, event.modifiers);
        }
    }
}
