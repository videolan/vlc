$Id$

This file describes commands available through the requests/ file:

Lines starting with < describe what the page sends back
Lines starting with > describe what you can send to the page

All parameters need to be URL encoded.
Examples:
 # -> %23
 % -> %25
 + -> %2B
 space -> +
 ...

status.xml:
===========
< Get VLC status information, current item info and meta.

> add <mrl> to playlist and start playback:
  ?command=in_play&input=<mrl>

> add <mrl> to playlist:
  ?command=in_enqueue&input=<mrl>

> play playlist item <id>:
  ?command=pl_play&id=<id>

> toggle pause. If current state was 'stop', play item <id>:
  ?command=pl_pause&id=<id>

> stop playback:
  ?command=pl_stop

> jump to next item:
  ?command=pl_next

> jump to previous item:
  ?command=pl_previous

> delete item <id> from playlist:
  ?command=pl_delete&id=<id>

> empty playlist:
  ?command=pl_empty

> sort playlist using sort mode <val> and order <id>:
  ?command=pl_sort&id=<id>&val=<val>
  If id=0 then items will be sorted in normal order, if id=1 they will be
  sorted in reverse order
  A non exhaustive list of sort modes:
    0 Id
    1 Name
    3 Author
    5 Random
    7 Track number

> toggle random playback:
  ?command=pl_random

> toggle loop:
  ?command=pl_loop

> toggle repeat:
  ?command=pl_repeat

> toggle enable service discovery module <val>:
  ?command=pl_sd&val=<val>
  Typical values are:
    sap
    shoutcast
    podcast
    hal

> toggle fullscreen:
  ?command=fullscreen

> set volume level to <val> (can be absolute integer, percent or +/- relative value):
  ?command=volume&val=<val>
  Allowed values are of the form:
    +<int>, -<int>, <int> or <int>%

> seek to <val>:
  ?command=seek&val=<val>
  Allowed values are of the form:
    [+ or -][<int><H or h>:][<int><M or m or '>:][<int><nothing or S or s or ">]
    or [+ or -]<int>%
    (value between [ ] are optional, value between < > are mandatory)
  examples:
    1000 -> seek to the 1000th second
    +1H:2M -> seek 1 hour and 2 minutes forward
    -10% -> seek 10% back

playlist.xml:
=============
< get the full playlist tree

browse.xml:
===========
< ?dir=<dir>
> get <dir>'s filelist

vlm.xml:
========
< get the full list of VLM elements

vlm_cmd.xml:
============
< execute VLM command <cmd>
  ?command=<cmd>
> get the error message from <cmd>
