package org.videolan.jvlc;

public class Audio implements AudioIntf {
    
	private long libvlcInstance;

	private native boolean	_getMute();
    private native void		_setMute( boolean value );
    private native void		_toggleMute();
    private native int		_getVolume();
    private native void		_setVolume( int volume );
    
    public Audio( long instance ) {
    	this.libvlcInstance = instance;
    }
    
    public boolean getMute() throws VLCException {
        return _getMute();
    }

    public void setMute(boolean value) throws VLCException {
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
