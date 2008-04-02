--[[ This code is public domain (since it really isn't very interesting) ]]--

msg = [[
This is the `dummy' VLC Lua interface module.
Please specify a VLC Lua interface to load with the --lua-intf option.
VLC Lua interface modules include: `rc', `telnet' and `hotkeys'.
For example: vlc -I lua --lua-intf rc]]
--You can also use the alternate syntax: vlc -I "lua{intf=rc}"]]

for line in string.gmatch(msg,"([^\n]+)\n*") do
    vlc.msg.err(line)
end
