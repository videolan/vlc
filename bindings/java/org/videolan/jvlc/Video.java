/**
 * 
 */
package org.videolan.jvlc;

import java.awt.Component;
import java.awt.Dimension;

public final class Video implements VideoIntf {
	
	private long libvlcInstance;
	
	public Video( long libvlcInstance) {
		this.libvlcInstance = libvlcInstance;
		
	}

	/*
     * Video native methods
     */
    private native void			_toggleFullscreen();
    private native void			_setFullscreen( boolean value);
    private native boolean		_getFullscreen();
    private native int			_getHeight();
    private native int			_getWidth();
    private native void			_getSnapshot(String filename);
    private native void			_destroyVideo();
    private native void			_reparent(Component component);
    private native void			_setSize(int width, int height);
	
	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#destroyVideo()
	 */
	public void destroyVideo() throws VLCException {
		_destroyVideo();
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#getFullscreen()
	 */
	public boolean getFullscreen() throws VLCException {
		return _getFullscreen();
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#getSnapshot(java.lang.String)
	 */
	public void getSnapshot(String filepath) throws VLCException {
		_getSnapshot( filepath );
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#getVideoHeight()
	 */
	public int getHeight() throws VLCException {
		return _getHeight();
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#getVideoWidth()
	 */
	public int getWidth() throws VLCException {
		return _getWidth();
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#reparentVideo(java.awt.Component)
	 */
	public void reparent(Component c) throws VLCException {
		_reparent(c);
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#resizeVideo(int, int)
	 */
	public void setSize(int width, int height) throws VLCException {
		_setSize( width, height );

	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#setFullscreen(boolean)
	 */
	public void setFullscreen(boolean fullscreen) throws VLCException {
		_setFullscreen( fullscreen );
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#toggleFullscreen()
	 */
	public void toggleFullscreen() throws VLCException {
		_toggleFullscreen();
	}
	
	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#getSize()
	 */
	public Dimension getSize() throws VLCException {
		return new Dimension (getWidth(), getHeight());
	}

	/* (non-Javadoc)
	 * @see org.videolan.jvlc.VideoIntf#setSize(java.awt.Dimension)
	 */
	public void setSize(Dimension d) throws VLCException {
		setSize(d.width, d.height);
	}

	public long getInstance() {
		return libvlcInstance;
	}
	
}
