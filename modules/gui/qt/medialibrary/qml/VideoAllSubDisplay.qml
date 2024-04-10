/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

VideoAll {
    id: root

    // Properties

    // NOTE: We are exposing a custom SortMenu with grouping options.
    property SortMenuVideo sortMenu: SortMenuVideo {
        ctx: MainCtx

        onGrouping: MainCtx.grouping = grouping
    }

    // Private

    property var _meta: null

    // Signals

    signal showList(var model, int reason)

    // Settings

    model: !!_meta ? _meta.model : null

    contextMenu: Util.MLContextMenu { model: _meta ? _meta.model : null; showPlayAsAudioAction: true }

    gridLabels: !!_meta ? _meta.gridLabels : root.getLabel

    listLabels: !!_meta ? _meta.listLabels : root.getLabel

    sectionProperty: !!_meta && !!_meta.sectionProperty ? _meta.sectionProperty : ""

    headerPositioning: headerItem.model.count > 0 ? ListView.InlineHeader : ListView.OverlayHeader

    // Functions

    function _updateMetaModel(groupping) {
        if (root._meta)
            root._meta.destroy()

        if (groupping === MainCtx.GROUPING_NAME) {
            root._meta = groupComponent.createObject(root)
        } else if (groupping === MainCtx.GROUPING_FOLDER) {
            root._meta = folderComponent.createObject(root)
        } else {
            root._meta = videoComponent.createObject(root)
        }
    }

    function getLabelGroup(model, string) {
        if (!model) return ""

        const count = model.count

        if (count === 1) {
            return getLabel(model)
        } else {
            if (count < 100)
                return [ string.arg(count) ]
            else
                return [ string.arg("99+") ]
        }
    }

    // VideoAll events reimplementation

    function onAction(indexes) { _meta.onAction(indexes) }

    function onDoubleClick(object) { _meta.onDoubleClick(object) }

    function isInfoExpandPanelAvailable(modelIndexData) {
        return _meta.isInfoExpandPanelAvailable(modelIndexData)
    }

    // Children

    Connections {
        target: MainCtx
        function onGroupingChanged() {
            root._updateMetaModel(MainCtx.grouping)
        }
    }

    Component.onCompleted: root._updateMetaModel(MainCtx.grouping)

    Component {
        id: videoComponent

        QtObject {
            id: metaVideo

            property var model: MLVideoModel {
                ml: MediaLib
                searchPattern: MainCtx.search.pattern
                sortOrder: MainCtx.sort.order
                sortCriteria: MainCtx.sort.criteria
            }

            property var gridLabels: root.getLabel

            property var listLabels: root.getLabel

            property string sectionProperty: {
                switch (model.sortCriteria) {
                case "title":
                    return "title_first_symbol"
                default:
                    return ""
                }
            }

            function onAction(indexes) {
                model.addAndPlay( indexes )
                History.push(["player"])
            }

            function onDoubleClick(object) {
                MediaLib.addAndPlay(object.id)
                History.push(["player"])
            }

            function isInfoExpandPanelAvailable(modelIndexData) { return true }
        }
    }

    Component {
        id: groupComponent

        QtObject {
            id: metaGroup

            property var model: MLVideoGroupsModel {
                ml: MediaLib
                searchPattern: MainCtx.search.pattern
                sortOrder: MainCtx.sort.order
                sortCriteria: MainCtx.sort.criteria
            }

            property string sectionProperty: {
                switch (model.sortCriteria) {
                case "title":
                    return "group_title_first_symbol"
                default:
                    return ""
                }
            }

            property var gridLabels: function (model) {
                return root.getLabelGroup(model, qsTr("%1 Videos"))
            }

            property var listLabels: function (model) {
                return root.getLabelGroup(model, qsTr("%1"))
            }

            function onAction(indexes) {
                const index = indexes[0]

                const object = model.getDataAt(index);

                if (object.isVideo) {
                    model.addAndPlay( indexes )
                    History.push(["player"])

                    return
                }

                root.showList(object, Qt.TabFocusReason)
            }

            function onDoubleClick(object) {
                if (object.isVideo) {
                    MediaLib.addAndPlay(object.id)
                    History.push(["player"])
                    return
                }

                root.showList(object, Qt.MouseFocusReason)
            }

            function isInfoExpandPanelAvailable(modelIndexData) {
                return modelIndexData.isVideo
            }
        }
    }

    Component {
        id: folderComponent

        QtObject {
            id: metaFolder

            property var model: MLVideoFoldersModel {
                ml: MediaLib
                searchPattern: MainCtx.search.pattern
                sortOrder: MainCtx.sort.order
                sortCriteria: MainCtx.sort.criteria
            }

            property string sectionProperty: {
                switch (model.sortCriteria) {
                case "title":
                    return "title_first_symbol"
                default:
                    return ""
                }
            }

            property var gridLabels: function (model) {
                return root.getLabelGroup(model, qsTr("%1 Videos"))
            }

            property var listLabels: function (model) {
                return root.getLabelGroup(model, qsTr("%1"))
            }

            function onAction(indexes) {
                const index = indexes[0]

                root.showList(model.getDataAt(index), Qt.TabFocusReason)
            }

            function onDoubleClick(object) {
                root.showList(object, Qt.MouseFocusReason)
            }

            function isInfoExpandPanelAvailable(modelIndexData) {
                return false
            }
        }
    }

    header: VideoRecentVideos {
        width: root.width

        leftPadding: root.leftPadding
        rightPadding: root.rightPadding

        nbItemPerRow: Helpers.get(root.currentItem, "nbItemPerRow", 0)

        subtitleText: (root.model && root.model.count > 0) ? qsTr("Videos") : ""

        Navigation.parentItem: root

        Navigation.downAction: function() {
            currentItem.setCurrentItemFocus(Qt.TabFocusReason)
        }

        onImplicitHeightChanged: {
            // implicitHeight depends on underlying ml model initialization
            // and may update after view did resetFocus on initialization which
            // will break resetFocus's scrolling (because header height changed)
            // try to reapply reset focus here (ref #27071)
            if (root.currentIndex <= 0 || root.currentIndex === root.initialIndex)
                root.resetFocus()
        }
    }
}
