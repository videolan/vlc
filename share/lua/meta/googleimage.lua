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
    if vlc.artist and vlc.album then
        title = vlc.artist.." "..vlc.album
    elseif vlc.title and vlc.artist then
        title = vlc.artist.." "..vlc.title
    elseif vlc.title then
        title = vlc.title
    elseif vlc.name then
        title = vlc.name
    else
        return nil
    end
    fd = vlc.stream( "http://images.google.com/images?q=" .. get_query( title ) )
    page = fd:read( 65653 )
    fd = nil
    _, _, arturl = string.find( page, "imgurl=([^&]+)" )
    if arturl then
        return vlc.strings.decode_uri(arturl)
    else
        return nil
    end
end
