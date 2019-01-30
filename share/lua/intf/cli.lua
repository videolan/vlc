--[==========================================================================[
 cli.lua: CLI module for VLC
--[==========================================================================[
 Copyright (C) 2007-2011 the VideoLAN team
 $Id$

 Authors: Antoine Cellerier <dionoea at videolan dot org>
          Pierre Ynard

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
 Command Line Interface for VLC

 This is a modules/control/oldrc.c look alike (with a bunch of new features).
 It also provides a VLM interface copied from the telnet interface.

 Use on local term:
    vlc -I cli
 Use on tcp connection:
    vlc -I cli --lua-config "cli={host='localhost:4212'}"
 Use on telnet connection:
    vlc -I cli --lua-config "cli={host='telnet://localhost:4212'}"
 Use on multiple hosts (term + plain tcp port + telnet):
    vlc -I cli --lua-config "cli={hosts={'*console','localhost:4212','telnet://localhost:5678'}}"

 Note:
    -I cli and -I luacli are aliases for -I luaintf --lua-intf cli

 Configuration options settable through the --lua-config option are:
    * hosts: A list of hosts to listen on.
    * host: A host to listen on. (won't be used if `hosts' is set)
    * password: The password used for telnet clients.
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

_ = vlc.gettext._
N_ = vlc.gettext.N_

running = true

--[[ Setup default environement ]]
env = { prompt = "> ";
        width = 70;
        autocompletion = 1;
        autoalias = 1;
        welcome = _("Command Line Interface initialized. Type `help' for help.");
        flatplaylist = 0;
      }

--[[ Import custom environement variables from the command line config (if possible) ]]
for k,v in pairs(env) do
    if config[k] then
        if type(env[k]) == type(config[k]) then
            env[k] = config[k]
            vlc.msg.dbg("set environment variable `"..k.."' to "..tostring(env[k]))
        else
            vlc.msg.err("environment variable `"..k.."' should be of type "..type(env[k])..". config value will be discarded.")
        end
    end
end

--[[ Command functions ]]
function set_env(name,client,value)
    if value then
        local var,val = split_input(value)
        if val then
            local s = string.gsub(val,"\"(.*)\"","%1")
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

function lock(name,client)
    if client.type == host.client_type.telnet then
        client:switch_status( host.status.password )
        client.buffer = ""
    else
        client:append("Error: the prompt can only be locked when logged in through telnet")
    end
end

function logout(name,client)
    if client.type == host.client_type.net
    or client.type == host.client_type.telnet then
        client:send("Bye-bye!\r\n")
        client:del()
    else
        client:append("Error: Can't logout of stdin/stdout. Use quit or shutdown to close VLC.")
    end
end

function shutdown(name,client)
    client:append("Bye-bye!")
    h:broadcast("Shutting down.\r\n")
    vlc.msg.info("Requested shutdown.")
    vlc.misc.quit()
    running = false
end

function quit(name,client)
    if client.type == host.client_type.net
    or client.type == host.client_type.telnet then
        logout(name,client)
    else
        shutdown(name,client)
    end
end

function add(name,client,arg)
    -- TODO: parse single and double quotes properly
    local f
    if name == "enqueue" then
        f = vlc.playlist.enqueue
    else
        f = vlc.playlist.add
    end
    local options = {}
    for o in string.gmatch(arg," +:([^ ]*)") do
        table.insert(options,o)
    end
    arg = string.gsub(arg," +:.*$","")
    local uri = vlc.strings.make_uri(arg)
    f({{path=uri,options=options}})
end

function move(name,client,arg)
    local x,y
    local tbl = {}
    for token in string.gmatch(arg, "[^%s]+") do
        table.insert(tbl,token)
    end
    x = tonumber(tbl[1])
    y = tonumber(tbl[2])
    local res = vlc.playlist.move(x,y)
    if res == (-1) then
        client:append("You should choose valid id.")
    end
end

function playlist_is_tree( client )
    if client.env.flatplaylist == 0 then
        return true
    else
        return false
    end
end

function format_item(item, is_current)
    local marker = ( item.id == current ) and "*" or " "
    local str = "|"..marker..tostring(item.id).." - "..
                    ( item.name or item.path )
    if item.duration > 0 then
        str = str.." ("..common.durationtostring(item.duration)..")"
    end
    return str
end

function playlist(name,client,arg)
    local current = vlc.playlist.current()
    client:append("+----[ Playlist - "..name.." ]")
    if arg == nil then
        -- print the whole playlist
        local list = vlc.playlist.list()
        for _, item in ipairs(list) do
            client:append(format_item(item, item.id == current))
        end
    else
        -- print the requested item (if it exists)
        local id = tonumber(arg)
        local item = vlc.playlist.get(id)
        if item ~= nil then
            client:append(format_item(item, false))
        end
    end
    client:append("+----[ End of playlist ]")
end

function playlist_sort(name,client,arg)
    if not arg then
        client:append("Valid sort keys are: id, title, artist, genre, random, duration, album.")
    else
        local tree = playlist_is_tree(client)
        vlc.playlist.sort(arg,false,tree)
    end
end

function load_vlm(name, client, value)
    if vlm == nil then
        vlm = vlc.vlm()
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
    if arg == nil and vlm ~= nil then
        client:append("+----[ VLM commands ]")
        local message, vlc_err = vlm:execute_command("help")
        vlm_message_to_string( client, message, "|" )
    end

    local width = client.env.width
    local long = (name == "longhelp")
    local extra = ""
    if arg then extra = "matching `" .. arg .. "' " end
    client:append("+----[ CLI commands "..extra.."]")
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
                str = str .. string.rep(" .",math.floor((width-(#str+#val.help)-1)/2))
                str = str .. string.rep(" ",width-#str-#val.help) .. val.help
            end
            client:append(str)
        end
    end
    client:append("+----[ end of help ]")
end

function input_info(name,client,id)
    local pl_item;

    if id then
        pl_item = vlc.playlist.get(id)
    else
        pl_item = vlc.playlist.current_item()
    end

    if pl_item == nil then
        return
    end

    local item = pl_item.item
    local infos = item:info()
    infos["Meta data"] = item:metas()

    -- Sort categories so the output is consistent
    local categories = {}
    for cat in pairs(infos) do
        table.insert(categories, cat)
    end
    table.sort(categories)

    for _, cat in ipairs(categories) do
        client:append("+----[ "..cat.." ]")
        client:append("|")
        for name, value in pairs(infos[cat]) do
            client:append("| "..name..": "..value)
        end
        client:append("|")
    end
    client:append("+----[ end of stream info ]")
end

function stats(name,client)
    local item = vlc.player.item()
    if(item == nil) then return end
    local stats_tab = item:stats()

    client:append("+----[ begin of statistical info")
    client:append("+-[Incoming]")
    client:append("| input bytes read : "..string.format("%8.0f KiB",stats_tab["read_bytes"]/1024))
    client:append("| input bitrate    :   "..string.format("%6.0f kb/s",stats_tab["input_bitrate"]*8000))
    client:append("| demux bytes read : "..string.format("%8.0f KiB",stats_tab["demux_read_bytes"]/1024))
    client:append("| demux bitrate    :   "..string.format("%6.0f kb/s",stats_tab["demux_bitrate"]*8000))
    client:append("| demux corrupted  :    "..string.format("%5i",stats_tab["demux_corrupted"]))
    client:append("| discontinuities  :    "..string.format("%5i",stats_tab["demux_discontinuity"]))
    client:append("|")
    client:append("+-[Video Decoding]")
    client:append("| video decoded    :    "..string.format("%5i",stats_tab["decoded_video"]))
    client:append("| frames displayed :    "..string.format("%5i",stats_tab["displayed_pictures"]))
    client:append("| frames lost      :    "..string.format("%5i",stats_tab["lost_pictures"]))
    client:append("|")
    client:append("+-[Audio Decoding]")
    client:append("| audio decoded    :    "..string.format("%5i",stats_tab["decoded_audio"]))
    client:append("| buffers played   :    "..string.format("%5i",stats_tab["played_abuffers"]))
    client:append("| buffers lost     :    "..string.format("%5i",stats_tab["lost_abuffers"]))
    client:append("+----[ end of statistical info ]")
end

function playlist_status(name,client)
    local item = vlc.player.item()
    if(item ~= nil) then
        client:append( "( new input: " .. vlc.strings.decode_uri(item:uri()) .. " )" )
    end
    client:append( "( audio volume: " .. tostring(vlc.volume.get()) .. " )")
    client:append( "( state " .. vlc.playlist.status() .. " )")
end

function is_playing(name,client)
    if vlc.player.is_playing() then client:append "1" else client:append "0" end
end

function get_title(name,client)
    local item = vlc.player.item()
    if item then
        client:append(item:name())
    else
        client:append("")
    end
end

function get_length(name,client)
    local item = vlc.player.item()
    if item then
        client:append(math.floor(item:duration()))
    else
        client:append("")
    end
end

function ret_print(foo,start,stop)
    local start = start or ""
    local stop = stop or ""
    return function(discard,client,...) client:append(start..tostring(foo(...))..stop) end
end

function get_time(name,client)
    if vlc.player.is_playing() then
        client:append(math.floor(vlc.player.get_time() / 1000000))
    else
        client:append("")
    end
end

function title(name,client,value)
    if value then
        vlc.player.title_goto(value)
    else
        local idx = vlc.player.get_title_index()
        client:append(idx)
    end
end

function title_next(name,client,value)
    vlc.player.title_next()
end

function title_previous(name,client,value)
    vlc.player.title_prev()
end

function chapter(name,client,value)
    if value then
        vlc.player.chapter_goto(value)
    else
        local idx = vlc.player.get_chapter_index()
        client:append(idx)
    end
end

function chapter_next(name,client,value)
    vlc.player.chapter_next()
end

function chapter_previous(name,client,value)
    vlc.player.chapter_prev()
end

function seek(name,client,value)
    common.seek(value)
end

function volume(name,client,value)
    if value then
        common.volume(value)
    else
        client:append(tostring(vlc.volume.get()))
    end
end

function rate_normal(name, client)
    vlc.player.set_rate(1)
end

function rate_faster(name, client)
    vlc.player.increment_rate()
end

function rate_slower(name, client)
    vlc.player.decrement_rate()
end

function rate(name, client, value)
    if value then
        local rate = common.us_tonumber(value)
        vlc.player.set_rate(rate)
    else
        client:append(vlc.player.get_rate())
    end
end

function frame(name,client)
    vlc.player.next_video_frame()
end

function listvalue(obj,var)
    return function(client,value)
        local o
        if obj == "aout" then
            o = vlc.object.aout()
        elseif obj == "vout" then
            o = vlc.object.vout()
        end
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

function vtrack(name, client, value)
    if value then
        vlc.player.toggle_video_track(value)
    else
        client:append("+----[ video tracks ]")
        local tracks = vlc.player.get_video_tracks()
        for _, track in ipairs(tracks) do
            local mark = track.selected and "*" or " "
            client:append("|"..mark..tostring(track.id).." - "..track.name)
        end
        client:append("+----[ end of video tracks ]")
    end
end

function atrack(name, client, value)
    if value then
        vlc.player.toggle_audio_track(value)
    else
        client:append("+----[ audio tracks ]")
        local tracks = vlc.player.get_audio_tracks()
        for _, track in ipairs(tracks) do
            local mark = track.selected and "*" or " "
            client:append("|"..mark..tostring(track.id).." - "..track.name)
        end
        client:append("+----[ end of audio tracks ]")
    end
end

function strack(name, client, value)
    if value then
        vlc.player.toggle_spu_track(value)
    else
        client:append("+----[ spu tracks ]")
        local tracks = vlc.player.get_spu_tracks()
        for _, track in ipairs(tracks) do
            local mark = track.selected and "*" or " "
            client:append("|"..mark..tostring(track.id).." - "..track.name)
        end
        client:append("+----[ end of spu tracks ]")
    end
end

function hotkey(name, client, value)
    if not value then
        client:append("Please specify a hotkey (ie key-quit or quit)")
    elseif not common.hotkey(value) and not common.hotkey("key-"..value) then
        client:append("Unknown hotkey '"..value.."'")
    end
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
    { "delete"; { func = skip2(vlc.playlist.delete); args = "[X]"; help = "delete item X in playlist" } };
    { "move"; { func = move; args = "[X][Y]"; help = "move item X in playlist after Y" } };
    { "sort"; { func = playlist_sort; args = "key"; help = "sort the playlist" } };
    { "play"; { func = skip2(vlc.playlist.play); help = "play stream" } };
    { "stop"; { func = skip2(vlc.playlist.stop); help = "stop stream" } };
    { "next"; { func = skip2(vlc.playlist.next); help = "next playlist item" } };
    { "prev"; { func = skip2(vlc.playlist.prev); help = "previous playlist item" } };
    { "goto"; { func = skip2(vlc.playlist.gotoitem); help = "goto item at index" ; aliases = { "gotoitem" } } };
    { "repeat"; { func = skip2(vlc.playlist.repeat_); args = "[on|off]"; help = "toggle playlist repeat" } };
    { "loop"; { func = skip2(vlc.playlist.loop); args = "[on|off]"; help = "toggle playlist loop" } };
    { "random"; { func = skip2(vlc.playlist.random); args = "[on|off]"; help = "toggle playlist random" } };
    { "clear"; { func = skip2(vlc.playlist.clear); help = "clear the playlist" } };
    { "status"; { func = playlist_status; help = "current playlist status" } };
    { "title"; { func = title; args = "[X]"; help = "set/get title in current item" } };
    { "title_n"; { func = title_next; help = "next title in current item" } };
    { "title_p"; { func = title_previous; help = "previous title in current item" } };
    { "chapter"; { func = chapter; args = "[X]"; help = "set/get chapter in current item" } };
    { "chapter_n"; { func = chapter_next; help = "next chapter in current item" } };
    { "chapter_p"; { func = chapter_previous; help = "previous chapter in current item" } };
    { "" };
    { "seek"; { func = seek; args = "X"; help = "seek in seconds, for instance `seek 12'" } };
    { "pause"; { func = skip2(vlc.playlist.pause); help = "toggle pause" } };
    { "fastforward"; { func = setarg(common.hotkey,"key-jump+extrashort"); help = "set to maximum rate" } };
    { "rewind"; { func = setarg(common.hotkey,"key-jump-extrashort"); help = "set to minimum rate" } };
    { "faster"; { func = rate_faster; help = "faster playing of stream" } };
    { "slower"; { func = rate_slower; help = "slower playing of stream" } };
    { "normal"; { func = rate_normal; help = "normal playing of stream" } };
    { "rate"; { func = rate; args = "[playback rate]"; help = "set playback rate to value" } };
    { "frame"; { func = frame; help = "play frame by frame" } };
    { "fullscreen"; { func = skip2(vlc.video.fullscreen); args = "[on|off]"; help = "toggle fullscreen"; aliases = { "f", "F" } } };
    { "info"; { func = input_info; args= "[X]"; help = "information about the current stream (or specified id)" } };
    { "stats"; { func = stats; help = "show statistical information" } };
    { "get_time"; { func = get_time; help = "seconds elapsed since stream's beginning" } };
    { "is_playing"; { func = is_playing; help = "1 if a stream plays, 0 otherwise" } };
    { "get_title"; { func = get_title; help = "the title of the current stream" } };
    { "get_length"; { func = get_length; help = "the length of the current stream" } };
    { "" };
    { "volume"; { func = volume; args = "[X]"; help = "set/get audio volume" } };
    { "volup"; { func = ret_print(vlc.volume.up,"( audio volume: "," )"); args = "[X]"; help = "raise audio volume X steps" } };
    { "voldown"; { func = ret_print(vlc.volume.down,"( audio volume: "," )"); args = "[X]"; help = "lower audio volume X steps" } };
    -- { "adev"; { func = skip(listvalue("aout","audio-device")); args = "[X]"; help = "set/get audio device" } };
    { "achan"; { func = skip(listvalue("aout","stereo-mode")); args = "[X]"; help = "set/get stereo audio output mode" } };
    { "atrack"; { func = atrack; args = "[X]"; help = "set/get audio track" } };
    { "vtrack"; { func = vtrack; args = "[X]"; help = "set/get video track" } };
    { "vratio"; { func = skip(listvalue("vout","aspect-ratio")); args = "[X]"; help = "set/get video aspect ratio" } };
    { "vcrop"; { func = skip(listvalue("vout","crop")); args = "[X]"; help = "set/get video crop"; aliases = { "crop" } } };
    { "vzoom"; { func = skip(listvalue("vout","zoom")); args = "[X]"; help = "set/get video zoom"; aliases = { "zoom" } } };
    { "vdeinterlace"; { func = skip(listvalue("vout","deinterlace")); args = "[X]"; help = "set/get video deinterlace" } };
    { "vdeinterlace_mode"; { func = skip(listvalue("vout","deinterlace-mode")); args = "[X]"; help = "set/get video deinterlace mode" } };
    { "snapshot"; { func = common.snapshot; help = "take video snapshot" } };
    { "strack"; { func = strack; args = "[X]"; help = "set/get subtitle track" } };
    { "hotkey"; { func = hotkey; args = "[hotkey name]"; help = "simulate hotkey press"; adv = true; aliases = { "key" } } };
    { "" };
    { "vlm"; { func = load_vlm; help = "load the VLM" } };
    { "set"; { func = set_env; args = "[var [value]]"; help = "set/get env var"; adv = true } };
    { "save_env"; { func = save_env; help = "save env vars (for future clients)"; adv = true } };
    { "alias"; { func = skip(alias); args = "[cmd]"; help = "set/get command aliases"; adv = true } };
    { "description"; { func = print_text("Description",description); help = "describe this module" } };
    { "license"; { func = print_text("License message",vlc.misc.license()); help = "print VLC's license message"; adv = true } };
    { "help"; { func = help; args = "[pattern]"; help = "a help message"; aliases = { "?" } } };
    { "longhelp"; { func = help; args = "[pattern]"; help = "a longer help message" } };
    { "lock"; { func = lock; help = "lock the telnet prompt" } };
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

function vlm_message_to_string(client,message,prefix)
    local prefix = prefix or ""
    if message.value then
        client:append(prefix .. message.name .. " : " .. message.value)
    else
        client:append(prefix .. message.name)
    end
    if message.children then
        for i,c in ipairs(message.children) do
            vlm_message_to_string(client,c,prefix.."    ")
        end
    end
end

--[[ Command dispatch ]]
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
        local a = arg and " "..arg or ""
        client:append("Error in `"..cmd..a.."' ".. msg)
    end
end

function call_vlm_command(cmd,client,arg)
    if vlm == nil then
        return -1
    end
    if arg ~= nil then
        cmd = cmd.." "..arg
    end
    local message, vlc_err = vlm:execute_command( cmd )
    -- the VLM doesn't let us know if the command exists,
    -- so we need this ugly hack
    if vlc_err ~= 0 and message.value == "Unknown VLM command" then
        return vlc_err
    end
    vlm_message_to_string( client, message )
    return 0
end

function call_libvlc_command(cmd,client,arg)
    local ok, vlcerr = pcall( vlc.var.libvlc_command, cmd, arg )
    if not ok then
        local a = arg and " "..arg or ""
        client:append("Error in `"..cmd..a.."' ".. vlcerr) -- when pcall fails, the 2nd arg is the error message.
    end
    return vlcerr
end

function client_command( client )
    local cmd,arg = split_input(client.buffer)
    client.buffer = ""

    if commands[cmd] then
        call_command(cmd,client,arg)
    elseif call_vlm_command(cmd,client,arg) == 0 then
        --
    elseif client.type == host.client_type.stdio
    and call_libvlc_command(cmd,client,arg) == 0 then
        --
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

--[[ Some telnet command special characters ]]
WILL = "\251" -- Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option.
WONT = "\252" -- Indicates the refusal to perform, or continue performing, the indicated option.
DO   = "\253" -- Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option.
DONT = "\254" -- Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option.
IAC  = "\255" -- Interpret as command

ECHO = "\001"

function telnet_commands( client )
    -- remove telnet command replies from the client's data
    client.buffer = string.gsub( client.buffer, IAC.."["..DO..DONT..WILL..WONT.."].", "" )
end

--[[ Client status change callbacks ]]
function on_password( client )
    client.env = common.table_copy( env )
    if client.type == host.client_type.telnet then
        client:send( "Password: " ..IAC..WILL..ECHO )
    else
        if client.env.welcome ~= "" then
            client:send( client.env.welcome .. "\r\n")
        end
        client:switch_status( host.status.read )
    end
end
-- Print prompt when switching a client's status to `read'
function on_read( client )
    client:send( client.env.prompt )
end
function on_write( client )
end

--[[ Setup host ]]
require("host")
h = host.host()

h.status_callbacks[host.status.password] = on_password
h.status_callbacks[host.status.read] = on_read
h.status_callbacks[host.status.write] = on_write

h:listen( config.hosts or config.host or "*console" )
password = config.password or "admin"

--[[ The main loop ]]
while running do
    local write, read = h:accept_and_select()

    for _, client in pairs(write) do
        local len = client:send()
        client.buffer = string.sub(client.buffer,len+1)
        if client.buffer == "" then client:switch_status(host.status.read) end
    end

    for _, client in pairs(read) do
        local input = client:recv(1000)

        if input == nil -- the telnet client program has left
            or ((client.type == host.client_type.net
                 or client.type == host.client_type.telnet)
                and input == "\004") then
            -- Caught a ^D
            client.cmds = "quit\n"
        else
            client.cmds = client.cmds .. input
        end

        client.buffer = ""
        -- split the command at the first '\n'
        while string.find(client.cmds, "\n") do
            -- save the buffer to send to the client
            local saved_buffer = client.buffer

            -- get the next command
            local index = string.find(client.cmds, "\n")
            client.buffer = strip(string.sub(client.cmds, 0, index - 1))
            client.cmds = string.sub(client.cmds, index + 1)

            -- Remove telnet commands from the command line
            if client.type == host.client_type.telnet then
                telnet_commands( client )
            end

            -- Run the command
            if client.status == host.status.password then
                if client.buffer == password then
                    client:send( IAC..WONT..ECHO.."\r\nWelcome, Master\r\n" )
                    client.buffer = ""
                    client:switch_status( host.status.write )
                elseif client.buffer == "quit" then
                    client_command( client )
                else
                    client:send( "\r\nWrong password\r\nPassword: " )
                    client.buffer = ""
                end
            else
                client:switch_status( host.status.write )
                client_command( client )
            end
            client.buffer = saved_buffer .. client.buffer
        end
    end
end

--[[ Clean up ]]
vlm = nil
