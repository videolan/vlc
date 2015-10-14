--[[
 $Id$

 Copyright © 2015 Videolabs SAS

 Authors: Felix Paul Kühne (fkuehne@videolan.org)

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 2.1 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]]

function get_url_param( url, name )
    local _, _, res = string.find( url, "[&?]"..name.."=([^&]*)" )
    return res
end

-- Probe function.
function probe()
        if vlc.access ~= "http" then
        return false
    end

    return ( string.match( vlc.path, "www.satip.info" ) )
end

-- Parse function.
function parse()
    local satiphost = get_url_param( vlc.path, "device")

    vlc.msg.dbg("Parsing SAT>IP playlist for host "..satiphost)

    -- Skip the prefix line
    line = vlc.readline()

    p = {}

    while true do
        name = vlc.readline()
        if not name then break end

        name = vlc.strings.from_charset( "ISO_8859-1", name )
        name = string.gsub(name,"#EXTINF:0,","")

        url = vlc.readline()
        if not url then break end

        finalurl = string.gsub(url,"sat.ip",satiphost)
        finalurl = string.gsub(finalurl,"rtsp","satip")

        table.insert( p, { path = finalurl, url = finalurl, name = name } )
    end

    return p
end
