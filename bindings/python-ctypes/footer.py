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
    print "Event:", event
    print "Data", data

if __name__ == '__main__':
    import sys
    if sys.argv[1:]:
        i=vlc.Instance()
        m=i.media_new(sys.argv[1])
        p=MediaPlayer()
        p.set_media(m)
        p.play()
