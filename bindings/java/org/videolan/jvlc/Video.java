/**
 * 
 */
package org.videolan.jvlc;

import java.awt.Dimension;
import java.awt.Graphics;

public final class Video implements VideoIntf {
	
	private long libvlcInstance;
	
    private JVLCCanvas actualCanvas;
    
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
    private native void			_getSnapshot(String filename,int width,int height);
    private native void			_destroyVideo();
    private native void			_reparent(JVLCCanvas component);
    private native void			_setSize(int width, int height);
    private native void			_paint(JVLCCanvas canvas, Graphics g);

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
	public void getSnapshot(String filepath,int width,int height) throws VLCException {
		_getSnapshot( filepath , width, height);
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
	public void reparent(JVLCCanvas c) throws VLCException {
		_reparent(c);
		setActualCanvas(c);
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
	
	public void paint(Graphics g) {
		_paint(actualCanvas, g);
	}

	public void setActualCanvas(JVLCCanvas canvas) {
		actualCanvas = canvas;
	}
	
	public long getInstance() {
		return libvlcInstance;
	}
	
}
