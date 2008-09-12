--[[
 Gets an artwork from amazon

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

-- Return the artwork
function fetch_art()
    local query
    if vlc.artist and vlc.album then
        query = "http://musicbrainz.org/ws/1/release/?type=xml&artist="..vlc.strings.encode_uri_component(vlc.artist).."&title="..vlc.strings.encode_uri_component(vlc.album)
    else
        return nil
    end

    local l = vlc.object.libvlc()
    local t = vlc.var.get( l, "musicbrainz-previousdate" )
    if t ~= nil then
        if t + 1000000. > vlc.misc.mdate() then
            vlc.msg.warn( "We must wait 1 second between requests unless we want to be blacklisted from the musicbrainz server." )
            vlc.misc.mwait( t + 1000000. )
        end
        vlc.var.set( l, "musicbrainz-previousdate", vlc.misc.mdate() )
    else
        vlc.var.create( l, "musicbrainz-previousdate", vlc.misc.mdate() )
    end
    l = nil
    vlc.msg.dbg( query )
    local s = vlc.stream( query )
    local page = s:read( 65653 )

    -- FIXME: multiple results may be available
    _,_,asin = string.find( page, "<asin>(.-)</asin>" )
    if asin ~= page then
        return "http://images.amazon.com/images/P/"..asin..".01._SCLZZZZZZZ_.jpg"
    else
        return nil
    end
end
