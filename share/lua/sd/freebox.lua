--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>

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
    return { title="Freebox TV" }
end

function main()
    local logos = {}

    -- fetch urls for basic channels logos
    local fd, msg = vlc.stream( "http://free.fr/adsl/pages/television/services-de-television/acces-a-plus-250-chaines/bouquet-basique.html" )
    if not fd then
        vlc.msg.warn(msg)
        -- not fatal
    else
        local channel, logourl
        local line = fd:readline()
        while line ~= nil do
            if( string.find( line, "tv%-chaine%-" ) ) then
                _, _, channel, logourl = string.find( line, "\"tv%-chaine%-(%d+)\".*<img%s*src=\"([^\"]*)\"" )
                -- fix spaces
                logourl = string.gsub( logourl, " ", "%%20" )
                logos[channel] = "http://free.fr" .. logourl
            end
            line = fd:readline()
        end
    end

    -- fetch urls for optional channels logos
    local fd, msg = vlc.stream( "http://www.free.fr/adsl/pages/television/services-de-television/acces-a-plus-250-chaines/en-option.html" )
    if not fd then
        vlc.msg.warn(msg)
        -- not fatal
    else
        local channel, logourl
        local line = fd:readline()
        while line ~= nil do
            if( string.find( line, "tv%-chaine%-" ) ) then
                _, _, channel, logourl = string.find( line, "\"tv%-chaine%-(%d+)\".*<img%s*src=\"([^\"]*)\"" )
                -- fix spaces
                logourl = string.gsub( logourl, " ", "%%20" )
                logos[channel] = "http://free.fr" .. logourl
            end
            line = fd:readline()
        end
    end

    -- fetch the playlist
    fd, msg = vlc.stream( "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u" )
    if not fd then
        vlc.msg.warn(msg)
        return nil
    end
    local line=  fd:readline()
    if line ~= "#EXTM3U" then
        return nil
    end
    line = fd:readline()
    local duration, artist, name, arturl
    local options={"deinterlace=1"}
    while line ~= nil do
        if( string.find( line, "#EXTINF" ) ) then
            _, _, duration, artist, name = string.find( line, ":(%w+),(%w+)%s*-%s*(.+)" )
            arturl = logos[artist]
        elseif( string.find( line, "#EXTVLCOPT" ) ) then
            _, _, option = string.find( line, ":(.+)" )
            table.insert( options, option )
        else
            vlc.sd.add_item( {path=line,duration=duration,artist=artist,title=name,arturl=arturl,options=options} )
            duration = nil
            artist = nil
            name = nil
            arturl = nil
            options={"deinterlace=1"}
        end
        line = fd:readline()
    end
end
