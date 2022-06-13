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

import QtQuick 2.11

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
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

    anchors.fill: parent

    model: !!_meta ? _meta.model : null

    contextMenu: Util.MLContextMenu { model: _meta ? _meta.model : null; showPlayAsAudioAction: true }

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

        var count = model.count

        if (count === 1) {
            return getLabel(model)
        } else {
            if (count < 100)
                return [ string.arg(count) ]
            else
                return [ string.arg("99+") ]
        }
    }

    // VideoAll reimplementation

    function setCurrentItemFocus(reason) {
        if (headerItem && headerItem.focus)
            headerItem.forceActiveFocus(reason)
        else
            _currentView.setCurrentItemFocus(reason)
    }

    // VideoAll events reimplementation

    function onAction(indexes) { _meta.onAction(indexes) }

    function onDoubleClick(object) { _meta.onDoubleClick(object) }

    function onLabelGrid(object) { return _meta.onLabelGrid(object) }
    function onLabelList(object) { return _meta.onLabelList(object) }

    function isInfoExpandPanelAvailable(modelIndexData) {
        return _meta.isInfoExpandPanelAvailable(modelIndexData)
    }

    // Children

    Connections {
        target: MainCtx
        onGroupingChanged: root._updateMetaModel(MainCtx.grouping)
    }

    Component.onCompleted: root._updateMetaModel(MainCtx.grouping)

    Component {
        id: videoComponent

        QtObject {
            id: metaVideo

            property var model: MLVideoModel { ml: MediaLib }

            function onAction(indexes) {
                g_mainDisplay.showPlayer()

                MediaLib.addAndPlay(model.getIdsForIndexes(indexes))
            }

            function onDoubleClick(object) { g_mainDisplay.play(MediaLib, object.id) }

            function onLabelGrid(object) { return root.getLabel(object) }
            function onLabelList(object) { return root.getLabel(object) }

            function isInfoExpandPanelAvailable(modelIndexData) { return true }
        }
    }

    Component {
        id: groupComponent

        QtObject {
            id: metaGroup

            property var model: MLVideoGroupsModel { ml: MediaLib }

            function onAction(indexes) {
                var index = indexes[0]

                var object = model.getDataAt(index);

                if (object.isVideo) {
                    g_mainDisplay.showPlayer()

                    MediaLib.addAndPlay(model.getIdsForIndexes(indexes))

                    return
                }

                root.showList(object, Qt.TabFocusReason)
            }

            function onDoubleClick(object) {
                if (object.isVideo) {
                    g_mainDisplay.play(MediaLib, object.id)

                    return
                }

                root.showList(object, Qt.MouseFocusReason)
            }

            function onLabelGrid(object) {
                return root.getLabelGroup(object, I18n.qtr("%1 Videos"))
            }

            function onLabelList(object) {
                return root.getLabelGroup(object, I18n.qtr("%1"))
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

            property var model: MLVideoFoldersModel { ml: MediaLib }

            function onAction(indexes) {
                var index = indexes[0]

                root.showList(model.getDataAt(index), Qt.TabFocusReason)
            }

            function onDoubleClick(object) {
                root.showList(object, Qt.MouseFocusReason)
            }

            function onLabelGrid(object) {
                return root.getLabelGroup(object, I18n.qtr("%1 Videos"))
            }

            function onLabelList(object) {
                return root.getLabelGroup(object, I18n.qtr("%1"))
            }

            function isInfoExpandPanelAvailable(modelIndexData) {
                return false
            }
        }
    }

    header: VideoDisplayRecentVideos {
        width: root.width

        subtitleText: (root.model && root.model.count > 0) ? I18n.qtr("Videos") : ""

        // NOTE: We want grid items to be visible on the sides.
        leftPadding: root.contentMargin

        Navigation.parentItem: root

        Navigation.downAction: function() {
            _currentView.setCurrentItemFocus(Qt.TabFocusReason);
        }

    }
}
