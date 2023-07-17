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

.pragma library

function clamp(num, min, max) {
  return num <= min ? min : num >= max ? max : num;
}

function isValidInstanceOf(object, type) {
    return (!!object && (object instanceof type))
}

// Returns the value associated with the key.
// If the hash contains no item with the key,
// or the value is invalid, returns defaultValue
function get(dict, key, defaultValue) {
    var v = typeof dict !== "undefined" && !!dict ? dict[key] : undefined
    return typeof v === "undefined" ? defaultValue : v
}

// NOTE: This allows us to force another 'reason' even when the item has activeFocus.
function enforceFocus(item, reason) {
    if (item.activeFocus && item.focusReason === reason)
        return

    item.focus = false;

    item.forceActiveFocus(reason);
}

function applyVolume(player, delta) {
    // Degrees to steps for standard mouse
    delta = delta / 8 / 15

    const steps = Math.ceil(Math.abs(delta))

    player.muted = false

    if (delta > 0)
        player.setVolumeUp(steps)
    else
        player.setVolumeDown(steps)
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
    const length = array.length

    if (length <= 1)
        return true

    const first = array[0]
    const last = array[length - 1]

    if (length === 2)
        return Math.abs(first - last) === 1

    const sum1 = array.reduce((i, j) => i + j)
    const sum2 = length * (first + last) / 2

    return (sum1 === sum2)
}

function itemsMovable(sortedItemIndexes, targetIndex) {
    return !isSortedIntegerArrayConsecutive(sortedItemIndexes) ||
            (targetIndex > (sortedItemIndexes[sortedItemIndexes.length - 1] + 1) ||
             targetIndex < sortedItemIndexes[0])
}
