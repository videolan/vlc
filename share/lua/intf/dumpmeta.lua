--[==========================================================================[
 rc.lua: remote control module for VLC
--[==========================================================================[
 Copyright (C) 2007-2009 the VideoLAN team
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

--[[ to dump meta data information in the debug output, run:
       vlc -I lua --lua-intf dumpmeta -v=0 coolmusic.mp3
--]]

local item
repeat
    item = vlc.input.item()
until (item and item:is_preparsed()) or vlc.misc.should_die()

local meta = item:metas()
vlc.msg.info("Dumping meta data")
if meta then
    for key, value in pairs(meta) do
        vlc.msg.info(key..": "..value)
    end
end

vlc.misc.quit()
