--[[
 $Id$

 Copyright © 2012 the VideoLAN team

 Authors: Cheng Sun <chengsun9atgmail.com>

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
        and string.match( vlc.path, "soundcloud%.com/.+/.+" )
end

-- Parse function.
function parse()
    if string.match ( vlc.path, "soundcloud%.com" ) then
        arturl = nil
        while true do
            line = vlc.readline()
            if not line then break end
            if string.match( line, "window%.SC%.bufferTracks%.push" ) then
                -- all the data is nicely stored on this one line
                _,_,uid,token,name = string.find (line,
                        "window%.SC%.bufferTracks%.push.*" ..
                        "\"uid\":\"([^\"]*)\".*" ..
                        "\"token\":\"([^\"]*)\".*" ..
                        "\"title\":\"([^\"]*)\"")
                -- we only want the first one of these lines
                break
            end
            -- try to get the art url
            if string.match( line, "artwork--download--link" ) then
                _,_,arturl = string.find( line, " href=\"(.*)\" " )
            end
        end
        path = "http://media.soundcloud.com/stream/"..uid.."?stream_token="..token
        return { { path = path; name = name; arturl = arturl } }
    end
    return {}
end
