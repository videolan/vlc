Instructions to code your own VLC Lua scripts and extensions
$Id$

1 - About Lua
=============

Lua documentation is available on http://www.lua.org/ . The reference manual
is very useful: http://www.lua.org/manual/5.1/ .
VLC uses Lua 5.1
All the Lua standard libraries are available.


2 - Lua in VLC
==============

Several types of VLC Lua scripts can currently be coded:
 * Playlist and websites parsers (see playlist/README.txt)
 * Art fetchers (see meta/README.txt)
 * Interfaces (see intf/README.txt)
 * Extensions (see extensions/README.txt)
 * Services Discovery (see sd/README.txt)

Lua scripts are tried in alphabetical order in the user's VLC config
directory lua/{playlist,meta,intf}/ subdirectory on Windows and Mac OS X or
in the user's local share directory (~/.local/share/vlc/lua/... on linux),
then in the global VLC lua/{playlist,meta,intf}/ directory.


3 - VLC specific Lua modules
============================

All VLC specifics modules are in the "vlc" object. For example, if you want
to use the "info" function of the "msg" VLC specific Lua module:
vlc.msg.info( "This is an info message and will be displayed in the console" )

Note: availability of the different VLC specific Lua modules depends on
the type of VLC Lua script your are in.

Configuration
-------------
config.get( name ): Get the VLC configuration option "name"'s value.
config.set( name, value ): Set the VLC configuration option "name"'s value.
config.datadir(): Get the VLC data directory.
config.userdatadir(): Get the user's VLC data directory.
config.homedir(): Get the user's home directory.
config.configdir(): Get the user's VLC config directory.
config.cachedir(): Get the user's VLC cache directory.

config.datadir_list( name ): Get the list of possible data directories in
                             order of priority, appended by "name"

