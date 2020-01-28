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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper

Widgets.NavigableFocusScope {
    id: root
    property alias columnLayout: columnLayout

    AboutModel {
        id: about
    }

    ButtonGroup {
        buttons: columnLayout.children
    }

    RowLayout {
        id: rowLayout
        anchors.fill: parent
        spacing: 0

        Rectangle {

            Layout.preferredWidth: columnLayout.implicitWidth
            Layout.fillHeight: true
            color: VLCStyle.colors.banner

            ColumnLayout {
                id: columnLayout
                anchors.fill: parent

                Widgets.TextToolButton {
                    id: authorsBtn
                    text: i18n.qtr("Authors")
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    onClicked: {
                        checked = true
                        textArea.text = about.authors
                    }
                    KeyNavigation.down: licenseBtn
                    KeyNavigation.right: textScroll
                    focus: true
                }

                Widgets.TextToolButton {
                    id: licenseBtn
                    text: i18n.qtr("License")
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    onClicked: textArea.text = about.license
                    KeyNavigation.down: creditBtn
                    KeyNavigation.right: textScroll
                }

                Widgets.TextToolButton {
                    id: creditBtn
                    text: i18n.qtr("Credit")
                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    KeyNavigation.down: backBtn
                    KeyNavigation.right: textScroll

                    onClicked: {
                        checked = true
                        textArea.text = about.thanks
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Widgets.IconToolButton {
                    id: backBtn
                    size: VLCStyle.icon_large
                    iconText: VLCIcons.exit
                    text: i18n.qtr("Back")
                    Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
                    KeyNavigation.right: textScroll

                    onClicked: {
                        history.previous()
                    }
                }
            }
        }


        Rectangle {

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment:  Qt.AlignHCenter

            color: VLCStyle.colors.bg

            ColumnLayout {
                id: columnLayout1
                anchors.fill: parent
                anchors.margins: 10

                Text {
                    id: text1
                    text: i18n.qtr("VLC Media Player")
                    color: VLCStyle.colors.text
                    font.pixelSize: VLCStyle.fontSize_xxxlarge
                }


                Text {
                    text: about.version
                    color: VLCStyle.colors.text
                    font.pixelSize: VLCStyle.fontSize_xlarge
                }


                ScrollView {
                    id: textScroll
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.alignment:  Qt.AlignHCenter

                    Keys.onPressed:  {
                        if (KeyHelper.matchLeft(event)) {
                            backBtn.focus = true
                            event.accepted = true
                        }
                    }

                    clip: true

                    Text {
                        id: textArea
                        text: about.thanks
                        horizontalAlignment: Text.AlignHLeft
                        font.family: "Courier New, Monospace"
                        anchors.fill: parent
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter

                        color: VLCStyle.colors.text
                        enabled: false
                    }
                }
            }
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)
}
