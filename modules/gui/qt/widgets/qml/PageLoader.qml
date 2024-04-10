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
import QtQuick
import org.videolan.vlc 0.1

import "qrc:///util/Helpers.js" as Helpers

StackViewExt {
    id: root

    // Properties

    //name of the loaded page
    property string pageName: ""

    //path of the current page loader
    property var pagePrefix: []

    //list of available pages
    property var pageModel: []

    //indicates whether the subview support grid/list mode
    readonly property bool hasGridListMode: currentItem?.hasGridListMode ?? false

    readonly property bool isSearchable: currentItem?.isSearchable ?? false

    readonly property var sortModel: currentItem?.sortModel ?? null

    //property is *not* readOnly, a PageLoader may define a localMenuDelegate common for its subviews (music, video)
    property Component localMenuDelegate: (currentItem?.localMenuDelegate
                                    && (currentItem.localMenuDelegate instanceof Component)) ? currentItem.localMenuDelegate : null


    // Functions

    /**
     * @arg {string[]} path - the (sub) path to load
     * @arg {Object.<string, Object>} properties - the properties to apply to the loaded view
     * @arg {number} focusReason - the initial focus reason
     */
    function loadView(path, properties, focusReason)
    {
        if (currentItem && typeof currentItem.dismiss === "function")
            currentItem.dismiss()

        if (path.length === 0) {
            const defaultPage = _getDefaultPage()
            if (defaultPage === undefined) {
                console.assert("trying to load an empty view path")
                return false
            }
            path = [defaultPage]
        }

        const head = path[0]

        //We always reload if the last node even if this is the same page, as initial properties may differ
        //for the intermediary pages, we can just forward the request down the tree
        if (pageName === head && path.length > 1) {
            return _reloadPage(path, properties, focusReason)
        }

        let found = false
        for (let tab = 0; tab < pageModel.length; tab++ ) {

            const model = pageModel[tab]
            if (model.name === head) {
                if (model.guard !== undefined && typeof model.guard === "function" && !model.guard(properties)) {
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

                    let pageProp = {
                        pagePrefix:[...pagePrefix, head]
                    }
                    for (const key of properties.keys()) {
                        pageProp[key] = properties[key]
                    }

                    root.replace(null, component, pageProp)
                    found = true
                    break;
                } else {
                    console.warn("component is not ready: " + component.errorString())
                }
            }
        }
        if (!found) {
            console.warn("unable to load view " + head)
            return false
        }

        pageName = head
        currentItem.Navigation.parentItem = root
        //pages like MainDisplay are not page PageLoader, so just check for the loadView function
        if (typeof currentItem.loadView === "function") {
            currentItem.loadView(path.slice(1), properties, focusReason)
        } else {
            setCurrentItemFocus(focusReason)
        }

        return true
    }

    /**
     * @brief return true if the PageLoader is on the default page for
     * the subpath @a path
     * @arg {string[]} path - the (sub) path to check
     * @return {boolean}
     */
    function isDefaulLoadedForPath(path) {
        console.assert(Array.isArray(path))

        let subPageName
        if (path.length === 0) {
            subPageName = _getDefaultPage()
        } else {
            //pops the first element of path, path now contains the tail of the list
            subPageName = path.shift()
        }

        if (subPageName === undefined )
            return false

        if (subPageName !== root.pageName)
            return false

        if (!currentItem)
            return false

        if (typeof currentItem.isDefaulLoadedForPath === "function") {
            return currentItem.isDefaulLoadedForPath(path)
        }

        return path.length === 0
    }

    function _getDefaultPage() {
        for (let tab = 0; tab < pageModel.length; tab++ ) {
            if (pageModel[tab].default) {
                return pageModel[tab].name
            }
        }
        console.assert("no default page set")
        return undefined
    }

    function _reloadPage(path, properties, focusReason)
    {
        if (!currentItem) {
            console.warn("try to update subpage, but page isn't loaded")
            return false
        }

        for (const key of properties.keys()) {
            if (currentItem.hasOwnProperty(key))
                currentItem[key] = properties[key]
        }

        if (typeof currentItem.loadView === "function") {
            currentItem.loadView(path.slice(1), properties, focusReason)
        } else if (path.length > 1) {
            console.warn("unable to load subpath", path.slice(1))
            return false
        }
        return true
    }
}
