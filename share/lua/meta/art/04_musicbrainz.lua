--[[
 Gets an artwork from amazon

 $Id$
 Copyright © 2007-2010 the VideoLAN team

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

function try_query(query)
    local l = vlc.object.libvlc()
    local t = vlc.var.get( l, "musicbrainz-previousdate" )
    if t ~= nil then
        if t + 2000000. > vlc.misc.mdate() then
            vlc.msg.warn( "We must wait 2 second between requests unless we want to be blacklisted from the musicbrainz server." )
            vlc.misc.mwait( t + 2000000. )
        end
        vlc.var.set( l, "musicbrainz-previousdate", vlc.misc.mdate() )
    else
        vlc.var.create( l, "musicbrainz-previousdate", vlc.misc.mdate() )
    end
    l = nil
    vlc.msg.dbg( query )
    local s = vlc.stream( query )
    if not s then return nil end
    local page = s:read( 65653 )

    -- FIXME: multiple results may be available
    asin = string.find( page, "<asin>(%w+)</asin>" )
    if asin then
        vlc.msg.dbg( asin )
        return "http://images.amazon.com/images/P/"..asin..".01._SCLZZZZZZZ_.jpg"
    else
        return nil
    end
end

-- Return the artwork
function fetch_art()
    local meta = vlc.item:metas()
    if not (meta["artist"] and meta["album"]) then
        return nil
    end

    local query1 = "http://mb.videolan.org/ws/1/release/?type=xml&artist="..vlc.strings.encode_uri_component(meta["artist"]).."&title=\""..vlc.strings.encode_uri_component(meta["album"].."\"")
    return try_query(query1)
end
