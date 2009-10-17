--[[
   Translate www.katsomo.fi video webpages URLs to the corresponding
   movie URL

 $Id$
 Copyright © 2009 the VideoLAN team

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
        and string.match( vlc.path, "www.katsomo.fi" )
        and ( string.match( vlc.path, "treeId" ) or string.match( vlc.path, "progId" ) )
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

-- Parse function.
function parse()
    p = {}
    if string.match( vlc.path, "progId" )
    then
       programid = string.match( vlc.path, "progId=(%d+)")
       path = "http://www.katsomo.fi/metafile.asx?p="..programid.."&bw=800"
       table.insert(p, { path = path; } )
       return p
    end
    while true
    do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "<title>" )
        then
            title = vlc.strings.decode_uri( find( line, "<title>(.-)<" ) )
        end
        for programid in string.gmatch( line, "<li class=\"program\" id=\"program(%d+)\"" ) do
            description = vlc.strings.resolve_xml_special_chars( find( line, "title=\"(.+)\"" ) )
            path = "http://www.katsomo.fi/metafile.asx?p="..programid.."&bw=800"
            table.insert( p, { path = path; name = title; description = description; url = vlc.path;} )
        end
    end
    return p
end
