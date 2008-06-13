--[==========================================================================[
 rc.lua: remote control module for VLC
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
 Remote control interface for VLC

 This is a modules/control/rc.c look alike (with a bunch of new features)
 
 Use on local term:
    vlc -I luarc
 Use on tcp connection:
    vlc -I luarc --lua-config "rc={host='localhost:4212'}"
 Use on multiple hosts (term + 2 tcp ports):
    vlc -I luarc --lua-config "rc={hosts={'*console','localhost:4212','localhost:5678'}}"
 
 Note:
    -I luarc is an alias for -I lua --lua-intf rc

 Configuration options setable throught the --lua-config option are:
    * hosts: A list of hosts to listen on.
    * host: A host to listen on. (won't be used if `hosts' is set)
 The following can be set using the --lua-config option or in the interface
 itself using the `set' command:
    * prompt: The prompt.
    * welcome: The welcome message.
    * width: The default terminal width (used to format text).
    * autocompletion: When issuing an unknown command, print a list of
                      possible commands to autocomplete with. (0 to disable,
                      1 to enable).
    * autoalias: If autocompletion returns only one possibility, use it
                 (0 to disable, 1 to enable).
    * flatplaylist: 0 to disable, 1 to enable.
]============================================================================]

require("common")
skip = common.skip
skip2 = function(foo) return skip(skip(foo)) end
setarg = common.setarg
strip = common.strip

--[[ Setup default environement ]]
env = { prompt = "> ";
        width = 70;
        autocompletion = 1;
        autoalias = 1;
        welcome = "Remote control interface initialized. Type `help' for help.";
        flatplaylist = 0;
      }

--[[ Import custom environement variables from the command line config (if possible) ]]
for k,v in pairs(env) do
    if config[k] then
        if type(env[k]) == type(config[k]) then
            env[k] = config[k]
            vlc.msg.dbg("set environement variable `"..k.."' to "..tonumber(env[k]))
        else
            vlc.msg.err("environement variable `"..k.."' should be of type "..type(env[k])..". config value will be discarded.")
        end
    end
end

--[[ Command functions ]]
function set_env(name,client,value)
    if value then
        local var,val = split_input(value)
        if val then
            s = string.gsub(val,"\"(.*)\"","%1")
            if type(client.env[var])==type(1) then
                client.env[var] = tonumber(s)
            else
                client.env[var] = s
            end
        else
            client:append( tostring(client.env[var]) )
        end
    else
        for e,v in common.pairs_sorted(client.env) do
            client:append(e.."="..v)
        end
    end
end

function save_env(name,client,value)
    env = common.table_copy(client.env)
end

function alias(client,value)
    if value then
        local var,val = split_input(value)
        if commands[var] and type(commands[var]) ~= type("") then
            client:append("Error: cannot use a primary command as an alias name")
        else
            if commands[val] then
                commands[var]=val
            else
                client:append("Error: unknown primary command `"..val.."'.")
            end
        end
    else
        for c,v in common.pairs_sorted(commands) do
            if type(v)==type("") then
                client:append(c.."="..v)
            end
        end
    end
end

function fixme(name,client)
    client:append( "FIXME: unimplemented command `"..name.."'." )
end

function logout(name,client)
    if client.type == host.client_type.net then
        client:send("Bye-bye!")
        client:del()
    else
        client:append("Error: Can't logout of stdin/stdout. Use quit or shutdown to close VLC.")
    end
end

function shutdown(name,client)
    client:append("Bye-bye!")
    h:broadcast("Shutting down.")
    vlc.msg.info("Requested shutdown.")
    vlc.misc.quit()
end

function quit(name,client)
    if client.type == host.client_type.net then
        logout(name,client)
    else
        shutdown(name,client)
    end
end

function add(name,client,arg)
    -- TODO: parse (and use) options
    local f
    if name == "enqueue" then
        f = vlc.playlist.enqueue
    else
        f = vlc.playlist.add
    end
    f({{path=arg}})
end

function playlist_is_tree( client )
    if client.env.flatplaylist == 0 then
        return true
    else
        return false
    end
end

function playlist(name,client,arg)
    function playlist0(item,prefix)
        local prefix = prefix or ""
        if not item.flags.disabled then
            local str = "| "..prefix..tostring(item.id).." - "..item.name
            if item.duration > 0 then
                str = str.." ("..common.durationtostring(item.duration)..")"
            end
            if item.nb_played > 0 then
                str = str.." [played "..tostring(item.nb_played).." time"
                if item.nb_played > 1 then
                    str = str .. "s"
                end
                str = str .. "]"
            end
            client:append(str)
        end
        if item.children then
            for _, c in ipairs(item.children) do
                playlist0(c,prefix.."  ")
            end
        end
    end
    local playlist
    local tree = playlist_is_tree(client)
    if name == "search" then
        playlist = vlc.playlist.search(arg or "", tree)
    else
        if tonumber(arg) then
            playlist = vlc.playlist.get(tonumber(arg), tree)
        elseif arg then
            playlist = vlc.playlist.get(arg, tree)
        else
            playlist = vlc.playlist.get(nil, tree)
        end
    end
    if name == "search" then
        client:append("+----[ Search - "..(arg or "`reset'").." ]")
    else
        client:append("+----[ Playlist - "..playlist.name.." ]")
    end
    if playlist.children then
        for _, item in ipairs(playlist.children) do
            playlist0(item)
        end
    else
        playlist0(playlist)
    end
    if name == "search" then
        client:append("+----[ End of search - Use `search' to reset ]")
    else
        client:append("+----[ End of playlist ]")
    end
end

function playlist_sort(name,client,arg)
    if not arg then
        client:append("Valid sort keys are: id, title, artist, genre, random, duration, album.")
    else
        local tree = playlist_is_tree(client)
        vlc.playlist.sort(arg,false,tree)
    end
end

function services_discovery(name,client,arg)
    if arg then
        if vlc.sd.is_loaded(arg) then
            vlc.sd.remove(arg)
            client:append(arg.." disabled.")
        else
            vlc.sd.add(arg)
            client:append(arg.." enabled.")
        end
    else
        local sd = vlc.sd.get_services_names()
        client:append("+----[ Services discovery ]")
        for n,ln in pairs(sd) do
            local status
            if vlc.sd.is_loaded(n) then
                status = "enabled"
            else
                status = "disabled"
            end
            client:append("| "..n..": " .. ln .. " (" .. status .. ")")
        end
        client:append("+----[ End of services discovery ]")
    end
end

function print_text(label,text)
    return function(name,client)
        client:append("+----[ "..label.." ]")
        client:append "|"
        for line in string.gmatch(text,".-\r?\n") do
            client:append("| "..string.gsub(line,"\r?\n",""))
        end
        client:append "|"
        client:append("+----[ End of "..string.lower(label).." ]")
    end
end

function help(name,client,arg)
    local width = client.env.width
    local long = (name == "longhelp")
    local extra = ""
    if arg then extra = "matching `" .. arg .. "' " end
    client:append("+----[ Remote control commands "..extra.."]")
    for i, cmd in ipairs(commands_ordered) do
        if (cmd == "" or not commands[cmd].adv or long)
        and (not arg or string.match(cmd,arg)) then
            local str = "| " .. cmd
            if cmd ~= "" then
                local val = commands[cmd]
                if val.aliases then
                    for _,a in ipairs(val.aliases) do
                        str = str .. ", " .. a
                    end
                end
                if val.args then str = str .. " " .. val.args end
                if #str%2 == 1 then str = str .. " " end
                str = str .. string.rep(" .",(width-(#str+#val.help)-1)/2)
                str = str .. string.rep(" ",width-#str-#val.help) .. val.help
            end
            client:append(str)
        end
    end
    client:append("+----[ end of help ]")
end

function input_info(name,client)
    local categories = vlc.input_info()
    for cat, infos in pairs(categories) do
        client:append("+----[ "..cat.." ]")
        client:append("|")
        for name, value in pairs(infos) do
            client:append("| "..name..": "..value)
        end
        client:append("|")
    end
    client:append("+----[ end of stream info ]")
end

function playlist_status(name,client)
    local a,b,c = vlc.playlist.status()
    client:append( "( new input: " .. tostring(a) .. " )" )
    client:append( "( audio volume: " .. tostring(b) .. " )")
    client:append( "( state " .. tostring(c) .. " )")
end

function is_playing(name,client)
    if vlc.input.is_playing() then client:append "1" else client:append "0" end
end

function ret_print(foo,start,stop)
    local start = start or ""
    local stop = stop or ""
    return function(discard,client,...) client:append(start..tostring(foo(...))..stop) end
end

function get_time(var,client)
    return function()
        local input = vlc.object.input()
        client:append(math.floor(vlc.var.get( input, var )))
    end
end

function titlechap(name,client,value)
    local input = vlc.object.input()
    local var = string.gsub( name, "_.*$", "" )
    if value then
        vlc.var.set( input, var, value )
    else
        local item = vlc.var.get( input, var )
        -- Todo: add item name conversion
        client:apped(item)
    end
end
function titlechap_offset(client,offset)
    return function(name,value)
        local input = vlc.object.input()
        local var = string.gsub( name, "_.*$", "" )
        vlc.var.set( input, var, vlc.var.get( input, var )+offset )
    end
end

function seek(name,client,value)
    common.seek(value)
end

function volume(name,client,value)
    if value then
        vlc.volume.set(value)
    else
        client:append(tostring(vlc.volume.get()))
    end
end

function rate(name,client)
    local input = vlc.object.input()
    if name == "normal" then
        vlc.var.set(input,"rate",1000) -- FIXME: INPUT_RATE_DEFAULT
    else
        vlc.var.set(input,"rate-"..name,nil)
    end
end

function listvalue(obj,var)
    return function(client,value)
        local o = vlc.object.find(nil,obj,"anywhere")
        if not o then return end
        if value then
            vlc.var.set( o, var, value )
        else
            local c = vlc.var.get( o, var )
            local v, l = vlc.var.get_list( o, var )
            client:append("+----[ "..var.." ]")
            for i,val in ipairs(v) do
                local mark = (val==c)and " *" or ""
                client:append("| "..tostring(val).." - "..tostring(l[i])..mark)
            end
            client:append("+----[ end of "..var.." ]")
        end
    end
end

function eval(client,val)
    client:append(loadstring("return "..val)())
end

--[[ Declare commands, register their callback functions and provide
     help strings here.
     Syntax is:
     "<command name>"; { func = <function>; [ args = "<str>"; ] help = "<str>"; [ adv = <bool>; ] [ aliases = { ["<str>";]* }; ] }
     ]]
commands_ordered = {
    { "add"; { func = add; args = "XYZ"; help = "add XYZ to playlist" } };
    { "enqueue"; { func = add; args = "XYZ"; help = "queue XYZ to playlist" } };
    { "playlist"; { func = playlist; help = "show items currently in playlist" } };
    { "search"; { func = playlist; args = "[string]"; help = "search for items in playlist (or reset search)" } };
    { "sort"; { func = playlist_sort; args = "key"; help = "sort the playlist" } };
    { "sd"; { func = services_discovery; args = "[sd]"; help = "show services discovery or toggle" } };
    { "play"; { func = skip2(vlc.playlist.play); help = "play stream" } };
    { "stop"; { func = skip2(vlc.playlist.stop); help = "stop stream" } };
    { "next"; { func = skip2(vlc.playlist.next); help = "next playlist item" } };
    { "prev"; { func = skip2(vlc.playlist.prev); help = "previous playlist item" } };
    { "goto"; { func = skip2(vlc.playlist.goto); help = "goto item at index" } };
    { "repeat"; { func = skip2(vlc.playlist.repeat_); args = "[on|off]"; help = "toggle playlist repeat" } };
    { "loop"; { func = skip2(vlc.playlist.loop); args = "[on|off]"; help = "toggle playlist loop" } };
    { "random"; { func = skip2(vlc.playlist.random); args = "[on|off]"; help = "toggle playlist random" } };
    { "clear"; { func = skip2(vlc.playlist.clear); help = "clear the playlist" } };
    { "status"; { func = playlist_status; help = "current playlist status" } };
    { "title"; { func = titlechap; args = "[X]"; help = "set/get title in current item" } };
    { "title_n"; { func = titlechap_offset(1); help = "next title in current item" } };
    { "title_p"; { func = titlechap_offset(-1); help = "previous title in current item" } };
    { "chapter"; { func = titlechap; args = "[X]"; help = "set/get chapter in current item" } };
    { "chapter_n"; { func = titlechap_offset(1); help = "next chapter in current item" } };
    { "chapter_p"; { func = titlechap_offset(-1); help = "previous chapter in current item" } };
    { "" };
    { "seek"; { func = seek; args = "X"; help = "seek in seconds, for instance `seek 12'" } };
    { "pause"; { func = setarg(common.hotkey,"key-play-pause"); help = "toggle pause" } };
    { "fastforward"; { func = setarg(common.hotkey,"key-jump+extrashort"); help = "set to maximum rate" } };
    { "rewind"; { func = setarg(common.hotkey,"key-jump-extrashort"); help = "set to minimum rate" } };
    { "faster"; { func = rate; help = "faster playing of stream" } };
    { "slower"; { func = rate; help = "slower playing of stream" } };
    { "normal"; { func = rate; help = "normal playing of stream" } };
    { "fullscreen"; { func = skip2(vlc.video.fullscreen); args = "[on|off]"; help = "toggle fullscreen"; aliases = { "f", "F" } } };
    { "info"; { func = input_info; help = "information about the current stream" } };
    { "get_time"; { func = get_time("time"); help = "seconds elapsed since stream's beginning" } };
    { "is_playing"; { func = is_playing; help = "1 if a stream plays, 0 otherwise" } };
    { "get_title"; { func = ret_print(vlc.input.get_title); help = "the title of the current stream" } };
    { "get_length"; { func = get_time("length"); help = "the length of the current stream" } };
    { "" };
    { "volume"; { func = volume; args = "[X]"; help = "set/get audio volume" } };
    { "volup"; { func = ret_print(vlc.volume.up,"( audio volume: "," )"); args = "[X]"; help = "raise audio volume X steps" } };
    { "voldown"; { func = ret_print(vlc.volume.down,"( audio volume: "," )"); args = "[X]"; help = "lower audio volume X steps" } };
    { "adev"; { func = skip(listvalue("aout","audio-device")); args = "[X]"; help = "set/get audio device" } };
    { "achan"; { func = skip(listvalue("aout","audio-channels")); args = "[X]"; help = "set/get audio channels" } };
    { "atrack"; { func = skip(listvalue("input","audio-es")); args = "[X]"; help = "set/get audio track" } };
    { "vtrack"; { func = skip(listvalue("input","video-es")); args = "[X]"; help = "set/get video track" } };
    { "vratio"; { func = skip(listvalue("vout","aspect-ratio")); args = "[X]"; help = "set/get video aspect ratio" } };
    { "vcrop"; { func = skip(listvalue("vout","crop")); args = "[X]"; help = "set/get video crop"; aliases = { "crop" } } };
    { "vzoom"; { func = skip(listvalue("vout","zoom")); args = "[X]"; help = "set/get video zoom"; aliases = { "zoom" } } };
    { "snapshot"; { func = common.snapshot; help = "take video snapshot" } };
    { "strack"; { func = skip(listvalue("input","spu-es")); args = "[X]"; help = "set/get subtitles track" } };
    { "hotkey"; { func = skip(common.hotkey); args = "[hotkey name]"; help = "simulate hotkey press"; adv = true; aliases = { "key" } } };
    { "menu"; { func = fixme; args = "[on|off|up|down|left|right|select]"; help = "use menu"; adv = true } };
    { "" };
    { "set"; { func = set_env; args = "[var [value]]"; help = "set/get env var"; adv = true } };
    { "save_env"; { func = save_env; help = "save env vars (for future clients)"; adv = true } };
    { "alias"; { func = skip(alias); args = "[cmd]"; help = "set/get command aliases"; adv = true } };
    { "eval"; { func = skip(eval); help = "eval some lua (*debug*)"; adv =true } }; -- FIXME: comment out if you're not debugging
    { "description"; { func = print_text("Description",description); help = "describe this module" } };
    { "license"; { func = print_text("License message",vlc.misc.license()); help = "print VLC's license message"; adv = true } };
    { "help"; { func = help; args = "[pattern]"; help = "a help message"; aliases = { "?" } } };
    { "longhelp"; { func = help; args = "[pattern]"; help = "a longer help message" } };
    { "logout"; { func = logout; help = "exit (if in a socket connection)" } };
    { "quit"; { func = quit; help = "quit VLC (or logout if in a socket connection)" } };
    { "shutdown"; { func = shutdown; help = "shutdown VLC" } };
    }
commands = {}
for i, cmd in ipairs( commands_ordered ) do
    if #cmd == 2 then
        commands[cmd[1]]=cmd[2]
        if cmd[2].aliases then
            for _,a in ipairs(cmd[2].aliases) do
                commands[a]=cmd[1]
            end
        end
    end
    commands_ordered[i]=cmd[1]
end
--[[ From now on commands_ordered is a list of the different command names
     and commands is a associative array indexed by the command name. ]]

-- Compute the column width used when printing a the autocompletion list
env.colwidth = 0
for c,_ in pairs(commands) do
    if #c > env.colwidth then env.colwidth = #c end
end
env.coldwidth = env.colwidth + 1

-- Count unimplemented functions
do
    local count = 0
    local list = "("
    for c,v in pairs(commands) do
        if v.func == fixme then
            count = count + 1
            if count ~= 1 then
                list = list..","
            end
            list = list..c
        end
    end
    list = list..")"
    if count ~= 0 then
        env.welcome = env.welcome .. "\r\nWarning: "..count.." functions are still unimplemented "..list.."."
    end
end

--[[ Utils ]]
function split_input(input)
    local input = strip(input)
    local s = string.find(input," ")
    if s then
        return string.sub(input,0,s-1), strip(string.sub(input,s))
    else
        return input
    end
end

function call_command(cmd,client,arg)
    if type(commands[cmd]) == type("") then
        cmd = commands[cmd]
    end
    local ok, msg
    if arg ~= nil then
        ok, msg = pcall( commands[cmd].func, cmd, client, arg )
    else
        ok, msg = pcall( commands[cmd].func, cmd, client )
    end
    if not ok then
        local a = arg or ""
        if a ~= "" then a = " " .. a end
        client:append("Error in `"..cmd..a.."' ".. msg)
    end
end

function call_libvlc_command(cmd,client,arg)
    local ok, vlcerr, vlcmsg = pcall( vlc.var.libvlc_command, cmd, arg )
    if not ok then
        local a = arg or ""
        if a ~= "" then a = " " .. a end
        client:append("Error in `"..cmd..a.."' ".. vlcerr) -- when pcall fails, the 2nd arg is the error message.
    end
    return vlcerr
end

--[[ Setup host ]]
require("host")
h = host.host()
-- No auth
h.status_callbacks[host.status.password] = function(client)
    client.env = common.table_copy( env )
    client:send( client.env.welcome .. "\r\n")
    client:switch_status(host.status.read)
end
-- Print prompt when switching a client's status to `read'
h.status_callbacks[host.status.read] = function(client)
    client:send( client.env.prompt )
end

h:listen( config.hosts or config.host or "*console" )

--[[ The main loop ]]
while not vlc.misc.should_die() do
    h:accept()
    local write, read = h:select(0.1)

    for _, client in pairs(write) do
        local len = client:send()
        client.buffer = string.sub(client.buffer,len+1)
        if client.buffer == "" then client:switch_status(host.status.read) end
    end

    for _, client in pairs(read) do
        local input = client:recv(1000)
        local done = false
        if string.match(input,"\n$") then
            client.buffer = string.gsub(client.buffer..input,"\r?\n$","")
            done = true
        elseif client.buffer == ""
           and ((client.type == host.client_type.stdio and input == "")
           or  (client.type == host.client_type.net and input == "\004")) then
            -- Caught a ^D
            client.buffer = "quit"
            done = true
        else
            client.buffer = client.buffer .. input
        end
        if done then
            local cmd,arg = split_input(client.buffer)
            client.buffer = ""
            client:switch_status(host.status.write)
            if commands[cmd] then
                call_command(cmd,client,arg)
            else
                if client.type == host.client_type.stdio 
                and call_libvlc_command(cmd,client,arg) == 0 then
                else
                    local choices = {}
                    if client.env.autocompletion ~= 0 then
                        for v,_ in common.pairs_sorted(commands) do
                            if string.sub(v,0,#cmd)==cmd then
                                table.insert(choices, v)
                            end
                        end
                    end
                    if #choices == 1 and client.env.autoalias ~= 0 then
                        -- client:append("Aliasing to \""..choices[1].."\".")
                        cmd = choices[1]
                        call_command(cmd,client,arg)
                    else
                        client:append("Unknown command `"..cmd.."'. Type `help' for help.")
                        if #choices ~= 0 then
                            client:append("Possible choices are:")
                            local cols = math.floor(client.env.width/(client.env.colwidth+1))
                            local fmt = "%-"..client.env.colwidth.."s"
                            for i = 1, #choices do
                                choices[i] = string.format(fmt,choices[i])
                            end
                            for i = 1, #choices, cols do
                                local j = i + cols - 1
                                if j > #choices then j = #choices end
                                client:append("  "..table.concat(choices," ",i,j))
                            end
                        end
                    end
                end
            end
        end
    end
end
