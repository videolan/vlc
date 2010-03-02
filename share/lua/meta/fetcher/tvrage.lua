--[[
 Gets metas for tv episode using tvrage.

 $Id$
 Copyright © 2010 the VideoLAN team

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

-- Replace non alphanumeric char by +
function get_query( title )
    -- If we have a .EXT remove the extension.
    str = string.gsub( title, "(.*)%....$", "%1" )
    return string.gsub( str, "([^%w ])",
         function (c) return string.format ("%%%02X", string.byte(c)) end)
end

function fetch_meta()
    local metas = vlc.item:metas()

    local showName = metas["showName"]
    if not showName then
        return false
    end

    local seasonNumber = metas["seasonNumber"];
    if not seasonNumber then
        return false
    end

    local episodeNumber = metas["episodeNumber"];
    if not episodeNumber then
        return false
    end

    local fd = vlc.stream("http://services.tvrage.com/feeds/search.php?show=" .. get_query(showName))
    if not fd then return nil end
    local page = fd:read( 65653 )
    fd = nil
    _, _, showid = string.find( page, "<showid>(.-)</showid>" )
    if not showid then
        return false
    end

    fd = vlc.stream("http://services.tvrage.com/feeds/full_show_info.php?sid=" .. showid)
    if not fd then return nil end
    page = fd:read( 65653 )
    fd = nil
    _, _, season = string.find(page, "<Season no=\""..seasonNumber.."\">(.-)</Season>")
    if not season then
        return false
    end

    _, _, episode = string.find(season, "<episode>(.-<seasonnum>"..episodeNumber.."</seasonnum>.-)</episode>")
    if not episode then
        return false
    end

    _, _, title, artwork = string.find(episode, "<title>(.-)</title><screencap>(.-)</screencap>")
    if not title then
        return false
    end

    vlc.item:set_meta("title", showName.. " S"..seasonNumber.."E"..episodeNumber.." - ".. title)
    vlc.item:set_meta("artwork_url", artwork)
    vlc.item:set_meta("episodeName", title)

    return true
end
