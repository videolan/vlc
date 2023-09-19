
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
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util

// FIXME: Keyboard navigation needs to be fixed for this Popup.
T.Popup {
    id: root

    // Settings
    property var preferredWidth : stackView.currentItem.preferredWidth

    width: Math.min((typeof preferredWidth !== "undefined")
                          ? preferredWidth : Number.MAX_VALUE
                    , root.parent.width)

    height: VLCStyle.dp(296, VLCStyle.scale)

    // Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    modal: true

    // Animations

    Behavior on width {
        SmoothedAnimation {
            duration: VLCStyle.duration_veryShort
            easing.type: Easing.InOutSine
        }
    }

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: popupTheme
        colorSet: ColorContext.Window
    }

    T.Overlay.modal: null

    background: Rectangle {
        // NOTE: The opacity should be stronger on a light background for readability.
        color: (popupTheme.palette.isDark)
               ? VLCStyle.setColorAlpha(popupTheme.bg.primary, 0.8)
               : VLCStyle.setColorAlpha(popupTheme.bg.primary, 0.96)

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right

            height: VLCStyle.margin_xxxsmall

            color: popupTheme.border
        }
    }

    contentItem: StackView {
        id: stackView

        focus: true
        clip: true

        //erf, popup are weird, content is not parented to the root
        //so, duplicate the context here for the childrens
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: popupTheme.colorSet
            palette: popupTheme.palette
        }

        initialItem: TracksListPage {
            trackMenuController: trackMenuController
        }

        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: VLCStyle.duration_long
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: VLCStyle.duration_long
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: VLCStyle.duration_long
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: VLCStyle.duration_long
            }
        }
    }

    QtObject {
      id: trackMenuController

      signal requestAudioPage()
      signal requestSubtitlePage()
      signal requestPlaybackSpeedPage()
      signal requestBack()

      onRequestBack: {
          stackView.pop()
      }

      onRequestAudioPage: {
          stackView.push("qrc:///player/TracksPageAudio.qml", {"trackMenuController": trackMenuController})
      }

      onRequestSubtitlePage: {
          stackView.push("qrc:///player/TracksPageSubtitle.qml", {"trackMenuController": trackMenuController})
      }

      onRequestPlaybackSpeedPage: {
          stackView.push("qrc:///player/TracksPageSpeed.qml", {"trackMenuController": trackMenuController})
      }
    }
}
