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
import QtQml.Models 2.2
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

NavigableFocusScope {
    id: root

    //forwarded from subview
    signal actionForSelection( var selection )

    property var sortModel: ListModel { }
    property var model: []

    property alias contentHeight: view.contentHeight

    property alias interactive: view.interactive

    Utils.SelectableDelegateModel {
        id: delegateModel

        model: root.model

        delegate: Package {
            id: element
            property var rowModel: model

            Rectangle {
                Package.name: "list"
                id: lineView

                width: parent.width
                height: VLCStyle.fontHeight_normal + VLCStyle.margin_xxsmall

                color:  VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, hoverArea.containsMouse, this.activeFocus)

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        delegateModel.updateSelection( mouse.modifiers , view.currentIndex, index)
                        view.currentIndex = rowModel.index
                        lineView.forceActiveFocus()
                    }

                    onDoubleClicked: {
                        actionForSelection(delegateModel.selectedGroup)
                    }

                    Row {
                        anchors.fill: parent

                        Repeater {
                            model: sortModel

                            Item {
                                height: VLCStyle.fontHeight_normal
                                width: model.width * view.width

                                Text {
                                    text: rowModel[model.criteria]
                                    elide: Text.ElideRight
                                    font.pixelSize: VLCStyle.fontSize_normal
                                    color: VLCStyle.colors.text

                                    anchors {
                                        fill: parent
                                        leftMargin: VLCStyle.margin_xxsmall
                                        rightMargin: VLCStyle.margin_xxsmall
                                    }
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignLeft
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    color: VLCStyle.colors.buttonBorder
                    antialiasing: true
                    anchors{
                        right: parent.right
                        bottom: parent.bottom
                        left: parent    .left
                    }
                    height: 1
                }
            }
        }
    }


    KeyNavigableListView {
        id: view

        anchors.fill: parent

        focus: true

        model : delegateModel.parts.list
        modelCount: delegateModel.items.count

        header: Rectangle {
            height: VLCStyle.fontHeight_normal
            width: parent.width
            color: VLCStyle.colors.button

            Row {
                anchors.fill: parent
                Repeater {
                    model: sortModel
                    MouseArea {
                        height: VLCStyle.fontHeight_normal
                        width: model.width * view.width
                        //Layout.alignment: Qt.AlignVCenter

                        Text {
                            text: model.text
                            elide: Text.ElideRight
                            font {
                                bold: true
                                pixelSize: VLCStyle.fontSize_normal

                            }
                            color: VLCStyle.colors.buttonText
                            horizontalAlignment: Text.AlignLeft
                            anchors {
                                fill: parent
                                leftMargin: VLCStyle.margin_xxsmall
                                rightMargin: VLCStyle.margin_xxsmall
                            }
                        }

                        Text {
                            text: (root.model.sortOrder === Qt.AscendingOrder) ? "▼" : "▲"
                            visible: root.model.sortCriteria === model.criteria
                            font.pixelSize: VLCStyle.fontSize_normal
                            color: VLCStyle.colors.accent
                            anchors {
                                right: parent.right
                                leftMargin: VLCStyle.margin_xxsmall
                                rightMargin: VLCStyle.margin_xxsmall
                            }
                        }
                        onClicked: {
                            if (root.model.sortCriteria !== model.criteria)
                                root.model.sortCriteria = model.criteria
                            else
                                root.model.sortOrder = (root.model.sortOrder === Qt.AscendingOrder) ? Qt.DescendingOrder : Qt.AscendingOrder
                        }
                    }
                }
            }

            //line below
            Rectangle {
                color: VLCStyle.colors.buttonBorder
                height: 1
                width: parent.width
                anchors.bottom: parent.bottom
            }
        }

        onSelectAll: delegateModel.selectAll()
        onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
        onActionLeft: root.actionLeft(index)
        onActionRight: root.actionRight(index)
        onActionUp: root.actionUp(index)
        onActionDown: root.actionDown(index)
        onActionCancel: root.actionCancel(index)
        onActionAtIndex: root.actionForSelection( delegateModel.selectedGroup )
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && delegateModel.items.count > 0 && delegateModel.selectedGroup.count === 0) {
            var initialIndex = 0
            if (view.currentIndex !== -1)
                initialIndex = view.currentIndex
            delegateModel.items.get(initialIndex).inSelected = true
            view.currentIndex = initialIndex
        }
    }

}
