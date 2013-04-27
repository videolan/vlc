--[[
   Translate trailers.apple.com video webpages URLs to the corresponding
   movie URL

 $Id$
 Copyright © 2007-2010 the VideoLAN team

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
        and string.match( vlc.path, "web.inc" )
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

function sort(a, b)
    if(a == nil) then return false end
    if(b == nil) then return false end

    local str_a
    local str_b

    if(string.find(a.name, '%(') == 1) then
        str_a = tonumber(string.sub(a.name, 2, string.find(a.name, 'p') - 1))
        str_b = tonumber(string.sub(b.name, 2, string.find(b.name, 'p') - 1))
    else
        str_a = string.sub(a.name, 1, string.find(a.name, '%(') - 2)
        str_b = string.sub(b.name, 1, string.find(b.name, '%(') - 2)
        if(str_a == str_b) then
            str_a = tonumber(string.sub(a.name, string.len(str_a) + 3, string.find(a.name, 'p', string.len(str_a) + 3) - 1))
            str_b = tonumber(string.sub(b.name, string.len(str_b) + 3, string.find(b.name, 'p', string.len(str_b) + 3) - 1))
        end
    end
    if(str_a > str_b) then return false else return true end
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

        if string.match( line, "h3>.-</h3" ) then
            description = find( line, "h3>(.-)</h3")
            vlc.msg.dbg(description)
        end
        if string.match( line, 'img src=') then
            for img in string.gmatch(line, '<img src="(http://.*%.jpg)" ') do
                art_url = img
            end
            for i,value in pairs(playlist) do
                if value.arturl == '' then
                    playlist[i].arturl = art_url
                end
            end
        end
        if string.match( line, 'class="hd".-%.mov') then
            for urlline,resolution in string.gmatch(line, 'class="hd".-href="(.-%.mov)".->(%d+.-p)') do
                urlline = string.gsub( urlline, "_"..resolution, "_h"..resolution )
                table.insert( playlist, { path = urlline,
                                          name = description.." "..resolution,
                                          arturl = art_url,
                                          options = {":http-user-agent=QuickTime/7.5", ":play-and-pause", ":demux=avformat"} } )
            end
        end
    end

    return playlist
end
