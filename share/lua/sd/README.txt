Instructions to code your own VLC Lua services discovery script.
$Id$

See lua/README.txt for generic documentation about Lua usage in VLC.

Examples: See fmc.lua, frenchtv.lua

VLC Lua SD modules should define two functions:
 * descriptor(): returns a table with information about the module.
                 The table has the following members:
                     .title: the name of the SD
 * main(): will be called when the SD is started

User defined modules stored in the share/lua/modules/ directory are
available. For example, to use the sandbox module, just use
'require "sandbox"' in your interface.

Available VLC specific Lua modules: input, msg, net, object, sd,
strings, variables, stream, gettext, xml. See lua/README.txt.
