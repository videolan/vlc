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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.SelectableDelegateModel {
    id: delegateModel
    property string viewIndexPropertyName: "currentIndex"
    delegate: Package {
        id: element
        Loader {
            id: delegateLoaderGrid
            focus: true
            Package.name: "grid"
            source: model.type == MLNetworkModel.TYPE_FILE ?
                        "qrc:///mediacenter/NetworkFileDisplayGrid.qml" :
                        "qrc:///mediacenter/NetworkDriveDisplayGrid.qml";
        }

        Loader {
            id: delegateLoader
            focus: true
            Package.name: "list"
            source: model.type == MLNetworkModel.TYPE_FILE ?
                        "qrc:///mediacenter/NetworkFileDisplay.qml" :
                        "qrc:///mediacenter/NetworkDriveDisplay.qml";
        }
        Connections {
            target: delegateLoader.item
            onActionLeft: root.navigationLeft(0)
            onActionRight: root.navigationRight(0)
        }

    }

    function actionAtIndex(index) {
        if ( delegateModel.selectedGroup.count > 1 ) {
            var list = []
            for (var i = 0; i < delegateModel.selectedGroup.count; i++) {
                var type = delegateModel.selectedGroup.get(i).model.type;
                var mrl = delegateModel.selectedGroup.get(i).model.mrl;
                if (type == MLNetworkModel.TYPE_FILE)
                    list.push(mrl)
            }
            medialib.addAndPlay( list )
        } else {
            if (delegateModel.items.get(index).model.type != MLNetworkModel.TYPE_FILE)  {
                console.log("not file")
                root.tree = delegateModel.items.get(index).model.tree
                history.push(["mc", "network", { tree: delegateModel.items.get(index).model.tree }], History.Go);
            } else {
                medialib.addAndPlay( delegateModel.items.get(index).model.mrl );
            }
        }
    }
}
