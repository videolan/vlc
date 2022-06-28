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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4
import QtQuick.Templates 2.4 as T
import org.videolan.vlc 0.1
import QtQuick.Layouts 1.11

import "qrc:///style/"


Item {
    id: root

    implicitHeight: menubarLayout.implicitHeight
    implicitWidth: menubarLayout.implicitWidth

    property color bgColor: "transparent"
    property color textColor: VLCStyle.colors.text
    property color highlightedBgColor: VLCStyle.colors.bgHover
    property color highlightedTextColor: VLCStyle.colors.bgHoverText
    property bool hovered: _countHovered !== 0
    property bool menuOpened: _menuIndex !== -1


    Action{ id: mediaMenu;    text: I18n.qtr("&Media")    ; onTriggered: menubar.popupMediaMenu(source);   }
    Action{ id: playbackMenu; text: I18n.qtr("&Playback") ; onTriggered: menubar.popupPlaybackMenu(source);}
    Action{ id: videoMenu;    text: I18n.qtr("&Video")    ; onTriggered: menubar.popupVideoMenu(source);   }
    Action{ id: audioMenu;    text: I18n.qtr("&Audio")    ; onTriggered: menubar.popupAudioMenu(source);   }
    Action{ id: subtitleMenu; text: I18n.qtr("&Subtitle") ; onTriggered: menubar.popupSubtitleMenu(source);}
    Action{ id: toolMenu;     text: I18n.qtr("&Tools")    ; onTriggered: menubar.popupToolsMenu(source);   }
    Action{ id: viewMenu;     text: I18n.qtr("V&iew")     ; onTriggered: menubar.popupViewMenu(source);    }
    Action{ id: helpMenu;     text: I18n.qtr("&Help")     ; onTriggered: menubar.popupHelpMenu(source);    }

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
        onNavigateMenu: {
            var i =  (root._menuIndex + root.toolbarModel.length + direction) % root.toolbarModel.length
            root.openMenu(menubarLayout.visibleChildren[i], root.toolbarModel[i], i)
        }

    }

    RowLayout {
        id: menubarLayout
        spacing: 0
        Repeater {
            model: root.toolbarModel

            T.Button {
                id: control

                text: modelData.text
                onClicked: root.openMenu(this, modelData, index)
                onHoveredChanged: root.updateHover(this, modelData, index, hovered)
                font.pixelSize: VLCStyle.fontSize_normal

                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                implicitWidth: contentItem.implicitWidth + VLCStyle.margin_xsmall * 2
                implicitHeight: contentItem.implicitHeight + VLCStyle.margin_xxxsmall * 2

                contentItem: IconLabel {
                    text: control.text
                    font: control.font
                    opacity: enabled ? 1.0 : 0.3
                    color: (control.hovered || index === root._menuIndex) ? root.highlightedTextColor : root.textColor
                }

                background: Rectangle {
                    color: (control.hovered || index === root._menuIndex) ? root.highlightedBgColor
                                                                          : root.bgColor
                }
            }
        }
    }
}
