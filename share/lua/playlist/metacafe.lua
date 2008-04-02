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
    return vlc.access == "http"
        and string.match( vlc.path, "metacafe.com" ) 
        and (  string.match( vlc.path, "watch/" )
            or string.match( vlc.path, "mediaURL=" ) )
end

-- Parse function.
function parse()
    if string.match( vlc.path, "watch/" )
    then -- This is the HTML page's URL
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<meta name=\"title\"" ) then
                name = string.gsub( line, "^.*content=\"Metacafe %- ([^\"]*).*$", "%1" )  
            end
            if string.match( line, "<meta name=\"description\"" ) then
                description = string.gsub( line, "^.*content=\"([^\"]*).*$", "%1" )  
            end
            if string.match( line, "<link rel=\"image_src\"" ) then
                arturl = string.gsub( line, "^.*href=\"([^\"]*)\".*$", "%1" )
            end
            if name and description and arturl then break end
        end
        return { { path = string.gsub( vlc.path, "^.*watch/(.*[^/])/?$", "http://www.metacafe.com/fplayer/%1.swf" ); name = name; description = description; arturl = arturl;  } }
    else -- This is the flash player's URL
        return { { path = string.gsub( vlc.path, "^.*mediaURL=([^&]*).*$", "%1" ) } }
    end
end
