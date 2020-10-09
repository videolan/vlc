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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var plmodel: PlaylistListModel {
        playlistId: mainctx.playlist
    }

    property int leftPadding: 0
    property int rightPadding: 0
    property alias backgroundColor: parentRect.color

    property bool forceDark: false
    property VLCColors _colors: forceDark ? vlcNightColors : VLCStyle.colors

    signal setItemDropIndicatorVisible(int index, bool isVisible, bool top)

    VLCColors {id: vlcNightColors; state: "night"}

    function sortPL(key) {
        if (mainPlaylistController.sortKey !== key) {
            mainPlaylistController.setSortOrder(PlaylistControllerModel.SORT_ORDER_ASC)
            mainPlaylistController.setSortKey(key)
        }
        else {
            mainPlaylistController.switchSortOrder()
        }

        mainPlaylistController.sort()
    }

    Rectangle {
        id: parentRect
        anchors.fill: parent
        color: _colors.banner

        //label for DnD
        Widgets.DNDLabel {
            id: dragItem

            _colors: root._colors
            color: parent.color

            property int _scrollingDirection: 0

            on_PosChanged: {
                var dragItemY = root.mapToGlobal(dragItem._pos.x, dragItem._pos.y).y
                var viewY     = root.mapToGlobal(view.x, view.y).y

                var topDiff    = (viewY + VLCStyle.dp(20, VLCStyle.scale)) - dragItemY
                var bottomDiff = dragItemY - (viewY + view.height - VLCStyle.dp(20, VLCStyle.scale))

                if(!view.listView.atYBeginning && topDiff > 0) {
                    _scrollingDirection = -1

                    view.fadeRectTopHovered = true
                }
                else if( !view.listView.atYEnd && bottomDiff > 0) {
                    _scrollingDirection = 1

                    view.fadeRectBottomHovered = true
                }
                else {
                    _scrollingDirection = 0

                    view.fadeRectTopHovered = false
                    view.fadeRectBottomHovered = false
                }
            }

            SmoothedAnimation {
                id: upAnimation
                target: view.listView
                property: "contentY"
                to: 0
                running: dragItem._scrollingDirection === -1 && dragItem.visible

                velocity: VLCStyle.dp(150, VLCStyle.scale)
            }

            SmoothedAnimation {
                id: downAnimation
                target: view.listView
                property: "contentY"
                to: view.listView.contentHeight - view.height
                running: dragItem._scrollingDirection === 1 && dragItem.visible

                velocity: VLCStyle.dp(150, VLCStyle.scale)
            }
        }

        PlaylistContextMenu {
            id: contextMenu
            model: root.plmodel
            controler: mainPlaylistController
        }

        PlaylistMenu {
            id: overlayMenu
            anchors.fill: parent
            z: 2

            navigationParent: root
            navigationLeftItem: view

            leftPadding: root.leftPadding
            rightPadding: root.rightPadding

            isPLEmpty: (root.plmodel.count === 0)
            isItemNotSelected: (root.plmodel.selectedCount === 0)

            //rootmenu
            Action { id:playAction;         text: i18n.qtr("Play");                      onTriggered: mainPlaylistController.goTo(root.plmodel.getSelection()[0], true); icon.source: "qrc:///toolbar/play_b.svg"                   }
            Action { id:streamAction;       text: i18n.qtr("Stream");                    onTriggered: dialogProvider.streamingDialog(root.plmodel.getSelection().map(function(i) { return root.plmodel.itemAt(i).url; }), false); icon.source: "qrc:/menu/stream.svg" }
            Action { id:saveAction;         text: i18n.qtr("Save");                      onTriggered: dialogProvider.streamingDialog(root.plmodel.getSelection().map(function(i) { return root.plmodel.itemAt(i).url; }));          }
            Action { id:infoAction;         text: i18n.qtr("Information");               onTriggered: dialogProvider.mediaInfoDialog(root.plmodel.itemAt(root.plmodel.getSelection()[0])); icon.source: "qrc:/menu/info.svg"        }
            Action { id:exploreAction;      text: i18n.qtr("Show Containing Directory"); onTriggered: mainPlaylistController.explore(root.plmodel.itemAt(root.plmodel.getSelection()[0])); icon.source: "qrc:/type/folder-grey.svg" }
            Action { id:addFileAction;      text: i18n.qtr("Add File...");               onTriggered: dialogProvider.simpleOpenDialog(false);                             icon.source: "qrc:/buttons/playlist/playlist_add.svg"     }
            Action { id:addDirAction;       text: i18n.qtr("Add Directory...");          onTriggered: dialogProvider.PLAppendDir();                                       icon.source: "qrc:/buttons/playlist/playlist_add.svg"     }
            Action { id:addAdvancedAction;  text: i18n.qtr("Advanced Open...");          onTriggered: dialogProvider.PLAppendDialog();                                    icon.source: "qrc:/buttons/playlist/playlist_add.svg"     }
            Action { id:savePlAction;       text: i18n.qtr("Save Playlist to File...");  onTriggered: dialogProvider.savePlayingToPlaylist();                                                                                       }
            Action { id:clearAllAction;     text: i18n.qtr("Clear Playlist");            onTriggered: mainPlaylistController.clear();                                     icon.source: "qrc:/toolbar/clear.svg"                     }
            Action { id:selectAllAction;    text: i18n.qtr("Select All");                onTriggered: root.plmodel.selectAll();                                                                                                     }
            Action { id:shuffleAction;      text: i18n.qtr("Shuffle Playlist");          onTriggered: mainPlaylistController.shuffle();                                   icon.source: "qrc:///buttons/playlist/shuffle_on.svg"     }
            Action { id:sortAction;         text: i18n.qtr("Sort");                      property string subMenu: "sortmenu";                                                                                                       }
            Action { id:selectTracksAction; text: i18n.qtr("Select Tracks");             onTriggered: view.mode = "select";                                                                                                         }
            Action { id:moveTracksAction;   text: i18n.qtr("Move Selection");            onTriggered: view.mode = "move";                                                                                                           }
            Action { id:deleteAction;       text: i18n.qtr("Remove Selected");           onTriggered: view.onDelete();                                                                                                              }

            readonly property var sortList: [sortTitleAction,
                                            sortDurationAction,
                                            sortArtistAction,
                                            sortAlbumAction,
                                            sortGenreAction,
                                            sortDateAction,
                                            sortTrackAction,
                                            sortURLAction,
                                            sortRatingAction]

            Connections {
                id: plControllerConnections
                target: mainPlaylistController

                property alias sortList: overlayMenu.sortList

                function setMark() {
                    for (var i = 0; i < sortList.length; i++) {
                        if(sortList[i].key === mainPlaylistController.sortKey) {
                            sortList[i].sortActiveMark = "✓"
                            sortList[i].sortOrderMark  = (mainPlaylistController.sortOrder === PlaylistControllerModel.SORT_ORDER_ASC ? "↓" : "↑")
                            continue
                        }

                        sortList[i].sortActiveMark = ""
                        sortList[i].sortOrderMark  = ""
                    }
                }

                onSortOrderChanged: {
                    plControllerConnections.setMark()
                }

                onSortKeyChanged: {
                    plControllerConnections.setMark()
                }
            }

            // sortmenu
            Action { id: sortTitleAction;   text: i18n.qtr("Title"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_TITLE;
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortDurationAction; text: i18n.qtr("Duration"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_DURATION
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortArtistAction;  text: i18n.qtr("Artist"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_ARTIST
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortAlbumAction;   text: i18n.qtr("Album"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_ALBUM
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortGenreAction;   text: i18n.qtr("Genre"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_GENRE
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortDateAction;    text: i18n.qtr("Date"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_DATE
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortTrackAction;   text: i18n.qtr("Track Number"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_TRACK_NUMBER
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortURLAction;     text: i18n.qtr("URL"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_URL
                property string sortActiveMark; property string sortOrderMark }
            Action { id: sortRatingAction;  text: i18n.qtr("Rating"); onTriggered: root.sortPL(key);
                readonly property int key: PlaylistControllerModel.SORT_KEY_RATING
                property string sortActiveMark; property string sortOrderMark }

            models: {
                "rootmenu" : {
                    title: i18n.qtr("Playlist"),
                    entries: [
                        playAction,
                        streamAction,
                        saveAction,
                        infoAction,
                        exploreAction,
                        addFileAction,
                        addDirAction,
                        addAdvancedAction,
                        savePlAction,
                        clearAllAction,
                        selectAllAction,
                        shuffleAction,
                        sortAction,
                        selectTracksAction,
                        moveTracksAction,
                        deleteAction
                    ]
                },
                "rootmenu_plempty" : {
                    title: i18n.qtr("Playlist"),
                    entries: [
                        addFileAction,
                        addDirAction,
                        addAdvancedAction
                    ]
                },
                "rootmenu_noselection" : {
                    title: i18n.qtr("Playlist"),
                    entries: [
                        addFileAction,
                        addDirAction,
                        addAdvancedAction,
                        savePlAction,
                        clearAllAction,
                        sortAction,
                        selectTracksAction
                    ]
                },
                "sortmenu" :{
                    title: i18n.qtr("Sort Playlist"),
                    entries: sortList
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.bottomMargin: VLCStyle.margin_normal

            ColumnLayout {
                id: headerTextLayout
                Layout.fillWidth: true
                Layout.leftMargin: root.leftPadding + VLCStyle.margin_normal
                Layout.topMargin: VLCStyle.margin_normal

                Widgets.SubtitleLabel {
                    text: i18n.qtr("Playqueue")
                    color: _colors.text
                }

                Widgets.CaptionLabel {
                    function getHoursMinutesText(duration) {
                        var hours = duration.toHours()
                        var minutes = duration.toMinutes()
                        var text
                        if (hours >= 1) {
                            minutes = minutes % 60
                            text = i18n.qtr("%1h %2min").arg(hours).arg(minutes)
                        }
                        else if (minutes > 0) {
                            text = i18n.qtr("%1 min").arg(minutes)
                        }
                        else {
                            text = i18n.qtr("%1 sec").arg(duration.toSeconds())
                        }

                        return text
                    }

                    anchors.topMargin: VLCStyle.margin_small
                    visible: plmodel.count !== 0
                    text: i18n.qtr("%1 elements, %2").arg(root.plmodel.count).arg(getHoursMinutesText(plmodel.duration))
                    color: _colors.caption
                }
            }

            RowLayout {
                id: content
                visible: plmodel.count !== 0

                Layout.topMargin: VLCStyle.margin_normal
                Layout.leftMargin: root.leftPadding + VLCStyle.margin_normal
                Layout.rightMargin: root.rightPadding + view.scrollBarWidth

                Widgets.IconLabel {
                    Layout.preferredWidth: VLCStyle.icon_normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: VLCIcons.album_cover
                    color: _colors.caption
                }

                Widgets.CaptionLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_large
                    verticalAlignment: Text.AlignVCenter
                    text: i18n.qtr("Title")
                    color: _colors.caption
                }

                Widgets.IconLabel {
                    Layout.rightMargin: VLCStyle.margin_xsmall
                    Layout.preferredWidth: durationMetric.width

                    text: VLCIcons.time
                    color: _colors.caption
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    TextMetrics {
                        id: durationMetric
                        font.pixelSize: VLCStyle.fontSize_normal
                        text: "-00:00-"
                    }
                }
            }

            Widgets.KeyNavigableListView {
                id: view

                Layout.fillWidth: true
                Layout.fillHeight: true

                focus: true

                model: root.plmodel
                modelCount: root.plmodel.count

                fadeColor: root.backgroundColor

                property int shiftIndex: -1
                property string mode: "normal"

                Connections {
                    target: root.plmodel
                    onRowsInserted: {
                        if (view.currentIndex == -1)
                            view.currentIndex = 0
                    }
                    onModelReset: {
                        if (view.currentIndex == -1 &&  root.plmodel.count > 0)
                            view.currentIndex = 0
                    }
                    onSelectedCountChanged: {
                        var selectedIndexes = root.plmodel.getSelection()
                        var modelCount = root.plmodel.count

                        if (modelCount === 0 || selectedIndexes.length === 0)
                            return

                        var bottomItemIndex = view.listView.indexAt(view.listView.contentX, (view.listView.contentY + view.height) - 2)
                        var topItemIndex    = view.listView.indexAt(view.listView.contentX, view.listView.contentY + 2)

                        if (topItemIndex !== -1 && (root.plmodel.isSelected(topItemIndex) || (modelCount >= 2 && root.plmodel.isSelected(topItemIndex + 1))))
                            view.fadeRectTopHovered = true
                        else
                            view.fadeRectTopHovered = false

                        if (bottomItemIndex !== -1 && (root.plmodel.isSelected(bottomItemIndex) || (root.plmodel.isSelected(bottomItemIndex - 1))))
                            view.fadeRectBottomHovered = true
                        else
                            view.fadeRectBottomHovered = false
                    }
                }

                footer: Item {
                    width: parent.width
                    height: Math.max(VLCStyle.icon_normal, view.height - y)

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton | Qt.LeftButton

                        onClicked: {
                            view.forceActiveFocus()
                            if( mouse.button === Qt.RightButton )
                                contextMenu.popup(-1, this.mapToGlobal(mouse.x, mouse.y))
                            else if ( mouse.button === Qt.LeftButton )
                                root.plmodel.deselectAll()
                        }
                    }

                    DropArea {
                        anchors.fill: parent
                        onEntered: {
                            if(!drag.hasUrls && drag.source.model.index === root.plmodel.count - 1)
                                return

                            root.setItemDropIndicatorVisible(view.modelCount - 1, true, false);
                        }
                        onExited: {


                            root.setItemDropIndicatorVisible(view.modelCount - 1, false, false);
                        }
                        onDropped: {
                            if(!drop.hasUrls && drop.source.model.index === root.plmodel.count - 1)
                                return

                            if (drop.hasUrls) {
                                //force conversion to an actual list
                                var urlList = []
                                for ( var url in drop.urls)
                                    urlList.push(drop.urls[url])
                                mainPlaylistController.insert(root.plmodel.count, urlList, false)
                            } else {
                                root.plmodel.moveItemsPost(root.plmodel.getSelection(), root.plmodel.count - 1)
                            }
                            root.setItemDropIndicatorVisible(view.modelCount - 1, false, false);
                            drop.accept()
                        }
                    }
                }

                ToolTip {
                    id: plInfoTooltip
                    delay: 750
                }

                delegate: Column {

                    Loader {
                        active: (index === 0) // load only for the first element to prevent overlapping
                        width: parent.width
                        height: 1
                        z: 0
                        sourceComponent: Rectangle {
                            color: _colors.playlistSeparator
                            opacity: _colors.isThemeDark ? 0.05 : 1.0
                        }
                    }

                    PLItem {
                        /*
                         * implicit variables:
                         *  - model: gives access to the values associated to PlaylistListModel roles
                         *  - index: the index of this item in the list
                         */
                        id: plitem
                        plmodel: root.plmodel
                        width: root.width
                        z: 1
                        leftPadding: root.leftPadding + VLCStyle.margin_normal
                        rightPadding: root.rightPadding + view.scrollBarWidth

                        onItemClicked : {
                            /* to receive keys events */
                            view.forceActiveFocus()
                            if (view.mode == "move") {
                                var selectedIndexes = root.plmodel.getSelection()
                                if (selectedIndexes.length === 0)
                                    return
                                var preTarget = index
                                /* move to _above_ the clicked item if move up, but
                                 * _below_ the clicked item if move down */
                                if (preTarget > selectedIndexes[0])
                                    preTarget++
                                view.currentIndex = selectedIndexes[0]
                                root.plmodel.moveItemsPre(selectedIndexes, preTarget)
                                return
                            } else if (view.mode == "select") {
                            } else if (!(root.plmodel.isSelected(index) && button === Qt.RightButton)) {
                                view.updateSelection(modifier, view.currentIndex, index)
                                view.currentIndex = index
                            }

                            if (button === Qt.RightButton)
                                contextMenu.popup(index, globalMousePos)
                        }
                        onItemDoubleClicked: mainPlaylistController.goTo(index, true)
                        color: _colors.getPLItemColor(model.selected, plitem.hovered, plitem.activeFocus)
                        _colors: root._colors

                        onDragStarting: {
                            if (!root.plmodel.isSelected(index)) {
                                /* the dragged item is not in the selection, replace the selection */
                                root.plmodel.setSelection([index])
                            }
                        }

                        onDropedMovedAt: {
                            if (drop.hasUrls) {
                                //force conversion to an actual list
                                var urlList = []
                                for ( var url in drop.urls)
                                    urlList.push(drop.urls[url])
                                mainPlaylistController.insert(target, urlList, false)
                                drop.accept(Qt.IgnoreAction)
                            } else {
                                root.plmodel.moveItemsPre(root.plmodel.getSelection(), target)
                            }
                        }

                        onHoveredChanged: {
                            var bottomItemIndex = view.listView.indexAt(plitem.width / 2, (view.listView.contentY + view.height) - 2)
                            var topItemIndex = view.listView.indexAt(plitem.width / 2, view.listView.contentY + 2)

                            if(bottomItemIndex !== -1 && model.index >= bottomItemIndex - 1)
                            {
                                view.fadeRectBottomHovered = plitem.hovered
                            }
                            if(topItemIndex !== -1 && model.index <= topItemIndex + 1)
                            {
                                view.fadeRectTopHovered = plitem.hovered
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        z: 0
                        color: _colors.playlistSeparator
                        opacity: _colors.isThemeDark ? 0.05 : 1.0
                    }
                }

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 200 }
                }

                displaced: Transition {
                    NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.OutSine }
                    NumberAnimation { property: "opacity"; to: 1.0 }
                }

                onSelectAll: root.plmodel.selectAll()
                onSelectionUpdated: {
                    if (view.mode === "select") {
                        console.log("update selection select")
                    } else if (mode == "move") {
                        var selectedIndexes = root.plmodel.getSelection()
                        if (selectedIndexes.length === 0)
                            return
                        /* always move relative to the first item of the selection */
                        var target = selectedIndexes[0];
                        if (newIndex > oldIndex) {
                            /* move down */
                            target++
                        } else if (newIndex < oldIndex && target > 0) {
                            /* move up */
                            target--
                        }

                        view.currentIndex = selectedIndexes[0]
                        /* the target is the position _after_ the move is applied */
                        root.plmodel.moveItemsPost(selectedIndexes, target)
                    } else { // normal
                        updateSelection(keyModifiers, oldIndex, newIndex);
                    }
                }

                Keys.onDeletePressed: onDelete()
                Keys.onMenuPressed: overlayMenu.open()

                navigationParent: root
                navigationRight: function(index) {
                    overlayMenu.open()
                }
                navigationLeft: function(index) {
                    if (mode === "normal") {
                        root.navigationLeft(index)
                    } else {
                        mode = "normal"
                    }
                }
                navigationCancel: function(index) {
                    if (mode === "normal") {
                        root.navigationCancel(index)
                    } else {
                        mode = "normal"
                    }
                }

                onActionAtIndex: {
                    if (index < 0)
                        return

                    if (mode === "select")
                        root.plmodel.toggleSelected(index)
                    else //normal
                        // play
                        mainPlaylistController.goTo(index, true)
                }

                function onPlay() {
                    let selection = root.plmodel.getSelection()
                    if (selection.length === 0)
                        return
                    mainPlaylistController.goTo(selection[0], true)
                }

                function onDelete() {
                    let selection = root.plmodel.getSelection()
                    if (selection.length === 0)
                        return
                    root.plmodel.removeItems(selection)
                }

                function _addRange(from, to) {
                    root.plmodel.setRangeSelected(from, to - from + 1, true)
                }

                function _delRange(from, to) {
                    root.plmodel.setRangeSelected(from, to - from + 1, false)
                }

                // copied from SelectableDelegateModel, which is intended to be removed
                function updateSelection( keymodifiers, oldIndex, newIndex ) {
                    if ((keymodifiers & Qt.ShiftModifier)) {
                        if ( shiftIndex === oldIndex) {
                            if ( newIndex > shiftIndex )
                                _addRange(shiftIndex, newIndex)
                            else
                                _addRange(newIndex, shiftIndex)
                        } else if (shiftIndex <= newIndex && newIndex < oldIndex) {
                            _delRange(newIndex + 1, oldIndex )
                        } else if ( shiftIndex < oldIndex && oldIndex < newIndex ) {
                            _addRange(oldIndex, newIndex)
                        } else if ( newIndex < shiftIndex && shiftIndex < oldIndex ) {
                            _delRange(shiftIndex, oldIndex)
                            _addRange(newIndex, shiftIndex)
                        } else if ( newIndex < oldIndex && oldIndex < shiftIndex  ) {
                            _addRange(newIndex, oldIndex)
                        } else if ( oldIndex <= shiftIndex && shiftIndex < newIndex ) {
                            _delRange(oldIndex, shiftIndex)
                            _addRange(shiftIndex, newIndex)
                        } else if ( oldIndex < newIndex && newIndex <= shiftIndex  ) {
                            _delRange(oldIndex, newIndex - 1)
                        }
                    } else {
                        shiftIndex = newIndex
                        if (keymodifiers & Qt.ControlModifier) {
                            root.plmodel.toggleSelected(newIndex)
                        } else {
                            root.plmodel.setSelection([newIndex])
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    visible: plmodel.count === 0

                    Widgets.IconLabel {
                        font.pixelSize: VLCStyle.dp(48, VLCStyle.scale)
                        anchors.horizontalCenter: parent.horizontalCenter
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: VLCIcons.playlist
                        color: view.activeFocus ? _colors.accent : _colors.text
                        opacity: 0.3
                    }

                    // ToDo: Use TitleLabel
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: VLCStyle.margin_xlarge
                        text: i18n.qtr("No content yet")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                        color: view.activeFocus ? _colors.accent : _colors.text
                        opacity: 0.4
                    }

                    // ToDo: Use BodyLabel
                    Label {
                        anchors.topMargin: VLCStyle.margin_normal
                        text: i18n.qtr("Drag & Drop some content here!")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: VLCStyle.fontSize_large
                        color: view.activeFocus ? _colors.accent : _colors.text
                        opacity: 0.4
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                height: VLCStyle.heightBar_normal
                visible: !(infoText.text === "")

                RectangularGlow {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter

                    width: infoText.width + VLCStyle.dp(18, VLCStyle.scale)
                    height: infoText.height + VLCStyle.dp(12, VLCStyle.scale)

                    glowRadius: 2
                    cornerRadius: 10
                    spread: 0.1
                    color: _colors.glowColorBanner
                }

                Label {
                    id: infoText
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter

                    text: (view.mode === "select")
                            ? i18n.qtr("Select tracks (%1)").arg(plmodel.selectedCount)
                        : (view.mode === "move")
                            ? i18n.qtr("Move tracks (%1)").arg(plmodel.selectedCount)
                        : ""
                    font.pixelSize: VLCStyle.fontSize_large
                    color: _colors.text
                    elide: Text.ElideRight
                }
            }

            PlaylistToolbar {
                Layout.fillWidth: true

                leftPadding: root.leftPadding
                rightPadding: root.rightPadding
                navigationParent: root
                navigationUpItem: view

                _colors: root._colors
            }
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: view
    Keys.onPressed: {
        if (event.matches(StandardKey.SelectAll))
        {
            root.plmodel.selectAll();
        }
        else
        {
            defaultKeyAction(event, 0)
        }
    }
}
