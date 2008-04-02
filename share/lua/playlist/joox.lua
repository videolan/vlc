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
        and string.match( vlc.path, "joox.net" ) or
            string.match( vlc.path, "/iframe.php%?video=1&" )
end

-- Parse function.
function parse()
    vidtitle = ""
    while true do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "iframe" ) then
            -- extract the iframe
            print((string.gsub( line, ".*iframe src=\"([^\"]*).*", "%1" ) ))
            return { { path = (string.gsub( line, ".*iframe src=\"([^\"]*).*", "%1" )) } }
        end
        if string.match( line, "<param name=\"src" ) then
            -- extract the video url from the iframe
            print( (string.gsub( line, ".*src\" value=\"([^\"]*).*", "%1" )))
            return { { path = (string.gsub( line, ".*src\" value=\"([^\"]*).*", "%1" )) } }
        end
    end
end
