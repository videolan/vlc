/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

TapHandler {
    id: root

    acceptedDevices: PointerDevice.TouchScreen

    grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

    property bool pendingContextMenu: false

    // - Delegate must have property `index`
    // - If `view` is not explicitly adjusted, delegate must
    //   have property `view`, if `{List,Grid,Table}View.view`
    //   is not applicable.
    required property Item delegate

    // TODO: Use `ItemView` type when applicable:
    property Item view: delegate ? (delegate.ListView.view ??
                                    delegate.GridView.view ??
                                    delegate.TableView.view ??
                                    delegate.view ??
                                    null)
                                 : null

    signal contextMenuRequested(int index, point globalPoint)

    onSingleTapped: (eventPoint, button) => {
        initialAction()
    }

    onLongPressed: {
        initialAction()

        pendingContextMenu = true
    }

    function invokeContextMenu(point : point) : bool {
        if (root.delegate && root?.pendingContextMenu) {
            root.contextMenuRequested(root.delegate.index, point)
        }
    }

    onPressedChanged: {
        if (!pressed) {
            // We need to do it asynchronously, because if tapping is canceled we need to acknowledge it:
            Qt.callLater(root.invokeContextMenu, parent.mapToGlobal(point.position.x,
                                                                    point.position.y))
        }
    }

    onCanceled: {
        pendingContextMenu = false
    }

    function initialAction() {
        delegate.forceActiveFocus(Qt.MouseFocusReason)

        // NOTE: Selection is not applicable to touch, so we don't want to manipulate the selection.
        //       That being said, we need to adjust the current index regardless:
        console.assert(root.view)
        root.view.currentIndex = delegate.index
    }
}
