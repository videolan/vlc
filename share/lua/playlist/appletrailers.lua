--[[
   Translate trailers.apple.com video webpages URLs to the corresponding
   movie URL

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
        and string.match( vlc.path, "trailers.apple.com" )
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

-- Parse function.
function parse()
    local playlist = {}
    local description = ''
    local art_url = ''

    while true
    do 
        line = vlc.readline()
        if not line then break end

        if string.match( line, "class=\".-first" ) then
            description = find( line, "h%d.->(.-)</h%d")
        end
        if string.match( line, 'img src=') then
            for img in string.gmatch(line, '<img src="(http://.*\.jpg)" ') do
                art_url = img
            end
            for i,value in pairs(playlist) do
                if value.arturl == '' then
                    playlist[i].arturl = art_url
                else break end
            end
        end
        if string.match( line, "class=\"hd\".-\.mov") then
            for urlline,resolution in string.gmatch(line, "class=\"hd\".-href=\"(.-.mov)\".-(%d+.-p)") do
                urlline = string.gsub( urlline, "_"..resolution, "_h"..resolution )
                table.insert( playlist, { path = urlline,
                                          name = description .. " (" .. resolution .. ")",
                                          arturl = art_url,
                                          options = {":http-user-agent=QuickTime/7.2", ":demux=avformat,ffmpeg",":play-and-pause"} } )
            end
        end
    end
    return playlist
end
