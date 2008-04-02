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

-- Parse function.
function parse()
    p = {}
    while true
    do 
        line = vlc.readline()
        if not line then break end
        if string.match( line, "http://movies.apple.com/movies/.*%.mov" )
        or string.match( line, "http://images.apple.com/movies/.*%.mov" )
        then
            if string.match( line, "http://movies.apple.com/movies/.*%.mov" ) then
                path = vlc.decode_uri( string.gsub( line, "^.*(http://movies.apple.com/movies/.*%.mov).*$", "%1" ) )
            elseif string.match( line, "http://images.apple.com/movies/.*%.mov" ) then
                path = vlc.decode_uri( string.gsub( line, "^.*(http://images.apple.com/movies/.*%.mov).*$", "%1" ) )
            end
            if string.match( path, "480p" ) then
                extraname = " (480p)"
            elseif string.match( path, "720p" ) then
                extraname = " (720p)"
            elseif string.match( path, "1080p" ) then
                extraname = " (1080p)"
            else
                extraname = ""
            end
            table.insert( p, { path = path; name = title..extraname; description = description; url = vlc.path } )
        end
        if string.match( line, "<title>" )
        then
            title = vlc.decode_uri( string.gsub( line, "^.*<title>([^<]*).*$", "%1" ) )
        end
        if string.match( line, "<meta name=\"Description\"" )
        then
            description = vlc.resolve_xml_special_chars( string.gsub( line, "^.*name=\"Description\" content=\"([^\"]*)\".*$", "%1" ) )
        end
    end
    return p
end
