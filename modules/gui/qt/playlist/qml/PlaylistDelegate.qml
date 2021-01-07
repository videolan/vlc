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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Rectangle {
    id: delegate

    // starts with an underscore to prevent the implicit delegate 'model' property
    property var _model

    signal itemClicked(int button, int modifier, var globalMousePos)
    signal itemDoubleClicked(int keys, int modifier, var globalMousePos)
    signal dragStarting()

    property alias hovered: mouse.containsMouse

    property var dragitem: null
    signal dropedMovedAt(int target, var drop)

    property int leftPadding: 0
    property int rightPadding: 0

    property VLCColors colors: VLCStyle.colors

    // Should the cover be displayed
    //property alias showCover: cover.visible

    // This item will become the parent of the dragged item during the drag operation
    //property alias draggedItemParent: draggable_item.draggedItemParent

    height: Math.max( VLCStyle.fontHeight_normal, VLCStyle.icon_normal ) + VLCStyle.margin_xsmall

    function showTooltip(binding) {
        plInfoTooltip.close()
        plInfoTooltip.text = Qt.binding(function() { return (textInfo.text + '\n' + textArtist.text); })
        plInfoTooltip.parent = textInfoColumn
        if (_model.getSelection().length > 1 && binding)
            plInfoTooltip.timeout = 2000
        else
            plInfoTooltip.timeout = 0
        plInfoTooltip.visible = Qt.binding(function() { return ( (binding ? model.selected : delegate.hovered) && !overlayMenu.visible &&
                                                                (textInfo.implicitWidth > textInfo.width || textArtist.implicitWidth > textArtist.width)); })

    }

    color: {
        if (selected)
            colors.plItemSelected
        else if (hovered)
            colors.plItemHovered
        else if (activeFocus)
            colors.plItemFocused
        else
            return "transparent"
    }
    function isDropAcceptable(drop, index) {
        console.assert(false, "parent should reimplement this function")
    }

    onHoveredChanged: {
        if(hovered)
            showTooltip(false)
    }

    readonly property bool selected : model.selected

    onSelectedChanged: {
        if(selected)
            showTooltip(true)
    }

    Connections {
        target: root

        onSetItemDropIndicatorVisible: {
            if (index === model.index)
            {
                if (top)
                {
                    // show top drop indicator bar
                    topDropIndicator.visible = isVisible
                }
                else
                {
                    // show bottom drop indicator bar
                    bottomDropIndicator.visible = isVisible
                }
            }
        }
    }

    // top drop indicator bar
    Rectangle {
        id: topDropIndicator
        z: 1
        width: parent.width
        height: 1
        anchors.top: parent.top
        antialiasing: true
        visible: false
        color: colors.accent
    }

    // bottom drop indicator bar
    // only active when the item is the last item in the list
    Loader {
        id: bottomDropIndicator
        active: model.index === _model.count - 1
        visible: false

        z: 1
        width: parent.width
        height: 1
        anchors.top: parent.bottom
        antialiasing: true

        sourceComponent: Rectangle {
            color: colors.accent
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true

        acceptedButtons: acceptedButtons | Qt.RightButton

        onClicked:{
            delegate.itemClicked(mouse.button, mouse.modifiers, this.mapToGlobal(mouse.x, mouse.y));
        }
        onDoubleClicked: {
            if (mouse.button !== Qt.RightButton)
                delegate.itemDoubleClicked(mouse.buttons, mouse.modifiers, this.mapToGlobal(mouse.x, mouse.y));
        }

        drag.target: dragItem

        property bool __rightButton : false

        Connections {
            target: mouse.drag
            onActiveChanged: {
                if (mouse.__rightButton)
                    return
                if (target.active) {
                    delegate.dragStarting()
                    dragItem.model = _model
                    dragItem.count = _model.getSelection().length
                    dragItem.visible = true
                } else {
                    dragItem.Drag.drop()
                    dragItem.visible = false
                }
            }
        }

        onPressed:  {
            if (mouse.button === Qt.RightButton)
            {
                __rightButton = true
                return
            }
            else
                __rightButton = false
        }

        onPositionChanged: {
            if (dragItem.visible)
            {
                var pos = this.mapToGlobal( mouseX, mouseY)
                dragItem.updatePos(pos.x + VLCStyle.dp(15, VLCStyle.scale), pos.y)
            }
        }

        RowLayout {
            id: content
            anchors {
                fill: parent
                leftMargin: delegate.leftPadding
                rightMargin: delegate.rightPadding
            }

            Item {
                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.icon_normal

                DropShadow {
                    id: effect
                    anchors.fill: artwork
                    source: artwork
                    radius: 8
                    samples: 17
                    color: colors.glowColorBanner
                    visible: artwork.visible
                    spread: 0.1
                }

                Image {
                    id: artwork
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: (model.artwork && model.artwork.toString()) ? model.artwork : VLCStyle.noArtCover
                    visible: !statusIcon.visible
                }

                Widgets.IconLabel {
                    id: statusIcon
                    anchors.fill: parent
                    visible: (model.isCurrent && text !== "")
                    width: height
                    height: VLCStyle.icon_normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: colors.accent
                    text: player.playingState === PlayerController.PLAYING_STATE_PLAYING ? VLCIcons.volume_high :
                                                    player.playingState === PlayerController.PLAYING_STATE_PAUSED ? VLCIcons.pause : ""
                }
            }

            Column {
                id: textInfoColumn
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_large

                Widgets.ListLabel {
                    id: textInfo

                    width: parent.width

                    font.weight: model.isCurrent ? Font.Bold : Font.Normal
                    text: model.title
                    color: colors.text
                }

                Widgets.ListSubtitleLabel {
                    id: textArtist

                    width: parent.width

                    font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                    text: (model.artist ? model.artist : i18n.qtr("Unknown Artist"))
                    color: colors.text
                }
            }

            Widgets.ListLabel {
                id: textDuration
                Layout.rightMargin: VLCStyle.margin_xsmall
                Layout.preferredWidth: durationMetric.width
                text: model.duration
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: colors.text

                TextMetrics {
                    id: durationMetric
                    font.pixelSize: VLCStyle.fontSize_normal
                    text: "-00:00-"
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            // exceed bottom boundary by the height of item separator to prevent drop indicator bar visible glitch
            anchors.bottomMargin: -1

            spacing: 0

            DropArea {
                id: higherDropArea
                Layout.fillWidth: true
                Layout.preferredHeight: parent.height / 2

                onEntered: {
                    if (isDropAcceptable(drag, model.index))
                        root.setItemDropIndicatorVisible(model.index, true, true)
                }
                onExited: {
                    root.setItemDropIndicatorVisible(model.index, false, true)
                }
                onDropped: {
                    if (!isDropAcceptable(drop, model.index))
                        return

                    delegate.dropedMovedAt(model.index, drop)
                    root.setItemDropIndicatorVisible(model.index, false, true)
                }
            }

            DropArea {
                id: lowerDropArea
                Layout.fillWidth: true
                Layout.fillHeight: true

                readonly property bool _isLastItem: model.index === delegate._model.count - 1
                readonly property int _targetIndex: _isLastItem ? model.index + 1 : model.index

                onEntered: {
                    if (!isDropAcceptable(drag, _targetIndex))
                        return

                    root.setItemDropIndicatorVisible(_targetIndex, true, !_isLastItem)
                }
                onExited: {
                    root.setItemDropIndicatorVisible(_targetIndex, false, !_isLastItem)
                }
                onDropped: {
                    if(!isDropAcceptable(drop, _targetIndex))
                        return

                    delegate.dropedMovedAt(_targetIndex, drop)
                    root.setItemDropIndicatorVisible(_targetIndex, false, !_isLastItem)
                }
            }
        }
    }
}
