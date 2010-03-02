--[[
 Gets an artwork from images.google.com

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

-- Replace non alphanumeric char by +
function get_query( title )
    -- If we have a .EXT remove the extension.
    str = string.gsub( title, "(.*)%....$", "%1" )
    return string.gsub( str, "([^%w ])",
         function (c) return string.format ("%%%02X", string.byte(c)) end)
end

-- Return the artwork
function fetch_art()
    -- This is disabled because we have too much false positive by the inherent nature of this script.
    if true then vlc.msg.dbg("10_googleimage.lua is disabled") return nil end

    if vlc.input == nil then return nil end

    local item = vlc.input.item()
    local meta = item:metas()
    if meta["artist"] and meta["album"] then
        title = meta["artist"].." "..meta["album"]
    elseif meta["artist"] and meta["title"] then
        title = meta["artist"].." "..meta["title"]
    elseif meta["title"] then
        title = meta["title"]
    elseif meta["filename"] then
        title = meta["filename"]
    else
        vlc.msg.err("No filename")
        return nil
    end
    fd = vlc.stream( "http://images.google.com/images?q=" .. get_query( title ) )
    if not fd then return nil end

    page = fd:read( 65653 )
    fd = nil
    _, _, arturl = string.find( page, "imgurl=([^&]+)" )
    return arturl
end
