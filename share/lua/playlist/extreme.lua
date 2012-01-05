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
        and string.match( vlc.path, "extreme%.com/." )
        or string.match( vlc.path, "freecaster%.tv/." )
        or string.match( vlc.path, "player%.extreme%.com/info/.")
end

-- Parse function.
function parse()
    if (string.match( vlc.path, "extreme%.com/." ) or string.match( vlc.path, "freecaster%.tv/." )) and not string.match( vlc.path, "player%.extreme%.com/info/") then
        while true do
            line = vlc.readline()
            if not line then break end
            -- Try to find id of the video
            if string.match( line, "http://player.extreme.com/FCPlayer.swf" ) then
                _,_,vid = string.find( line, "id=(.*)\"" )
                break
            end
        end
        return { { path = "http://player.extreme.com/info/" .. vid; name = "extreme.com video"; } }
    end

    if string.match( vlc.path, "player%.extreme%.com/info/." ) then
        prefres = get_prefres()
        gostraight = true
        while true do
            line = vlc.readline()
            if not line then break end
            -- Try to find the video's title
            if string.match( line, "title>(.*)<" ) then
                _,_,name = string.find( line, "<title>(.*)<" )
            end

            -- Try to find image for thumbnail
            if string.match( line, "<path>(*.)</path>" ) then
                _,_,arturl = string.find( line, "<path>(*.)</path>" )
            end

            -- Try to find out if its a freecaster streaming or just a link to some
            -- other video streaming website
            -- We assume freecaster now streams in http
            if string.match( line, "<streams type=\"5\" server=\"(.*)\">" )
                then
                _,_,videoserver = string.find( line, "<streams type=\"5\" server=\"(.*)\">" )
                gostraight = false
            end

            -- if we're going outside, we need to find out the path
            if gostraight then
                if string.match( line, ">(.*)</stream>" ) then
                    _,_,path = string.find( line, "bitrate=\"0\" duration=\"\">(.*)</stream>" )
                end
            end

            -- and if we're using freecaster, use appropriate resolution
            if not gostraight then
                if string.match( line, "height=\"(.*)\" duration" ) then
                    _,_,height = string.find( line, "height=\"(%d+)\" duration" )
                    _,_,playpath  = string.find( line, "\">(.*)</stream>" )
                    if ( prefres < 0 or tonumber( height ) <= prefres ) then
                        path = videoserver .. playpath
                    end
                end
            end
        end

        return { { path = path; name = name; arturl = arturl } }

    end

end
