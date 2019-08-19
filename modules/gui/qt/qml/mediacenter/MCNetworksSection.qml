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
import QtQml 2.11
import "qrc:///utils/" as Utils
import "qrc:///style/"

Rectangle {
    id: networkSection

    property var selectableDelegateModel: null
    property alias text: sectionSeparator.text
    property alias currentIndex: sectionListView.currentIndex

    signal actionDown(int index)
    signal actionUp(int index)

    function shiftX(index){
        return sectionListView.shiftX(index)
    }
    function adjustTopFlickableViewBound(){

        const itemTop = networkSection.y + sectionListView.currentItem.y
        const itemBottom = sectionSeparator.height + networkSection.y + sectionListView.currentItem.y + sectionListView.currentItem.height
        if (itemTop < flickable.contentY)
            flickable.contentY = itemTop

        else if (itemBottom > (flickable.contentY + flickable.height))
            flickable.contentY = itemBottom - flickable.height
    }
    anchors {
        left: parent.left
        right: parent.right
    }
    implicitHeight: childrenRect.height
    color: "transparent"
    enabled: visible

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if(activeFocus){
            sectionListView.forceActiveFocus()
        }

        if (networkSection.selectableDelegateModel.items.count > 0 && networkSection.selectableDelegateModel.selectedGroup.count === 0) {
            var initialIndex = 0
            if (view[networkSection.selectableDelegateModel.viewIndexPropertyName] !== -1)
                initialIndex = view.currentIndexProvider
            networkSection.selectableDelegateModel.items.get(initialIndex).inSelected = true
            view[networkSection.selectableDelegateModel.viewIndexPropertyName] = initialIndex
        }

        adjustTopFlickableViewBound()
    }

    Utils.LabelSeparator {
        id: sectionSeparator
    }
   Rectangle{
       id: sectionRect
       anchors {
           left: parent.left
           right: parent.right
           top: sectionSeparator.bottom
       }
       height: gridRect.height
       color: "transparent"

    Rectangle {
        id: gridRect
        color: VLCStyle.colors.bg
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: VLCStyle.margin_large
        anchors.rightMargin: VLCStyle.margin_large
        height: sectionListView.contentHeight
        Utils.KeyNavigableListView {
            id: sectionListView
            anchors.fill: parent

            model: networkSection.selectableDelegateModel.parts.grid
            modelCount: networkSection.selectableDelegateModel.items.count
            currentIndex: networkSection.currentIndex
            orientation: ListView.Horizontal
            onCurrentItemChanged: adjustTopFlickableViewBound()

            onSelectAll: networkSection.selectableDelegateModel.selectAll()
            onSelectionUpdated:  networkSection.selectableDelegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: networkSection.selectableDelegateModel.actionAtIndex(index)

            onActionLeft: root.actionLeft(index)
            onActionRight: root.actionRight(index)
            onActionUp: networkSection.actionUp(index)
            onActionDown: networkSection.actionDown(index)
            onActionCancel: root.actionCancel(index)

        }

    }

    Utils.RoundButton{
        id: leftBtn
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        text:"<"
        onClicked: sectionListView.prevPage()
        visible: sectionListView.contentWidth > sectionListView.width
        enabled: visible
    }


    Utils.RoundButton{
        id: rightBtn
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        text:">"
        onClicked: sectionListView.nextPage()
        visible: sectionListView.contentWidth > sectionListView.width
        enabled: visible
    }
   }
}
