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
        instance=Instance()
        media=instance.media_new(sys.argv[1])
        player=instance.media_player_new()
        player.set_media(media)
        player.play()

        event_manager=player.event_manager()
        event_manager.event_attach(EventType.MediaPlayerEndReached, end_callback, None)

        def print_info():
            """Print information about the media."""
            media=player.get_media()
            print "State:", player.get_state()
            print "Media:", media.get_mrl()
            try:
                print "Current time:", player.get_time(), "/", media.get_duration()
                print "Position:", player.get_position()
                print "FPS:", player.get_fps()
                print "Rate:", player.get_rate()
                print "Video size: (%d, %d)" % (player.video_get_width(), player.video_get_height())
            except Exception:
                pass

        def forward():
            """Go forward 1s"""
            player.set_time(player.get_time() + 1000)

        def one_frame_forward():
            """Go forward one frame"""
            player.set_time(player.get_time() + long(1000 / (player.get_fps() or 25)))

        def one_frame_backward():
            """Go backward one frame"""
            player.set_time(player.get_time() - long(1000 / (player.get_fps() or 25)))

        def backward():
            """Go backward 1s"""
            player.set_time(player.get_time() - 1000)

        def print_help():
            """Print help
            """
            print "Commands:"
            for k, m in keybindings.iteritems():
                print "  %s: %s" % (k, (m.__doc__ or m.__name__).splitlines()[0])
            print " 1-9: go to the given fraction of the movie"

        def quit_app():
            """Exit."""
            sys.exit(0)

        keybindings={
            'f': player.toggle_fullscreen,
            ' ': player.pause,
            '+': forward,
            '-': backward,
            '.': one_frame_forward,
            ',': one_frame_backward,
            '?': print_help,
            'i': print_info,
            'q': quit_app,
            }

        print "Press q to quit, ? to get help."
        while True:
            k=getch()
            o=ord(k)
            method=keybindings.get(k, None)
            if method is not None:
                method()
            elif o >= 49 and o <= 57:
                # Numeric value. Jump to a fraction of the movie.
                v=0.1*(o-48)
                player.set_position(v)


