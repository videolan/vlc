#!/usr/bin/env python

import sys, time

# For gtk/glade
import pygtk
pygtk.require ('2.0')
import gtk
import gtk.glade

# For CORBA
import ORBit, CORBA
# FIXME: How do we make this portable to windows ?
ORBit.load_typelib ("./MediaControl.so")
import VLC

class Connect:
    """Abstract class defining helper functions to interconnect
    glade XML files and methods of a python class."""    
    def create_dictionary (self):
        """Create a (name, function) dictionnary for the current class"""
        dict = {}
        self.create_dictionary_for_class (self.__class__, dict)
        return dict
    
    def create_dictionary_for_class (self, a_class, dict):
        """Create a (name, function) dictionnary for the specified class"""
        bases = a_class.__bases__
        for iteration in bases:
            self.create_dictionary_for_class (iteration, dict)
        for iteration in dir(a_class):
            dict[iteration] = getattr(self, iteration)

    def connect (self):
        """Connects the class methods with the UI"""
        self.gui.signal_autoconnect(self.create_dictionary ())

    def gtk_widget_hide (self, widget):
        widget.hide ()
        return gtk.TRUE
        
    def on_exit(self, source=None, event=None):
        """Generic exit callback"""
        gtk.main_quit()

class DVDControl (Connect):
    def __init__ (self, gladefile):
        """Initializes the GUI and other attributes"""
        # Glade init.
        self.gui = gtk.glade.XML(gladefile)
        self.connect ()
        # Frequently used GUI widgets
        self.gui.logmessages = self.gui.get_widget("logmessages")
        self.gui.position_label = self.gui.get_widget("position_label")
        self.gui.fs = gtk.FileSelection ("Select a file")
        self.gui.fs.ok_button.connect_after ("clicked", lambda win: self.gui.fs.hide ())
        self.gui.fs.cancel_button.connect ("clicked", lambda win: self.gui.fs.destroy ())

        # CORBA init.
        self.mc = None
        self.currentpos = None
        self.status = None
        # FIXME: portability
        self.iorfile = "/tmp/vlc-ior.ref"
        
        # Various
        # Default FF/RW time : 5 seconds
        self.default_time_increment = 5

    def update_title (self, title):
        # Update the title of the main window
        self.gui.get_widget ("win").set_title (title)
        
    def launch_player (self):
        """Launch the VLC corba plugin"""
        #print "Launching vlc server..."
        # FIXME: spawn is portable, but how can we make sure that
        # launch-vlc-corba launches the application in the background ?
        # FIXME: portability
        import distutils.spawn
        distutils.spawn.spawn (["launch-vlc-corba"], True, True)
        # Wait a little for the server to initialize. We could instead test
        # on the existence and validity of self.iorfile
        time.sleep (2)
        return

    def main (self):
        """Mainloop : CORBA initalization and Gtk mainloop setup"""
        self.orb = CORBA.ORB_init(sys.argv)

        errormessage = """Unable to get a MediaControl object
Please try to run the following command:
vlc --intf corba"""
        
        try:
            ior = open(self.iorfile).readline()
        except:
            # The iorfile does not existe : the player is maybe not active
            self.launch_player ()
            try:
                ior = open(self.iorfile).readline()
            except:
                print errormessage
                sys.exit(1)

        self.mc = self.orb.string_to_object(ior)

        if self.mc._non_existent ():
            # The remote object is not available. Let's run the
            # VLC server
            self.launch_player ()
            try:
                ior = open(self.iorfile).readline()
            except:
                print errormessage
                sys.exit(1)
            self.mc = self.orb.string_to_object(ior)
            if self.mc._non_existent ():
                print errormessage
                sys.exit(1)

        self.currentpos = VLC.Position ()
        self.currentpos.value = 0
        self.currentpos.key = VLC.MediaTime
        self.currentpos.origin = VLC.RelativePosition
            
        gtk.timeout_add (20, self.update_display, self.orb)
        gtk.main ()

    def log (self, msg):
        """Adds a new log message to the logmessage window"""
        buf = self.gui.logmessages.get_buffer ()
        mes = str(msg) + "\n"
        buf.insert_at_cursor (mes, len(mes))

        endmark = buf.create_mark ("end",
                                   buf.get_end_iter (),
                                   gtk.TRUE)
        self.gui.logmessages.scroll_mark_onscreen (endmark)
        return
        
    def on_exit (self, source=None, event=None):
        """General exit callback"""
        self.status = "Stop"
        # Terminate the VLC server
        try:
            self.mc.exit()
        except:
            pass
        gtk.main_quit ()

    def file_selector (self, callback=None, label="Select a file",
                       default=""):
        """Display the file selector"""
        self.gui.fs.set_property ("title", label)
        self.gui.fs.set_property ("filename", default)
        self.gui.fs.set_property ("select-multiple", False)
        self.gui.fs.set_property ("show-fileops", False)

        if callback:
            # Disconnecting the old callback
            try:
                self.gui.fs.ok_button.disconnect (self.gui.fs.connect_id)
            except:
                pass
            # Connecting the new one
            self.gui.fs.connect_id = self.gui.fs.ok_button.connect ("clicked", callback, self.gui.fs)
        self.gui.fs.show ()
	return gtk.TRUE

    def file_selected_cb (self, button, fs):
        """Open and play the selected movie file"""
        file = self.gui.fs.get_property ("filename")
        self.mc.add_to_playlist (file)
        self.status = "Play"
        return gtk.TRUE

    def move_position (self, value):
        """Helper function : fast forward or rewind by value seconds"""
        print "Moving by %d seconds" % value
        pos = VLC.Position ()
        pos.value = value
        pos.key = VLC.MediaTime
        pos.origin = VLC.RelativePosition
        self.mc.set_media_position (pos)
        return

    def update_display (self, orb):
        """Update the interface"""
        if self.status == "Play":
            pos = self.mc.get_media_position (VLC.AbsolutePosition,
                                              VLC.ByteCount)
            self.gui.position_label.set_text (str(pos.value))
        elif self.status == "Stop":
            self.gui.position_label.set_text ("N/C")
        return gtk.TRUE

    # Callbacks function. Skeletons can be generated by glade2py
    def on_win_key_press_event (self, win=None, event=None):
        # Navigation keys
        if event.keyval == gtk.keysyms.Tab:
            self.on_b_pause_clicked (win, event)
            return gtk.TRUE
        elif event.keyval == gtk.keysyms.Right:
            self.on_b_forward_clicked (win, event)
            return gtk.TRUE
        elif event.keyval == gtk.keysyms.Left:
            self.on_b_rewind_clicked (win, event)
            return gtk.TRUE
        elif event.keyval == gtk.keysyms.Home:
            pos = VLC.Position ()
            pos.value = 0
            pos.key = VLC.MediaTime
            pos.origin = VLC.AbsolutePosition
            self.mc.set_media_position (pos)
            return gtk.TRUE
        elif event.keyval == gtk.keysyms.End:
            pos = VLC.Position ()
            pos.value = -self.default_time_increment
            pos.key = VLC.MediaTime
            pos.origin = VLC.ModuloPosition
            self.mc.set_media_position (pos)
            return gtk.TRUE
        elif event.keyval == gtk.keysyms.Page_Down:
            # FIXME: Next chapter
            return gtk.TRUE
        elif event.keyval == gtk.keysyms.Page_Up:
            # FIXME: Previous chapter
            return gtk.TRUE
        return gtk.TRUE

    def on_quit1_activate (self, button=None, data=None):
        """Gtk callback to quit"""
        self.on_exit (button, data)
        return gtk.TRUE
    
    def on_about1_activate (self, button=None, data=None):
        self.gui.get_widget("about").show ()
	return gtk.TRUE

    def about_hide (self, button=None, data=None):
        self.gui.get_widget("about").hide ()
	return gtk.TRUE
        
    def on_b_rewind_clicked (self, button=None, data=None):
        if self.status == "Play":
            self.move_position (-self.default_time_increment)
	return gtk.TRUE

    def on_b_play_clicked (self, button=None, data=None):
        if self.status != "Play":
            self.mc.start (self.currentpos)
            self.status = "Play"
        return gtk.TRUE

    def on_b_pause_clicked (self, button=None, data=None):
        if self.status == "Play":
            self.mc.pause (self.currentpos)
            self.status = "Pause"
        elif self.status == "Pause":
            self.mc.pause (self.currentpos)
            self.status = "Play"
	return gtk.TRUE

    def on_b_stop_clicked (self, button=None, data=None):
        self.mc.stop (self.currentpos)
        self.status = "Stop"
	return gtk.TRUE

    def on_b_forward_clicked (self, button=None, data=None):
        if self.status == "Play":
            self.move_position (self.default_time_increment)
	return gtk.TRUE

    def on_b_addfile_clicked (self, button=None, data=None):
        self.file_selector (callback=self.file_selected_cb,
                            label="Play a movie file")
	return gtk.TRUE

    def on_b_selectdvd_clicked (self, button=None, data=None):
        """Play a DVD"""
        self.mc.add_to_playlist ("dvd:///dev/dvd at 1,1")
        self.mc.start (self.currentpos)
        self.status = "Play"
        return gtk.TRUE
    
    def on_b_exit_clicked (self, button=None, data=None):
        self.on_exit (button, data)
	return gtk.TRUE

    def on_logmessages_insert_at_cursor (self, button=None, data=None):
	print "on_logmessages_insert_at_cursor activated (%s, %s, %s)" % (self, button, data)
        # FIXME: faire défiler la scrollmark (cf gtkshell)
	return gtk.TRUE

if __name__ == '__main__':
    v = DVDControl ("simpleplayer.glade")
    v.main ()
