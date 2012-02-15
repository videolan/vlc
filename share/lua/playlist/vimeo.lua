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

function get_prefres()
    local prefres = -1
    if vlc.var and vlc.var.inherit then
        prefres = vlc.var.inherit(nil, "preferred-resolution")
        if prefres == nil then
            prefres = -1
        end
    end
    return prefres
end

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "vimeo.com/%d+" )
            or string.match( vlc.path, "vimeo.com/moogaloop/load" )
end

-- Parse function.
function parse()
    if string.match ( vlc.path, "vimeo.com/%d+" ) then
        _,_,id = string.find( vlc.path, "vimeo.com/(.*)")
        -- Vimeo disables HD if the user-agent contains "VLC", so we
        -- set it to something inconspicuous. We do it here because
        -- they seem to do some detection across requests
        return { { path = "http://vimeo.com/moogaloop/load/clip:" .. id .. "/local/", name = "Vimeo playlist", options = { ":http-user-agent=Mozilla/5.0 (Windows NT 6.1; rv:6.0.2) Gecko/20100101 Firefox/6.0.2" } } }
    end

    if string.match ( vlc.path, "vimeo.com/moogaloop" ) then
        prefres = get_prefres()
        ishd = false
        -- Try to find id of the video
        _,_,id = string.find (vlc.path, "vimeo.com/moogaloop/load/clip:(.*)/local/")
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<caption>(.*)</caption>" ) then
                _,_,name = string.find (line, "<caption>(.*)</caption>" )
            end
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
            if string.match( line, "<height>%d+</height>" ) then
                _,_,height = string.find( line, "<height>(%d+)</height>" )
            end
        end
        path = "http://vimeo.com/moogaloop/play/clip:"..id.."/"..rsig.."/"..rsigtime
        if ishd and ( not height or prefres < 0 or prefres >= tonumber(height) ) then
            path = path.."/?q=hd"
        end
        return { { path = path; name = name; arturl = arturl } }
    end
    return {}
end
