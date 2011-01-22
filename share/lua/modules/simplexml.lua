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

local function parsexml(stream, errormsg)
    if not stream then return nil, errormsg end
    local xml = vlc.xml()
    local reader = xml:create_reader(stream)

    local tree
    local parents = {}
    local nodetype, nodename = reader:next_node()

    while nodetype > 0 do
        if nodetype == 1 then
            local node = { name= nodename, attributes= {}, children= {} }
            local attr, value = reader:next_attr()
            while attr ~= nil do
                node.attributes[attr] = value
                attr, value = reader:next_attr()
            end
            if tree then
                table.insert(tree.children, node)
                table.insert(parents, tree)
            end
            tree = node
        elseif nodetype == 2 then
            if #parents > 0 then
                local tmp = {}
                while nodename ~= tree.name do
                    if #parents == 0 then
                        error("XML parser error/faulty logic")
                    end
                    local child = tree
                    tree = parents[#parents]
                    table.remove(parents)
                    table.remove(tree.children)
                    table.insert(tmp, 1, child)
                    for i, node in pairs(child.children) do
                        table.insert(tmp, i+1, node)
                    end
                    child.children = {}
                end
                for _, node in pairs(tmp) do
                    table.insert(tree.children, node)
                end
                tree = parents[#parents]
                table.remove(parents)
            end
        elseif nodetype == 3 then
            table.insert(tree.children, nodename)
        end
        nodetype, nodename = reader:next_node()
    end

    if #parents > 0 then
        error("XML parser error/Missing closing tags")
    end
    return tree
end

function parse_url(url)
    return parsexml(vlc.stream(url))
end

function parse_string(str)
    return parsexml(vlc.memory_stream(str))
end

function add_name_maps(tree)
    tree.children_map = {}
    for _, node in pairs(tree.children) do
        if type(node) == "table" then
            if not tree.children_map[node.name] then
                tree.children_map[node.name] = {}
            end
            table.insert(tree.children_map[node.name], node)
            add_name_maps(node)
        end
    end
end

