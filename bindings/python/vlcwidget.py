#! /usr/bin/python

"""VLC Widget classes.

This module provides two helper classes, to ease the embedding of a
VLC component inside a pygtk application.

VLCWidget is a simple VLC widget.

DecoratedVLCWidget provides simple player controls.

$Id$
"""

import gtk
import sys
import vlc

from gettext import gettext as _

class VLCWidget(gtk.DrawingArea):
    """Simple VLC widget.

    Its player can be controlled through the 'player' attribute, which
    is a MediaControl instance.
    """
    def __init__(self, *p):
        gtk.DrawingArea.__init__(self)
        self.player=vlc.MediaControl(*p)
        def handle_embed(*p):
            if sys.platform == 'win32':
                xidattr='handle'
            else:
                xidattr='xid'
            self.player.set_visual(getattr(self.window, xidattr))
            return True
        self.connect("map-event", handle_embed)
        self.set_size_request(320, 200)


class DecoratedVLCWidget(gtk.VBox):
    """Decorated VLC widget.

    VLC widget decorated with a player control toolbar.

    Its player can be controlled through the 'player' attribute, which
    is a MediaControl instance.
    """
    def __init__(self, *p):
        gtk.VBox.__init__(self)
        self._vlc_widget=VLCWidget(*p)
        self.player=self._vlc_widget.player
        self.pack_start(self._vlc_widget, expand=True)
        self._toolbar = self.get_player_control_toolbar()
        self.pack_start(self._toolbar, expand=False)

    def get_player_control_toolbar(self):
        """Return a player control toolbar
        """
        tb=gtk.Toolbar()
        tb.set_style(gtk.TOOLBAR_ICONS)

        def on_play(b):
            self.player.start(0)
            return True

        def on_stop(b):
            self.player.stop(0)
            return True

        def on_pause(b):
            self.player.pause(0)
            return True

        tb_list = (
            (_("Play"), _("Play"), gtk.STOCK_MEDIA_PLAY,
             on_play),
            (_("Pause"), _("Pause"), gtk.STOCK_MEDIA_PAUSE,
             on_pause),
            (_("Stop"), _("Stop"), gtk.STOCK_MEDIA_STOP,
             on_stop),
            )

        for text, tooltip, stock, callback in tb_list:
            b=gtk.ToolButton(stock)
            b.connect("clicked", callback)
            tb.insert(b, -1)
        tb.show_all()
        return tb

class VideoPlayer:
    """Example video player.
    """
    def __init__(self):
        self.vlc = DecoratedVLCWidget()

    def main(self, fname):
        self.vlc.player.set_mrl(fname)
        self.popup()
        gtk.main()

    def popup(self):
        w=gtk.Window()
        w.add(self.vlc)
        w.show_all()
        w.connect("destroy", gtk.main_quit)
        return w

if __name__ == '__main__':
    if not sys.argv[1:]:
       print "You must provide a movie filename"
       sys.exit(1)
    p=VideoPlayer()
    p.main(sys.argv[1])
