--[==========================================================================[
 simplexml.lua: Lua simple xml parser wrapper
--[==========================================================================[
 Copyright (C) 2010 Antoine Cellerier
 $Id$

 Authors: Antoine Cellerier <dionoea at videolan dot org>

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

module("simplexml",package.seeall)

--[[ Returns the xml tree structure
--   Each node is of one of the following types:
--     { name (string), attributes (key->value map), children (node array) }
--     text content (string)
--]]

local function parsexml(stream)
    local xml = vlc.xml()
    local reader = xml:create_reader(stream)

    local tree
    local parents = {}
    while reader:read() > 0 do
        local nodetype = reader:node_type()
        if nodetype == 'startelem' then
            local name = reader:name()
            local node = { name: '', attributes: {}, children: {} }
            node.name = name
            while reader:NextAttr() == 0 do
                node.attributes[reader:Name()] = reader:Value()
            end
            if tree then
                tree.children[#tree.children] = node
                parents[#parents] = tree
                tree = node
            end
        elseif nodetype == 'endelem' then
            tree = parents[#parents-1]
        elseif nodetype == 'text' then
            node.children[#node.children] = reader:Value()
        end
    end

    return tree
end

function parse_url(url)
    return parsexml(vlc.stream(url))
end

function parse_string(str)
    return parsexml(vlc.memory_stream(str))
end
