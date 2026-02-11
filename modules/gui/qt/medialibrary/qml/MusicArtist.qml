/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick
import QtQuick.Window
import QtQml.Models
import QtQuick.Layouts

import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style

FocusScope {
    id: root

    required property var artistId

    readonly property int _extraMargin: VLCStyle.dynamicAppMargins(width)
    readonly property int _contentLeftMargin: VLCStyle.layout_left_margin + _extraMargin
    readonly property int _contentRightMargin: VLCStyle.layout_right_margin + _extraMargin

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    //the index to "go to" when the view is loaded
    property int initialIndex: 0

    property Item headerItem: _currentView ? _currentView.headerItem : null

    property bool isSearchable: true

    property string searchPattern
    property int sortOrder
    property string sortCriteria

    readonly property MLBaseModel _effectiveModel: MainCtx.gridView ? albumModel : trackModel

    onSearchPatternChanged: {
        _effectiveModel.searchPattern = root.searchPattern
    }

    onSortOrderChanged: {
        _effectiveModel.sortOrder = root.sortOrder
    }

    onSortCriteriaChanged: {
        // FIXME: Criteria is set to empty for a brief period during initialization,
        //        call later prevents setting the criteria empty.
        Qt.callLater(() => {
            _effectiveModel.sortCriteria = root.sortCriteria
        })
    }

    Connections {
        target: root._effectiveModel

        function onSearchPatternChanged() {
            if (root.searchPattern !== root._effectiveModel.searchPattern)
                root.searchPattern = root._effectiveModel.searchPattern
        }

        function onSortOrderChanged() {
            if (root.sortOrder !== root._effectiveModel.sortOrder)
                root.sortOrder = root._effectiveModel.sortOrder
        }

        function onSortCriteriaChanged() {
            if (root.sortCriteria !== root._effectiveModel.sortCriteria)
                root.sortCriteria = root._effectiveModel.sortCriteria
        }
    }

    // current index of album model
    readonly property int currentIndex: {
        if (!_currentView)
           return -1
        else
           return _currentView.currentIndex
    }

    property real rightPadding

    property alias _currentView: loader.item

    property var _artist: ({
        id: 0
    })

    function navigationShowHeader(y, height) {
        if (!(_currentView instanceof Flickable))
            return

        const newContentY = Helpers.flickablePositionContaining(_currentView, y, height, 0, 0)

        if (newContentY !== _currentView.contentY)
            _currentView.contentY = newContentY
    }

    property Component header: T.Pane {
        id: headerFs

        focus: true
        height: col.height
        width: root.width

        implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                implicitContentWidth + leftPadding + rightPadding)
        implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                 implicitContentHeight + topPadding + bottomPadding)

        signal changeToNextSectionRequested()
        signal changeToPreviousSectionRequested()

        function setCurrentItemFocus(reason) {
            headerFs.forceActiveFocus(reason)
        }

        contentItem: Column {
            id: col

            height: implicitHeight
            width: headerFs.width

            ArtistTopBanner {
                id: artistBanner

                focus: true
                width: headerFs.width

                rightPadding: root.rightPadding

                artist: root._artist

                onActiveFocusChanged: {
                    // make sure content is visible with activeFocus
                    if (activeFocus)
                        root.navigationShowHeader(0, height)
                }

                Connections {
                    enabled: !MainCtx.gridView
                    target: trackModel

                    function onSortCriteriaChanged() {
                        if (MainCtx.albumSections &&
                            trackModel.sortCriteria !== "album_title") {
                            MainCtx.albumSections = false
                        }
                    }
                }

                property string _oldSortCriteria

                function adjustAlbumSections() {
                    if (!artistBanner) // context is lost, Qt 6.2 bug
                        return

                    if (!(root._currentView instanceof Widgets.TableViewExt))
                        return

                    if (MainCtx.albumSections) {
                        const albumTitleSortCriteria = "album_title"
                        if (trackModel.sortCriteria !== albumTitleSortCriteria) {
                            artistBanner._oldSortCriteria = trackModel.sortCriteria
                            trackModel.sortCriteria = albumTitleSortCriteria
                        }
                    } else {
                        if (artistBanner._oldSortCriteria.length > 0) {
                            trackModel.sortCriteria = artistBanner._oldSortCriteria
                            artistBanner._oldSortCriteria = ""
                        }
                    }

                    if (root._currentView)
                        root._currentView.albumSections = MainCtx.albumSections
                }

                Component.onCompleted: {
                    MainCtx.albumSectionsChanged.connect(artistBanner, adjustAlbumSections)
                    root._currentViewChanged.connect(artistBanner, adjustAlbumSections)
                    adjustAlbumSections()
                }

                Navigation.parentItem: root
                Navigation.downItem: pinnedMusicAlbumSectionLoader
            }

            Loader {
                id: pinnedMusicAlbumSectionLoader

                anchors.left: parent.left
                anchors.right: parent.right

                active: (root._currentView instanceof Widgets.TableViewExt)
                visible: active

                sourceComponent: MusicAlbumSectionDelegate {
                    id: pinnedMusicAlbumSection

                    model: albumModel

                    verticalPadding: VLCStyle.margin_xsmall

                    focus: true

                    readonly property Widgets.TableViewExt tableView: root._currentView

                    section: tableView.currentSection || ""

                    previousSectionButtonEnabled: tableView._firstSectionInstance && (tableView._firstSectionInstance.section !== section)
                    nextSectionButtonEnabled: tableView._lastSectionInstance && (tableView._lastSectionInstance.section !== section)

                    Component.onCompleted: {
                        changeToPreviousSectionRequested.connect(headerFs, headerFs.changeToPreviousSectionRequested)
                        changeToNextSectionRequested.connect(headerFs, headerFs.changeToNextSectionRequested)
                    }

                    background: Rectangle {
                        color: theme.bg.secondary
                    }

                    contentItem: RowLayout {
                        id: _contentItem

                        spacing: pinnedMusicAlbumSection.spacing

                        readonly property bool compactButtons: (pinnedMusicAlbumSection.width < (displayPositioningButtons ? VLCStyle.colWidth(6)
                                                                                                                           : VLCStyle.colWidth(5)))
                        readonly property bool displayPositioningButtons: !!pinnedMusicAlbumSection.tableView?.albumSections // not possible otherwise

                        Widgets.ImageExt {
                            Layout.fillHeight: true

                            Layout.preferredHeight: VLCStyle.trackListAlbumCover_heigth
                            Layout.preferredWidth: VLCStyle.trackListAlbumCover_width

                            source: pinnedMusicAlbumSection._albumCover ? pinnedMusicAlbumSection._albumCover : VLCStyle.noArtArtist

                            sourceSize: Qt.size(width * eDPR, height * eDPR)

                            readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                            backgroundColor: theme.bg.primary

                            fillMode: Image.PreserveAspectFit

                            asynchronous: true
                            cache: true

                            Widgets.DefaultShadow {
                                visible: (parent.status === Image.Ready)
                            }
                        }

                        Column {
                            Layout.fillWidth: true

                            MusicAlbumSectionDelegate.TitleLabel {
                                id: titleLabel

                                anchors.left: parent.left
                                anchors.right: parent.right

                                delegate: pinnedMusicAlbumSection

                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                                Layout.fillWidth: true

                                Layout.maximumWidth: implicitWidth

                                font.pixelSize: VLCStyle.fontSize_normal
                                font.weight: Font.DemiBold
                            }

                            MusicAlbumSectionDelegate.CaptionLabel {
                                id: captionLabel

                                anchors.left: parent.left
                                anchors.right: parent.right

                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                                delegate: pinnedMusicAlbumSection
                            }
                        }

                        MusicAlbumSectionDelegate.PlayButton {
                            id: playButton

                            delegate: pinnedMusicAlbumSection

                            showText: !_contentItem.compactButtons
                            focus: true

                            Navigation.parentItem: pinnedMusicAlbumSection
                            Navigation.rightItem: enqueueButton
                        }

                        MusicAlbumSectionDelegate.EnqueueButton {
                            id: enqueueButton

                            delegate: pinnedMusicAlbumSection

                            showText: !_contentItem.compactButtons

                            Navigation.parentItem: pinnedMusicAlbumSection
                            // Navigation.rightItem: previousSectionButton
                            Navigation.leftItem: playButton
                        }

                        // MusicAlbumSectionDelegate.PreviousSectionButton {
                        //     id: previousSectionButton

                        //     delegate: pinnedMusicAlbumSection

                        //     visible: _contentItem.displayPositioningButtons

                        //     showText: !_contentItem.compactButtons

                        //     Navigation.parentItem: pinnedMusicAlbumSection
                        //     Navigation.rightItem: nextSectionButton
                        //     Navigation.leftItem: enqueueButton
                        // }

                        // MusicAlbumSectionDelegate.NextSectionButton {
                        //     id: nextSectionButton

                        //     delegate: pinnedMusicAlbumSection

                        //     visible: _contentItem.displayPositioningButtons

                        //     showText: !_contentItem.compactButtons

                        //     Navigation.parentItem: pinnedMusicAlbumSection
                        //     Navigation.leftItem: previousSectionButton
                        // }
                    }

                    Navigation.parentItem: root
                    Navigation.upItem: artistBanner
                    Navigation.downAction: function() {
                        tableView.setCurrentItemFocus(Qt.TabFocusReason)
                    }
                }
            }

            Widgets.ViewHeader {
                view: root

                leftPadding: root._contentLeftMargin
                bottomPadding: VLCStyle.layoutTitle_bottom_padding -
                               (MainCtx.gridView ? 0 : VLCStyle.gridItemSelectedBorder)
                topPadding: pinnedMusicAlbumSectionLoader.active ? bottomPadding : VLCStyle.layoutTitle_top_padding

                text: qsTr("Albums")
            }
        }
    }

    focus: true

    onInitialIndexChanged: resetFocus()

    onArtistIdChanged: fetchArtistData()

    function setCurrentItemFocus(reason) {
        if (loader.item === null) {
            Qt.callLater(setCurrentItemFocus, reason)
            return
        }
        loader.item.setCurrentItemFocus(reason);
    }

    function resetFocus() {
        if (albumModel.count === 0) {
            return
        }
        let initialIndex = root.initialIndex
        if (initialIndex >= albumModel.count)
            initialIndex = 0
        albumSelectionModel.select(initialIndex, ItemSelectionModel.ClearAndSelect)
        const albumsListView = MainCtx.gridView ? _currentView : headerItem.albumsListView
        if (albumsListView) {
            albumsListView.currentIndex = initialIndex
            albumsListView.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    function _actionAtIndex(index, model, selectionModel) {
        if (selectionModel.selectedIndexes.length > 1) {
            model.addAndPlay( selectionModel.selectedIndexes )
        } else {
            model.addAndPlay( new Array(index) )
        }
    }

    function _onNavigationCancel() {
        if (_currentView.currentIndex <= 0) {
            root.Navigation.defaultNavigationCancel()
        } else {
            _currentView.currentIndex = 0;
            _currentView.positionViewAtIndex(0, ItemView.Contain)
        }

        if (tableView_id.currentIndex <= 0)
            root.Navigation.defaultNavigationCancel()
        else
            tableView_id.currentIndex = 0;
    }

    function fetchArtistData() {
        if (!artistId)
            return

        if (artistModel.loading)
            return

        artistModel.getDataById(artistId)
            .then((artistData) => {
                root._artist = artistData
            })
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }


    MLArtistModel {
        id: artistModel
        ml: MediaLib

        onLoadingChanged: {
            if (!loading)
                fetchArtistData()
        }
    }

    MLAlbumModel {
        id: albumModel

        ml: MediaLib
        parentId: artistId

        onCountChanged: {
            if (albumModel.count > 0 && !albumSelectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    ListSelectionModel {
        id: albumSelectionModel
        model: albumModel
    }

    Widgets.MLDragItem {
        id: albumDragItem

        view: (root._currentView instanceof Widgets.TableViewExt) ? (root._currentView?.headerItem?.albumsListView ?? null)
                                                                  : root._currentView
        indexes: indexesFlat ? albumSelectionModel.selectedIndexesFlat
                             : albumSelectionModel.selectedIndexes
        indexesFlat: !!albumSelectionModel.selectedIndexesFlat
        defaultCover: VLCStyle.noArtAlbumCover
    }

    MLAudioModel {
        id: trackModel

        ml: MediaLib
        parentId: albumModel.parentId
    }

    MLContextMenu {
        id: contextMenu

        model: albumModel
    }

    MLContextMenu {
        id: trackContextMenu

        model: trackModel
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridItemView {
            id: gridView_id

            basePictureWidth: VLCStyle.gridCover_music_width
            basePictureHeight: VLCStyle.gridCover_music_height

            focus: true
            activeFocusOnTab:true
            headerDelegate: root.header
            selectionModel: albumSelectionModel
            model: albumModel

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            Connections {
                target: albumModel
                // selectionModel updates but doesn't trigger any signal, this forces selection update in view
                function onParentIdChanged() {
                    currentIndex = -1
                }
            }

            delegate: AudioGridItem {
                id: audioGridItem

                width: gridView_id.cellWidth
                height: gridView_id.cellHeight

                pictureWidth: gridView_id.maxPictureWidth
                pictureHeight: gridView_id.maxPictureHeight

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem

                onItemClicked : (modifier) => {
                    gridView_id.leftClickOnItem(modifier, index)
                }

                onItemDoubleClicked: {
                    gridView_id.switchExpandItem(index)
                }

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView_id.rightClickOnItem(index)
                    contextMenu.popup(albumSelectionModel.selectedIndexes, globalMousePos, { "information" : index})
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: VLCStyle.duration_short
                    }
                }
            }

            expandDelegate: MusicAlbumsGridExpandDelegate {
                id: expandDelegateId

                x: 0
                width: gridView_id.width
                onRetract: gridView_id.retract()
                Navigation.parentItem: root

                Navigation.cancelAction: function() {
                    gridView_id.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.upAction: function() {
                    gridView_id.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.downAction: function() {}
            }

            onActionAtIndex: (index) => {
                if (albumSelectionModel.selectedIndexes.length === 1) {
                    switchExpandItem(index);

                    expandItem.setCurrentItemFocus(Qt.TabFocusReason);
                } else {
                    _actionAtIndex(index, albumModel, albumSelectionModel);
                }
            }

            Navigation.parentItem: root

            Navigation.upAction: function() {
                headerItem.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: root._onNavigationCancel

            Connections {
                target: contextMenu
                function onShowMediaInformation(index) {
                    gridView_id.switchExpandItem( index )
                }
            }
        }

    }

    Component {
        id: tableComponent

        Widgets.TableViewExt {
            id: tableView_id

            model: trackModel

            onActionForSelection: (selection) => {
                model.addAndPlay(selection)
            }

            header: root.header
            rowHeight: VLCStyle.tableCoverRow_height

            property bool albumSections: true

            section.property: "album_id"
            section.delegate: albumSections ? musicAlbumSectionDelegateComponent : null

            readonly property var _artistId: root.artistId

            on_ArtistIdChanged: {
                if (albumSections) {
                    _sections.length = 0 // This clears the sections list

                    // FIXME: Sections may get invalid section name on artist change, Qt bug?
                    albumSections = false
                    albumSections = true
                }
            }

            Binding on listView.cacheBuffer {
                // FIXME
                // https://doc.qt.io/qt-6/qml-qtquick-listview.html#variable-delegate-size-and-section-labels
                when: tableView_id.albumSections
                value: Math.max(tableView_id.height * 2, tableView_id.Screen.desktopAvailableHeight)
            }

            property alias contentYBehavior: contentYBehavior

            property MusicAlbumSectionDelegate _firstSectionInstance
            property MusicAlbumSectionDelegate _lastSectionInstance

            property list<MusicAlbumSectionDelegate> _sections

            Component {
                id: musicAlbumSectionDelegateComponent

                MusicAlbumSectionDelegate {
                    id: musicAlbumSectionDelegate

                    width: tableView_id.width

                    model: albumModel

                    previousSectionButtonEnabled: tableView_id._firstSectionInstance && (tableView_id._firstSectionInstance !== this)
                    nextSectionButtonEnabled: tableView_id._lastSectionInstance && (tableView_id._lastSectionInstance !== this)

                    Navigation.parentItem: tableView_id
                    Navigation.upAction: function() {
                        let item = tableView_id.listView.itemAt(0, y - 1)
                        if (item) {
                            item.forceActiveFocus(Qt.BacktabFocusReason)
                            tableView_id.currentIndex = item.index
                            tableView_id.positionViewAtIndex(item.index, ItemView.Contain)
                        }
                    }
                    Navigation.downAction: function() {
                        let item = tableView_id.listView.itemAt(0, y + height + 1)
                        if (item) {
                            item.forceActiveFocus(Qt.TabFocusReason)
                            tableView_id.currentIndex = item.index
                            tableView_id.positionViewAtIndex(item.index, ItemView.Contain)
                        }
                    }

                    Component.onCompleted: {
                        tableView_id._sections.push(this)

                        // WARNING: Sections are reused.
                        // NOTE: Scrolling does not change the y of items of content item,
                        //       listening to y and visible changes is not really terrible.
                        sectionChanged.connect(this, adjustSectionInstances)
                        yChanged.connect(this, adjustSectionInstances)
                        adjustSectionInstances()
                    }

                    function adjustSectionInstances() {
                        if (tableView_id._firstSectionInstance) {
                            if (y < tableView_id._firstSectionInstance.y)
                                tableView_id._firstSectionInstance = this
                        } else {
                            tableView_id._firstSectionInstance = this
                        }

                        if (tableView_id._lastSectionInstance) {
                            if (y > tableView_id._lastSectionInstance.y)
                                tableView_id._lastSectionInstance = this
                        } else {
                            tableView_id._lastSectionInstance = this
                        }
                    }

                    Connections {
                        target: tableView_id.headerItem

                        function onChangeToPreviousSectionRequested() {
                            if (tableView_id.currentSection === musicAlbumSectionDelegate.section)
                                musicAlbumSectionDelegate.changeToPreviousSectionRequested()
                        }

                        function onChangeToNextSectionRequested() {
                            if (tableView_id.currentSection === musicAlbumSectionDelegate.section)
                                musicAlbumSectionDelegate.changeToNextSectionRequested()
                        }
                    }

                    function changeSection(forward: bool) {
                        // We have to probe the section on demand, as otherwise we
                        // would have to track the section unnecessarily. Note that
                        // reusing sections complicates things a lot.

                        const currentSectionFirstItemPosY = (y + height + 1)
                        let item = tableView_id.listView.itemAt(0, currentSectionFirstItemPosY) // current section first item

                        if (!item || item.ListView.section !== section) {
                            // If there is no first item, there should be no section:
                            console.warn("Could not find the required first item at y-pos: %1 of section: %2 (%3)! Manually iterating all items...".arg(currentSectionFirstItemPosY)
                                                                                                                                                   .arg(section).arg(this))
                            item = null

                            for (let i = 0; i < tableView_id.count; ++i) {
                                const t = tableView_id.itemAtIndex(i)
                                if (t && (t.ListView.section === section)) {
                                    item = t
                                    break
                                } else if (!t) {
                                    console.debug(this, ": ListView count is %1, but itemAtIndex(%2) returned null!".arg(tableView_id.count).arg(i)) // Too low cacheBuffer?
                                }
                            }

                            if (!item) {
                                console.error(this, ": Could not find the required first item! Try again after increasing the cache buffer of the view.")
                                return
                            }
                        }

                        console.assert(item.index !== undefined)

                        let itemIndex = item.index
                        let targetSectionName

                        // FIXME: We check each item until reaching the next/previous section. This does not mean that the
                        //        whole list is checked, still, this should be removed when Qt provides such functionality.
                        for (let i = itemIndex; forward ? (i < tableView_id.count) : (i >= 0); forward ? ++i : --i) {
                            item = tableView_id.itemAtIndex(i)

                            if (!item) {
                                if (!targetSectionName) {
                                    // This function is called when there is no previous/next section:
                                    console.error(this, ": Expected item at index", i, "does not exist! Try again after increasing the cache buffer of the view.")
                                    return
                                } else {
                                    break
                                }
                            }

                            const currentItemSection = item.ListView.section
                            console.assert(currentItemSection && currentItemSection.length > 0)
                            if (currentItemSection !== section) {
                                itemIndex = i
                                if (forward) {
                                    // First item of the next section.
                                    targetSectionName = currentItemSection
                                    break
                                } else {
                                    if (!targetSectionName) {
                                        targetSectionName = currentItemSection
                                    } else if (currentItemSection !== targetSectionName) {
                                        // First item of the previous section.
                                        ++itemIndex
                                        break
                                    }
                                }
                            }
                        }

                        if (!targetSectionName) {
                            console.error(this, ": Could not find the target section! Possible Qt bug.")
                            return
                        }

                        if (activeFocus) {
                            // If this section has focus, the target section
                            // should also have focus:
                            for (let i = 0; i < _sections.length; ++i) {
                                const targetSection = _sections[i]
                                if (targetSection?.section === targetSectionName) {
                                    targetSection.focus = true
                                    break
                                }
                            }
                        }

                        // FIXME: Not the best approach, but Qt does not seem to provide an alternative.
                        //        Adjusting `contentY` is proved to be unreliable, especially when there
                        //        are sections.
                        // FIXME: Qt does not provide the `contentY` with `positionViewAtIndex()` for us
                        //        to animate. For that reason, we capture the new `contentY`, adjust
                        //        `contentY` to it is old value then enable the animation and set `contentY`
                        //        to its new value.
                        const oldContentY = tableView_id.contentY
                        // NOTE: `positionViewAtIndex()` actually positions the view to the section, so
                        //       we do not need to subtract the section height here:
                        tableView_id.positionViewAtIndex(itemIndex, ListView.Beginning)
                        const newContentY = tableView_id.contentY
                        if (Math.abs(oldContentY - newContentY) >= Number.EPSILON) {
                            tableView_id.contentYBehavior.enabled = false
                            tableView_id.contentY = oldContentY
                            tableView_id.contentYBehavior.enabled = true
                            tableView_id.contentY = newContentY
                            tableView_id.contentYBehavior.enabled = false
                        }
                    }

                    onChangeToPreviousSectionRequested: {
                        changeSection(false)
                    }

                    onChangeToNextSectionRequested: {
                        changeSection(true)
                    }
                }
            }

            useCurrentSectionLabel: false

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            property var _modelSmall: [{
                weight: 1,

                model: {
                    criteria: "title",

                    subCriterias: [ "duration", "album_title" ],

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                weight: 1,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }, {
                weight: 1,

                model: {
                    criteria: "album_title",

                    text: qsTr("Album")
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    text: qsTr("Duration"),

                    showSection: "",

                    headerDelegate: tableColumns.timeHeaderDelegate,
                    colDelegate: tableColumns.timeColDelegate
                }
            }]

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            dragItem: tableDragItem

            rowContextMenu: trackContextMenu

            Navigation.parentItem: root

            Navigation.upAction: function() {
                headerItem.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: root._onNavigationCancel

            onItemDoubleClicked: MediaLib.addAndPlay(model.id)
            onRightClick: trackContextMenu.popup(tableView_id.selectionModel.selectedIndexes, globalMousePos)

            onDragItemChanged: console.assert(tableView_id.dragItem === tableDragItem)

            Behavior on listView.contentY {
                id: contentYBehavior

                enabled: false

                // NOTE: Usage of `SmoothedAnimation` is intentional here.
                SmoothedAnimation {
                    duration: VLCStyle.duration_veryLong
                    easing.type: Easing.InOutSine
                }
            }

            Widgets.MLDragItem {
                id: tableDragItem

                view: tableView_id

                indexes: indexesFlat ? tableView_id.selectionModel.selectedIndexesFlat
                                     : tableView_id.selectionModel.selectedIndexes
                indexesFlat: !!tableView_id.selectionModel.selectedIndexesFlat

                defaultCover: VLCStyle.noArtArtistCover
            }

            Widgets.MLTableColumns {
                id: tableColumns

                showCriterias: (tableView_id.sortModel === tableView_id._modelSmall)
            }
        }
    }

    Loader {
        id: loader

        anchors.fill: parent
        anchors.rightMargin: root.rightPadding

        focus: albumModel.count !== 0
        sourceComponent: MainCtx.gridView ? gridComponent : tableComponent
    }
}
