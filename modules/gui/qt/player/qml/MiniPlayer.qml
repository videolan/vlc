import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

FocusScope {
    id: root

    implicitHeight: controlBar.implicitHeight
    height: 0

    readonly property bool expanded: (height !== 0)

    property var mainContent: undefined

    state: (player.playingState === PlayerController.PLAYING_STATE_STOPPED) ? ""
                                                                            : "expanded"

    states: State {
        id: stateExpanded
        name: "expanded"

        PropertyChanges {
            target: root
            height: implicitHeight
        }
    }

    transitions: Transition {
        from: ""; to: "expanded"
        reversible: true
        NumberAnimation { property: "height"; easing.type: Easing.InOutSine; duration: VLCStyle.duration_normal; }
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

        tint: VLCStyle.colors.lowerBanner
    }

    ControlBar {
        id: controlBar

        anchors.fill: parent

        rightPadding: VLCStyle.applicationHorizontalMargin
        leftPadding: rightPadding
        bottomPadding: VLCStyle.applicationVerticalMargin

        focus: true
        colors: VLCStyle.colors
        textPosition: ControlBar.TimeTextPosition.Hide
        sliderHeight: VLCStyle.dp(3, VLCStyle.scale)
        sliderBackgroundColor: colors.sliderBarMiniplayerBgColor
        sliderProgressColor: colors.accent
        identifier: PlayerControlbarModel.Miniplayer
        Navigation.parentItem: root

        Keys.onPressed: {
            controlBar.Navigation.defaultKeyAction(event)

            if (!event.accepted) {
                MainCtx.sendHotkey(event.key, event.modifiers)
            }
        }
    }
}
