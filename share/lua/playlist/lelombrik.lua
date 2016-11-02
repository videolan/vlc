--[[
 French humor site: http://lelombrik.net

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
    local path = vlc.path:gsub("^www%.", "")
    return vlc.access == "http"
        and string.match( path, "^lelombrik%.net/videos" )
end

-- Parse function.
function parse()
    while true do
        line = vlc.readline()
        if not line then
            vlc.msg.err("Couldn't extract the video URL from lelombrik")
            return { }
        end

        if string.match( line, "id=\"nom_fichier\">" ) then
            title = string.gsub( line, ".*\"nom_fichier\">([^<]*).*", "%1" )
            if title then
                title = vlc.strings.from_charset( "ISO_8859-1", title )
            end
        elseif string.match( line, "'file'" ) then
            _,_,path = string.find( line, "'file', *'([^']*)")
        elseif string.match( line, "flashvars=" ) then
            path = string.gsub( line, "flashvars=.*&file=([^&]*).*", "%1" )
            arturl = string.gsub( line, "flashvars=.*&image=([^&]*).*", "%1" )
        elseif string.match( line, "'image'" ) then
            _,_,arturl = string.find( line, "'image', *'([^']*)")
        end

        if path and arturl and title then
            return { { path = path; arturl = arturl; title = title } }
        end
    end
end
