--[[
   Translate trailers.apple.com video webpages URLs to the corresponding
   movie URL

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

-- Probe function
function probe()
    return (vlc.access == "http" or vlc.access == "https")
        and string.match( vlc.path, "^trailers%.apple%.com/trailers/.+/.+" )
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

function parse_json(url)
    local json   = require("dkjson")
    local stream = vlc.stream(url)
    local string = ""
    local line   = ""

    if not stream then
        return nil, nil, "Failed creating VLC stream"
    end

    while true do
        line = stream:readline()

        if not line then
            break
        end

        string = string .. line
    end

    if string == "" then
        return nil, nil, "Got empty response from server."
    end

    return json.decode(string)
end

-- Parse function.
function parse()
    local video_id = nil
    local playlist = {}

    while true do
        line = vlc.readline()
        if not line then break end

        if string.match(line, "FilmId%s+=%s+'%d+'") then
            video_id = find(line, "FilmId%s+=%s+'(%d+)'")
            vlc.msg.dbg("Found FilmId " .. video_id)
            break
        end
    end

    -- Found a video id
    if video_id ~= nil then
        -- Lookup info from the json endpoint
        local info, _, err = filmid_info(video_id)

        if err ~= nil then
        	vlc.msg.err("Error to parse JSON response from Apple trailers: " .. err)
        	return playlist
        end

        -- Parse data
        if info["clips"] == nil then
            vlc.msg.err("Unexpected JSON response from Apple trailers")
            return playlist
        end

        local movietitle = lookup_keys(info, "details/locale/en/movie_title")
        local desc       = lookup_keys(info, "details/locale/en/synopsis")

        for _, clip in ipairs(info["clips"]) do
            local item          = {}

            if clip["title"] == nil then
                item["name"] = movietitle
            else
                item["name"] = movietitle .. " (" .. clip["title"] .. ")"
            end
            item["path"]        = get_preferred_src(clip)
            item["artist"]      = clip["artist"]
            item["arturl"]      = clip["thumb"]
            item["description"] = desc
            item["url"]         = vlc.path

            table.insert(playlist, item)
        end
    else
        vlc.msg.err("Couldn't extract trailer video URL")
    end

    return playlist
end

-- Request, parse and return the info for a FilmID
function filmid_info(id)
    local film_url = "https://trailers.apple.com/trailers/feeds/data/" .. id .. ".json"
    vlc.msg.dbg("Fetching FilmID info from " .. film_url)

    return parse_json(film_url)
end

-- Get the user-preferred quality src
function get_preferred_src(clip)
    local resolution = vlc.var.inherit(nil, "preferred-resolution")
    if resolution == -1 then
        return lookup_keys(clip, "versions/enus/sizes/hd1080/srcAlt")
    end
    if resolution >= 1080 then
        return lookup_keys(clip, "versions/enus/sizes/hd1080/srcAlt")
    end
    if resolution >= 720 then
        return lookup_keys(clip, "versions/enus/sizes/hd720/srcAlt")
    end
    return lookup_keys(clip, "versions/enus/sizes/sd/srcAlt")
end

-- Resolve a "path" in a table or return nil if any of
-- the keys are not found
function lookup_keys(table, path)
    local value = table
    for token in path:gmatch( "[^/]+" ) do
        value = value[token]
        if value == nil then
            break
        end
    end
    return value
end
