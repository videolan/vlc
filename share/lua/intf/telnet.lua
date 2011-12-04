--[==========================================================================[
 telnet.lua: wrapper for legacy telnet configuration
--[==========================================================================[
 Copyright (C) 2011 the VideoLAN team
 $Id$

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
--]==========================================================================]

if config.hosts == nil and config.host == nil then
    config.host = "telnet://localhost:4212"
else
    if config.hosts == nil then
        config.hosts = { config.host }
    end

    for i,host in pairs(config.hosts) do
        if host ~= "*console" then
            local proto = string.match(host, "^%s*(.-)://")
            if proto == nil or proto ~= "telnet" then
                local newhost
                if proto == nil then
                    newhost = "telnet://"..host
                else
                    newhost = string.gsub(host, "^%s*.-://", "telnet://")
                end
                --vlc.msg.warn("Replacing host `"..host.."' with `"..newhost.."'")
                config.hosts[i] = newhost
            end
        end
    end
end

--[[ Launch vlm ]]
vlm = vlc.vlm()

dofile(wrapped_file)

