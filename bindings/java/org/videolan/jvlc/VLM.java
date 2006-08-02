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
	
	public long getInstance() {
		return libvlcInstance;
	}
}
