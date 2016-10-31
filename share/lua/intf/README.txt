Instructions to code your own VLC Lua interface script.
$Id$

See lua/README.txt for generic documentation about Lua usage in VLC.

Examples: cli.lua, http.lua

The "config" global variable is set to the value specified in the
--lua-config VLC option. For example:
--lua-config "rc={a='test',c=3},telnet={a='hello'}"
config will be set to {a='test',c=3} in the rc interface, to {a='hello'}
in the telnet interface and won't be set in other interfaces. 

User defined modules stored in the share/lua/intf/modules/ directory are
available. For example, to use the sandbox module, just use
'require "sandbox"' in your interface.

VLC defines a global vlc object with the following members:
All the VLC specific Lua modules are available.
