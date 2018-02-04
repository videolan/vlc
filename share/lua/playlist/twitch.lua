--[[
Resolve Twitch channel and video URLs to the actual stream URL

 $Id$
 Copyright © 2017 the VideoLAN team

 Author: Marvin Scholz <epirat07 at gmail dot com>

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
        and ( vlc.path:match("^www%.twitch%.tv/videos/.+") or
              vlc.path:match("^www%.twitch%.tv/.+") or
              vlc.path:match("^go%.twitch%.tv/.+") or 
              vlc.path:match("^go%.twitch%.tv/videos/.+") )
end

-- Download and parse a JSON document from the specified URL
-- Returns: obj, pos, err (see dkjson docs)
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

    return json.decode(string)
end

-- Make a request to the Twitch API endpoint given by url
-- Returns: obj, err
function twitch_api_req(url)
    local obj, pos, err = parse_json(url .. "?client_id=1ht9oitznxzdo3agmdbn3dydbm06q2")

    if err then
        return nil, "Error getting JSON object: " .. err
    end

    -- In case of error, the API will return an object with
    -- error and message given
    if obj.error then
        local err = "Twitch API error: " .. obj.error
        if obj.message then
            err = err .. " (" .. obj.message .. ")"
        end
        return nil, err
    end

    return obj, nil
end

-- Parser for twitch video urls
function parse_video()
    local playlist = {}
    local item = {}
    local url, obj, err = nil

    -- Parse video id out of url
    local video_id = vlc.path:match("/videos/(%d+)")

    if video_id == nil then
        vlc.msg.err("Twitch: Failed to parse twitch url for video id")
        return playlist
    end

    vlc.msg.dbg("Twitch: Loading video url for " .. video_id)

    -- Request video token (required for the video stream)
    url = "https://api.twitch.tv/api/vods/" ..  video_id .. "/access_token"
    obj, err = twitch_api_req(url)

    if err then
        vlc.msg.err("Error getting request token from Twitch: " .. err)
        return playlist
    end

    -- Construct stream url
    local stream_url = "http://usher.twitch.tv/vod/" .. video_id
    stream_url = stream_url .. "?player=twitchweb"
    stream_url = stream_url .. "&nauth=" .. vlc.strings.encode_uri_component(obj.token)
    stream_url = stream_url .. "&nauthsig=" .. obj.sig
    stream_url = stream_url .. "&allow_audio_only=true&allow_source=true"

    item["path"]   = stream_url

    -- Set base information
    item["name"]   = "Twitch: " .. video_id

    -- Request video information
    url = "https://api.twitch.tv/kraken/videos/v" .. video_id
    obj, err = twitch_api_req(url)

    if err then
        vlc.msg.warn("Error getting video info from Twitch: " .. err)
        table.insert(playlist, item)
        return playlist
    end

    -- Overwrite with now obtained better info
    item["name"]        = "Twitch: " .. obj.title
    item["artist"]      = obj.channel.display_name
    item["description"] = obj.description
    item["url"]         = vlc.path

    table.insert(playlist, item)

    return playlist
end

-- Parser for twitch stream urls
function parse_stream()
    local playlist = {}
    local item = {}
    local url, obj, err = nil

    -- Parse channel name out of url
    local channel = vlc.path:match("/([a-zA-Z0-9_]+)")

    if channel == nil then
        vlc.msg.err("Twitch: Failed to parse twitch url for channel name")
        return playlist
    end

    vlc.msg.dbg("Twitch: Loading stream url for " .. channel)

    -- Request channel token (required for the stream m3u8)
    url = "https://api.twitch.tv/api/channels/" .. channel .. "/access_token"
    obj, err = twitch_api_req(url)

    if err then
        vlc.msg.err("Error getting request token from Twitch: " .. err)
        return playlist
    end

    -- Construct stream url
    local stream_url = "http://usher.twitch.tv/api/channel/hls/" .. channel .. ".m3u8"
    stream_url = stream_url .. "?player=twitchweb"
    stream_url = stream_url .. "&token=" .. vlc.strings.encode_uri_component(obj.token)
    stream_url = stream_url .. "&sig=" .. obj.sig
    stream_url = stream_url .. "&allow_audio_only=true&allow_source=true&type=any"

    item["path"]   = stream_url

    -- Set base information
    item["name"]   = "Twitch: " .. channel
    item["artist"] = channel

    -- Request channel information
    url = "https://api.twitch.tv/api/channels/" .. channel
    obj, err = twitch_api_req(url)

    if err then
        vlc.msg.warn("Error getting channel info from Twitch: " .. err)
        table.insert(playlist, item)
        return playlist
    end

    -- Overwrite with now obtained better info
    item["name"]        = "Twitch: " .. obj.display_name
    item["nowplaying"]  = obj.display_name .. " playing " .. obj.game
    item["artist"]      = obj.display_name
    item["description"] = obj.status
    item["url"]         = vlc.path

    table.insert(playlist, item)

    return playlist
end

-- Parse function
function parse()
    if vlc.path:match("/videos/.+") then
        return parse_video()
    else
        return parse_stream()
    end
end
