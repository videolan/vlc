--[[
 $Id$

 Copyright © 2012 VideoLAN and AUTHORS

 Authors: François Revol <revol at free dot fr>

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

function descriptor()
    return { title="Assemblée Nationale" }
end


-- TODO: get correct names and arturl from current agenda

-- web entry point:
-- http://www.assemblee-nationale.tv/direct.html?flux=7#

-- advertised command line:
-- vlc --rtsp-tcp rtsp://wowg.tdf-cdn.com/5638/live

-- references & interesting URLs:
-- http://www.assemblee-nationale.tv/js/direct.js
-- http://www.assemblee-nationale.tv/ahp/scripts/
-- arturl http://www.assemblee-nationale.tv/live/images/live1.jpg
-- seems wrong though, actually depends on agenda

-- 2013-02-02: it seems they changed some urls...
-- rtsp://wowg.tdf-cdn.com/5638/live
-- http://chkg.tdf-cdn.com/5707/stream1.m3u8
-- we should probably probe those but I'm lazy
-- the live feeds list has changed too:
-- http://www.assemblee-nationale.tv/live/live.txt?rnd=

function main()
    -- disregard live checks and list all possible feeds
    local show_all = false
    --local name_prefix = "Assemblée Nationale"
    local name_prefix = "DirectAN"
    -- enable protocol-specific streams
    -- rtmp isn't well supported yet...
    local do_rtmp = false
    local do_rtsp = true
    local do_iphone = true
    -- number of probed possible streams
    local nb_streams = 0
    -- base URI for each protocol
    -- XXX: should be probed as in the js code...
    local iphone_base = "http://chkg.tdf-cdn.com/"
    -- from http://www.assemblee-nationale.tv/json/data.json
    -- XXX: maybe parse this file directly?
    local rtmp_base = "rtmp://172.25.13.10/live/"
    local rtsp_base = "rtsp://wowg.tdf-cdn.com/"
    -- RTMP application names, probed from JS file (XXX: use data.json instead?)
    -- latest js has a list for other protocols as well
    local rtmp_names = {}
    local app_names = {}
    local http_names = {}
    local rtsp_names = {}
    local hls_names = {}
    local idx
    -- live status of each stream
    local live_streams = {}

    -- fetch the main JS file
    fd, msg = vlc.stream( "http://www.assemblee-nationale.tv/js/direct.js" )
    if not fd then
        vlc.msg.warn(msg)
        return nil
    end

    local duration, artist, name, arturl

    line = fd:readline()
    while line ~= nil do
        if( string.find( line, "var NB_FLUX = " ) ) then
            _, _, str = string.find( line, "var NB_FLUX = ([0-9]+);" )
            nb_streams = tonumber(str)
        elseif( string.find( line, "var urlSdp = \"rtsp%://" ) ) then
            _, _, path = string.find( line, "var urlSdp = \"([^\"]+)\"" )
            rtsp_base = path
        elseif( string.find( line, "urlIPhone = \"" ) ) then
            _, _, path = string.find( line, "\"([^\"]+)\"" )
            iphone_base = path
        end
        line = fd:readline()
    end

    -- fetch the streams JS file
    fd, msg = vlc.stream( "http://www.assemblee-nationale.tv/ahp/scripts/streams.js" )
    if not fd then
        vlc.msg.warn(msg)
        return nil
    end

    line = fd:readline()
    while line ~= nil do
        if( string.find( line, "var streamNames = new Array" ) ) then
            _, _, str = string.find( line, "Array%( (.*)%);" )
            repeat
                _, len, s = string.find( str, "\"([^\"]+)\"" )
                str = string.sub(str, len + 2)
                table.insert( rtmp_names, s )
            until string.len(str) == 0
            -- vlc.msg.warn(table.concat(rtmp_names,","))
        elseif( string.find( line, "var appNames = new Array" ) ) then
            _, _, str = string.find( line, "Array%( (.*)%);" )
            repeat
                _, len, s = string.find( str, "\"([^\"]+)\"" )
                str = string.sub(str, len + 2)
                table.insert( app_names, s )
            until string.len(str) == 0
            -- vlc.msg.warn(table.concat(app_names,","))
        elseif( string.find( line, "var streamNamesHttp = new Array" ) ) then
            _, _, str = string.find( line, "Array%( (.*)%);" )
            repeat
                _, len, s = string.find( str, "\"([^\"]+)\"" )
                str = string.sub(str, len + 2)
                table.insert( http_names, s )
            until string.len(str) == 0
            -- vlc.msg.warn(table.concat(http_names,","))
        elseif( string.find( line, "var streamNamesRtsp = new Array" ) ) then
            _, _, str = string.find( line, "Array%( (.*)%);" )
            repeat
                _, len, s = string.find( str, "\"([^\"]+)\"" )
                str = string.sub(str, len + 2)
                table.insert( rtsp_names, s )
            until string.len(str) == 0
            -- vlc.msg.warn(table.concat(rtsp_names,","))
        elseif( string.find( line, "var streamNamesHls = new Array" ) ) then
            _, _, str = string.find( line, "Array%( (.*)%);" )
            repeat
                _, len, s = string.find( str, "\"([^\"]+)\"" )
                str = string.sub(str, len + 2)
                table.insert( hls_names, s )
            until string.len(str) == 0
            -- vlc.msg.warn(table.concat(hls_names,","))
        end
        line = fd:readline()
    end

    if not rtmp_base then
        vlc.msg.warn("Unable to locate rmsp base url")
    end
    if not rtsp_base then
        vlc.msg.warn("Unable to locate rtsp base url")
    end
    if not iphone_base then
        vlc.msg.warn("Unable to locate iphone base url")
    end

    -- fetch the current live status
    -- XXX: disable live checks if show_all?
    fd, msg = vlc.stream( "http://www.assemblee-nationale.tv/live/live.txt?rnd=" .. os.time() )
    if not fd then
        vlc.msg.warn(msg)
        return nil
    end

    line = fd:readline()
    while line ~= nil do
        local stream = tonumber(line)
        if stream ~= nil then
            live_streams[stream] = 1
            vlc.msg.dbg("live: " .. tostring(stream))
        end
        line = fd:readline()
    end

    -- build playlist
    for idx=1,nb_streams,1 do
        -- we have a live stream
        local is_live = live_streams[idx] ~= nil
        -- default art for this feed; agenda can have an alternate one
        local arturl = "http://www.assemblee-nationale.tv/live/images/live" .. idx .. ".jpg"
        local line

        if is_live or show_all then

            -- add rtsp streams
            if do_rtsp then
                local options={"deinterlace=1", "rtsp-tcp=1"}
                local path
                path = rtsp_base .. rtsp_names[idx] .. "/live"
                vlc.sd.add_item( {path=path,
                duration=duration,
                artist=artist,
                title=name_prefix .. " " .. idx .. " (rtsp)",
                arturl=arturl,
                options=options} )
            end

            -- add rtmp
            if do_rtmp then
                local options={"deinterlace=1"}
                local path = rtmp_base .. rtmp_names[idx]
                vlc.sd.add_item( {path=path,
                duration=duration,
                artist=artist,
                title=name_prefix .. " " .. idx .. " (rtmp)",
                arturl=arturl,
                options=options} )
            end

            -- add iphone (m3u8) streams, VLC doesn't like them much yet
            if do_iphone then
                local options={"deinterlace=1"}
                local path = iphone_base .. hls_names[idx] .. '/stream1.m3u8'
                vlc.sd.add_item( {path=path,
                duration=duration,
                artist=artist,
                title=name_prefix .. " " .. idx .. " (iphone)",
                arturl=arturl,
                options=options} )
            end
        end
    end
end
