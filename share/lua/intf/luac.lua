--[==========================================================================[
 luac.lua: lua compilation module for VLC (duplicates luac)
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

usage = 
[[
To compile a lua script to bytecode (luac) run:
  vlc -I luaintf --lua-intf --lua-config 'luac={input="file.lua",output="file.luac"}'
Output will be similar to that of the luac command line tool provided with lua with the following arguments:
  luac -o file.luac file.lua
]]

require "string"
require "io"

function compile()
    vlc.msg.info("About to compile lua file")
    vlc.msg.info("  Input is '"..tostring(config.input).."'")
    vlc.msg.info("  Output is '"..tostring(config.output).."'")
    if not config.input or not config.output then
        vlc.msg.err("Input and output config options must be set")
        return false
    end

    local bytecode, msg = loadfile(config.input)
    if not bytecode then
        vlc.msg.err("Error while loading file '"..config.input.."': "..msg)
        return false
    end

    local output, msg = io.open(config.output, "wb")
    if not output then
        vlc.msg.err("Error while opening file '"..config.output.."' for output: "..msg)
        return false
    else
        output:write(string.dump(bytecode))
        return true
    end
end


if not compile() then
    for line in string.gmatch(usage,"([^\n]+)\n*") do
        vlc.msg.err(line)
    end
end
vlc.misc.quit()

