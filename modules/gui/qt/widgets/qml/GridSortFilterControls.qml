import QtQuick

import VLC.MainInterface
import VLC.Style
import VLC.Util
import VLC.Menus

NavigableRow {
    id: root

    spacing: VLCStyle.margin_normal
    enabled: gridListBtn.visible || sortControl.visible || searchBox.visible

    property SortMenu sortMenu: null
    required property SortCtx sort
    required property SearchCtx search


    //TODO: visible value of root.sort.available and MainCtx.hasGridListMode is initialised correctly but on first load still shows up
    IconToolButton {
        id: gridListBtn

        visible: MainCtx.hasGridListMode

        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        text: MainCtx.gridView ? VLCIcons.list : VLCIcons.grid
        description: qsTr("List/Grid")
        onClicked: MainCtx.gridView = !MainCtx.gridView
    }

    SortControl {
        id: sortControl

        visible: root.sort.available
        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        description: qsTr("Sort")

        model: root.sort.model

        menu: root.sortMenu

        sortKey:  root.sort.criteria
        sortOrder: root.sort.order

        onSortSelected: (key) => {
            root.sort.criteria = key
        }
        onSortOrderSelected: (type) => {
            root.sort.order = type
        }
    }

    SearchBox {
        id: searchBox

        //TODO: initialise visible value with MainCtx
        visible: root.search.available
        height: VLCStyle.bannerButton_height
        buttonWidth: VLCStyle.bannerButton_width

        Binding {
            target: root.search
            property: "pattern"
            value: searchBox.searchPattern
        }

        Connections {
            target: root.search
            function onAskShow() {
                searchBox.expandAndFocus()
            }
        }
    }
}
