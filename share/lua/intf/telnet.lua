--[==========================================================================[
 telnet.lua: VLM interface plugin
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

description=
[============================================================================[
 VLM Interface plugin

 Copy (features wise) of the original VLC modules/control/telnet.c module.

 Differences are:
    * it's in Lua
    * 'lock' command to lock the telnet promt
    * possibility to listen on different hosts including stdin
      for example:
        listen on stdin: vlc -I lua --lua-intf telnet --lua-config "telnet={host='*console'}"
        listen on stdin + 2 ports on localhost: vlc -I lua --lua-intf telnet --lua-config "telnet={hosts={'localhost:4212','localhost:5678','*console'}}"
 
 Configuration options setable throught the --lua-config option are:
    * hosts: A list of hosts to listen on (see examples above).
    * host: A host to listen on. (won't be used if `hosts' is set)
    * password: The password used for remote clients.
    * prompt: The prompt.
]============================================================================]

require "host"

--[[ Some telnet command special characters ]]
WILL = "\251" -- Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option.
WONT = "\252" -- Indicates the refusal to perform, or continue performing, the indicated option.
DO   = "\253" -- Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option.
DONT = "\254" -- Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option.
IAC  = "\255" -- Interpret as command

ECHO = "\001"

--[[ Client status change callbacks ]]
function on_password( client )
    if client.type == host.client_type.net then
        client:send( "Password: " ..IAC..WILL..ECHO )
    else
        -- no authentification needed on stdin
        client:switch_status( host.status.read )
    end
end
function on_read( client )
    client:send( config.prompt and tostring(config.prompt) or "> " )
end
function on_write( client )
end

--[[ Misc functions ]]
function telnet_commands( client )
    -- remove telnet command replies from the client's data
    client.buffer = string.gsub( client.buffer, IAC.."["..DO..DONT..WILL..WONT.."].", "" )
end

function vlm_message_to_string(client,message,prefix)
    local prefix = prefix or ""
    if message.value then
        client:append(prefix .. message.name .. " : " .. message.value)
        return
    else
        client:append(prefix .. message.name)
        if message.children then
            for i,c in ipairs(message.children) do
                vlm_message_to_string(client,c,prefix.."    ")
            end
        end
        return
    end
end

--[[ Configure the host ]]
h = host.host()

h.status_callbacks[host.status.password] = on_password
h.status_callbacks[host.status.read] = on_read
h.status_callbacks[host.status.write] = on_write

h:listen( config.hosts or config.host or "localhost:4212" )
password = config.password or "admin"

--[[ Launch vlm ]]
vlm = vlc.vlm()

--[[ Commands ]]
function shutdown(client)
    h:broadcast("Shutting down.\r\n")
    vlc.msg.err("shutdown requested")
    vlc.misc.quit()
    return true
end
function logout(client)
    client:del()
    return true
end
function quit(client)
    if client.type == host.client_type.net then
        return logout(client)
    else
        return shutdown(client)
    end
end
function lock(client)
    client:send("\r\n")
    client:switch_status( host.status.password )
    client.buffer = ""
    return false
end
function print_text(text)
    return function(client)
        client:append(string.gsub(text,"\r?\n","\r\n"))
        return true
    end
end
function help(client)
    client:append("    Telnet Specific Commands:")
    for c,t in pairs(commands) do
        client:append("        "..c.." : "..t.help)
    end
    return true
end
commands = {
    ["shutdown"]    = { func = shutdown, help = "shutdown VLC" },
    ["quit"]        = { func = quit, help = "logout from telnet/shutdown VLC from local shell" },
    ["logout"]      = { func = logout, help = "logout" },
    ["lock"]        = { func = lock, help = "lock the telnet prompt" },
    ["description"] = { func = print_text(description), help = "describe this module" },
    ["license"]     = { func = print_text(vlc.misc.license()), help = "print VLC's license message" },
    ["help"]        = { func = help, help = "show this help", dovlm = true },
    }

function client_command( client )
    local cmd = client.buffer
    client.buffer = ""
    if not commands[cmd] or not commands[cmd].func or commands[cmd].dovlm then
        -- if it's not an interface specific command, it has to be a VLM command
        local message, vlc_err = vlm:execute_command( cmd )
        vlm_message_to_string( client, message )
        if not commands[cmd] or not commands[cmd].func and not commands[cmd].dovlm then
            if vlc_err ~= 0 then client:append( "Type `help' for help." ) end
            return true
        end
    end
    ok, msg = pcall( commands[cmd].func, client )
    if not ok then
        client:append( "Error in `"..cmd.."' "..msg )
        return true
    end
    return msg
end

--[[ The main loop ]]
while not vlc.misc.should_die() do
    h:accept()
    local w, r = h:select( 0.1 )

    -- Handle writes
    for _, client in pairs(w) do
        local len = client:send()
        client.buffer = string.sub(client.buffer,len+1)
        if client.buffer == "" then client:switch_status( host.status.read ) end
    end

    -- Handle reads
    for _, client in pairs(r) do
        local str = client:recv(1000)
        local done = false
        if string.match(str,"\n$") then
            client.buffer = string.gsub(client.buffer..str,"\r?\n$","")
            done = true
        elseif client.buffer == ""
           and ((client.type == host.client_type.stdio and str == "")
           or  (client.type == host.client_type.net and str == "\004")) then
            -- Caught a ^D
            client.buffer = "quit"
            done = true
        else
            client.buffer = client.buffer .. str
        end
        if client.type == host.client_type.net then
            telnet_commands( client )
        end
        if done then
            if client.status == host.status.password then
                if client.buffer == password then
                    client:send( IAC..WONT..ECHO.."\r\nWelcome, Master\r\n" )
                    client.buffer = ""
                    client:switch_status( host.status.write )
                else
                    client:send( "\r\nWrong password\r\nPassword: " )
                    client.buffer = ""
                end
            elseif client_command( client ) then
                client:switch_status( host.status.write )
            end
        end
    end
end

--[[ Clean up ]]
vlm = nil
