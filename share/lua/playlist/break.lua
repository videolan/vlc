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
        and ( string.match( vlc.path, "^break.com" )
           or string.match( vlc.path, "^www.break.com" ) )
end

-- Parse function.
function parse()
    filepath = ""
    filename = ""
    filetitle = ""
    arturl = ""
    while true do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "sGlobalContentFilePath=" ) then
            _,_,filepath= string.find( line, "sGlobalContentFilePath='(.-)'" )
        end
        if string.match( line, "sGlobalFileName=" ) then
            _,_,filename = string.find( line, ".*sGlobalFileName='(.-)'")
        end
        if string.match( line, "sGlobalContentTitle=" ) then
            _,_,filetitle = string.find( line, "sGlobalContentTitle='(.-)'")
        end
        if string.match( line, "el=\"videothumbnail\" href=\"" ) then
            _,_,arturl = string.find( line, "el=\"videothumbnail\" href=\"(.-)\"" )
        end
        if string.match( line, "videoPath" ) then
            _,_,videopath = string.find( line, ".*videoPath', '(.-)'" )
            return { { path = videopath..filepath.."/"..filename..".flv"; title = filetitle; arturl = arturl } }
        end
    end
end
