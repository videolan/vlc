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

function msToString(time, short) {
    if (time < 0) return "--:--"

    var t_sec = Math.floor(time / 1000)
    var sec = t_sec % 60
    var min = Math.floor(t_sec / 60) % 60
    var hour = Math.floor(t_sec / 3600)

    function prefixZero(number) {
        return number < 10 ? "0" + number : number;
    }

    if (hour === 0)
        return "%1:%2".arg(prefixZero(min)).arg(prefixZero(sec))
    if (!!short)
        return "%1h%2".arg(hour.toFixed()).arg(prefixZero(min))
    return "%1:%2:%3".arg(prefixZero(hour)).arg(prefixZero(min)).arg(prefixZero(sec))
}

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
    var v = typeof dict !== "undefined" ? dict[key] : undefined
    return !v ? defaultValue : v
}
