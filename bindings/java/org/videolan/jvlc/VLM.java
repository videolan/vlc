package org.videolan.jvlc;

public class VLM implements VLMIntf {

	private long libvlcInstance;

	private native void _addBroadcast(String mediaName, String meditInputMRL,
			String mediaOutputMRL, String[] additionalOptions,
			boolean enableBroadcast, boolean isPlayableInLoop);

	private native void _deleteMedia(String mediaName);
	private native void _setEnabled(String mediaName, boolean newStatus);
	private native void _setOutput(String mediaName, String mediaOutputMRL);
	private native void _setInput(String mediaName, String mediaInputMRL);
	private native void _setLoop(String mediaName, boolean isPlayableInLoop);
	private native void _changeMedia(String newMediaName, String inputMRL,
			String outputMRL, String[] additionalOptions,
			boolean enableNewBroadcast, boolean isPlayableInLoop);
	private native void _playMedia(String mediaName);
	private native void _stopMedia(String mediaName);
	private native void _pauseMedia(String mediaName);
	private native void _seekMedia(String mediaName, float percentage);
	private native String _showMedia(String mediaName);
	private native float _getMediaposition(String name, int mediaInstance);
	private native int _getMediatime(String name, int mediaInstance);
	private native int _getMedialength(String name, int mediaInstance);
	private native int _getMediarate(String name, int mediaInstance);
	private native int _getMediatitle(String name, int mediaInstance);
	private native int _getMediachapter(String name, int mediaInstance);
	private native int _getMediaseekable(String name, int mediaInstance);

    public VLM( long instance ) {
    	this.libvlcInstance = instance;
    }
	
	public void addBroadcast(String name, String input, String output,
			String[] options, boolean enabled, boolean loop)
			throws VLCException {
		_addBroadcast(name, input, output, options, enabled, loop);
	}

	public void deleteMedia(String name) throws VLCException {
		_deleteMedia(name);
	}

	public void setEnabled(String name, boolean enabled) throws VLCException {
		_setEnabled(name, enabled);
	}

	public void setOutput(String name, String output) throws VLCException {
		_setOutput(name, output);
	}

	public void setInput(String name, String input) throws VLCException {
		_setInput(name, input);
	}

	public void setLoop(String name, boolean loop) throws VLCException {
		_setLoop(name, loop);
	}

	public void changeMedia(String name, String input, String output,
			String[] options, boolean enabled, boolean loop)
			throws VLCException {
		_changeMedia(name, input, output, options, enabled, loop);
	}

	public void playMedia(String name) throws VLCException {
		_playMedia(name);
	}

	public void stopMedia(String name) throws VLCException {
		_stopMedia(name);
	}

	public void pauseMedia(String name) throws VLCException {
		_pauseMedia(name);
	}
	
	public void seekMedia(String name, float percentage) throws VLCException {
		_seekMedia(name, percentage);
	}

	public String showMedia(String name) throws VLCException {
		return _showMedia(name);
	}
	
	public float getMediaPosition(String name, int mediaInstance) throws VLCException {
		return _getMediaposition(name, mediaInstance);
	}

	public int getMediaTime(String name, int mediaInstance) throws VLCException {
		return _getMediatime(name, mediaInstance);
	}

	public int getMediaLength(String name, int mediaInstance) throws VLCException {
		return _getMedialength(name, mediaInstance);
	}

	public int getMediaRate(String name, int mediaInstance) throws VLCException {
		return _getMediarate(name, mediaInstance);
	}

	public int getMediaTitle(String name, int mediaInstance) throws VLCException {
		return _getMediatitle(name, mediaInstance);
	}

	public int getMediaChapter(String name, int mediaInstance) throws VLCException {
		return _getMediachapter(name, mediaInstance);
	}

	public boolean getMediaSeekable(String name, int mediaInstance) throws VLCException {
		return _getMediaseekable(name, mediaInstance) > 0;
	}


	public long getInstance() {
		return libvlcInstance;
	}
}
