## Generic instructions about VLC Lua meta scripts.

There are 3 types of Lua meta modules: art, fetcher and reader.
See their respective README.txt for documentation.

See lua/README.txt for generic documentation about Lua usage in VLC.

## API

VLC Lua meta modules should define a descriptor function:
  * descriptor(): returns a table with information about the module.
    This table has the following member:
      .scope: Search scope of the script. Can be "network" or "local".
              and defaults to "network".
              "local" scripts are considered fast and processed first.
              "network" scripts are considered slow and processed in a
              second pass only if policy allows it.

    Example:
    function descriptor()
        return { scope="network" }
    end

