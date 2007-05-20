package org.videolan.jvlc;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import org.videolan.jvlc.listener.VolumeListener;

public class Audio implements AudioIntf {

	private long libvlcInstance;

	private native int _getTrack();

	private native void _setTrack(int track);

	private native int _getChannel();

	private native void _setChannel(int channel);

	private native boolean _getMute();

	private native void _setMute(boolean value);

	private native void _toggleMute();

	private native int _getVolume();

	private native void _setVolume(int volume);

	private native void _install_callback();

	private static Map objListeners = new HashMap();

	public Audio(long instance) {
		this.libvlcInstance = instance;
		install_callaback();
	}

	private void install_callaback() {
		objListeners.put(this, new HashSet());
		_install_callback();
	}

	public int getTrack() throws VLCException {
		return _getTrack();
	}

	public void setTrack(int track) throws VLCException {
		_setTrack(track);
	}

	public int getChannel() throws VLCException {
		return _getChannel();
	}

	public void setChannel(int channel) throws VLCException {
		_setChannel(channel);
	}

	public boolean getMute() throws VLCException {
		return _getMute();
	}

	public void setMute(boolean value) throws VLCException {
		_setMute(value);

	}

	public void toggleMute() throws VLCException {
		_toggleMute();
	}

	public int getVolume() throws VLCException {
		return _getVolume();
	}

	public void setVolume(int volume) throws VLCException {
		_setVolume(volume);
	}

	public boolean addVolumeListener(VolumeListener listener) {
		HashSet listeners = (HashSet) objListeners.get(this);
		return listeners.add(listener);
	}

	public boolean removeVolumeListener(VolumeListener listener) {
		HashSet listeners = (HashSet) objListeners.get(this);
		return listeners.remove(listener);
	}

	// this method is invoked natively
	private static void wakeupListeners() {
		Set audioObjects = objListeners.keySet();
		Iterator audioObjectsIterator = audioObjects.iterator();
		
		while (audioObjectsIterator.hasNext()) {
			Audio audioObject = (Audio) audioObjectsIterator.next();
			HashSet listeners = (HashSet) objListeners.get(audioObject);

			Iterator listenerIterator = listeners.iterator();
			while (listenerIterator.hasNext()) {
				VolumeListener listener = (VolumeListener) listenerIterator.next();
				listener.volumeChanged();
			}
		}
	}

	public long getInstance() {
		return libvlcInstance;
	}
}
