--[[
 $Id$

 Copyright © 2011 the VideoLAN team

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
        and string.match( vlc.path, "zapiks.fr/(.*).html" )
            or string.match( vlc.path, "zapiks.fr/view/." )
            or string.match( vlc.path, "26in.fr/videos/." )
end

-- Parse function.
function parse()
    if string.match ( vlc.path, "zapiks.fr/(.+).html" ) or string.match( vlc.path, "26in.fr/videos/" ) then
        while true do
            line = vlc.readline()
            if not line then break end
            -- Try to find video id number
            if string.match( line, "video_src(.+)file=(%d+)\"" ) then
                _,_,id = string.find( line, "file=(%d+)")
            end
            -- Try to find title
            if string.match( line, "(.*)</title>" ) then
                _,_,name = string.find( line, "(.*)</title>" )
            end
        end
        return { { path = "http://www.zapiks.fr/view/index.php?file=" .. id, name = name } }
    end

    if string.match ( vlc.path, "zapiks.fr/view/." ) then
        prefres = get_prefres()
        while true do
            line = vlc.readline()
            if not line then break end
            -- Try to find URL of video
            if string.match( line, "<file>(.*)</file>" ) then
                _,_,path = string.find ( line, "<file>(.*)</file>" )
            end
            -- Try to find image for arturl
            if string.match( line, "<image>(.*)</image>" ) then
                _,_,arturl = string.find( line, "<image>(.*)</image>" )
            end
            if string.match( line, "title=\"(.*)\"" ) then
                _,_,name = string.find( line, "title=\"(.*)\"" )
            end
            -- Try to find whether video is HD actually
            if( prefres <0 or prefres >= 720 ) then
                if string.match( line, "<hd.file>(.*)</hd.file>" ) then
                    _,_,path = string.find( line, "<hd.file>(.*)</hd.file>" )
                end
            end
        end
        return { { path = path; name = name; arturl = arturl } }
    end
    vlc.msg.err( "Could not extract the video URL from zapiks.fr" )
    return {}
end
