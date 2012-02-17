Instructions to code your own VLC Lua meta script.
$Id$

See lua/README.txt for generic documentation about Lua usage in VLC.

Examples: See googleimage.lua .

VLC Lua meta modules should define one of the following functions:
 * fetch_art(): returns a path to an artwork for the given item

Available VLC specific Lua modules: msg, stream, strings, variables, item,
objects and xml. See lua/README.txt
