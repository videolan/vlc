package org.videolan.jvlc;

public class Audio implements AudioIntf {
    
	private long libvlcInstance;

	private native int      _getTrack();
	private native void     _setTrack(int track);
	private native int	    _getChannel();
	private native void     _setChannel(int channel);
	private native boolean	_getMute();
    private native void		_setMute( boolean value );
    private native void		_toggleMute();
    private native int		_getVolume();
    private native void		_setVolume( int volume );
    
    public Audio( long instance ) {
    	this.libvlcInstance = instance;
    }
    
	public int getTrack() throws VLCException {
		return _getTrack();
	}

	public void setTrack( int track ) throws VLCException {
		_setTrack(track);
	}

	public int getChannel() throws VLCException {
		return _getChannel();
	}

	public void setChannel( int channel ) throws VLCException {
		_setChannel(channel);
	}    
    
    public boolean getMute() throws VLCException {
        return _getMute();
    }

    public void setMute( boolean value ) throws VLCException {
        _setMute( value );
        
    }
    
    public void toggleMute() throws VLCException {
    	_toggleMute();
    }

    public int getVolume() throws VLCException {
        return _getVolume();        
    }

    public void setVolume(int volume) throws VLCException {
        _setVolume( volume );
        
    }
    
	public long getInstance() {
		return libvlcInstance;
	}
}
