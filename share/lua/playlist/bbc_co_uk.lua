--[[
 $Id$

 Copyright © 2008 the VideoLAN team

 Authors: Dominique Leuenberger <dominique-vlc.suse@leuenberger.net>

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
        and string.match( path, "^bbc%.co%.uk/iplayer/.+" )
end

-- Parse function.
function parse()
    p = {}
    while true do
        -- Try to find the video's title
        line = vlc.readline()
        if not line then break end
        if string.match( line, "title: " ) then
            _,_,name = string.find( line, "title: \"(.*)\"" )
        end
        if string.match( line, "metaFile: \".*%.ram\"" ) then
            _,_,video = string.find( line, "metaFile: \"(.-)\"" )
            table.insert( p, { path = video; name = name } )
        end
    end
    return p
end
