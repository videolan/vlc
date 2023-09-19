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

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///style/"

T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    horizontalPadding: VLCStyle.margin_large
    verticalPadding: VLCStyle.margin_small

    from: 0
    to: 100

    value: MediaLib.parsingProgress

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    indeterminate: MediaLib.discoveryPending

    background: Rectangle {
        color: theme.bg.primary
    }

    contentItem: Column {

        spacing: VLCStyle.margin_small

        Item {
            anchors.left: parent.left
            anchors.right: parent.right

            implicitHeight: VLCStyle.heightBar_xxsmall
            implicitWidth: 200

            ColorContext {
                id: progressBarTheme
                colorSet: ColorContext.Slider
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter

                implicitHeight: VLCStyle.heightBar_xxxsmall

                color: progressBarTheme.bg.primary
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter

                implicitWidth: parent.width * control.visualPosition
                implicitHeight: VLCStyle.heightBar_xxsmall

                // NOTE: We want round corners.
                radius: height

                visible: !control.indeterminate


                color: progressBarTheme.accent
            }

            Rectangle {
                property real position: 0

                anchors.verticalCenter: parent.verticalCenter

                // NOTE: Why 0.24 though ?
                implicitWidth: parent.width * 0.24
                implicitHeight: VLCStyle.heightBar_xxsmall

                x: Math.round((parent.width - width) * position)

                // NOTE: We want round corners.
                radius: height
                visible: control.indeterminate

                color: progressBarTheme.accent

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

        SubtitleLabel {
            anchors.left: parent.left
            anchors.right: parent.right

            text: (MediaLib.discoveryPending) ? qsTr("Scanning %1")
                                                .arg(MediaLib.discoveryEntryPoint)
                                              : qsTr("Indexing Medias (%1%)")
                                                .arg(MediaLib.parsingProgress)

            elide: Text.ElideMiddle

            font.pixelSize: VLCStyle.fontSize_large
            font.weight: Font.Normal
            color: theme.fg.primary
        }
    }
}
