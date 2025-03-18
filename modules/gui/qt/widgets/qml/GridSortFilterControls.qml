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

    //TODO: visible value of MainCtx.sort.available and MainCtx.hasGridListMode is initialised correctly but on first load still shows up
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

        visible: MainCtx.sort.available
        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        description: qsTr("Sort")

        model: MainCtx.sort.model

        menu: root.sortMenu

        sortKey:  MainCtx.sort.criteria
        sortOrder: MainCtx.sort.order

        onSortSelected: (key) => {
            MainCtx.sort.criteria = key
        }
        onSortOrderSelected: (type) => {
            MainCtx.sort.order = type
        }
    }

    SearchBox {
        id: searchBox

        //TODO: initialise visible value with MainCtx
        visible: MainCtx.search.available
        height: VLCStyle.bannerButton_height
        buttonWidth: VLCStyle.bannerButton_width

        Binding {
            target: MainCtx.search
            property: "pattern"
            value: searchBox.searchPattern
        }

        Connections {
            target: MainCtx.search
            function onAskShow() {
                searchBox.expandAndFocus()
            }
        }
    }
}
