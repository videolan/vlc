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

-- Return the artwork
function fetch_art()
    if vlc.item == nil then return nil end

    local meta = vlc.item:metas()

-- Radio Entries
    if meta["Listing Type"] == "radio"
    then
        title = meta["title"] .. " radio logo"
-- TV Entries
    elseif meta["Listing Type"] == "tv"
    then
        title = meta["title"] .. " tv logo"
-- Album entries
    elseif meta["artist"] and meta["album"] then
        title = meta["artist"].." "..meta["album"].." cover"
    elseif meta["artist"] and meta["title"] then
        title = meta["artist"].." "..meta["title"].." cover"
    else
        return nil
    end
    fd = vlc.stream( "http://images.google.com/images?q="..vlc.strings.encode_uri_component( title ) )
    if not fd then return nil end

    page = fd:read( 65653 )
    fd = nil
    _, _, arturl = string.find( page, "imgurl=([^&]*)" )
    return arturl
end
