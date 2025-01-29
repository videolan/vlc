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

pragma Singleton

import QtQuick

QtObject {

    function clamp(num, min, max) {
      return num <= min ? min : num >= max ? max : num;
    }

    function isValidInstanceOf(object, type) {
        return (!!object && (object instanceof type))
    }

    // NOTE: This allows us to force another 'reason' even when the item has activeFocus.
    function enforceFocus(item, reason) {
        if (item.activeFocus && item.focusReason === reason)
            return

        item.focus = false;

        item.forceActiveFocus(reason);
    }

    function pointInRadius(x, y, radius) {
         return (x * x + y * y < radius * radius)
    }

    // checks if point `pos` lies in rect `rect`
    function contains(rect, pos) {
        return (clamp(pos.x, rect.x, rect.x + rect.width) === pos.x)
                && (clamp(pos.y, rect.y, rect.y + rect.height) === pos.y)
    }

    function isInteger(data) {
        return (typeof data === 'number' && (data % 1) === 0)
    }

    function compareFloat(a, b) {
        return (Math.abs(a - b) < Number.EPSILON)
    }

    function alignUp(a, b) {
        return Math.ceil(a / b) * b
    }

    function alignDown(a, b) {
        return Math.floor(a / b) * b
    }

    function isSortedIntegerArrayConsecutive(array) {
        for (let i = 1; i < array.length; ++i) {
            if ((array[i] - array[i - 1]) !== 1)
                return false
        }

        return true
    }

    function itemsMovable(sortedItemIndexes, targetIndex) {
        return !isSortedIntegerArrayConsecutive(sortedItemIndexes) ||
                (targetIndex > (sortedItemIndexes[sortedItemIndexes.length - 1] + 1) ||
                 targetIndex < sortedItemIndexes[0])
    }

    /**
     * calculate content y for flickable such that item with given param will be fully visible
     * @param type:Flickable flickable
     * @param type:real y
     * @param type:real height
     * @param type:real topMargin
     * @param type:real bottomMargin
     * @return type:real appropriate contentY for flickable
     */
    function flickablePositionContaining(flickable, y, height, topMargin, bottomMargin) {
        const itemTopY = flickable.originY + y
        const itemBottomY = itemTopY + height

        const viewTopY = flickable.contentY
        const viewBottomY = viewTopY + flickable.height

        let newContentY

        if (itemTopY < viewTopY)
             //item above view
            newContentY = itemTopY - topMargin
        else if (itemBottomY > viewBottomY)
             //item below view
            newContentY = itemBottomY + bottomMargin - flickable.height
        else
            newContentY = flickable.contentY

        return newContentY
    }

    function isArray(obj) {
        return (obj?.length !== undefined) ?? false
    }

    // Similar to Item::contains(), but accepts area (rectangle):
    function itemIntersects(item: Item, area: rect) : bool {
        if (!item)
            return false
        if (area.width <= 0.0 || area.height <= 0.0)
            return false
        return !(((item.x < (area.x + area.width)) && ((item.x + item.width) > area.x)) &&
                 ((item.y < (area.y + area.height)) && ((item.y + item.height) > area.y)))
    }
}
