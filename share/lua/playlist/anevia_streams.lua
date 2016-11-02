--[[
 $Id$

 Parse list of available streams on Anevia Toucan servers.
 The URI http://ipaddress:554/list_streams.idp describes a list of
 available streams on the server.

 Copyright © 2009 M2X BV

 Authors: Jean-Paul Saman <jpsaman@videolan.org>

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
        and string.match( vlc.path, "/list_streams%.idp" )
end

-- Parse function.
function parse()
    p = {}
    _,_,server = string.find( vlc.path, "(.*)/list_streams.idp" )
    while true do
        line = vlc.readline()
        if not line then break end

        if string.match( line, "<streams[-]list> </stream[-]list>" ) then
            break
        elseif string.match( line, "<streams[-]list xmlns=\"(.*)\">" ) then
            while true do
                line = vlc.readline()
                if not line then break end
                if string.match( line, "<streamer name=\"(.*)\"> </streamer>" ) then
                    break;
                elseif string.match( line, "<streamer name=\"(.*)\">" ) then
                    _,_,streamer = string.find( line, "name=\"(.*)\"" )
                    while true do
                        line = vlc.readline()
                        if not line then break end
                        -- ignore <host name=".." /> tags
                        -- ignore <port number=".." /> tags
                        if string.match( line, "<input name=\"(.*)\">" ) then
                            _,_,path = string.find( line, "name=\"(.*)\"" )
                            while true do
                                line = vlc.readline()
                                if not line then break end
                                if string.match( line, "<stream id=\"(.*)\" />" ) then
                                    _,_,media = string.find( line, "id=\"(.*)\"" )
                                    video = "rtsp://" .. tostring(server) .. "/" .. tostring(path) .. "/" .. tostring(media)
                                    vlc.msg.dbg( "adding to playlist " .. tostring(video) )
                                    table.insert( p, { path = video; name = media, url = video } )
                                end
--                              end of input tag found
                                if string.match( line, "</input>" ) then
                                    break
                                end
                            end
                        end
                    end
                    if not line then break end
--                  end of streamer tag found
                    if string.match( line, "</streamer>" ) then
                        break
                    end
                end
                if not line then break end
--              end of streams-list tag found
                if string.match( line, "</streams-list>" ) then
                    break
                end
            end
        end

    end
    return p
end
