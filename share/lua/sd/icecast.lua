--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>

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
require "simplexml"

function descriptor()
    return { title="Icecast Directory" }
end

function main()
    local tree = simplexml.parse_url("http://dir.xiph.org/yp.xml")
    for _, station in ipairs( tree.children ) do
        simplexml.add_name_maps( station )
        vlc.sd.add_item( {path=station.children_map["listen_url"][1].children[1],
                          title=station.children_map["server_name"][1].children[1],
                          genre=station.children_map["genre"][1].children[1],
                          nowplaying=station.children_map["current_song"][1].children[1],
                          meta={["Icecast"]={["Bitrate"]=station.children_map["bitrate"][1].children[1];
                                             ["Server type"]=station.children_map["server_type"][1].children[1]}}} )
    end
end
