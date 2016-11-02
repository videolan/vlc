--[[
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

-- Probe function.
function probe()
    local path = vlc.path:gsub("^www%.", "")
    return vlc.access == "http"
        and string.match( vlc.path, "^metacafe%.com/" )
        and (  string.match( vlc.path, "watch/" )
            or string.match( vlc.path, "mediaURL=" ) )
end

-- Parse function.
function parse()
    vlc.msg.warn("FIXME")
    if string.match( vlc.path, "watch/" )
    then -- This is the HTML page's URL
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<meta name=\"title\"" ) then
                _,_,name = string.find( line, "content=\"Metacafe %- (.-)\"" )  
            end
            if string.match( line, "<meta name=\"description\"" ) then
                _,_,description = string.find( line, "content=\"(.-)\"" )  
            end
            if string.match( line, "<link rel=\"image_src\"" ) then
                _,_,arturl = string.find( line, "href=\"(.-)\"" )
            end
            if name and description and arturl then break end
        end
        return { { path = string.gsub( vlc.path, "^.*watch/(.*[^/])/?$", "http://www.metacafe.com/fplayer/%1.swf" ); name = name; description = description; arturl = arturl;  } }
    else -- This is the flash player's URL
        local _,_,path = string.find( vlc.path, "mediaURL=([^&]*)" )
        return { { path = path } }
    end
end
