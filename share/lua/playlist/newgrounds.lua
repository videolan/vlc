--[[
 $Id$

 Copyright © 2016 the VideoLAN team

 Authors: Pierre Ynard

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
    return ( vlc.access == "http" or vlc.access == "https" )
        and string.match( vlc.path, "^www%.newgrounds%.com/.*/%d+" )
end

-- Parse function.
function parse()
    local path, title, description, arturl, author
    while true do
        local line = vlc.readline()
        if not line then break end

        if not title then
            title = string.match( line, "<meta property=\"og:title\" content=\"(.-)\"" )
            if title then
                title = vlc.strings.resolve_xml_special_chars( title )
            end
        end

        if not description then
            description = string.match( line, "<meta property=\"og:description\" content=\"(.-)\"" )
            if description then
                description = vlc.strings.resolve_xml_special_chars( description )
            end
        end

        if not arturl then
            arturl = string.match( line, "<meta property=\"og:image\" content=\"(.-)\"" )
            if arturl then
                arturl = vlc.strings.resolve_xml_special_chars( arturl )
            end
        end

        if not author then
            author = string.match( line, "<em>Author <a [^>]*>([^<]+)</a></em>" )
            if author then
                author = vlc.strings.resolve_xml_special_chars( author )
            end
        end

        if not path then
            path = string.match( line, 'new embedController%(%[{"url":"([^"]+)"' )
            if path then
                path = string.gsub( path, "\\/", "/" )
            end
        end
    end

    if not path then
        vlc.msg.err( "Couldn't extract newgrounds media URL" )
        return { }
    end

    return { { path = path, name = title, description = description, arturl = arturl, artist = author } }
end
