--[[
 $Id$

 Copyright © 2009 the VideoLAN team

 Authors: Konstantin Pavlov (thresh@videolan.org)

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
        and string.match( vlc.path, "video.mpora.com/watch/" )
end

-- Parse function.
function parse()
    p = {}
    while true do
        -- Try to find the video's title
        line = vlc.readline()
        if not line then break end
        if string.match( line, "meta name=\"title\"" ) then
            _,_,name = string.find( line, "content=\"(.*)\" />" )
        end
        if string.match( line, "image_src" ) then
            _,_,arturl = string.find( line, "image_src\" href=\"(.*)\" />" )
        end

        if string.match( line, "filmID" ) then
            _,_,video = string.find( line, "var filmID = \'(.*)\';")
            table.insert( p, { path = "http://cdn0.mpora.com/play/video/"..video.."/mp4/"; name = name; arturl = arturl } )
        end
        if string.match( line, "definitionLink hd" ) then
            table.insert( p, { path = "http://cdn0.mpora.com/play/video/"..video.."_hd/mp4/"; name = name.." (HD)", arturl = arturl } )
        end
    end
    return p
end
