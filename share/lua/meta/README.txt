Instructions to code your own VLC Lua meta script.
$Id$

Examples: See googleimage.lua .

VLC Lua meta modules should define one of the following functions:
 * fetch_art(): returns a path to an artwork for the given item
 * fetch_meta(): returns a table with the following items:
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
 * vlc.stream_new
 * vlc.stream_delete
 * vlc.stream_readline
 * vlc.stream_read
 * vlc.decode_uri( <string> ): decode %xy characters in a string.
 * vlc.resolve_xml_special_chars( <string> ): decode &abc; characters in a
					      string.
 * vlc.msg_dbg( <string> ): print a debug message.
 * vlc.msg_warn( <string> ): print a warning message.
 * vlc.msg_err( <string> ): print an error message.
 * vlc.msg_info( <string> ): print an info message.

Lua scripts are tried in alphabetical order in the user's VLC config
director lua/meta/ subdirectory, then in the global VLC lua/meta/
directory.

Lua documentation is available on http://www.lua.org .
VLC uses Lua 5.1
All the Lua standard libraries are available.
