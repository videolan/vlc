--[[
 $Id$

 Copyright © 2009 the VideoLAN team

 Authors: Konstantin Pavlov (thresh@videolan.org)
          François Revol (revol@free.fr)

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
        and string.match( vlc.path, "vimeo.com/%d+$" )
        -- do not match other addresses,
        -- else we'll also try to decode the actual video url
end

-- Parse function.
function parse()
    agent = vlc.var.inherit(nil,"http-user-agent")

    if string.match( string.lower(agent), ".*vlc.*" ) then
        vlc.msg.dbg("Wrong agent, adapting...")
        return { { path = vlc.access .. "://" .. vlc.path; options = {":http-user-agent=Mozilla/5.0" } } }
    end

    _,_,id = string.find( vlc.path, "vimeo.com/([0-9]*)")
    prefres = get_prefres()
    ishd = false
    quality = "sd"
    codec = nil
    line2 = ""
    while true do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "{config:.*") then
                line2 = line;
                while not string.match( line2, "}};") do
                        line2 = vlc.readline()
                        if not line2 then break end
                        line = line .. line2;
                end
        end
        -- Try to find the video's title
        if string.match( line, "<meta property=\"og:title\"" ) then
            _,_,name = string.find (line, "content=\"(.*)\">" )
        end
        if string.match( line, "{config:.*\"title\":\"" ) then
            _,_,name = string.find (line, "\"title\":\"([^\"]*)\"," )
        end
        -- Try to find image for thumbnail
        if string.match( line, "<meta property=\"og:image\"" ) then
            _,_,arturl = string.find (line, "content=\"(.*)\">" )
        end
        if string.match( line, "<meta itemprop=\"thumbnailUrl\"" ) then
            _,_,arturl = string.find (line, "content=\"(.*)\">" )
        end
        -- Try to find duration
        if string.match( line, "{config:.*\"duration\":" ) then
            _,_,duration = string.find (line, "\"duration\":([0-9]*)," )
        end
        -- Try to find request signature (needed to construct video url)
        if string.match( line, "{config:.*\"signature\":" ) then
            _,_,rsig = string.find (line, "\"signature\":\"([0-9a-f]*)\"," )
        end
        -- Try to find request signature time (needed to construct video url)
        if string.match( line, "{config:.*\"timestamp\":" ) then
            _,_,tstamp = string.find (line, "\"timestamp\":([0-9]*)," )
        end
        -- Try to find the available codecs
        if string.match( line, "{config:.*,\"files\":{\"vp6\":" ) then
            codec = "vp6"
        end
        if string.match( line, "{config:.*,\"files\":{\"vp8\":" ) then
            codec = "vp8"
        end
        if string.match( line, "{config:.*,\"files\":{\"h264\":" ) then
            codec = "h264"
        end
        -- Try to find whether video is HD actually
        if string.match( line, "{config:.*,\"hd\":1" ) then
            ishd = true
        end
        if string.match( line, "{config:.*\"height\":" ) then
            _,_,height = string.find (line, "\"height\":([0-9]*)," )
        end
        if not line2 then break end
    end

    if not codec then
        vlc.msg.err("unable to find codec info")
        return {}
    end

    if ishd and ( not height or prefres < 0 or prefres >= tonumber(height) ) then
        quality = "hd"
    end
    path = "http://player.vimeo.com/play_redirect?quality="..quality.."&codecs="..codec.."&clip_id="..id.."&time="..tstamp.."&sig="..rsig.."&type=html5_desktop_local"
    return { { path = path; name = name; arturl = arturl; duration = duration } }
end
