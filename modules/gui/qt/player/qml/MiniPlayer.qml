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

    readonly property bool expanded: root.implicitHeight === root.childrenRect.height

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

    Column {
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: VLCStyle.dp(-progressBar.height / 2, VLCStyle.scale)

        SliderBar {
            id: progressBar
            value: player.position
            visible: progressBar.value >= 0.0 && progressBar.value <= 1.0
            z: 1

            isMiniplayer: true

            anchors {
                left: parent.left
                right: parent.right
            }

            Keys.onDownPressed: buttonrow_left.focus = true
            Keys.onUpPressed: root.navigationUpItem.focus = true
        }


        Item {
            id: mainRect

            anchors {
                left: parent.left
                right: parent.right
            }
            z: 0
            height: VLCStyle.miniPlayerHeight

            Widgets.FrostedGlassEffect {
                anchors.fill: parent

                source: mainContent
                sourceRect: Qt.rect(root.x, root.y, root.width, root.height)

                tint: VLCStyle.colors.blendColors(VLCStyle.colors.bg, VLCStyle.colors.banner, 0.85)
            }

            Item {
                anchors {
                    fill: parent

                    leftMargin: VLCStyle.applicationHorizontalMargin
                    rightMargin: VLCStyle.applicationHorizontalMargin
                    bottomMargin: VLCStyle.applicationVerticalMargin
                }

                PlayerButtonsLayout {
                    id: buttonrow_left

                    model: miniPlayerModel_left
                    defaultSize: VLCStyle.icon_normal

                    implicitHeight: buttonrow.implicitHeight

                    anchors {
                        left: parent.left
                        top: parent.top
                        bottom: parent.bottom

                        leftMargin: VLCStyle.margin_normal
                    }

                    visible: model.count > 0 && (miniPlayerModel_center.count > 0 ? ((x+width) < buttonrow_center.x) : true)

                    navigationParent: root
                    navigationRightItem: buttonrow_center

                    focus: true

                    navigationUp: function(index) {
                        if (progressBar.enabled)
                            progressBar.focus = true
                        else
                            root.navigationUp(index)
                    }

                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: defaultKeyAction(event, 0)
                }

                PlayerButtonsLayout {
                    id: buttonrow_center

                    model: miniPlayerModel_center
                    defaultSize: VLCStyle.icon_normal

                    anchors {
                        centerIn: parent
                    }

                    navigationParent: root
                    navigationLeftItem: buttonrow_left
                    navigationRightItem: buttonrow_right

                    navigationUp: function(index) {
                        if (progressBar.enabled)
                            progressBar.focus = true
                        else
                            root.navigationUp(index)
                    }

                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: defaultKeyAction(event, 0)
                }

                PlayerButtonsLayout {
                    id: buttonrow_right

                    model: miniPlayerModel_right
                    defaultSize: VLCStyle.icon_normal

                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom

                        rightMargin: VLCStyle.margin_normal
                    }

                    visible: model.count > 0 && (miniPlayerModel_center.count > 0 ? ((buttonrow_center.x + buttonrow_center.width) < x)
                                                                                  : !(((buttonrow_left.x + buttonrow_left.width) > x) && miniPlayerModel_left.count > 0))

                    navigationParent: root
                    navigationLeftItem: buttonrow_center

                    navigationUp: function(index) {
                        if (progressBar.enabled)
                            progressBar.focus = true
                        else
                            root.navigationUp(index)
                    }

                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: defaultKeyAction(event, 0)
                }
            }

            Connections{
                target: mainInterface
                onToolBarConfUpdated: {
                    miniPlayerModel_left.reloadModel()
                    miniPlayerModel_center.reloadModel()
                    miniPlayerModel_right.reloadModel()
                }
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

            ControlButtons {
                id: controlmodelbuttons

                isMiniplayer: true
                parentWindow: mainInterfaceRect
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
