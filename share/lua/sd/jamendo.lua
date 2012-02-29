--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>
          Rémi Duraffort  <ivoire at videolan dot org>

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
    return { title="Jamendo Selections" }
end

function main()
    add_top_tracks( "ratingweek_desc", "rock", 100 )
    add_top_tracks( "ratingweek_desc", "pop", 100 )
    add_top_tracks( "ratingweek_desc", "jazz", 100 )
    add_top_tracks( "ratingweek_desc", "dance", 100 )
    add_top_tracks( "ratingweek_desc", "hipop+rap", 100 )
    add_top_tracks( "ratingweek_desc", "world+reggae", 100 )
    add_top_tracks( "ratingweek_desc", "lounge+ambient", 100 )
    add_top_tracks( "ratingweek_desc", nil, 100 )
    add_top_albums( "ratingweek_desc", nil, 20 )
    add_radio_from_id( "9", 20 )
    add_radio_from_id( "8", 20 )
    add_radio_from_id( "6", 20 )
    add_radio_from_id( "5", 20 )
    add_radio_from_id( "7", 20 )
    add_radio_from_id( "4", 20 )
end


function add_top_albums( album_order, tag, max_results )
    local url = "http://api.jamendo.com/get2/id+name+artist_name+album_image/album/xml/?imagesize=500&order=" .. album_order .. "&n=" .. max_results
    if tag ~= nil then
        url = url .. "&tag_idstr=" .. tag
    end
    local tree = simplexml.parse_url( url )
    local node_name = "Top " .. max_results
    if album_order == "rating_desc" then node_name = node_name .. " most popular albums"
    elseif album_order == "ratingmonth_desc" then node_name = node_name .. " most popular albums this month"
    elseif album_order == "ratingweek_desc" then node_name = node_name .. " most popular albums this week"
    elseif album_order == "releasedate_desc" then node_name = node_name .. " latest released albums"
    elseif album_order == "downloaded_desc" then node_name = node_name .. " most downloaded albums"
    elseif album_order == "listened_desc" then node_name = node_name .. " most listened to albums"
    elseif album_order == "starred_desc" then node_name = node_name .. " most starred albums"
    elseif album_order == "playlisted_desc" then node_name = node_name .. " most playlisted albums"
    elseif album_order == "needreviews_desc" then node_name = node_name .. " albums requiring review"
    end
    if tag ~= nil then
        node_name = tag .. " - " .. node_name
    end
    local node = vlc.sd.add_node( {title=node_name} )
    for _, album in ipairs( tree.children ) do
        simplexml.add_name_maps( album )
        local album_node = node:add_subitem(
                { path     = 'http://api.jamendo.com/get2/id+name+duration+artist_name+album_name+album_genre+album_dates+album_image/track/xml/track_album+album_artist/?album_id=' .. album.children_map["id"][1].children[1],
                  title    = album.children_map["artist_name"][1].children[1] .. ' - ' .. album.children_map["name"][1].children[1],
                  arturl   = album.children_map["album_image"][1].children[1] })
    end
end

function add_top_tracks( track_order, tag, max_results )
    local url = "http://api.jamendo.com/get2/id+name+duration+artist_name+album_name+genre+album_image+album_dates/track/xml/track_album+album_artist/?imagesize=500&order=" .. track_order .. "&n=" .. max_results
    if tag ~= nil then
        url = url .. "&tag_minweight=0.35&tag_idstr=" .. tag
    end

    local tree = simplexml.parse_url( url )
    local node_name = "Top " .. max_results
    if track_order == "rating_desc" then node_name = node_name .. " most popular tracks"
    elseif track_order == "ratingmonth_desc" then node_name = node_name .. " most popular tracks this month"
    elseif track_order == "ratingweek_desc" then node_name = node_name .. " most popular tracks this week"
    elseif track_order == "releasedate_desc" then node_name = node_name .. " latest released tracks"
    elseif track_order == "downloaded_desc" then node_name = node_name .. " most downloaded tracks"
    elseif track_order == "listened_desc" then node_name = node_name .. " most listened to tracks"
    elseif track_order == "starred_desc" then node_name = node_name .. " most starred tracks"
    elseif track_order == "playlisted_desc" then node_name = node_name .. " most playlisted tracks"
    elseif track_order == "needreviews_desc" then node_name = node_name .. " tracks requiring review"
    end
    if tag ~= nil then
        node_name = string.upper(tag) .. " - " .. node_name
    end
    local node = vlc.sd.add_node( {title=node_name} )
    for _, track in ipairs( tree.children ) do
        simplexml.add_name_maps( track )
        node:add_subitem( {path="http://api.jamendo.com/get2/stream/track/redirect/?id=" .. track.children_map["id"][1].children[1],
                           title=track.children_map["artist_name"][1].children[1].." - "..track.children_map["name"][1].children[1],
                           artist=track.children_map["artist_name"][1].children[1],
                           album=track.children_map["album_name"][1].children[1],
                           genre=track.children_map["genre"][1].children[1],
                           date=track.children_map["album_dates"][1].children_map["year"][1].children[1],
                           arturl=track.children_map["album_image"][1].children[1],
                           duration=track.children_map["duration"][1].children[1]} )
    end
end

function add_radio_from_id( id, max_results )
    local radio_name
    if id == "9" then radio_name="Rock"
    elseif id == "8" then radio_name="Pop / Songwriting"
    elseif id == "6" then radio_name="Jazz"
    elseif id == "5" then radio_name="Hip-Hop"
    elseif id == "7" then radio_name="Lounge"
    elseif id == "4" then radio_name="Dance / Electro"
    end
    vlc.sd.add_item( {path="http://api.jamendo.com/get2/id+name+artist_name+album_name+duration+album_genre+album_image+album_dates/track/xml/radio_track_inradioplaylist+track_album+album_artist/?imagesize=500&order=random_desc&radio_id=" .. id .. "&n=" .. max_results,
                      title=radio_name} )
end

