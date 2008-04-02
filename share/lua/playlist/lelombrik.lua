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
    return vlc.access == "http"
        and string.match( vlc.path, "lelombrik.net/videos" ) 
end

-- Parse function.
function parse()
    vidtitle = ""
    while true do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "id=\"nom_fichier\">" ) then
            vidtitle = string.gsub( line, ".*\"nom_fichier\">([^<]*).*", "%1" )
        end
        if string.match( line, "flvplayer.swf" ) then
            -- fallback: retrieve the title from the url if we didn't find it
            if vidtitle == "" then
                vidtitle = string.gsub( vlc.path, ".*/([^.]*).*", "%1" )
            end
            return { { path = string.gsub( line, ".*flashvars=\"&file=([^&]*).*", "%1" ); arturl = string.gsub( line, ".*&image=([^&]*).*", "%1" ); title = vidtitle } }
        end
    end
end
