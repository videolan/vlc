--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.

 $Id$

 Copyright © 2007-2011 the VideoLAN team

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
        and string.match( vlc.path, "www.dailymotion.com/video" )
end

function find( haystack, needle )
    local _,_,ret = string.find( haystack, needle )
    return ret
end

-- Parse function.
function parse()
    prefres = get_prefres()
    while true
    do
        line = vlc.readline()
        if not line then
            break
        end
        if string.match( line, "sequence=")
        then
            line = vlc.strings.decode_uri(line):gsub("\\/", "/")

            arturl = find( line, "\"videoPreviewURL\":\"([^\"]*)\"")
            name = find( line, "\"videoTitle\":\"([^\"]*)\"")
            if name then
                name = string.gsub( name, "+", " " )
            end
            description = find( line, "\"videoDescription\":\"([^\"]*)\"")
            if description then
                description = string.gsub( description, "+", " " )
            end

            for _,param in ipairs({ "hd1080URL", "hd720URL", "hqURL", "sdURL", "video_url" }) do
                path = string.match( line, "\""..param.."\":\"([^\"]*)\"" )
                if path then
                    path = vlc.strings.decode_uri(path)
                    if prefres < 0 then
                        break
                    end
                    height = string.match( path, "/cdn/%w+%-%d+x(%d+)/video/" )
                    if not height then
                        height = string.match( param, "(%d+)" )
                    end
                    if not height or tonumber(height) <= prefres then
                        break
                    end
                end
            end

            if not path then
                break
            end

            return { { path = path; name = name; description = description; url = vlc.path; arturl = arturl } }
        end
    end

    vlc.msg.err("Couldn't extract the video URL from dailymotion")
    return { }
end