Dialog
------
local d = vlc.dialog( "My VLC Extension" ): Create a new UI dialog, with a human-readable title: "My VLC Extension"
d:show(): Show this dialog.
d:hide(): Hide (but not close) this dialog.
d:delete(): Close and delete this dialog.
d:set_title( title ): set the title of this dialog.
d:update(): Update the dialog immediately (don't wait for the current function to return)
d:del_widget( widget ): Delete 'widget'. It disappears from the dialog and repositioning may occur.

In the following functions, you can always add some optional parameters: col, row, col_span, row_span, width, height.
They define the position of a widget in the dialog:
- row, col are the absolute positions on a grid of widgets. First row, col are 1.
- row_span, col_span represent the relative size of a widget on the grid. A widget with col_span = 4 will be displayed as wide as 4 widgets of col_span = 1.
- width and height are size hints (in pixels) but may be discarded by the GUI module
Example: w = d:add_label( "My Label", 2, 3, 4, 5 ) will create a label at row 3, col 2, with a relative width of 4, height of 5.

d:add_button( text, func, ... ): Create a button with caption "text" (string). When clicked, call function "func". func is a function reference.
d:add_label( text, ... ): Create a text label with caption "text" (string).
d:add_html( text, ... ): Create a rich text label with caption "text" (string), that supports basic HTML formatting (such as <i> or <h1> for instance).
d:add_text_input( text, ... ): Create an editable text field, in order to read user input.
d:add_password( text, ... ): Create an editable text field, in order to read user input. Text entered in this box will not be readable (replaced by asterisks).
d:add_check_box( text, state, ... ): Create a check box with a text. They have a boolean state (true/false).
d:add_dropdown( ... ): Create a drop-down widget. Only 1 element can be selected the same time.
d:add_list( ... ): Create a list widget. Allows multiple or empty selections.
d:add_image( path, ... ): Create an image label. path is a relative or absolute path to the image on the local computer.

Some functions can also be applied on widgets:
w:set_text( text ): Change text displayed by the widget. Applies to: button, label, html, text_input, password, check_box.
w:get_text(): Read text displayed by the widget. Returns a string. Applies to: button, label, html, text_input, password, check_box.
w:set_checked( bool ): Set check state of a check box. Applies to: check_box.
w:get_checked(): Read check state of a check box. Returns a boolean. Applies to: check_box.
w:add_value( text, id ): Add a value with identifier 'id' (integer) and text 'text'. It's always best to have unique identifiers. Applies to: drop_down.
w:get_value(): Return identifier of the selected item. Corresponds to the text value chosen by the user. Applies to: drop_down.
w:clear(): Clear a list or drop_down widget. After that, all values previously added are lost.
w:get_selection(): Retrieve a table representing the current selection. Keys are the ids, values are the texts associated. Applies to: list.


Extension
---------
deactivate(): Deactivate current extension (after the end of the current function).

HTTPd
-----
http( host, port, [cert, key, ca, crl]): create a new HTTP (SSL) daemon.

local h = vlc.httpd( "localhost", 8080 )
h:handler( url, user, password, callback, data ) -- add a handler for given url. If user and password are non nil, they will be used to authenticate connecting clients. callback will be called to handle connections. The callback function takes 7 arguments: data, url, request, type, in, addr, host. It returns the reply as a string.
h:file( url, mime, user, password, callback, data ) -- add a file for given url with given mime type. If user and password are non nil, they will be used to authenticate connecting clients. callback will be called to handle connections. The callback function takes 2 arguments: data and request. It returns the reply as a string.
h:redirect( url_dst, url_src ): Redirect all connections from url_src to url_dst.

Input
-----
input.is_playing(): Return true if input exists.
input.add_subtitle(url): Add a subtitle file (by path) to the current input
input.item(): Get the current input item. Input item methods are:
  :uri(): Get item's URI.
  :name(): Get item's name.
  :duration(): Get item's duration in seconds or negative value if unavailable.
  :is_preparsed(): Return true if meta data has been preparsed
  :metas(): Get meta data as a table.
  :set_meta(key, value): Set meta data.
  :info(): Get the current input's info. Return value is a table of tables. Keys of the top level table are info category labels.
  :stats(): Get statistics about the input. This is a table with the following fields:
    .read_packets
    .read_bytes
    .input_bitrate
    .average_input_bitrate
    .demux_read_packets
    .demux_read_bytes
    .demux_bitrate
    .average_demux_bitrate
    .demux_corrupted
    .demux_discontinuity
    .decoded_audio
    .decoded_video
    .displayed_pictures
    .lost_pictures
    .sent_packets
    .sent_bytes
    .send_bitrate
    .played_abuffers
    .lost_abuffers

Messages
--------
msg.dbg( [str1, [str2, [...]]] ): Output debug messages (-vv).
msg.warn( [str1, [str2, [...]]] ): Output warning messages (-v).
msg.err( [str1, [str2, [...]]] ): Output error messages.
msg.info( [str1, [str2, [...]]] ): Output info messages.

Misc (Interfaces only)
----------------------
----------------------------------------------------------------
/!\ NB: this namespace is ONLY usable for interfaces.
---
----------------------------------------------------------------
misc.version(): Get the VLC version string.
misc.copyright(): Get the VLC copyright statement.
misc.license(): Get the VLC license.

misc.action_id( name ): get the id of the given action.

misc.mdate(): Get the current date (in microseconds).
misc.mwait(): Wait for the given date (in microseconds).

misc.quit(): Quit VLC.

Net
---
----------------------------------------------------------------
/!\ NB: this namespace is ONLY usable for interfaces and extensions.
---
----------------------------------------------------------------
net.url_parse( url ): Deprecated alias for strings.url_parse().
  Will be removed in VLC 4.x.
net.listen_tcp( host, port ): Listen to TCP connections. This returns an
  object with an accept and an fds method. accept is blocking (Poll on the
  fds with the net.POLLIN flag if you want to be non blocking).
  The fds method returns a list of fds you can call poll on before using
  the accept method. For example:
local l = vlc.net.listen_tcp( "localhost", 1234 )
while true do
  local fd = l:accept()
  if fd >= 0 do
    net.send( fd, "blabla" )
    net.close( fd )
  end
end
net.connect_tcp( host, port ): open a connection to the given host:port (TCP).
net.close( fd ): Close file descriptor.
net.send( fd, string, [length] ): Send data on fd.
net.recv( fd, [max length] ): Receive data from fd.
net.poll( { fd = events } ): Implement poll function.
  Returns the numbers of file descriptors with a non 0 revent. The function
  modifies the input table to { fd = revents }. See "man poll". This function
  is not available on Windows.
net.POLLIN/POLLPRI/POLLOUT/POLLRDHUP/POLLERR/POLLHUP/POLLNVAL: poll event flags
net.read( fd, [max length] ): Read data from fd. This function is not
  available on Windows.
net.write( fd, string, [length] ): Write data to fd. This function is not
  available on Windows.
net.stat( path ): Stat a file. Returns a table with the following fields:
    .type
    .mode
    .uid
    .gid
    .size
    .access_time
    .modification_time
    .creation_time
net.opendir( path ): List a directory's contents.

Objects
-------
object.input(): Get the current input object.
object.playlist(): Get the playlist object.
object.libvlc(): Get the libvlc object.
object.aout(): Get the audio output object.
object.vout(): Get the video output object.

object.find( object, type, mode ): Return nil. DO NOT USE.

OSD
---
osd.icon( type, [id] ): Display an icon on the given OSD channel. Uses the
  default channel is none is given. Icon types are: "pause", "play",
  "speaker" and "mute".
osd.message( string, [id], [position], [duration] ): Display the text message on
  the given OSD channel. Position types are: "center", "left", "right", "top",
  "bottom", "top-left", "top-right", "bottom-left" or "bottom-right". The
  duration is set in microseconds.
osd.slider( position, type, [id] ): Display slider. Position is an integer
  from 0 to 100. Type can be "horizontal" or "vertical".
osd.channel_register(): Register a new OSD channel. Returns the channel id.
osd.channel_clear( id ): Clear OSD channel.

Playlist
--------
playlist.prev(): Play previous track.
playlist.next(): Play next track.
playlist.skip( n ): Skip n tracks.
playlist.play(): Play.
playlist.pause(): Pause.
playlist.stop(): Stop.
playlist.clear(): Clear the playlist.
playlist.repeat_( [status] ): Toggle item repeat or set to specified value.
playlist.loop( [status] ): Toggle playlist loop or set to specified value.
playlist.random( [status] ): Toggle playlist random or set to specified value.
playlist.goto( id ): Go to specified track.
playlist.add( ... ): Add a bunch of items to the playlist.
  The playlist is a table of playlist items.
  A playlist item has the following members:
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
                example: .options = { "run-time=60" }
      .duration: stream duration in seconds (OPTIONAL)
      .meta: custom meta data (OPTIONAL, meta data)
             A .meta field is a table of custom meta key value pairs.
             example: .meta = { ["GVP docid"] = "-5784010886294950089", ["GVP version] = "1.1", Hello = "World!" }
  Invalid playlist items will be discarded by VLC.
playlist.enqueue( ... ): like playlist.add() except that track isn't played.
playlist.get( [what, [tree]] ): Get the playlist.
  If "what" is a number, then this will return the corresponding playlist
  item's playlist hierarchy. If it is "normal" or "playlist", it will
  return the normal playlist. If it is "ml" or "media library", it will
  return the media library. If it is "root" it will return the full playlist.
  If it is a service discovery module's name, it will return that service
  discovery's playlist. If it is any other string, it won't return anything.
  Else it will return the full playlist.
  The second argument, "tree", is optional. If set to true or unset, the
  playlist will be returned in a tree layout. If set to false, the playlist
  will be returned using the flat layout.
  Each playlist item returned will have the following members:
      .item: The input item.
      .id: The item's id.
      .flags: a table with the following members if the corresponding flag is
              set:
          .disabled
          .ro
      .name:
      .path:
      .duration: (-1 if unknown)
      .nb_played:
      .children: A table of children playlist items.
playlist.search( name ): filter the playlist items with the given string
playlist.current(): return the current playlist item id
playlist.sort( key ): sort the playlist according to the key.
  Key must be one of the followings values: 'id', 'title', 'title nodes first',
                                            'artist', 'genre', 'random', 'duration',
                                            'title numeric' or 'album'.
playlist.status(): return the playlist status: 'stopped', 'playing', 'paused' or 'unknown'.
playlist.delete( id ): check if item of id is in playlist and delete it. returns -1 when invalid id.
playlist.move( id_item, id_where ): take id_item and if id_where has children, it put it as first children, 
   if id_where don't have children, id_item is put after id_where in same playlist. returns -1 when invalid ids.

FIXME: add methods to get an item's meta, options, es ...

Services discovery
------------------

Interfaces and extensions can use the following SD functions:

sd.get_services_names(): Get a table of all available service discovery
  modules. The module name is used as key, the long name is used as value.
sd.add( name ): Add service discovery.
sd.remove( name ): Remove service discovery.
sd.is_loaded( name ): Check if service discovery is loaded.

Services discovery scripts can use the following SD functions:

sd.add_node( ... ): Add a node to the service discovery.
  The node object has the following members:
      .title: the node's name
      .arturl: the node's ArtURL (OPTIONAL)
      .category: the node's category (OPTIONAL)
sd.add_item( ... ): Add an item to the service discovery.
  The item object has the same members as the one in playlist.add().
  Returns the input item.
sd.remove_item( item ): remove the item.

n = vlc.sd.add_node( {title="Node"} )
n:add_subitem( ... ): Same as sd.add_item(), but as a child item of node n.
n:add_subnode( ... ): Same as sd.add_node(), but as a subnode of node n.

d = vlc.sd.add_item( ... ) Get an item object to perform following set operations on it:
d:set_name(): the item's name in playlist
d:set_title(): the item's Title (OPTIONAL, meta data)
d:set_artist(): the item's Artist (OPTIONAL, meta data)
d:set_genre(): the item's Genre (OPTIONAL, meta data)
d:set_copyright(): the item's Copyright (OPTIONAL, meta data)
d:set_album(): the item's Album (OPTIONAL, meta data)
d:set_tracknum(): the item's Tracknum (OPTIONAL, meta data)
d:set_description(): the item's Description (OPTIONAL, meta data)
d:set_rating(): the item's Rating (OPTIONAL, meta data)
d:set_date(): the item's Date (OPTIONAL, meta data)
d:set_setting(): the item's Setting (OPTIONAL, meta data)
d:set_url(): the item's URL (OPTIONAL, meta data)
d:set_language(): the item's Language (OPTIONAL, meta data)
d:set_nowplaying(): the item's NowPlaying (OPTIONAL, meta data)
d:set_publisher(): the item's Publisher (OPTIONAL, meta data)
d:set_encodedby(): the item's EncodedBy (OPTIONAL, meta data)
d:set_arturl(): the item's ArtURL (OPTIONAL, meta data)
d:set_trackid(): the item's TrackID (OPTIONAL, meta data)

Stream
------
stream( url ): Instantiate a stream object for specific url.
memory_stream( string ): Instantiate a stream object containing a specific string.
  Those two functions return the stream object upon success and nil if an
  error occurred, in that case, the second return value will be an error message.

s = vlc.stream( "http://www.videolan.org/" )
s:read( 128 ) -- read up to 128 characters. Return 0 if no more data is available (FIXME?).
s:readline() -- read a line. Return nil if EOF was reached.
s:addfilter() -- add a stream filter. If no argument was specified, try to add all automatic stream filters.

Strings
-------
strings.decode_uri( [uri1, [uri2, [...]]] ): Decode a list of URIs. This
  function returns as many variables as it had arguments.
strings.encode_uri_component( [uri1, [uri2, [...]]] ): Encode a list of URI
  components. This function returns as many variables as it had arguments.
strings.make_uri( path, [scheme] ): Convert a file path to a URI.
strings.url_parse( url ): Parse URL. Returns a table with
  fields "protocol", "username", "password", "host", "port", path" and
  "option".
strings.resolve_xml_special_chars( [str1, [str2, [...]]] ): Resolve XML
  special characters in a list of strings. This function returns as many
  variables as it had arguments.
strings.convert_xml_special_chars( [str1, [str2, [...]]] ): Do the inverse
  operation.
strings.from_charset( charset, str ): convert a string from a specified
  character encoding into UTF-8; return an empty string on error.

Variables
---------
var.inherit( object, name ): Find the variable "name"'s value inherited by
  the object. If object is unset, the current module's object will be used.
var.get( object, name ): Get the object's variable "name"'s value.
var.get_list( object, name ): Get the object's variable "name"'s value list.
  1st return value is the value list, 2nd return value is the text list.
var.set( object, name, value ): Set the object's variable "name" to "value".
var.create( object, name, value ): Create and set the object's variable "name"
  to "value". Created vars can be of type float, string, bool or void.
  For a void variable the value has to be 'nil'.

var.trigger_callback( object, name ): Trigger the callbacks associated with the
  object's "name" variable.

var.libvlc_command( name, argument ): Issue libvlc's "name" command with
  argument "argument".

var.inc_integer( name ): Increment the given integer.
var.dec_integer( name ): Decrement the given integer.
var.count_choices( name ): Return the number of choices.
var.toggle_bool( name ): Toggle the given boolean.

Video
-----
video.fullscreen( [status] ):
 * toggle fullscreen if no arguments are given
 * switch to fullscreen 1st argument is true
 * disable fullscreen if 1st argument is false

VLM
---
vlm(): Instanciate a VLM object.

v = vlc.vlm()
v:execute_command( "new test broadcast" ) -- execute given VLM command

Note: if the VLM object is deleted and you were the last person to hold
a reference to it, all VLM items will be deleted.

Volume
------
volume.get(): Get volume.
volume.set( level ): Set volume to an absolute level between 0 and 1024.
  256 is 100%.
volume.up( [n] ): Increment volume by n steps of 32. n defaults to 1.
volume.down( [n] ): Decrement volume by n steps of 32. n defaults to 1.

Win
---
This module is only available on Windows builds
win.console_init(): Initialize the windows console.
win.console_wait([timeout]): Wait for input on the console for timeout ms.
                             Returns true if console input is available.
win.console_read(): Read input from the windows console. Note that polling and
                    reading from stdin does not work under windows.

XML
---
xml = vlc.xml(): Create an xml object.
reader = xml:create_reader( stream ): create an xml reader that use the given stream.
reader:read(): read some data, return -1 on error, 0 on EOF, 1 on start of XML
  element, 2 on end of XML element, 3 on text
reader:name(): name of the element
reader:value(): value of the element
reader:next_attr(): next attribute of the element
reader:node_empty(): queries whether the previous invocation of reader:read()
  refers to an empty node ("<tag/>"). Returns a value less than 0 on error,
  1 if the node is empty, and 0 if it is not.

The simplexml module can also be used to parse XML documents easily.
