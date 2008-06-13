--[[
  Parse YouTube homepage and browse pages. Next step is to recode firefox
  in VLC ... using Lua of course ;)

 $Id$

 Copyright © 2007 the VideoLAN team

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
--]]

function probe()
    return vlc.access == "http" and ( string.match( vlc.path, "youtube.com/%?$" ) or string.match( vlc.path, "youtube.com/browse" ) )
end

function parse()
    p = {}
    while true
    do
        line = vlc.readline()
        if not line then break end
        for _path, _artist, _name in string.gmatch( line, "href=\"(/watch%?v=[^\"]*)\" onclick=\"_hbLink%('([^']*)','Vid[^\']*'%);\">([^<]*)</a><br/>" )
        do
            path = "http://www.youtube.com" .. _path
            name = vlc.strings.resolve_xml_special_chars( _name )
            artist = _artist
        end
        for _min, _sec in string.gmatch( line, "<span class=\"runtime\">(%d*):(%d*)</span>" )
        do
            duration = 60 * _min + _sec
        end
        if path and name and artist and duration then
            table.insert( p, { path = path; name = name; artist = artist; duration = duration } )
            path = nil
            name = nil
            artist = nil
            duration = nil
        end
    end
    return p
end
