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
        and string.match( vlc.path, "^www%.katsomo%.fi/" )
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
    title=""
    arturl="http://www.katsomo.fi/multimedia/template/logos/katsomo_logo_2.gif"
    while true
    do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "<title>" )
        then
            title = vlc.strings.decode_uri( find( line, "<title>(.-)<" ) )
            title = vlc.strings.from_charset( "ISO_8859-1", title )
        end
        if( find( line, "img class=\"pngImg\" src=\"/multimedia/template/logos" ) )
        then
            arturl = "http://www.katsomo.fi"..find( line, " src=\"(.-)\" alt=" )
        end
        for treeid,name in string.gmatch( line, '/%?treeId=(%d+)">([^<]+)</a') do
            name = vlc.strings.resolve_xml_special_chars( name )
            name = vlc.strings.from_charset( "ISO_8859-1", name )
            path = "http://www.katsomo.fi/?treeId="..treeid
            table.insert( p, { path = path; name = name; arturl=arturl; } )
        end
        for programid in string.gmatch( line, "<li class=\"program.*\" id=\"program(%d+)\" title=\".+\"" ) do
           description = vlc.strings.resolve_xml_special_chars( find( line, "title=\"(.+)\"" ) )
           description = vlc.strings.from_charset( "ISO_8859-1", description )
           path = "http://www.katsomo.fi/metafile.asx?p="..programid.."&bw=800"
           table.insert( p, { path = path; name = description; arturl=arturl; } )
        end
    end
    return p
end
