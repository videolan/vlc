--[==========================================================================[
 sandbox.lua: Lua sandboxing facilities
--[==========================================================================[
 Copyright (C) 2007 the VideoLAN team
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

module("sandbox",package.seeall)

-- See Programming in Lua (second edition) for sandbox examples
-- See http://lua-users.org/wiki/SandBoxes for a list of SAFE/UNSAFE variables

local sandbox_blacklist = {
    collectgarbage = true,
    dofile = true,
    _G = true,
    getfenv = true,
    getmetatable = true,
    load = true, -- Can be protected I guess
    loadfile = true, -- Can be protected I guess
    loadstring = true, -- Can be protected I guess
    rawequal = true,
    rawget = true,
    rawset = true,
    setfenv = true,
    setmetatable = true,
    module = true,
    require = true,
    package = true,
    debug = true,
}

function readonly_table_proxy(name,src,blacklist)
    if type(src)=="nil" then return end
    if type(src)~="table" then error("2nd argument must be a table (or nil)") end
    local name = name
    local t = src
    local blist = {}
    if blacklist then
        for _, v in pairs(blacklist) do
            blist[v] = true
        end
    end
    local metatable_readonly = {
        __index = function(self,key)
            if blist[key] then
                error("Sandbox: Access to `"..name.."."..key.."' is forbidden.")
            end
            return t[key]
        end,
        __newindex = function(self,key,value)
            error("It is forbidden to modify elements of this table.")
        end,
    }
    return setmetatable({},metatable_readonly)
end

-- Of course, all of this is useless if the sandbox calling code has
-- another reference to one of these tables in his global environement.
local sandbox_proxy = {
    coroutine   = readonly_table_proxy("coroutine",coroutine),
    string      = readonly_table_proxy("string",string,{"dump"}),
    table       = readonly_table_proxy("table",table),
    math        = readonly_table_proxy("math",math),
    io          = readonly_table_proxy("io",io),
    os          = readonly_table_proxy("os",os,{"exit","getenv","remove",
                                                "rename","setlocale"}),
    sandbox     = readonly_table_proxy("sandbox",sandbox),
}

function sandbox(func,override)
    local _G = getfenv(2)
    local override = override or {}
    local sandbox_metatable =
    {
        __index = function(self,key)
            if override[key] then
                return override[key]
            end
            if sandbox_blacklist[key] then
                error( "Sandbox: Access to `"..key.."' is forbidden." )
            end
            --print(key,"not found in env. Looking in sandbox_proxy and _G")
            local value = sandbox_proxy[key] or _G[key]
            rawset(self,key,value) -- Keep a local copy
            return value
        end,
        __newindex = function(self,key,value)
            if override and override[key] then
                error( "Sandbox: Variable `"..key.."' is read only." )
            end
            return rawset(self,key,value)
        end,
    }
    local sandbox_env = setmetatable({},sandbox_metatable)
    return function(...)
        setfenv(func,sandbox_env)
        local ret = {func(...)} -- Not perfect (if func returns nil before
                                -- another return value) ... but it's better
                                -- than nothing
        setfenv(func,_G)
        return unpack(ret)
    end
end