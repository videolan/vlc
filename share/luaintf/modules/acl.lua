--[==========================================================================[
 acl.lua: VLC Lua interface ACL object
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

--[==========================================================================[
require "acl"
local a = acl.new(true) -> new ACL with default set to allow
a:check("10.0.0.1") -> 0 == allow, 1 == deny, -1 == error
a("10.0.0.1") -> same as a:check("10.0.0.1")
a:duplicate() -> duplicate ACL object
a:add_host("10.0.0.1",true) -> allow 10.0.0.1
a:add_net("10.0.0.0",24,true) -> allow 10.0.0.0/24 (not sure)
a:load_file("/path/to/acl") -> load ACL from file
--]==========================================================================]

module("acl",package.seeall)

methods = {
    check = function(this,ip)
        return vlc.acl.check(this.private,ip)
    end,
    duplicate = function(this)
        return setmetatable({ private = vlc.acl.duplicate( rawget(this,"private") ) },metatable)
    end,
    add_host = function(this,ip,allow)
        return vlc.acl.add_host(this.private,ip,allow)
    end,
    add_net = function(this,ip,len,allow)
        return vlc.acl.add_net(this.private,ip,len,allow)
    end,
    load_file = function(this,path)
        return vlc.acl.load_file(this.private,path)
    end,
}

metatable = {
    __index = function(this,key)
        if methods[key] then
            return methods[key]
        elseif key == "private" then
            error("Forbidden")
        else
            return rawget(this,key)
        end
    end,
    __newindex = function(this,key,value)
        if key == "private" or methods[key] then
            error("Forbidden")
        else
            rawset(this,key,value)
        end
    end,
    __call = methods.check,
    __gc = function(this)
        vlc.acl.delete(this.private)
    end,
    __metatable = "None of your business.",
}

function new( allow )
    return setmetatable({ private = vlc.acl.create( allow ) },metatable)
end
