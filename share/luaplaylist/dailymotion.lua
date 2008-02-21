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
        and string.match( vlc.peek( 256 ), "<!DOCTYPE.*<title>Video " )
end

-- Parse function.
function parse()
    while true
    do 
        line = vlc.readline()
        if not line then break end
        if string.match( line, "param name=\"flashvars\" value=\".*video=" )
        then
            arturl = vlc.decode_uri( string.gsub( line, "^.*param name=\"flashvars\" value=\".*preview=([^&]*).*$", "%1" ) )
            videos = vlc.decode_uri( string.gsub( line, "^.*param name=\"flashvars\" value=\".*video=([^&]*).*$", "%1" ) )
       --[[ we get a list of different streams available, at various codecs
            and resolutions:
            /A@@spark||/B@@spark-mini||/C@@vp6-hd||/D@@vp6||/E@@h264
            Not everybody can decode HD, not everybody has a 80x60 screen,
            H264/MP4 is buggy , so i choose VP6

            Ideally, VLC would propose the different streams available, codecs
            and resolutions (the resolutions are part of the URL)
         ]]
            for n in string.gmatch(videos, "[^|]+") do
                i = string.find(n, "@@")
                if i then
                    video = string.sub( n, 0, i - 1)
                    codec = string.sub( n, i + 2 )
                    if video and codec and string.match(codec, "vp6") then
                        path = "http://dailymotion.com" .. video
                        break
                    end
                end
            end
        end
        if string.match( line, "<meta name=\"description\"" )
        then
            name = vlc.resolve_xml_special_chars( string.gsub( line, "^.*name=\"description\" content=\"%w+ (.*) %w+ %w+ %w+ %w+ Videos\..*$", "%1" ) )
            description = vlc.resolve_xml_special_chars( string.gsub( line, "^.*name=\"description\" content=\"%w+ .* %w+ %w+ %w+ %w+ Videos\. ([^\"]*)\".*$", "%1" ) )
        end
        if path and name and description and arturl then break end
    end
    return { { path = path; name = name; description = description; url = vlc.path; arturl = arturl } }
end
