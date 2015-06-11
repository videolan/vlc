--[[
 Gets an artwork for french TV channels

 $Id$
 Copyright © 2007 the VideoLAN team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]]

function descriptor()
    return { scope="local" }
end

function trim (s)
  return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

function read_meta()
    local metas = vlc.item:metas()

    -- Don't do anything if there is already a title
    if metas["title"] then
        return
    end

    local name = metas["filename"];
    if not name then
        return
    end

    -- Find "Show.Name.S01E12-blah.avi"
    local title, seasonNumber
    _, _, showName, seasonNumber, episodeNumber = string.find(name, "(.+)S(%d+)E(%d+).*")
    if not showName then
        return
    end

    -- Remove . in showName
    showName = trim(string.gsub(showName, "%.", " "))
    vlc.item:set_meta("title", showName.." S"..seasonNumber.."E"..episodeNumber)
    vlc.item:set_meta("showName", showName)
    vlc.item:set_meta("episodeNumber", episodeNumber)
    vlc.item:set_meta("seasonNumber", seasonNumber)
end
