## Instructions to code your own VLC Lua services discovery script.
$Id$

See lua/README.txt for generic documentation about Lua usage in VLC.

Examples: See fmc.lua, frenchtv.lua

## API
VLC Lua SD modules should define two functions:
  * descriptor(): returns a table with information about the module.
    The table has the following members:
      .title: the name of the SD
      .capabilities: A list of your SD's capabilities. Only the
        following flags are supported yet:
        * 'search' : Does your SD handle search himself

    Example:
    function descriptor()
      return { title = "My SD's title", capabilities={"search"}}
    end

  * main(): will be called when the SD is started. It should use VLC's SD API
    described in lua/README.txt do add the items found.

  * search(query_string): Will be called with a string to search for
    services/medias matching that string.


User defined modules stored in the share/lua/modules/ directory are
available. Read the 'Lazy initialization' section

Available VLC specific Lua modules: input, msg, net, object, sd,
strings, variables, stream, gettext, xml. See lua/README.txt.

## Lazy Initialization

SD Lua scripts are actually ran in two different contexts/interpreters. One of
them is the one that will call your main() and search() functions. The other one
is a lighter one that will only fetch your description(). Due to threading
issues and to reduce implementation complexity (NDLR: i guess), the
description() interpreter doesn't load/expose VLC's API nor add
share/lua/modules to the lua load path (these modules are using vlc API anyway).
This has some implications to the way you need to load modules.

This means you cannot make a global/top-level require for the module you use but
instead use lazily load them from the main() and/or search() functions. Here's
an example implementation:

-------------------------------------------------------------------------------
lazily_loaded = false
dkjson        = nil

function lazy_load()
  if lazily_loaded then return nil end
  dkjson = require("dkjson")
  lazily_loaded = true
end

function descriptor()
  return { title = "..." }
end

function main()
  lazy_load()
  -- Do stuff here
end

function search(query)
  lazy_load()
  -- Do stuff here
end
-------------------------------------------------------------------------------
