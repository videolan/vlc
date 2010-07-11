--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.

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
        and string.match( vlc.path, "dailymotion." )
        and string.match( vlc.peek( 2048 ), "<!DOCTYPE.*video_type" )
end

function find( haystack, needle )
    local _,_,ret = string.find( haystack, needle )
    return ret
end

-- Parse function.
function parse()
    while true
    do
        line = vlc.readline()
        if not line
        then
            vlc.msg.err("Couldn't extract the video URL from dailymotion")
            return { }
        end
        if string.match( line, "\"sequence\",")
        then
            line = vlc.strings.decode_uri(line):gsub("\\/", "/")

            arturl = find( line, "\"videoPreviewURL\":\"([^\"]*)\"")
            name = find( line, "\"videoTitle\":\"([^\"]*)\"")
            description = find( line, "\"videoDescription\":\"([^\"]*)\"")

           --[[ we get a list of different streams available, at various codecs
                and resolutions:

                Ideally, VLC would propose the different streams available,
                codecs and resolutions (the resolutions are part of the URL)

                For now we just built a list of preferred codecs : lowest value
                means highest priority
             ]]--

            -- FIXME: the hd/hq versions (in mp4) cause a lot of seeks,
            -- for now we only get the sd (in flv) URL

            -- if not path then path = find( line, "\"hqURL\":\"([^\"]*)\"") end
            -- if not path then path = find( line, "\"hdURL\":\"([^\"]*)\"") end
            if not path then path = find( line, "\"sdURL\":\"([^\"]*)\"") end

            return { { path = path; name = name; description = description; url = vlc.path; arturl = arturl } }
        end
    end
end
