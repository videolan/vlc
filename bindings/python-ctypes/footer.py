### Start of footer.py ###

class MediaEvent(ctypes.Structure):
    _fields_ = [
        ('media_name', ctypes.c_char_p),
        ('instance_name', ctypes.c_char_p),
        ]

class EventUnion(ctypes.Union):
    _fields_ = [
        ('meta_type', ctypes.c_uint),
        ('new_child', ctypes.c_uint),
        ('new_duration', ctypes.c_longlong),
        ('new_status', ctypes.c_int),
        ('media', ctypes.c_void_p),
        ('new_state', ctypes.c_uint),
        # Media instance
        ('new_position', ctypes.c_float),
        ('new_time', ctypes.c_longlong),
        ('new_title', ctypes.c_int),
        ('new_seekable', ctypes.c_longlong),
        ('new_pausable', ctypes.c_longlong),
        # FIXME: Skipped MediaList and MediaListView...
        ('filename', ctypes.c_char_p),
        ('new_length', ctypes.c_longlong),
        ('media_event', MediaEvent),
        ]

class Event(ctypes.Structure):
    _fields_ = [
        ('type', EventType),
        ('object', ctypes.c_void_p),
        ('u', EventUnion),
        ]

# Decorator for callback methods
callbackmethod=ctypes.CFUNCTYPE(None, Event, ctypes.c_void_p)

# Example callback method
@callbackmethod
def debug_callback(event, data):
    print "Debug callback method"
    print "Event:", event.type
    print "Data", data

if __name__ == '__main__':
    import sys
    try:
        from msvcrt import getch
    except ImportError:
        def getch():
            import tty
            import termios
            fd=sys.stdin.fileno()
            old_settings=termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                ch=sys.stdin.read(1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
            return ch

    @callbackmethod
    def end_callback(event, data):
        print "End of stream"
        sys.exit(0)

    if sys.argv[1:]:
        i=Instance()
        m=i.media_new(sys.argv[1])
        p=i.media_player_new()
        p.set_media(m)
        p.play()

        e=p.event_manager()
        e.event_attach(EventType.MediaPlayerStopped, end_callback, None)

        def print_info():
            """Print information about the media."""
            m=p.get_media()
            print "Playing ", m.get_mrl()
            print "Current time:", p.get_time(), "/", m.get_duration()
            print "Position", p.get_position()
        
        def forward():
            """Go forward 1s"""
            p.set_time(p.get_time() + 1000)

        def backward():
            """Go backward 1s"""
            p.set_time(p.get_time() - 1000)

        def print_help():
            """Print help
            """
            print "Commands:"
            for k, m in keybindings.iteritems():
                print "  %s: %s" % (k, (m.__doc__ or m.__name__).splitlines()[0])

        def quit():
            """Exit."""
            sys.exit(0)

        keybindings={
            'f': p.toggle_fullscreen,
            ' ': p.pause,
            '+': forward,
            '-': backward,
            '?': print_help,
            'i': print_info,
            'q': quit,
            }

        while True:
            k=getch()
            method=keybindings.get(k, None)
            if method is not None:
                method()
