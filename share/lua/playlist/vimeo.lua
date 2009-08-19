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
        and string.match( vlc.path, "vimeo.com/%d+" )
            or string.match( vlc.path, "vimeo.com/moogaloop/load" )
end

-- Parse function.
function parse()
    p = {}
    if string.match ( vlc.path, "vimeo.com/%d+" ) then
        print (" vlc path is : " .. vlc.path )
        _,_,id = string.find( vlc.path, "vimeo.com/(.*)")
        print (" id is : " .. id )
        return { { path = "http://vimeo.com/moogaloop/load/clip:" .. id .. "/local/", name = "Vimeo playlist" } }
    end

    if string.match ( vlc.path, "vimeo.com/moogaloop" ) then
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<caption>(.*)</caption>" ) then
                _,_,name = string.find (line, "<caption>(.*)</caption>" )
            end
            -- Try to find id of the video
            _,_,id = string.find (vlc.path, "vimeo.com/moogaloop/load/clip:(.*)/local/")
            -- Try to find image for thumbnail
            if string.match( line, "<thumbnail>(.*)</thumbnail>" ) then
                _,_,arturl = string.find (line, "<thumbnail>(.*)</thumbnail>" )
            end
            -- Try to find request signature (needed to construct video url)
            if string.match( line, "<request_signature>(.*)</request_signature>" ) then
                _,_,rsig = string.find (line, "<request_signature>(.*)</request_signature>" )
            end
            -- Try to find request signature expiration time (needed to construct video url)
            if string.match( line, "<request_signature_expires>(.*)</request_signature_expires>" ) then
                _,_,rsigtime = string.find (line, "<request_signature_expires>(.*)</request_signature_expires>" )
            end
            -- Try to find whether video is HD actually
            if string.match( line, "<isHD>1</isHD>" ) then
                ishd = true
            end
        end
        table.insert( p, { path = "http://vimeo.com/moogaloop/play/clip:"..id.."/"..rsig.."/"..rsigtime; name = name; arturl = arturl } )
        if ishd == true then
            table.insert( p, { path = "http://vimeo.com/moogaloop/play/clip:"..id.."/"..rsig.."/"..rsigtime.."/?q=hd"; name = name.." (HD)"; arturl = arturl } )
        end
    end
    return p
end
