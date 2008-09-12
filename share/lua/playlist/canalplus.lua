--[[
 $Id: $

 Copyright (c) 2007 the VideoLAN team

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
    return vlc.access == "http" and string.match( vlc.path, "www.canalplus.fr" )
end

-- Parse function.
function parse()
    p = {}
    --vlc.msg.dbg( vlc.path )
    if string.match( vlc.path, "www.canalplus.fr/.*%?pid=.*" )
    then -- This is the HTML page's URL
        local _,_,pid = string.find( vlc.path, "pid(%d-)%-" )
        local id, name, description, arturl
        while true do
            -- Try to find the video's title
            local line = vlc.readline()
            if not line then break end
            -- vlc.msg.dbg( line )
            if string.match( line, "aVideos" ) then
                if string.match( line, "CONTENT_ID.*=" ) then
                    _,_,id = string.find( line, "\"(.-)\"" )
                elseif string.match( line, "CONTENT_VNC_TITRE" ) then
                    _,_,arturl = string.find( line, "src=\"(.-)\"" )
                    _,_,name = string.find( line, "title=\"(.-)\"" )
                elseif string.match( line, "CONTENT_VNC_DESCRIPTION" ) then
                    _,_,description = string.find( line, "\"(.-)\"" )
                end
                if id and string.match( line, "new Array" ) then
                    add_item( p, id, name, description, arturl )
                    id = nil
                    name = nil
                    arturl = nil
                    description = nil
                end
            end
        end
        if id then
            add_item( p, id, name, description, arturl )
        end
        return p
    elseif string.match( vlc.path, "embed%-video%-player" ) then
        while true do
            local line = vlc.readline()
            if not line then break end
            --vlc.msg.dbg( line )
            if string.match( line, "<hi" ) then
                local _,_,path = string.find( line, "%[(http.-)%]" )
                return { { path = path } }
            end
        end
    end
end

function get_url_param( url, name )
    local _,_,ret = string.find( url, "[&?]"..name.."=([^&]*)" )
    return ret
end

function add_item( p, id, name, description, arturl )
    --[[vlc.msg.dbg( "id: " .. tostring(id) )
    vlc.msg.dbg( "name: " .. tostring(name) )
    vlc.msg.dbg( "arturl: " .. tostring(arturl) )
    vlc.msg.dbg( "description: " .. tostring(description) )
    --]]
    --local path = "http://www.canalplus.fr/flash/xml/configuration/configuration-embed-video-player.php?xmlParam="..id.."-"..get_url_param(vlc.path,"pid")
    local path = "http://www.canalplus.fr/flash/xml/module/embed-video-player/embed-video-player.php?video_id="..id.."&pid="..get_url_param(vlc.path,"pid")
    table.insert( p, { path = path; name = name; description = description; arturl = arturl } )
end
