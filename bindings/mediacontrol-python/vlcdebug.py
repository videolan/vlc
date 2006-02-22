"""Wrapper around vlc module in order to ease the use of vlc.Object
class (completion in ipython, access variable as attributes, etc).
"""
import vlc

class VLCObject(object):
    def __init__(self, id):
        object.__setattr__(self, '_o', vlc.Object(id))

    def find(self, typ):
	"""Returns a VLCObject for the given child.

	See vlc.Object.find_object.__doc__ for the different values of typ.
	"""
        t=self._o.find_object(typ)
        if t is not None:
            return VLCObject(t.info()['object-id'])
        else:
            return None

    def __str__(self):
	"""Returns a string representation of the object.
	"""
        i=self._o.info()
        return "VLCObject %d (%s) : %s" % (i['object-id'],
                                           i['object-type'],
                                           i['object-name'])

    def tree(self, prefix=" "):
        """Displays all children as a tree of VLCObject
	"""
        print prefix, self
        for i in self._o.children():
            t=VLCObject(i)
            t.tree(prefix=prefix + " ")
        return

    def __getattribute__(self, attr):
	"""Converts attribute access to access to variables.
	"""
        if attr == '__members__':
	    # Return the list of variables
            o=object.__getattribute__(self, '_o')
            l=dir(o)
            l.extend([ n.replace('-','_') for n in o.list() ])
            return l
        try:
            return object.__getattribute__ (self, attr)
        except AttributeError, e:
            try:
                return self._o.__getattribute__ (attr)
            except AttributeError, e:
                attr=attr.replace('_', '-')
                if attr in self._o.list():
                    return self._o.get(attr)
                else:
                    raise e

    def __setattr__(self, name, value):
	"""Handle attribute assignment.
	"""
        n=name.replace('_', '-')
        if n in self._o.list():
            self._o.set(n, value)
        else:
            object.__setattr__(self, name, value)

def test(f='/tmp/k.mpg'):
    global mc,o
    mc=vlc.MediaControl()
    mc.playlist_add_item(f)
    mc.start(0)
    mc.pause(0)
    o=VLCObject(0)
    v=o.find('vout')

