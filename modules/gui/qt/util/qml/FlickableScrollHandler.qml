/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import QtQml 2.11
import QtQml.Models 2.11

import org.videolan.vlc 0.1 as VLC

VLC.FlickableScrollHandler {
    id: handler

    scaleFactor: VLC.MainCtx.intfScaleFactor

    enabled: !VLC.MainCtx.smoothScroll

    Component.onCompleted: {
        if (!enabled) {
            // QTBUG-56075
            // Note that this workaround is not effective when enabled dynamically changes
            var qtVersion = VLC.MainCtx.qtVersion()
            if ((qtVersion >= VLC.MainCtx.qtVersionCheck(6, 0, 0) && qtVersion < VLC.MainCtx.qtVersionCheck(6, 2, 0)) ||
                (qtVersion < VLC.MainCtx.qtVersionCheck(5, 15, 8))) {
                handler.enabled = true
                var smoothScroll = Qt.binding(function() { return VLC.MainCtx.smoothScroll })
                handler.handleOnlyPixelDelta = smoothScroll
                _behaviorAdjuster.when = smoothScroll
                _behaviorAdjuster.model.append( {property: "flickDeceleration", value: 3500} )
            }
        }
    }

    readonly property MultipleBinding _behaviorAdjuster: MultipleBinding {
        target: handler.parent
        when: !handler.enabled

        model: ListModel {
            ListElement {property: "boundsBehavior"; value: 0 /* Flickable.StopAtBounds */}
        }
    }
}
