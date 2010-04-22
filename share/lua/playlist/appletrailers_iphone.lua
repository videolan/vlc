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
        and string.match( vlc.path, "trailers.apple.com/trailers/iphone" ) 
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

-- Parse function.
function parse()
    p = {}
    path=""
    arturl=""
    title=""
    description=""
    while true
    do 
        line = vlc.readline()
        if not line then break end
        for urli in string.gmatch( line, "http://trailers.apple.com/movies/.-%.mov" ) do
            path = vlc.strings.decode_uri( urli )
            vlc.msg.err(path)
        end
        for urli in string.gmatch( line, "http://.-%/poster.jpg") do
            arturl = vlc.strings.decode_uri( urli )
        end
        if string.match( line, "<title>" )
        then
            title = vlc.strings.resolve_xml_special_chars( find( line, "<title>(.-)<" ) )
        end
        if string.match( line, "<meta name=\"Description\"" )
        then
            description = vlc.strings.resolve_xml_special_chars( find( line, "name=\"Description\" content=\"(.-)\"" ) )
        end
    end
    for index,resolution in ipairs({"480p","720p","1080p"}) do
        locationurl = string.gsub( path, "r320i.mov","h"..resolution..".mov")
        table.insert( p, { path=locationurl ; name=title.." ("..resolution..")"; arturl=arturl; description=description; options={":http-user-agent=Quicktime/7.2.0 vlc lua edition",":input-fast-seek",":play-and-stop"};} )
    end
    return p
end
