/*****************************************************************************
 * TransportButton.cpp
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Tony Castley <tcastley@mail.powerup.com.au>
 *          Stephan Aßmus <stippi@yellowbites.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <Bitmap.h>
#include <Debug.h>
#include <MessageFilter.h>
#include <Screen.h>
#include <Window.h>

#include <map>

#include "TransportButton.h"
#include "DrawingTidbits.h"

class BitmapStash {
// Bitmap stash is a simple class to hold all the lazily-allocated
// bitmaps that the TransportButton needs when rendering itself.
// signature is a combination of the different enabled, pressed, playing, etc.
// flavors of a bitmap. If the stash does not have a particular bitmap,
// it turns around to ask the button to create one and stores it for next time.
public:
	BitmapStash(TransportButton *);
	~BitmapStash();
	BBitmap *GetBitmap(uint32 signature);
	
private:
	TransportButton *owner;
	map<uint32, BBitmap *> stash;
};

BitmapStash::BitmapStash(TransportButton *owner)
	:	owner(owner)
{
}

BBitmap *
BitmapStash::GetBitmap(uint32 signature)
{
	if (stash.find(signature) == stash.end()) {
		BBitmap *newBits = owner->MakeBitmap(signature);
		ASSERT(newBits);
		stash[signature] = newBits;
	}
	
	return stash[signature];
}

BitmapStash::~BitmapStash()
{
	// delete all the bitmaps
	for (map<uint32, BBitmap *>::iterator i = stash.begin(); i != stash.end(); i++) 
		delete (*i).second;
}


class PeriodicMessageSender {
	// used to send a specified message repeatedly when holding down a button
public:
	static PeriodicMessageSender *Launch(BMessenger target,
		const BMessage *message, bigtime_t period);
	void Quit();

private:
	PeriodicMessageSender(BMessenger target, const BMessage *message,
		bigtime_t period);
	~PeriodicMessageSender() {}
		// use quit

	static status_t TrackBinder(void *);
	void Run();
	
	BMessenger target;
	BMessage message;

	bigtime_t period;
	
	bool requestToQuit;
};


PeriodicMessageSender::PeriodicMessageSender(BMessenger target,
	const BMessage *message, bigtime_t period)
	:	target(target),
		message(*message),
		period(period),
		requestToQuit(false)
{
}

PeriodicMessageSender *
PeriodicMessageSender::Launch(BMessenger target, const BMessage *message,
	bigtime_t period)
{
	PeriodicMessageSender *result = new PeriodicMessageSender(target, message, period);
	thread_id thread = spawn_thread(&PeriodicMessageSender::TrackBinder,
		"ButtonRepeatingThread", B_NORMAL_PRIORITY, result);
	
	if (thread <= 0 || resume_thread(thread) != B_OK) {
		// didn't start, don't leak self
		delete result;
		result = 0;
	}

	return result;
}

void 
PeriodicMessageSender::Quit()
{
	requestToQuit = true;
}

status_t 
PeriodicMessageSender::TrackBinder(void *castToThis)
{
	((PeriodicMessageSender *)castToThis)->Run();
	return 0;
}

void 
PeriodicMessageSender::Run()
{
	for (;;) {
		snooze(period);
		if (requestToQuit)
			break;
		target.SendMessage(&message);
	}
	delete this;
}

class SkipButtonKeypressFilter : public BMessageFilter {
public:
	SkipButtonKeypressFilter(uint32 shortcutKey, uint32 shortcutModifier,
		TransportButton *target);

protected:
	filter_result Filter(BMessage *message, BHandler **handler);

private:
	uint32 shortcutKey;
	uint32 shortcutModifier;
	TransportButton *target;
};

SkipButtonKeypressFilter::SkipButtonKeypressFilter(uint32 shortcutKey,
	uint32 shortcutModifier, TransportButton *target)
	:	BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE),
		shortcutKey(shortcutKey),
		shortcutModifier(shortcutModifier),
		target(target)
{
}

filter_result 
SkipButtonKeypressFilter::Filter(BMessage *message, BHandler **handler)
{
	if (target->IsEnabled()
		&& (message->what == B_KEY_DOWN || message->what == B_KEY_UP)) {
		uint32 modifiers;
		uint32 rawKeyChar = 0;
		uint8 byte = 0;
		int32 key = 0;
		
		if (message->FindInt32("modifiers", (int32 *)&modifiers) != B_OK
			|| message->FindInt32("raw_char", (int32 *)&rawKeyChar) != B_OK
			|| message->FindInt8("byte", (int8 *)&byte) != B_OK
			|| message->FindInt32("key", &key) != B_OK)
			return B_DISPATCH_MESSAGE;

		modifiers &= B_SHIFT_KEY | B_COMMAND_KEY | B_CONTROL_KEY
			| B_OPTION_KEY | B_MENU_KEY;
			// strip caps lock, etc.

		if (modifiers == shortcutModifier && rawKeyChar == shortcutKey) {
			if (message->what == B_KEY_DOWN)
				target->ShortcutKeyDown();
			else
				target->ShortcutKeyUp();
			
			return B_SKIP_MESSAGE;
		}
	}

	// let others deal with this
	return B_DISPATCH_MESSAGE;
}

TransportButton::TransportButton(BRect frame, const char *name,
	const unsigned char *normalBits,
	const unsigned char *pressedBits,
	const unsigned char *disabledBits,
	BMessage *invokeMessage, BMessage *startPressingMessage,
	BMessage *pressingMessage, BMessage *donePressingMessage, bigtime_t period,
	uint32 key, uint32 modifiers, uint32 resizeFlags)
	:	BControl(frame, name, "", invokeMessage, resizeFlags, B_WILL_DRAW | B_NAVIGABLE),
		bitmaps(new BitmapStash(this)),
		normalBits(normalBits),
		pressedBits(pressedBits),
		disabledBits(disabledBits),
		startPressingMessage(startPressingMessage),
		pressingMessage(pressingMessage),
		donePressingMessage(donePressingMessage),
		pressingPeriod(period),
		mouseDown(false),
		keyDown(false),
		messageSender(0),
		keyPressFilter(0)
{
	if (key)
		keyPressFilter = new SkipButtonKeypressFilter(key, modifiers, this);
}


void 
TransportButton::AttachedToWindow()
{
	_inherited::AttachedToWindow();
	if (keyPressFilter)
		Window()->AddCommonFilter(keyPressFilter);
	
	// transparent to reduce flicker
	SetViewColor(B_TRANSPARENT_COLOR);
}

void 
TransportButton::DetachedFromWindow()
{
	if (keyPressFilter)
		Window()->RemoveCommonFilter(keyPressFilter);
	_inherited::DetachedFromWindow();
}


TransportButton::~TransportButton()
{
	delete startPressingMessage;
	delete pressingMessage;
	delete donePressingMessage;
	delete bitmaps;
	delete keyPressFilter;
}

void 
TransportButton::WindowActivated(bool state)
{
	if (!state)
		ShortcutKeyUp();
	
	_inherited::WindowActivated(state);
}

void 
TransportButton::SetEnabled(bool on)
{
	if (on != IsEnabled()) {
		_inherited::SetEnabled(on);
		if (!on)
			ShortcutKeyUp();
	}	
}

const unsigned char *
TransportButton::BitsForMask(uint32 mask) const
{
	switch (mask) {
		case 0:
			return normalBits;
		case kDisabledMask:
			return disabledBits;
		case kPressedMask:
			return pressedBits;
		default:
			break;
	}	
	TRESPASS();
	return 0;
}


BBitmap *
TransportButton::MakeBitmap(uint32 mask)
{
	BRect r(Bounds());
	BBitmap *result = new BBitmap(r, B_CMAP8);

	uint8* src = (uint8*)BitsForMask(mask);

	if (src && result && result->IsValid()) {
		// int32 width = r.IntegerWidth() + 1;
		int32 height = r.IntegerHeight() + 1;
		int32 bpr = result->BytesPerRow();
		uint8* dst = (uint8*)result->Bits();
		// copy source bits into bitmap line by line,
		// taking possible alignment into account
		// since the source data has been generated
		// by QuickRes, it still contains aligment too
		// (hence skipping bpr and not width bytes)
		for (int32 y = 0; y < height; y++) {
			memcpy(dst, src, bpr);
			src += bpr;
			dst += bpr;
		}
		ReplaceTransparentColor(result, Parent()->ViewColor());
	} else {
		delete result;
		result = NULL;
	}
	
	return result;
}

uint32 
TransportButton::ModeMask() const
{
	return (IsEnabled() ? 0 : kDisabledMask)
		| (Value() ? kPressedMask : 0);
}

void 
TransportButton::Draw(BRect)
{
	DrawBitmapAsync(bitmaps->GetBitmap(ModeMask()));
}


void 
TransportButton::StartPressing()
{
	SetValue(1);
	if (startPressingMessage)
		Invoke(startPressingMessage);
	
	if (pressingMessage) {
		ASSERT(pressingMessage);
		messageSender = PeriodicMessageSender::Launch(Messenger(),
			pressingMessage, pressingPeriod);
	}
}

void 
TransportButton::MouseCancelPressing()
{
	if (!mouseDown || keyDown)
		return;

	mouseDown = false;

	if (pressingMessage) {
		ASSERT(messageSender);
		PeriodicMessageSender *sender = messageSender;
		messageSender = 0;
		sender->Quit();
	}

	if (donePressingMessage)
		Invoke(donePressingMessage);
	SetValue(0);
}

void 
TransportButton::DonePressing()
{	
	if (pressingMessage) {
		ASSERT(messageSender);
		PeriodicMessageSender *sender = messageSender;
		messageSender = 0;
		sender->Quit();
	}

	Invoke();
	SetValue(0);
}

void 
TransportButton::MouseStartPressing()
{
	if (mouseDown)
		return;
	
	mouseDown = true;
	if (!keyDown)
		StartPressing();
}

void 
TransportButton::MouseDonePressing()
{
	if (!mouseDown)
		return;
	
	mouseDown = false;
	if (!keyDown)
		DonePressing();
}

void 
TransportButton::ShortcutKeyDown()
{
	if (!IsEnabled())
		return;

	if (keyDown)
		return;
	
	keyDown = true;
	if (!mouseDown)
		StartPressing();
}

void 
TransportButton::ShortcutKeyUp()
{
	if (!keyDown)
		return;
	
	keyDown = false;
	if (!mouseDown)
		DonePressing();
}


void 
TransportButton::MouseDown(BPoint)
{
	if (!IsEnabled())
		return;

	ASSERT(Window()->Flags() & B_ASYNCHRONOUS_CONTROLS);
	SetTracking(true);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	MouseStartPressing();
}

void 
TransportButton::MouseMoved(BPoint point, uint32 code, const BMessage *)
{
	if (IsTracking() && Bounds().Contains(point) != Value()) {
		if (!Value())
			MouseStartPressing();
		else
			MouseCancelPressing();
	}
}

void 
TransportButton::MouseUp(BPoint point)
{
	if (IsTracking()) {
		if (Bounds().Contains(point))
			MouseDonePressing();
		else
			MouseCancelPressing();
		SetTracking(false);
	}
}

void 
TransportButton::SetStartPressingMessage(BMessage *message)
{
	delete startPressingMessage;
	startPressingMessage = message;
}

void 
TransportButton::SetPressingMessage(BMessage *message)
{
	delete pressingMessage;
	pressingMessage = message;
}

void 
TransportButton::SetDonePressingMessage(BMessage *message)
{
	delete donePressingMessage;
	donePressingMessage = message;
}

void 
TransportButton::SetPressingPeriod(bigtime_t newTime)
{
	pressingPeriod = newTime;
}


PlayPauseButton::PlayPauseButton(BRect frame, const char *name,
	const unsigned char *normalBits, const unsigned char *pressedBits,
	const unsigned char *disabledBits, const unsigned char *normalPlayingBits,
	const unsigned char *pressedPlayingBits, const unsigned char *normalPausedBits,
	const unsigned char *pressedPausedBits,
	BMessage *invokeMessage, uint32 key, uint32 modifiers, uint32 resizeFlags)
	:	TransportButton(frame, name, normalBits, pressedBits,
			disabledBits, invokeMessage, 0,
			0, 0, 0, key, modifiers, resizeFlags),
		normalPlayingBits(normalPlayingBits),
		pressedPlayingBits(pressedPlayingBits),
		normalPausedBits(normalPausedBits),
		pressedPausedBits(pressedPausedBits),
		state(PlayPauseButton::kStopped),
		lastPauseBlinkTime(0),
		lastModeMask(0)
{
}

void 
PlayPauseButton::SetStopped()
{
	if (state == kStopped || state == kAboutToPlay)
		return;
	
	state = kStopped;
	Invalidate();
}

void 
PlayPauseButton::SetPlaying()
{
	if (state == kPlaying || state == kAboutToPause)
		return;
	
	state = kPlaying;
	Invalidate();
}

const bigtime_t kPauseBlinkPeriod = 600000;

void 
PlayPauseButton::SetPaused()
{
	if (state == kAboutToPlay)
		return;

	// in paused state blink the LED on and off
	bigtime_t now = system_time();
	if (state == kPausedLedOn || state == kPausedLedOff) {
		if (now - lastPauseBlinkTime < kPauseBlinkPeriod)
			return;
		
		if (state == kPausedLedOn)
			state = kPausedLedOff;
		else
			state = kPausedLedOn;
	} else
		state = kPausedLedOn;
	
	lastPauseBlinkTime = now;
	Invalidate();
}

uint32 
PlayPauseButton::ModeMask() const
{
	if (!IsEnabled())
		return kDisabledMask;
	
	uint32 result = 0;

	if (Value())
		result = kPressedMask;

	if (state == kPlaying || state == kAboutToPlay)
		result |= kPlayingMask;
	else if (state == kAboutToPause || state == kPausedLedOn)		
		result |= kPausedMask;
	
	return result;
}

const unsigned char *
PlayPauseButton::BitsForMask(uint32 mask) const
{
	switch (mask) {
		case kPlayingMask:
			return normalPlayingBits;
		case kPlayingMask | kPressedMask:
			return pressedPlayingBits;
		case kPausedMask:
			return normalPausedBits;
		case kPausedMask | kPressedMask:
			return pressedPausedBits;
		default:
			return _inherited::BitsForMask(mask);
	}	
	TRESPASS();
	return 0;
}


void 
PlayPauseButton::StartPressing()
{
	if (state == kPlaying)
		state = kAboutToPause;
	else
	 	state = kAboutToPlay;
	
	_inherited::StartPressing();
}

void 
PlayPauseButton::MouseCancelPressing()
{
	if (state == kAboutToPause)
	 	state = kPlaying;
	else
		state = kStopped;
	
	_inherited::MouseCancelPressing();
}

void 
PlayPauseButton::DonePressing()
{
	if (state == kAboutToPause) {
	 	state = kPausedLedOn;
		lastPauseBlinkTime = system_time();
	} else if (state == kAboutToPlay)
		state = kPlaying;
	
	_inherited::DonePressing();
}


