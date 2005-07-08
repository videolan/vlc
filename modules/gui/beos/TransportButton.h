/*****************************************************************************
 * TransportButton.h
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Tony Castley <tcastley@mail.powerup.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef __MEDIA_BUTTON__
#define __MEDIA_BUTTON__

#include <Control.h>

class BMessage;
class BBitmap;
class PeriodicMessageSender;
class BitmapStash;

// TransportButton must be installed into a window with B_ASYNCHRONOUS_CONTROLS on
// currently no button focus drawing

class TransportButton : public BControl {
public:

	TransportButton(BRect frame, const char *name,
		const unsigned char *normalBits,
		const unsigned char *pressedBits,
		const unsigned char *disabledBits,
		BMessage *invokeMessage,			// done pressing over button
		BMessage *startPressingMessage = 0, // just clicked button
		BMessage *pressingMessage = 0, 		// periodical still pressing
		BMessage *donePressing = 0, 		// tracked out of button/didn't invoke
		bigtime_t period = 0,				// pressing message period
		uint32 key = 0,						// optional shortcut key
		uint32 modifiers = 0,				// optional shortcut key modifier
		uint32 resizeFlags = B_FOLLOW_LEFT | B_FOLLOW_TOP);
	
	virtual ~TransportButton();

	void SetStartPressingMessage(BMessage *);
	void SetPressingMessage(BMessage *);
	void SetDonePressingMessage(BMessage *);
	void SetPressingPeriod(bigtime_t);

	virtual void SetEnabled(bool);

protected:		

	enum {
		kDisabledMask = 0x1,
		kPressedMask = 0x2
	};
	
	virtual void AttachedToWindow();
	virtual void DetachedFromWindow();
	virtual void Draw(BRect);
	virtual void MouseDown(BPoint);
	virtual	void MouseMoved(BPoint, uint32 code, const BMessage *);
	virtual	void MouseUp(BPoint);
	virtual	void WindowActivated(bool);

	virtual BBitmap *MakeBitmap(uint32);
		// lazy bitmap builder
	
	virtual uint32 ModeMask() const;
		// mode mask corresponding to the current button state
		// - determines which bitmap will be used
	virtual const unsigned char *BitsForMask(uint32) const;
		// pick the right bits based on a mode mask

		// overriding class can add swapping between two pairs of bitmaps, etc.
	virtual void StartPressing();
	virtual void MouseCancelPressing();
	virtual void DonePressing();

private:
	void ShortcutKeyDown();
	void ShortcutKeyUp();
	
	void MouseStartPressing();
	void MouseDonePressing();

	BitmapStash *bitmaps;
		// using BitmapStash * here instead of a direct member so that the class can be private in
		// the .cpp file

	// bitmap bits used to build bitmaps for the different states
	const unsigned char *normalBits;
	const unsigned char *pressedBits;
	const unsigned char *disabledBits;
	
	BMessage *startPressingMessage;
	BMessage *pressingMessage;
	BMessage *donePressingMessage;
	bigtime_t pressingPeriod;
	
	bool mouseDown;
	bool keyDown;
	PeriodicMessageSender *messageSender;
	BMessageFilter *keyPressFilter;

	typedef BControl _inherited;
	
	friend class SkipButtonKeypressFilter;
	friend class BitmapStash;
};

class PlayPauseButton : public TransportButton {
// Knows about playing and paused states, blinks
// the pause LED during paused state
public:
	PlayPauseButton(BRect frame, const char *name,
		const unsigned char *normalBits,
		const unsigned char *pressedBits,
		const unsigned char *disabledBits,
		const unsigned char *normalPlayingBits,
		const unsigned char *pressedPlayingBits,
		const unsigned char *normalPausedBits,
		const unsigned char *pressedPausedBits,
		BMessage *invokeMessage,			// done pressing over button
		uint32 key = 0,						// optional shortcut key
		uint32 modifiers = 0,				// optional shortcut key modifier
		uint32 resizeFlags = B_FOLLOW_LEFT | B_FOLLOW_TOP);
	
	// These need get called periodically to update the button state
	// OK to call them over and over - once the state is correct, the call
	// is very low overhead
	void SetStopped();
	void SetPlaying();
	void SetPaused();

protected:
	
	virtual uint32 ModeMask() const;
	virtual const unsigned char *BitsForMask(uint32) const;

	virtual void StartPressing();
	virtual void MouseCancelPressing();
	virtual void DonePressing();

private:
	const unsigned char *normalPlayingBits;
	const unsigned char *pressedPlayingBits;
	const unsigned char *normalPausedBits;
	const unsigned char *pressedPausedBits;
	
	enum PlayState {
		kStopped,
		kAboutToPlay,
		kPlaying,
		kAboutToPause,
		kPausedLedOn,
		kPausedLedOff
	};
	
	enum {
		kPlayingMask = 0x4,
		kPausedMask = 0x8
	};
	
	PlayState state;
	bigtime_t lastPauseBlinkTime;
	uint32 lastModeMask;
	
	typedef TransportButton _inherited;
};

#endif	// __MEDIA_BUTTON__
