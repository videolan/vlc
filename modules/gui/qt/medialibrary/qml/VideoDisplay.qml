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
import QtQml.Models 2.2

import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    readonly property var currentIndex: view.currentItem.currentIndex
    //the index to "go to" when the view is loaded
    property var initialIndex: 0

    property alias contentModel: videoModel ;

    navigationCancel: function() {
        if (view.currentItem.currentIndex <= 0) {
            defaultNavigationCancel()
        } else {
            view.currentItem.currentIndex = 0;
            view.currentItem.positionViewAtIndex(0, ItemView.Contain)
        }
    }

    onCurrentIndexChanged: {
        history.update([ "mc", "video", {"initialIndex": currentIndex}])
    }

    onInitialIndexChanged: resetFocus()
    onContentModelChanged: resetFocus()

    function resetFocus() {
        if (videoModel.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= videoModel.count)
            initialIndex = 0
        selectionModel.select(videoModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        if (view.currentItem)
            view.currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)
    }

    function _actionAtIndex(index) {
        g_mainDisplay.showPlayer()
        medialib.addAndPlay( videoModel.getIdsForIndexes( selectionModel.selectedIndexes ) )
    }

    MLVideoModel {
        id: videoModel
        ml: medialib

        onCountChanged: {
            if (videoModel.count > 0 && !selectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: videoModel
    }

    VideoContextMenu {
        id: contextMenu
        model: videoModel
    }


    Component {
        id: gridComponent

        MainInterface.MainGridView {
            id: videosGV
            property Item currentItem: Item{}

            activeFocusOnTab:true
            delegateModel: selectionModel
            model: videoModel

            headerDelegate: Widgets.SubtitleLabel {
                text: i18n.qtr("Videos")
                leftPadding: videosGV.rowX
                topPadding: VLCStyle.margin_large
                bottomPadding: VLCStyle.margin_normal
                width: root.width
            }

            delegate: VideoGridItem {
                id: videoGridItem

                opacity: videosGV.expandIndex !== -1 && videosGV.expandIndex !== videoGridItem.index ? .7 : 1
                dragItem: videoDragItem

                onContextMenuButtonClicked: {
                    videosGV.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos, {
                        "information" : index
                    } )
                }

                onItemClicked : videosGV.leftClickOnItem(modifier, index)

                Behavior on opacity {
                    NumberAnimation {
                        duration: 100
                    }
                }
            }

            expandDelegate: VideoInfoExpandPanel {
                onRetract: videosGV.retract()

                navigationParent: videosGV
                navigationCancel:  function() {  videosGV.retract() }
                navigationUp: function() {  videosGV.retract() }
                navigationDown: function() { videosGV.retract() }
            }

            navigationParent: root

            /*
             *define the intial position/selection
             * This is done on activeFocus rather than Component.onCompleted because selectionModel.
             * selectedGroup update itself after this event
             */
            onActiveFocusChanged: {
                if (activeFocus && videoModel.count > 0 && !selectionModel.hasSelection) {
                    selectionModel.select(videoModel.index(0,0), ItemSelectionModel.ClearAndSelect)
                }
            }

            cellWidth: VLCStyle.gridItem_video_width
            cellHeight: VLCStyle.gridItem_video_height

            onSelectAll:selectionModel.selectAll()
            onSelectionUpdated: selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: _actionAtIndex( index )

            Connections {
                target: contextMenu
                onShowMediaInformation: videosGV.switchExpandItem(index)
            }
        }

    }


    Component {
        id: listComponent
        VideoListDisplay {
            height: view.height
            width: view.width
            model: videoModel
            dragItem: videoDragItem
            navigationParent: root

            selectionDelegateModel: selectionModel

            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, menuParent.mapToGlobal(0,0) )

            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos )

        }
    }


    Widgets.DragItem {
        id: videoDragItem

        function updateComponents(maxCovers) {
          var items = selectionModel.selectedIndexes.slice(0, maxCovers).map(function (x){
            return videoModel.getDataAt(x.row)
          })
          var title = items.map(function (item){ return item.title}).join(", ")
          var covers = items.map(function (item) { return {artwork: item.thumbnail || VLCStyle.noArtCover}})
          return {
            covers: covers,
            title: title,
            count: selectionModel.selectedIndexes.length
          }
        }

        function insertIntoPlaylist(index) {
            medialib.insertIntoPlaylist(index, videoModel.getIdsForIndexes(selectionModel.selectedIndexes))
        }
    }

    Widgets.StackViewExt {
        id: view
        anchors.fill:parent
        clip: true
        focus: videoModel.count !== 0
        initialItem: mainInterface.gridView ? gridComponent : listComponent
        Connections {
            target: mainInterface
            onGridViewChanged: {
                if (mainInterface.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(listComponent)
            }
        }

    }

    EmptyLabel {
        anchors.fill: parent
        visible: videoModel.count === 0
        focus: visible
        text: i18n.qtr("No video found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
    }
}
