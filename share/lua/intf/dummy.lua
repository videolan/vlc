--[[ This code is public domain (since it really isn't very interesting) ]]--

msg = [[
This is the `dummy' VLC Lua interface module.
Please specify a VLC Lua interface to load with the --lua-intf option.
VLC Lua interface modules include: `cli' and `http'.
For example: vlc -I luaintf --lua-intf cli
You can also use the alternate syntax: vlc -I "luaintf{intf=cli}"
See share/lua/intf/README.txt for more information about lua interface modules.]]

for line in string.gmatch(msg,"([^\n]+)\n*") do
    vlc.msg.err(line)
end

vlc.misc.quit()
