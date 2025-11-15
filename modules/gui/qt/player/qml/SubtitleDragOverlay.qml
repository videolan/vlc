/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import VLC.Player
import VLC.Style

// Subtitle drag overlay for adjusting subtitle position
// Activated by holding Ctrl key and dragging in the subtitle area
MouseArea {
    id: subtitleDragOverlay

    // Properties
    property bool isDragging: false
    property real dragStartY: 0
    property int currentMargin: 0  // Tracks the accumulated margin across drag sessions
    property int dragSessionStartMargin: 0  // Margin at the start of current drag
    property int lastAppliedMargin: 0  // Last margin value sent to backend
    property real lastMouseY: 0
    
    anchors.fill: parent
    acceptedButtons: Qt.LeftButton
    enabled: Player.hasVideoOutput
    propagateComposedEvents: true  // Allow events to pass through when not handled
    
    // Only activate drag when Ctrl is held and click is in subtitle area
    onPressed: (mouse) => {
        if ((mouse.modifiers & Qt.ControlModifier) && mouse.y > parent.height * 0.4) {
            isDragging = true
            dragStartY = mouse.y
            lastMouseY = mouse.y
            dragSessionStartMargin = currentMargin
            lastAppliedMargin = currentMargin
            cursorShape = Qt.ClosedHandCursor
            mouse.accepted = true
        } else {
            // Let other components handle the event
            mouse.accepted = false
        }
    }
    
    onReleased: (mouse) => {
        if (isDragging) {
            isDragging = false
            cursorShape = Qt.ArrowCursor
            // Update the persistent margin value
            currentMargin = dragSessionStartMargin - Math.round(mouse.y - dragStartY)
            currentMargin = Math.max(0, currentMargin)  // Ensure non-negative
            mouse.accepted = true
        } else {
            mouse.accepted = false
        }
    }
    
    onPositionChanged: (mouse) => {
        if (isDragging) {
            // Calculate margin change based on drag distance from start of this session
            // Dragging down (positive deltaY) decreases margin (moves subtitle down)
            // Dragging up (negative deltaY) increases margin (moves subtitle up)
            const deltaY = mouse.y - dragStartY
            const newMargin = Math.max(0, dragSessionStartMargin - Math.round(deltaY))
            
            // Only update backend if margin value actually changed (throttling)
            if (newMargin !== lastAppliedMargin) {
                Player.setSubtitleMargin(newMargin)
                lastAppliedMargin = newMargin
            }
            
            lastMouseY = mouse.y
            mouse.accepted = true
        } else {
            mouse.accepted = false
        }
    }
    
    // Visual feedback when dragging
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        visible: subtitleDragOverlay.isDragging
        
        Rectangle {
            id: feedbackTooltip
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.min(Math.max(subtitleDragOverlay.lastMouseY - height / 2, 0), parent.height - height)
            width: VLCStyle.dp(240, VLCStyle.scale)
            height: VLCStyle.dp(50, VLCStyle.scale)
            color: Qt.rgba(0, 0, 0, 0.8)
            radius: VLCStyle.dp(8, VLCStyle.scale)
            border.color: Qt.rgba(1, 1, 1, 0.3)
            border.width: 1
            
            // Smooth animation for position changes
            Behavior on y {
                NumberAnimation {
                    duration: 50
                    easing.type: Easing.OutQuad
                }
            }
            
            Text {
                anchors.centerIn: parent
                text: qsTr("Adjusting subtitle position...")
                color: "white"
                font.pixelSize: VLCStyle.fontSize_normal
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
