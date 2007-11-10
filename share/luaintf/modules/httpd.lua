--[==========================================================================[
 httpd.lua: VLC Lua interface HTTPd module
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
--]==========================================================================]

module("httpd",package.seeall)

local function generic_metatable(methods,destructor)
    return {
        __index = function(this,key)
            if methods and methods[key] then
                return methods[key]
            elseif key == "private" then
                error("Forbidden")
            else
                return rawget(this,key)
            end
        end,
        __newindex = function(this,key,value)
            if key == "private" or (methods and methods[key]) then
                error("Forbidden")
            else
                rawset(this,key,value)
            end
        end,
        __gc = function(this)
            if destructor then
                destructor(rawget(this,"private"))
            end
        end,
        --__metatable = "None of your business.",
    }
end

local handler_metatable = generic_metatable({},vlc.httpd.handler_delete)
local file_metatable = generic_metatable({},vlc.httpd.file_delete)
local redirect_metatable = generic_metatable({},vlc.httpd.redirect_delete)

local host_methods = {
    handler_new = function(this,...)
        return setmetatable({private=vlc.httpd.handler_new(rawget(this,"private"),...),parent=this},handler_metatable)
    end,
    file_new = function(this,...)
        return setmetatable({private=vlc.httpd.file_new(rawget(this,"private"),...),parent=this},file_metatable)
    end,
    redirect_new = function(this,...)
        return setmetatable({private=vlc.httpd.redirect_new(rawget(this,"private"),...),parent=this},redirect_metatable)
    end,
}
local host_metatable = generic_metatable(host_methods,vlc.httpd.host_delete)

function new( ... )
    return setmetatable({private=vlc.httpd.host_new(...)},host_metatable)
end
