--[==========================================================================[
 hotkeys.lua: hotkey handling for VLC
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
    This is meant to replace modules/control/hotkeys.c
    (which will require some changes in the VLC core hotkeys stuff)
--]==========================================================================]

require("common")
--common.table_print(vlc,"vlc.\t")

bindings = {
    ["Ctrl-q"] = "quit",
    ["Space"] = "play-pause",
    [113] --[[q]] = "quit",
    [119] --[[w]] = "demo",
    [120] --[[x]] = "demo2",
    }

function quit()
    print("Bye-bye!")
    vlc.quit()
end

function demo()
    vlc.osd.icon("speaker")
end

function demo2()
    if not channel1 then
        channel1 = vlc.osd.channel_register()
        channel2 = vlc.osd.channel_register()
    end
    vlc.osd.message("Hey!",channel1)
    vlc.osd.slider( 10, "horizontal", channel2 )
end

function action(func,delta)
    return { func = func, delta = delta or 0, last = 0, times = 0 }
end

actions = {
    ["quit"] = action(quit),
    ["play-pause"] = action(play_pause),
    ["demo"] = action(demo),
    ["demo2"] = action(demo2),
    }

action = nil

queue = {}

function action_trigger( action )
    print("action_trigger:",tostring(action))
    local a = actions[action]
    if a then
        local date = vlc.misc.mdate()
        if a.delta and date > a.last + a.delta then
            a.times = 0
        else
            a.times = a.times + 1
        end
        a.last = date
        table.insert(queue,action)
        vlc.misc.signal()
    else
        vlc.msg.err("Key `"..key.."' points to unknown action `"..bindings[key].."'.")
    end
end

function key_press( var, old, new, data )
    local key = new
    print("key_press:",tostring(key))
    if bindings[key] then
        action_trigger(bindings[key])
    else
        vlc.msg.err("Key `"..key.."' isn't bound to any action.")
    end
end

vlc.var.add_callback( vlc.object.libvlc(), "key-pressed", key_press )
--vlc.var.add_callback( vlc.object.libvlc(), "action-triggered", action_trigger )

while not die do
    if #queue ~= 0 then
        local action = actions[queue[1]]
        local ok, msg = pcall( action.func )
        if not ok then
            vlc.msg.err("Error while executing action `"..queue[1].."': "..msg)
        end
        table.remove(queue,1)
    else
        die = vlc.misc.lock_and_wait()
    end
end

-- Clean up
vlc.var.del_callback( vlc.object.libvlc(), "key-pressed", key_press )
--vlc.var.del_callback( vlc.object.libvlc(), "action-triggered", action_trigger )
