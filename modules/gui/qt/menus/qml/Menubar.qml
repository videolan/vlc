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
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Style
import VLC.Menus

Item {
    id: root

    implicitHeight: menubarLayout.implicitHeight
    implicitWidth: menubarLayout.implicitWidth

    property bool hovered: _countHovered !== 0
    property bool menuOpened: _menuIndex !== -1

    readonly property ColorContext colorContext: ColorContext {
        colorSet: ColorContext.MenuBar
    }

    Action{ id: mediaMenu;    text: menubar.menuEntryTitle(QmlMenuBar.MEDIA)    ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.MEDIA);   }
    Action{ id: playbackMenu; text: menubar.menuEntryTitle(QmlMenuBar.PLAYBACK) ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.PLAYBACK);}
    Action{ id: videoMenu;    text: menubar.menuEntryTitle(QmlMenuBar.VIDEO)    ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.VIDEO);   }
    Action{ id: audioMenu;    text: menubar.menuEntryTitle(QmlMenuBar.AUDIO)    ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.AUDIO);   }
    Action{ id: subtitleMenu; text: menubar.menuEntryTitle(QmlMenuBar.SUBTITLE) ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.SUBTITLE);}
    Action{ id: toolMenu;     text: menubar.menuEntryTitle(QmlMenuBar.TOOL)     ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.TOOL);    }
    Action{ id: viewMenu;     text: menubar.menuEntryTitle(QmlMenuBar.VIEW)     ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.VIEW);    }
    Action{ id: helpMenu;     text: menubar.menuEntryTitle(QmlMenuBar.HELP)     ; onTriggered: (source) => menubar.popupMenuEntry(source, QmlMenuBar.HELP);    }

    readonly property var toolbarModel: [
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
        playerViewVisible: History.match(History.viewPath, ["player"])

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
