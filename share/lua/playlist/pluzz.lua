--[[
 $Id$

 Copyright © 2011 VideoLAN

 Authors: Ludovic Fauvet <etix at l0cal dot com>

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

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "pluzz.fr/%w+" )
        or string.match( vlc.path, "info.francetelevisions.fr/.+")
        or string.match( vlc.path, "france4.fr/%w+")
end

-- Helpers
function key_match( line, key )
    return string.match( line, "name=\"" .. key .. "\"" )
end

function get_value( line )
    local _,_,r = string.find( line, "content=\"(.*)\"" )
    return r
end

-- Parse function.
function parse()
    p = {}

    if string.match ( vlc.path, "www.pluzz.fr/%w+" ) then
        while true do
            line = vlc.readline()
            if not line then break end
            if string.match( line, "id=\"current_video\"" ) then
                _,_,redirect = string.find (line, "href=\"(.-)\"" )
                print ("redirecting to: " .. redirect )
                return { { path = redirect } }
            end
        end
    end

    if string.match ( vlc.path, "www.france4.fr/%w+" ) then
        while true do
            line = vlc.readline()
            if not line then break end
	    -- maybe we should get id from tags having video/cappuccino type instead
            if string.match( line, "id=\"lavideo\"" ) then
                _,_,redirect = string.find (line, "href=\"(.-)\"" )
                print ("redirecting to: " .. redirect )
                return { { path = redirect } }
            end
        end
    end

    if string.match ( vlc.path, "info.francetelevisions.fr/.+" ) then
        title = ""
        arturl = "http://info.francetelevisions.fr/video-info/player_sl/Images/PNG/gene_ftv.png"
        while true do
            line = vlc.readline()
            if not line then break end
            -- Try to find the video's path
            if key_match( line, "urls--url--video" ) then
                video = get_value( line )
            end
            -- Try to find the video's title
            if key_match( line, "vignette--titre--court" ) then
                title = get_value( line )
                title = vlc.strings.resolve_xml_special_chars( title )
                print ("playing: " .. title )
            end
            -- Try to find the video's thumbnail
            if key_match( line, "vignette" ) then
                arturl = get_value( line )
                if not string.match( line, "http://" ) then
                    arturl = "http://info.francetelevisions.fr/" .. arturl
                end
            end
        end
        if video then
            -- base url is hardcoded inside a js source file
            -- see http://www.pluzz.fr/layoutftv/arches/common/javascripts/jquery.player-min.js
            base_url = "mms://a988.v101995.c10199.e.vm.akamaistream.net/7/988/10199/3f97c7e6/ftvigrp.download.akamai.com/10199/cappuccino/production/publication/"
            table.insert( p, { path = base_url .. video; name = title; arturl = arturl; } )
        end
    end

    return p
end

