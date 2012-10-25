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
-- vlc --rtsp-tcp rtsp://a625.l31335None.c31335.g.lq.akamaistream.net/7/625/31335/v0001/assembleenat.download.akamai.com/31335/mp4/fluxh264live7-169.sdp

-- references & interesting URLs:
-- http://www.assemblee-nationale.tv/js/direct.js
-- urlSdp + "fluxh264live"+stream+".sdp";
-- urlIPhone+'live'+stream+'/stream'+stream+'.m3u8
-- http://www.assemblee-nationale.tv/ahp/scripts/
-- arturl http://www.assemblee-nationale.tv/live/images/live1.jpg
-- seems wrong though, actually depends on agenda
-- alive checks:
-- http://www.assemblee-nationale.tv/php/testurl.php?checkUrl=http://www.assemblee-nationale.tv/../../../live/live1/live.txt&rnd=123456
-- http://www.assemblee-nationale.tv/live/live1/live.txt
-- http://www.assemblee-nationale.tv/live/live1/detection.txt

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
    local iphone_base
    -- from http://www.assemblee-nationale.tv/json/data.json
    -- XXX: maybe parse this file directly?
    local rtmp_base = "rtmp://172.25.13.10/live/"
    local rtsp_base
    -- RTMP application names, probed from JS file (XXX: use data.json instead?)
    local rtmp_names = {}
    local idx

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
        elseif( string.find( line, "var streamNames = new Array" ) ) then
            _, _, str = string.find( line, "Array%( (.*)%);" )
            repeat
                _, len, s = string.find( str, "\"([^\"]+)\"" )
                str = string.sub(str, len + 2)
                table.insert( rtmp_names, s )
            until string.len(str) == 0
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

    for idx=1,nb_streams,1 do
        -- we have a live stream
        local is_live = false
        -- the stream is 16/9
        local is_169 = false
        -- default art for this feed; agenda can have an alternate one
        local arturl = "http://www.assemblee-nationale.tv/live/images/live" .. idx .. ".jpg"
        local line

        vlc.msg.dbg("assembleenationale: checking live stream " .. tostring(idx))

        -- XXX: disable live checks if show_all?

        -- check for live stream
        fd, msg = vlc.stream( "http://www.assemblee-nationale.tv/php/testurl.php?checkUrl=http://www.assemblee-nationale.tv/../../../live/live" .. idx .. "/live169.txt&rnd=" .. os.time() )
        if not fd then
            vlc.msg.warn(msg)
            return nil
        end
        line = fd:readline()
        if line == "true" then
            --vlc.msg.dbg("LIVE 16/9")
            is_live = true
            is_169 = true
        else
            fd, msg = vlc.stream( "http://www.assemblee-nationale.tv/php/testurl.php?checkUrl=http://www.assemblee-nationale.tv/../../../live/live" .. idx .. "/live.txt&rnd=" .. os.time() )
            if not fd then
                vlc.msg.warn(msg)
                return nil
            end
            line = fd:readline()
            if line == "true" then
                --vlc.msg.dbg("LIVE")
                is_live = true
            end
        end

        if is_live or show_all then

            -- add rtsp streams
            if do_rtsp then
                local options={"deinterlace=1", "rtsp-tcp=1"}
                local path
                if is_169 then
                    path = rtsp_base .. "fluxh264live" .. tostring(idx) .. "-169.sdp"
                else
                    path = rtsp_base .. "fluxh264live" .. tostring(idx) .. ".sdp"
                end
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
                local path = iphone_base .. 'live' .. tostring(idx) .. '/stream' .. tostring(idx) .. '.m3u8'
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
