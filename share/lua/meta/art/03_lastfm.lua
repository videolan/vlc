--[[
 Gets an artwork from last.fm

 $Id$
 Copyright © 2010 the VideoLAN team

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

    if meta["Listing Type"] == "radio"
    or meta["Listing Type"] == "tv"
    then return nil end

    if meta["artist"] and meta["album"] then
        title = meta["artist"].."/"..meta["album"]
    else
        return nil
    end
    -- remove -.* from string
    title = string.gsub( title, " ?%-.*", "" )
    -- remove (info..) from string
    title = string.gsub( title, "%(.*%)", "" )
    -- remove CD2 etc from string
    title = string.gsub( title, "CD%d+", "" )
    -- remove Disc \w+ from string
    title = string.gsub( title, "Disc %w+", "" )
    fd = vlc.stream( "http://www.last.fm/music/" .. title )
    if not fd then return nil end
    page = fd:read( 65653 )
    fd = nil
    _, _, arturl = string.find( page, "<img  width=\"174\" src=\"([^\"]+)\" class=\"art\" />\n" )
    -- Don't use default album-art (not found one)
    if not arturl or string.find( arturl, "default_album_mega.png") then
       return nil
    end
    return arturl
end
