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
        and string.match( vlc.path, "dailymotion.com" ) 
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
        if not line then break end
        if string.match( line, "param name=\"flashvars\" value=\".*video=" )
        then
            arturl = vlc.strings.decode_uri( find( line, "param name=\"flashvars\" value=\".*preview=([^&]*)" ) )
            videos = vlc.strings.decode_uri( find( line, "param name=\"flashvars\" value=\".*video=([^&]*)" ) )
       --[[ we get a list of different streams available, at various codecs
            and resolutions:
            /A@@spark||/B@@spark-mini||/C@@vp6-hd||/D@@vp6||/E@@h264
            Not everybody can decode HD, not everybody has a 80x60 screen,
            H264/MP4 is buggy , so i choose VP6 as the highest priority

            Ideally, VLC would propose the different streams available, codecs
            and resolutions (the resolutions are part of the URL)

            For now we just built a list of preferred codecs : lowest value
            means highest priority
         ]]
            local pref = { ["vp6"]=0, ["spark"]=1, ["h264"]=2, ["vp6-hd"]=3, ["spark-mini"]=4 }
            local available = {}
            for n in string.gmatch(videos, "[^|]+") do
                i = string.find(n, "@@")
                if i then
                    available[string.sub(n, i+2)] = string.sub(n, 0, i-1)
                end
            end
            local score = 666
            local bestcodec
            for codec,_ in pairs(available) do
                if pref[codec] == nil then
                    vlc.msg.warn( "Unknown codec: " .. codec )
                    pref[codec] = 42 -- try the 1st unknown codec if other fail
                end
                if pref[codec] < score then
                    bestcodec = codec
                    score = pref[codec]
                end
            end
            if bestcodec then
                path = "http://dailymotion.com" .. available[bestcodec]
            end
        end
        if string.match( line, "<meta name=\"title\"" )
        then
            name = vlc.strings.resolve_xml_special_chars( find( line, "name=\"title\" content=\"(.-)\"" ) )
        end
        if string.match( line, "<meta name=\"description\"" )
        then
            description = vlc.strings.resolve_xml_special_chars( find( line, "name=\"description\" content=\"(.-)\"" ) )
        end
        if path and name and description and arturl then break end
    end
    return { { path = path; name = name; description = description; url = vlc.path; arturl = arturl } }
end
