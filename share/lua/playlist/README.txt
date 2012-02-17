Instructions to code your own VLC Lua playlist script.
$Id$

See lua/README.txt for generic documentation about Lua usage in VLC.

Examples: See dailymotion.lua, googlevideo.lua, metacafe.lua, youtube.lua
          and youtube_homepage.lua .

VLC Lua playlist modules should define two functions:
 * probe(): returns true if we want to handle the playlist in this script
 * parse(): read the incoming data and return playlist item(s)
            Playlist items use the same format as that expected in the
            playlist.add() function (see general lua/README.txt)

VLC defines a global vlc object with the following members:
 * vlc.path: the URL string (without the leading http:// or file:// element)
 * vlc.access: the access used ("http" for http://, "file" for file://, etc.)
 * vlc.peek( <int> ): return the first <int> characters from the playlist file.
 * vlc.read( <int> ): read <int> characters from the playlist file.
                      THIS FUNCTION CANNOT BE USED IN probe().
 * vlc.readline(): return a new line of playlist data on each call.
                   THIS FUNCTION CANNOT BE USED IN probe().

Available VLC specific Lua modules: msg, strings, stream, variables and
xml. See lua/README.txt.
