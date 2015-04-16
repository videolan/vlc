--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>
          Rémi Duraffort  <ivoire at videolan dot org>
          Gian Marco Sibilla <techos at jamendo dot com>

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

local xml = nil
local playlist_tracks_url = "https://api.jamendo.com/v3.0/playlists/tracks/?client_id=3dce8b55&id=%s&track_type=single+albumtrack&order=track_position_desc&format=xml"
local track_jamendo_url = "https://www.jamendo.com/track/%s"
local album_jamendo_url = "https://www.jamendo.com/album/%s"
local playlist_max_tracks = 100

function descriptor()
    return { title = "Jamendo Selections", capabilities = {} }
end

function activate()
    main()
end

function main()
    lazy_load_xml()

    add_playlist( "Jamendo's Finest - Trending tracks", { "222810" } )
    add_playlist( "Jamendo's Trending Lounge/Relaxation Tracks", { "211938", "213936" } )
    add_playlist( "Jamendo's Trending Classical Tracks", { "214065" } )
    add_playlist( "Jamendo's Trending Electronic Tracks", { "211555" } )
    add_playlist( "Jamendo's Trending Jazz Tracks", { "211407" } )
    add_playlist( "Jamendo's Trending Pop Tracks", { "211032" } )
    add_playlist( "Jamendo's Trending Hip-Hop Tracks", { "211404" } )
    add_playlist( "Jamendo's Trending Rock Tracks", { "211064" } )
    add_playlist( "Jamendo's Trending Songwriter Tracks", { "211066" } )
    add_playlist( "Jamendo's Trending World Tracks", { "212188" } )
    add_playlist( "Jamendo's Trending Metal Tracks", { "226459" } )
    add_playlist( "Jamendo's Trending Soundtracks", { "226468" } )
end

function lazy_load_xml()
    if xml ~= nil then return nil end

    xml = require("simplexml")
end

function add_playlist( node_title, ids )
    local node = vlc.sd.add_node( { title=node_title } )
    local subitems = {}
    -- Used to detect duplicated
    local added = {}
    -- Compute the subitem position increment to interpolate different playlists tracks
    local position_increment = #ids

    for start, id in ipairs(ids) do
        -- Compute starting position according to index in list
        local position = start - 1
        -- Build current playlist URL
        local url = string.format( playlist_tracks_url, id )
        -- Request & parse URL
        local playlist = parse_xml( url )

        if playlist ~= nil then
            log( "Playlist for '" .. node_title .. "': " .. playlist.children_map["name"][1].children[1] )

            -- Iterate through found tracks
            for index, track in ipairs( playlist.children_map["tracks"][1].children ) do
                local track_id = track.children_map["id"][1].children[1]
                log( "Processing track #" .. index .. ": " .. track_id )

                -- Create the item to be added to current node
                local item = { path = track.children_map["audio"][1].children[1],
                    title = track.children_map["artist_name"][1].children[1] .. " - " .. track.children_map["name"][1].children[1],
                    name = track.children_map["name"][1].children[1],
                    artist = track.children_map["artist_name"][1].children[1],
                    duration = track.children_map["duration"][1].children[1],
                    url = string.format( track_jamendo_url, track_id ),
                    arturl = track.children_map["image"][1].children[1],
                    meta = {
                        ["Download URL"] = track.children_map["audiodownload"][1].children[1]
                    } }
                -- Add album title if not a single
                local album = track.children_map["album_id"][1].children[1]
                if album ~= nil then item["meta"]["Album URL"] = string.format( album_jamendo_url, album ) end

                -- Check if track isn't already present
                if not added[track_id] then
                    -- Append item to subitem table in the correct position
                    subitems[position] = item
                    added[track_id] = true

                    position = position + position_increment
                end 

                if position > playlist_max_tracks then
                    log( position / position_increment .. " tracks added from playlist " .. id )
                    break
                end
            end
        else
            log( "No result for playlist #" .. id )
        end

    end

    -- Add subitems under node for parsed tracks
    for _, item in ipairs( subitems ) do
        node:add_subitem( item )
    end
end

function parse_xml( url )
    local response = xml.parse_url( url )
    xml.add_name_maps( response )

    if response == nil or #response.children < 2 then return nil end

    if response.children[1].children_map["status"][1].children[1] == "success" 
            and tonumber( response.children[1].children_map["results_count"][1].children[1] ) > 0 then
        return response.children[2].children[1].children[1]
    else
        log( "No result found" )
        return nil
    end
end

function log( msg )
    vlc.msg.dbg( "[JAMENDO] " .. msg )
end
