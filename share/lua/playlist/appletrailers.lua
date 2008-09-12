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
        and string.match( vlc.path, "www.apple.com/trailers" ) 
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

-- Parse function.
function parse()
    p = {}
    while true
    do 
        line = vlc.readline()
        if not line then break end
        for path in string.gmatch( line, "http://movies.apple.com/movies/.-%.mov" ) do
            path = vlc.strings.decode_uri( path )
            if string.match( path, "320" ) then
                extraname = " (320p)"
            elseif string.match( path, "480" ) then
                extraname = " (480p)"
            elseif string.match( path, "640" ) then
                extraname = " (640p)"
            elseif string.match( path, "720" ) then
                extraname = " (720p)"
            elseif string.match( path, "1080" ) then
                extraname = " (1080p)"
            else
                extraname = ""
            end
            table.insert( p, { path = path; name = title..extraname; description = description; url = vlc.path } )
        end
        if string.match( line, "<title>" )
        then
            title = vlc.strings.decode_uri( find( line, "<title>(.-)<" ) )
        end
        if string.match( line, "<meta name=\"Description\"" )
        then
            description = vlc.strings.resolve_xml_special_chars( find( line, "name=\"Description\" content=\"(.-)\"" ) )
        end
    end
    return p
end
