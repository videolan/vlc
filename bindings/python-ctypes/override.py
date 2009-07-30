class Instance:
    @staticmethod
    def new(*p):
        """Create a new Instance.
        """
        e=VLCException()
        return libvlc_new(len(p), p, e)

class MediaControl:
    @staticmethod
    def new(*p):
        """Create a new MediaControl
        """
        e=MediaControlException()
        return mediacontrol_new(len(p), p, e)

    @staticmethod
    def new_from_instance(i):
        """Create a new MediaControl from an existing Instance.
        """
        e=MediaControlException()
        return mediacontrol_new_from_instance(i, e)

class MediaList:
    def __len__(self):
        e=VLCException()
        return libvlc_media_list_count(self, e)

    def __getitem__(self, i):
        e=VLCException()
        return libvlc_media_list_item_at_index(self, i, e)
