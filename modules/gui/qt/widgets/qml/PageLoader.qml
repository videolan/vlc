/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick 2.12
import org.videolan.vlc 0.1

StackViewExt {
    id: root

    // Properties

    property var view: null

    property var pageModel: []

    // loadDefaultView - function ()
    // a function that loads the default page,
    // must be implemented by the user of the class
    // one may use `loadPage(string pageName)` to load the page from 'pageModel'
    property var loadDefaultView: null

    //indicates whether the subview support grid/list mode
    readonly property bool hasGridListMode: (currentItem
                                            && currentItem.hasGridListMode !== undefined
                                            && currentItem.hasGridListMode)

    readonly property bool isSearchable: (currentItem
                                    && currentItem.isSearchable !== undefined
                                    && currentItem.isSearchable)

    readonly property var sortModel: (currentItem
                                    && currentItem.sortModel !== undefined) ? currentItem.sortModel : null

    //property is *not* readOnly, a PageLoader may define a localMenuDelegate common for its subviews (music, video)
    property Component localMenuDelegate: (currentItem
                                    && currentItem.localMenuDelegate
                                    && (currentItem.localMenuDelegate instanceof Component)) ? currentItem.localMenuDelegate : null

    // Private

    property bool _ready: false

    property string _currentView: ""

    // Signals

    signal pageChanged(string page)

    // Events

    Component.onCompleted: {
        _ready = true

        _loadView()
    }

    onViewChanged: _loadView()

    // Functions

    function _loadView() {
        // NOTE: We wait for the item to be fully loaded to avoid size glitches.
        if (_ready === false)
            return

        if (view === null) {
            if (!loadDefaultView)
                console.error("both 'view' and 'loadDefaultView' is null, history -", JSON.stringify(History.current))
            else
                loadDefaultView()
            return
        }

        if (view.name === "") {
            console.error("view is not defined")
            return
        }
        if (pageModel.length === 0) {
            console.error("pageModel is not defined")
            return
        }

        const reason = History.takeFocusReason()

        const found = root.loadView(root.pageModel, view.name, view.properties)
        if (!found) {
            console.error("failed to load", JSON.stringify(History.current))
            return
        }

        currentItem.Navigation.parentItem = root

        if (reason !== Qt.OtherFocusReason)
            setCurrentItemFocus(reason)

        currentItemChanged(currentItem)
    }

    function loadPage(page) {
        view = {"name": page, "properties": {}}
    }

    /**
     * viewModel: model with the definition of the available view
     *            elements should contains at least :
     *     name: name of the view
     *     url or component: the url of the Component or the component to load
     * view: string (name of the view to load)
     * viewProperties: map of the propertes to apply to the view
     */
    function loadView(viewModel, view, viewProperties)
    {
        if (root.currentItem && root.currentItem.hasOwnProperty("dismiss"))
            root.currentItem.dismiss()

        if (view === _currentView) {
            if (Object.keys(viewProperties).length === 0 && root.currentItem.hasOwnProperty("loadDefaultView") ) {
                root.currentItem.loadDefaultView()
            } else {
                for ( let viewProp in viewProperties ) {
                    if ( root.currentItem.hasOwnProperty(viewProp) ) {
                        root.currentItem[viewProp] = viewProperties[viewProp]
                    }
                }
            }
            return true
        }

        let found = false
        for (let tab = 0; tab < viewModel.length; tab++ )
        {
            const model = viewModel[tab]
            if (model.name === view) {
                if (model.guard !== undefined && typeof model.guard === "function" && !model.guard(viewProperties)) {
                    continue //we're not allowed to load this page
                }

                //we can't use push(url, properties) as Qt interprets viewProperties
                //as a second component to load
                let component = undefined
                if (model.component) {
                    component = model.component
                } else if ( model.url ) {
                    component = Qt.createComponent(model.url)
                } else {
                    console.warn( "you should define either component or url of the view to load" )
                    return false
                }

                if (component.status === Component.Ready ) {
                    //note doesn't work with qt 5.9
                    root.replace(null, component, viewProperties)
                    found = true
                    break;
                } else {
                    console.warn("component is not ready: " + component.errorString())
                }
            }
        }
        if (!found)
            console.warn("unable to load view " + view)
        else
            _currentView = view
        return found
    }
}
