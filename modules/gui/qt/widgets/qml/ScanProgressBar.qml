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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///style/"

FrostedGlassEffect {
    // Settings

    height: column.implicitHeight + VLCStyle.margin_small * 2

    tint: VLCStyle.colors.lowerBanner

    // Children

    ColumnLayout {
        id: column

        anchors.fill: parent

        anchors.leftMargin: VLCStyle.margin_large
        anchors.rightMargin: VLCStyle.margin_large
        anchors.topMargin: VLCStyle.margin_small
        anchors.bottomMargin: VLCStyle.margin_small

        spacing: VLCStyle.margin_small

        T.ProgressBar {
            id: control

            Layout.fillWidth: true

            height: VLCStyle.heightBar_xxsmall

            from: 0
            to: 100

            value: MediaLib.parsingProgress

            indeterminate: MediaLib.discoveryPending

            contentItem: Item {
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right

                    anchors.verticalCenter: parent.verticalCenter

                    height: VLCStyle.heightBar_xxxsmall

                    color: VLCStyle.colors.sliderBarMiniplayerBgColor
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter

                    width: parent.width * control.visualPosition
                    height: VLCStyle.heightBar_xxsmall

                    // NOTE: We want round corners.
                    radius: height

                    visible: (control.indeterminate === false)

                    color: VLCStyle.colors.accent
                }

                Rectangle {
                    property real position: 0

                    anchors.verticalCenter: parent.verticalCenter

                    // NOTE: Why 0.24 though ?
                    width: parent.width * 0.24
                    height: VLCStyle.heightBar_xxsmall

                    x: Math.round((parent.width - width) * position)

                    // NOTE: We want round corners.
                    radius: height

                    visible: control.indeterminate

                    color: VLCStyle.colors.accent

                    SequentialAnimation on position {
                        loops: Animation.Infinite

                        running: visible

                        NumberAnimation {
                            from: 0
                            to: 1.0

                            duration: VLCStyle.durationSliderBouncing
                            easing.type: Easing.OutBounce
                        }

                        NumberAnimation {
                            from: 1.0
                            to: 0

                            duration: VLCStyle.durationSliderBouncing
                            easing.type: Easing.OutBounce
                        }
                    }
                }
            }
        }

        SubtitleLabel {
            Layout.fillWidth: true

            text: (MediaLib.discoveryPending) ? I18n.qtr("Scanning %1")
                                                .arg(MediaLib.discoveryEntryPoint)
                                              : I18n.qtr("Indexing Medias (%1%)")
                                                .arg(MediaLib.parsingProgress)

            elide: Text.ElideMiddle

            font.pixelSize: VLCStyle.fontSize_large
            font.weight: Font.Normal
        }
    }
}
