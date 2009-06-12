--[[
 Parse list of available streams on Anevia servers.
 The URI http://ipaddress/ws/Mgmt/* describes a list of
 available streams on the server.

 Copyright © 2009 M2X BV

 Authors: Jean-Paul Saman <jpsaman@videolan.org>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]]

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "/ws/Mgmt/" )
end

-- Fake readline. Read <>(..*)</> whole, otherwise pretend a newline.
-- In lua, indices are from 1, not 0, so no +1 needed.
function readline()
    local n = string.find(vlc.peek(998),"><") -- A random large number
    return n and vlc.read(n) or vlc.readline()
end

-- Parse function.
function parse()
    local p = {}
    local line
    _,_,server = string.find( vlc.path, "(.*)/ws/Mgmt/" )
    while true do
        line = readline()
        if not line then break end
        if string.match( line, "<struct name=\"stream\">" ) then
            while true do
                line = readline()
                if not line then break end
                if string.match( line, "<field name=\"name\">" ) then
                    _,_,name = string.find( line, "name=\"name\">(.*)</field>" )
                end
                if string.match( line, "<choice name=\"destination\">" ) then
                    while true do
                        line = readline()
                        if not line then break end
                        if string.match( line, "<struct name=\"(.*)\">" ) then
                            _,_,protocol = string.find( line, "<struct name=\"(.*)\">" )
                            while true do
                                line = readline()
                                if not line then break end
                                if string.match( line, "<field name=\"address\">(.*)</field>" ) then
                                    _,_,address = string.find( line, "<field name=\"address\">(.*)</field>" )
                                end
                                if string.match( line, "<field name=\"port\">(.*)</field>" ) then
                                    _,_,port = string.find( line, "<field name=\"port\">(.*)</field>" )
                                end
                                -- end of struct tag
                                if string.match( line, "</struct>" ) then
                                    media = tostring(protocol) .. "://@" .. tostring(address) .. ":" .. tostring(port)
                                    table.insert( p, { path = media; name = name, url = media } )
                                    break
                                end
                            end
                        end
                        if not line then break end
                        -- end of choice tag
                        if string.match( line, "</choice>" ) then break end
                    end
                end
            end
        end

    end
    return p
end
