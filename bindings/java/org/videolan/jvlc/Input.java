package org.videolan.jvlc;

public class Input implements InputIntf {
	
    private long libvlcInstance;

	/*
     *  Input native methods
     */
    private native long     _getLength();
    private native float    _getPosition();
    private native long     _getTime();
    private native float	_getFPS();
    private native void		_setTime(long value);
    private native void		_setPosition(float value);
    private native boolean  _isPlaying();
    private native boolean  _hasVout();

    
    public Input( long instance ) {
    	this.libvlcInstance = instance;
    }

	public long getLength() throws VLCException {
        return _getLength();        
    }

    public long getTime() throws VLCException {
        return _getTime();
    }

    public float getPosition() throws VLCException {
        return _getPosition();
        
    }

    public void setTime(long time) throws VLCException {
    	_setTime(time);
    }

    public void setPosition(float position) throws VLCException {
    	_setPosition(position);
    }

    public double getFPS() throws VLCException {
        return _getFPS();
    }
    
    public boolean isPlaying() throws VLCException {
        return _isPlaying();
    }

    public boolean hasVout() throws VLCException {
        return _hasVout();
    }
    
	public long getInstance() {
		return libvlcInstance;
	}

}
