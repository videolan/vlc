--[==========================================================================[
 host.lua: VLC Lua interface command line host module
--[==========================================================================[
 Copyright (C) 2007 the VideoLAN team
 $Id$

 Authors: Antoine Cellerier <dionoea at videolan dot org>

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
--]==========================================================================]

--[==========================================================================[
Example use:

    require "host"
    h = host.host()

    -- Bypass any authentication
    function on_password( client )
        client:switch_status( host.status.read )
    end
    h.status_callbacks[host.status.password] = on_password

    h:listen( "localhost:4212" )
    h:listen( "*console" )
    --or h:listen( { "localhost:4212", "*console" } )

    -- The main loop
    while not vlc.misc.should_die() do
        -- accept new connections and select active clients
        local write, read = h:accept_and_select()

        -- handle clients in write mode
        for _, client in pairs(write) do
            client:send()
            client.buffer = ""
            client:switch_status( host.status.read )
        end

        -- handle clients in read mode
        for _, client in pairs(read) do
            local str = client:recv(1000)
            str = string.gsub(str,"\r?\n$","")
            client.buffer = "Got `"..str.."'.\r\n"
            client:switch_status( host.status.write )
        end
    end

For complete examples see existing VLC Lua interface modules (ie telnet.lua)
--]==========================================================================]

module("host",package.seeall)

status = { init = 0, read = 1, write = 2, password = 3 }
client_type = { net = 1, stdio = 2, fifo = 3 }

function is_flag_set(val, flag)
    return (((val - (val % flag)) / flag) % 2 ~= 0)
end

function host()
    -- private data
    local clients = {}
    local listeners = {}
    local status_callbacks = {}

    -- private methods
    local function fd_client( client )
        if client.status == status.read then
            return client.rfd
        else -- status.write
            return client.wfd
        end
    end

    local function send( client, data, len )
        if len then
            return vlc.net.send( client.wfd, data, len )
        else
            return vlc.net.send( client.wfd, data or client.buffer )
        end
    end

    local function recv( client, len )
        if len then
            return vlc.net.recv( client.rfd, len )
        else
            return vlc.net.recv( client.rfd )
        end
    end

    local function write( client, data )
        return vlc.net.write( client.wfd, data or client.buffer )
    end

    local function read( client, len )
        if len then
            return vlc.net.read( client.rfd, len )
        else
            return vlc.net.read( client.rfd )
        end
    end

    local function del_client( client )
        if client.type == client_type.stdio then
            client:send( "Cannot delete stdin/stdout client.\n" )
            return
        end
        for i, c in pairs(clients) do
            if c == client then
                if client.type == client_type.net then
                    if client.wfd ~= client.rfd then
                        vlc.net.close( client.rfd )
                    end
                    vlc.net.close( client.wfd )
                end
                clients[i] = nil
                return
            end
        end
        vlc.msg.err("couldn't find client to remove.")
    end
    
    local function switch_status( client, s )
        if client.status == s then return end
        client.status = s
        if status_callbacks[s] then
            status_callbacks[s]( client )
        end
    end

    -- append a line to a client's (output) buffer
    local function append( client, string )
        client.buffer = client.buffer .. string .. "\r\n"
    end

    local function new_client( h, fd, wfd, t )
        if fd < 0 then return end
        local w, r
        if t == client_type.net then
            w = send
            r = recv
        else if t == client_type.stdio or t == client_type.fifo then
            w = write
            r = read
        else
            error("Unknown client type", t )
        end end
        local client = { -- data
                         rfd = fd,
                         wfd = wfd or fd,
                         status = status.init,
                         buffer = "",
                         cmds = "",
                         type = t,
                         -- methods
                         fd = fd_client,
                         send = w,
                         recv = r,
                         del = del_client,
                         switch_status = switch_status,
                         append = append,
                       }
        client:send( "VLC media player "..vlc.misc.version().."\n" )
        table.insert(clients, client)
        client:switch_status(status.password)
    end

    -- public methods
    local function _listen_tcp( h, host, port )
        if listeners.tcp and listeners.tcp[host]
                         and listeners.tcp[host][port] then
            error("Already listening on tcp host `"..host..":"..tostring(port).."'")
        end
        if not listeners.tcp then
            listeners.tcp = {}
        end
        if not listeners.tcp[host] then
            listeners.tcp[host] = {}
        end
        local listener = vlc.net.listen_tcp( host, port )
        listeners.tcp[host][port] = listener
        if not listeners.tcp.list then
            -- FIXME: if host == "list" we'll have a problem
            listeners.tcp.list = {}
            local m = { __mode = "v" } -- week values
            setmetatable( listeners.tcp.list, m )
        end
        table.insert( listeners.tcp.list, listener )
    end

    local function _listen_stdio( h )
        
        if listeners.stdio then
            error("Already listening on stdio")
        end
        new_client( h, 0, 1, client_type.stdio )
        listeners.stdio = true
    end

    local function _listen( h, url )
        if type(url)==type({}) then
            for _,u in pairs(url) do
                h:listen( u )
            end
        else
            vlc.msg.info( "Listening on host \""..url.."\"." )
            if url == "*console" then
                h:listen_stdio()
            else
                u = vlc.net.url_parse( url )
                h:listen_tcp( u.host, u.port )
            end
        end
    end

    local function _accept_and_select( h, timeout )
        local function filter_client( fds, status, event )
            for _, client in pairs(clients) do
                if client.status == status then
                    fds[client:fd()] = event
                end
            end
        end

        local pollfds = {}
        filter_client( pollfds, status.read, vlc.net.POLLIN )
        filter_client( pollfds, status.password, vlc.net.POLLIN )
        filter_client( pollfds, status.write, vlc.net.POLLOUT )
        if listeners.tcp then
            for _, listener in pairs(listeners.tcp.list) do
                for _, fd in pairs({listener:fds()}) do
                    pollfds[fd] = vlc.net.POLLIN
                end
            end
        end

        local ret = vlc.net.poll( pollfds )
        local wclients = {}
        local rclients = {}
        if ret > 0 then
            for _, client in pairs(clients) do
                if is_flag_set(pollfds[client:fd()], vlc.net.POLLERR)
                or is_flag_set(pollfds[client:fd()], vlc.net.POLLHUP)
                or is_flag_set(pollfds[client:fd()], vlc.net.POLLNVAL) then
                    del_client(client)
                elseif is_flag_set(pollfds[client:fd()], vlc.net.POLLOUT) then
                    table.insert(wclients,client)
                elseif is_flag_set(pollfds[client:fd()], vlc.net.POLLIN) then
                    table.insert(rclients,client)
                end
            end
            if listeners.tcp then
                for _, listener in pairs(listeners.tcp.list) do
                    for _, fd in pairs({listener:fds()}) do
                        if is_flag_set(pollfds[fd], vlc.net.POLLIN) then
                            local afd = listener:accept()
                            new_client( h, afd, afd, client_type.net )
                            break
                        end
                    end
                end
            end
        end

        return wclients, rclients
    end

    local function destructor( h )
        print "destructor"
        for _,client in pairs(clients) do
            client:send("Shutting down.")
            if client.type == client_type.tcp then
                if client.wfd ~= client.rfd then
                    vlc.net.close(client.rfd)
                end
                vlc.net.close(client.wfd)
            end
        end
    end

    local function _broadcast( h, msg )
        for _,client in pairs(clients) do
            client:send( msg )
        end
    end

    -- the instance
    local h = { -- data
                status_callbacks = status_callbacks,
                -- methods
                listen = _listen,
                listen_tcp = _listen_tcp,
                listen_stdio = _listen_stdio,
                accept_and_select = _accept_and_select,
                broadcast = _broadcast,
              }

    -- the metatable
    local m = { -- data
                __metatable = "Nothing to see here. Move along.",
                -- methods
                __gc = destructor,
              }

    setmetatable( h, m )

    return h
end
