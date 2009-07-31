class Instance:
    """Create a new Instance instance.

    It may take as parameter either:
     * a string
     * a list of strings as first parameters
     * the parameters given as the constructor parameters (must be strings)
     * a MediaControl instance
    """
    def __new__(cls, *p):
        if p and p[0] == 0:
            return None
        elif p and isinstance(p[0], (int, long)):
            # instance creation from ctypes
            o=object.__new__(cls)
            o._as_parameter_=ctypes.c_void_p(p[0])
            return o
        elif len(p) == 1 and isinstance(p[0], basestring):
            # Only 1 string parameter: should be a parameter line
            p=p[0].split()
        elif len(p) == 1 and isinstance(p[0], (tuple, list)):
            p=p[0]

        if p and isinstance(p[0], MediaControl):
            return p[0].get_instance()
        else:
            e=VLCException()
            return libvlc_new(len(p), p, e)

class MediaControl:
    """Create a new MediaControl instance

    It may take as parameter either:
     * a string
     * a list of strings as first parameters
     * the parameters given as the constructor parameters (must be strings)
     * a vlc.Instance
    """
    def __new__(cls, *p):
        if p and p[0] == 0:
            return None
        elif p and isinstance(p[0], (int, long)):
            # instance creation from ctypes
            o=object.__new__(cls)
            o._as_parameter_=ctypes.c_void_p(p[0])
            return o
        elif len(p) == 1 and isinstance(p[0], basestring):
            # Only 1 string parameter: should be a parameter line
            p=p[0].split()
        elif len(p) == 1 and isinstance(p[0], (tuple, list)):
            p=p[0]

        if p and isinstance(p[0], Instance):
            e=MediaControlException()
            return mediacontrol_new_from_instance(p[0])
        else:
            e=MediaControlException()
            return mediacontrol_new(len(p), p, e)
