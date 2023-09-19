/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Controls
import QtQuick.Controls.impl 
import QtQuick.Templates as T
import org.videolan.vlc 0.1
import QtQuick.Layouts

import "qrc:///style/"


Item {
    id: root

    implicitHeight: menubarLayout.implicitHeight
    implicitWidth: menubarLayout.implicitWidth

    property bool hovered: _countHovered !== 0
    property bool menuOpened: _menuIndex !== -1

    readonly property ColorContext colorContext: ColorContext {
        colorSet: ColorContext.MenuBar
    }

    Action{ id: mediaMenu;    text: qsTr("&Media")    ; onTriggered: (source) => menubar.popupMediaMenu(source);   }
    Action{ id: playbackMenu; text: qsTr("&Playback") ; onTriggered: (source) => menubar.popupPlaybackMenu(source);}
    Action{ id: videoMenu;    text: qsTr("&Video")    ; onTriggered: (source) => menubar.popupVideoMenu(source);   }
    Action{ id: audioMenu;    text: qsTr("&Audio")    ; onTriggered: (source) => menubar.popupAudioMenu(source);   }
    Action{ id: subtitleMenu; text: qsTr("&Subtitle") ; onTriggered: (source) => menubar.popupSubtitleMenu(source);}
    Action{ id: toolMenu;     text: qsTr("&Tools")    ; onTriggered: (source) => menubar.popupToolsMenu(source);   }
    Action{ id: viewMenu;     text: qsTr("V&iew")     ; onTriggered: (source) => menubar.popupViewMenu(source);    }
    Action{ id: helpMenu;     text: qsTr("&Help")     ; onTriggered: (source) => menubar.popupHelpMenu(source);    }

    property var toolbarModel: [
        mediaMenu,
        playbackMenu,
        videoMenu,
        audioMenu,
        subtitleMenu,
        toolMenu,
        viewMenu,
        helpMenu,
    ]

    property int _menuIndex: -1
    property int _countHovered: 0


    // Accessible
    Accessible.role: Accessible.MenuBar

    function openMenu(obj, cb, index) {
        cb.trigger(obj)
        root._menuIndex = index
    }

    function updateHover(obj, cb, index, hovered ) {
        root._countHovered += hovered ? 1 : -1

        if (hovered && menubar.openMenuOnHover) {
            cb.trigger(obj)
            root._menuIndex = index
        }
    }

    QmlMenuBar {
        id: menubar
        ctx: MainCtx
        menubar: menubarLayout

        onMenuClosed: _menuIndex = -1
        onNavigateMenu: (direction) => {
            const i =  (root._menuIndex + root.toolbarModel.length + direction) % root.toolbarModel.length
            root.openMenu(menubarLayout.visibleChildren[i], root.toolbarModel[i], i)
        }
    }

    RowLayout {
        id: menubarLayout
        spacing: 0
        Repeater {
            model: root.toolbarModel

            T.MenuItem {
                id: control

                text: modelData.text
                onClicked: root.openMenu(this, modelData, index)
                onHoveredChanged: root.updateHover(this, modelData, index, hovered)
                font.pixelSize: VLCStyle.fontSize_normal

                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                implicitWidth: contentItem.implicitWidth + VLCStyle.margin_xsmall * 2
                implicitHeight: contentItem.implicitHeight + VLCStyle.margin_xxxsmall * 2

                Accessible.onPressAction: control.clicked()

                ColorContext {
                    id: theme
                    colorSet: root.colorContext.colorSet

                    enabled: control.enabled
                    focused: index === root._menuIndex
                    hovered: control.hovered
                }

                contentItem: IconLabel {
                    text: control.text
                    font: control.font
                    opacity: enabled ? 1.0 : 0.3
                    color: theme.fg.primary
                }

                background: Rectangle {
                    color: theme.bg.primary
                }
            }
        }
    }
}
