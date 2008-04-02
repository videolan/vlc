Instructions to code your own VLC Lua playlist script.
$Id$

Examples: See dailymotion.lua, googlevideo.lua, metacafe.lua, youbtube.lua
          and youtube_homepage.lua .

VLC Lua playlist modules should define two functions:
 * probe(): returns true if we want to handle the playlist in this script
 * parse(): read the incoming data and return playlist item(s)
            The playlist is a table of playlist objects.
            A playlist object has the following members:
                .path: the item's full path / URL
                .name: the item's name in playlist (OPTIONAL)
                .title: the item's Title (OPTIONAL, meta data)
                .artist: the item's Artist (OPTIONAL, meta data)
                .genre: the item's Genre (OPTIONAL, meta data)
                .copyright: the item's Copyright (OPTIONAL, meta data)
                .album: the item's Album (OPTIONAL, meta data)
                .tracknum: the item's Tracknum (OPTIONAL, meta data)
                .description: the item's Description (OPTIONAL, meta data)
                .rating: the item's Rating (OPTIONAL, meta data)
                .date: the item's Date (OPTIONAL, meta data)
                .setting: the item's Setting (OPTIONAL, meta data)
                .url: the item's URL (OPTIONAL, meta data)
                .language: the item's Language (OPTIONAL, meta data)
                .nowplaying: the item's NowPlaying (OPTIONAL, meta data)
                .publisher: the item's Publisher (OPTIONAL, meta data)
                .encodedby: the item's EncodedBy (OPTIONAL, meta data)
                .arturl: the item's ArtURL (OPTIONAL, meta data)
                .trackid: the item's TrackID (OPTIONAL, meta data)
                .options: a list of VLC options (OPTIONAL)
                          example: .options = { "fullscreen" }
                .duration: stream duration in seconds (OPTIONAL)
                .meta: custom meta data (OPTIONAL, meta data)
                       A .meta field is a table of custom meta categories which
                       each have custom meta properties.
                       example: .meta = { ["Google video"] = { ["docid"] = "-5784010886294950089"; ["GVP version"] = "1.1" }; ["misc"] = { "Hello" = "World!" } }
            Invalid playlist items will be discarded by VLC.

VLC defines a global vlc object with the following members:
 * vlc.path: the URL string (without the leading http:// or file:// element)
 * vlc.access: the access used ("http" for http://, "file" for file://, etc.)
 * vlc.peek( <int> ): return the first <int> characters from the playlist file.
 * vlc.read( <int> ): read <int> characters from the playlist file.
                      THIS FUNCTION CANNOT BE USED IN peek().
 * vlc.readline(): return a new line of playlist data on each call.
                   THIS FUNCTION CANNOT BE USED IN peek().
 * vlc.decode_uri( <string> ): decode %xy characters in a string.
 * vlc.resolve_xml_special_chars( <string> ): decode &abc; characters in a
					      string.
 * vlc.msg_dbg( <string> ): print a debug message.
 * vlc.msg_warn( <string> ): print a warning message.
 * vlc.msg_err( <string> ): print an error message.
 * vlc.msg_info( <string> ): print an info message.

Lua scripts are tried in alphabetical order in the user's VLC config
director lua/playlist/ subdirectory, then in the global VLC lua/playlist/
directory.

Lua documentation is available on http://www.lua.org .
VLC uses Lua 5.1
All the Lua standard libraries are available.
