/*****************************************************************************
 * MediaControlView.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Tony Castley <tony@castley.net>
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

/* System headers */
#include <InterfaceKit.h>
#include <AppKit.h>
#include <String.h>
#include <string.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/input.h>
extern "C"
{
  #include <audio_output.h>
}

/* BeOS interface headers */
#include "Bitmaps.h"
#include "DrawingTidbits.h"
#include "InterfaceWindow.h"
#include "MsgVals.h"
#include "TransportButton.h"
#include "ListViews.h"
#include "MediaControlView.h"

#define BORDER_INSET 6.0
#define MIN_SPACE 4.0
#define SPEAKER_SLIDER_DIST 6.0
#define VOLUME_MIN_WIDTH 70.0
#define DIM_LEVEL 0.4
#define VOLUME_SLIDER_LAYOUT_WEIGHT 2.0
#define SEEK_SLIDER_KNOB_WIDTH 8.0

// slider colors are hardcoded here, because that's just
// what they currently are within those bitmaps
const rgb_color kGreen = (rgb_color){ 152, 203, 152, 255 };
const rgb_color kGreenShadow = (rgb_color){ 102, 152, 102, 255 };
const rgb_color kBackground = (rgb_color){ 216, 216, 216, 255 };
const rgb_color kSeekGreen = (rgb_color){ 171, 221, 161, 255 };
const rgb_color kSeekGreenShadow = (rgb_color){ 144, 186, 136, 255 };
const rgb_color kSeekRed = (rgb_color){ 255, 0, 0, 255 };
const rgb_color kSeekRedLight = (rgb_color){ 255, 152, 152, 255 };
const rgb_color kSeekRedShadow = (rgb_color){ 178, 0, 0, 255 };

#define DISABLED_SEEK_MESSAGE _("Drop files to play")
#define SEEKSLIDER_RANGE 2048

enum
{
	MSG_REWIND				= 'rwnd',
	MSG_FORWARD				= 'frwd',
	MSG_SKIP_BACKWARDS		= 'skpb',
	MSG_SKIP_FORWARD		= 'skpf',
};

// constructor
MediaControlView::MediaControlView( intf_thread_t * _p_intf, BRect frame)
	: BBox(frame, NULL, B_FOLLOW_NONE, B_WILL_DRAW | B_FRAME_EVENTS | B_PULSE_NEEDED,
		   B_PLAIN_BORDER),
      p_intf( _p_intf ),
      fCurrentRate(INPUT_RATE_DEFAULT),
      fCurrentStatus(-1),
      fBottomControlHeight(0.0),
      fIsEnabled( true )
{
	BRect frame(0.0, 0.0, 10.0, 10.0);
	
    // Seek Slider
    fSeekSlider = new SeekSlider( p_intf, frame, "seek slider", this );
    fSeekSlider->SetValue(0);
    fSeekSlider->ResizeToPreferred();
    AddChild( fSeekSlider );

    // Buttons
    // Skip Back
    frame.SetRightBottom(kSkipButtonSize);
	fBottomControlHeight = kRewindBitmapHeight - 1.0;
    fSkipBack = new TransportButton(frame, B_EMPTY_STRING,
                                    kSkipBackBitmapBits,
                                    kPressedSkipBackBitmapBits,
                                    kDisabledSkipBackBitmapBits,
                                    new BMessage(MSG_SKIP_BACKWARDS));
    AddChild( fSkipBack );

	// Play Pause
    frame.SetRightBottom(kPlayButtonSize);
	if (fBottomControlHeight < kPlayPauseBitmapHeight - 1.0)
		fBottomControlHeight = kPlayPauseBitmapHeight - 1.0;
    fPlayPause = new PlayPauseButton(frame, B_EMPTY_STRING,
                                     kPlayButtonBitmapBits,
                                     kPressedPlayButtonBitmapBits,
                                     kDisabledPlayButtonBitmapBits,
                                     kPlayingPlayButtonBitmapBits,
                                     kPressedPlayingPlayButtonBitmapBits,
                                     kPausedPlayButtonBitmapBits,
                                     kPressedPausedPlayButtonBitmapBits,
                                     new BMessage(START_PLAYBACK));

    AddChild( fPlayPause );

    // Skip Foward
    frame.SetRightBottom(kSkipButtonSize);
    fSkipForward = new TransportButton(frame, B_EMPTY_STRING,
                                       kSkipForwardBitmapBits,
                                       kPressedSkipForwardBitmapBits,
                                       kDisabledSkipForwardBitmapBits,
                                       new BMessage(MSG_SKIP_FORWARD));
    AddChild( fSkipForward );

	// Forward
	fForward = new TransportButton(frame, B_EMPTY_STRING,
								   kForwardBitmapBits,
								   kPressedForwardBitmapBits,
								   kDisabledForwardBitmapBits,
								   new BMessage(MSG_FORWARD));
//	AddChild( fForward );

	// Rewind
	fRewind = new TransportButton(frame, B_EMPTY_STRING,
								  kRewindBitmapBits,
								  kPressedRewindBitmapBits,
								  kDisabledRewindBitmapBits,
								  new BMessage(MSG_REWIND));
//	AddChild( fRewind );

    // Stop
    frame.SetRightBottom(kStopButtonSize);
	if (fBottomControlHeight < kStopBitmapHeight - 1.0)
		fBottomControlHeight = kStopBitmapHeight - 1.0;
    fStop = new TransportButton(frame, B_EMPTY_STRING,
                                kStopButtonBitmapBits,
                                kPressedStopButtonBitmapBits,
                                kDisabledStopButtonBitmapBits,
                                new BMessage(STOP_PLAYBACK));
	AddChild( fStop );

	// Mute
    frame.SetRightBottom(kSpeakerButtonSize);
	if (fBottomControlHeight < kSpeakerIconBitmapHeight - 1.0)
		fBottomControlHeight = kSpeakerIconBitmapHeight - 1.0;
    fMute = new TransportButton(frame, B_EMPTY_STRING,
                                kSpeakerIconBits,
                                kPressedSpeakerIconBits,
                                kSpeakerIconBits,
                                new BMessage(VOLUME_MUTE));

	AddChild( fMute );

    // Volume Slider
	fVolumeSlider = new VolumeSlider(BRect(0.0, 0.0, VOLUME_MIN_WIDTH,
										   kVolumeSliderBitmapHeight - 1.0),
									 "volume slider", 1, AOUT_VOLUME_MAX,
									 new BMessage(VOLUME_CHG));
	fVolumeSlider->SetValue( config_GetInt( p_intf, "volume" ) );
	AddChild( fVolumeSlider );
	
	// Position Info View
    fPositionInfo = new PositionInfoView(BRect(0.0, 0.0, 10.0, 10.0), "led",
                                         p_intf);
    fPositionInfo->ResizeToPreferred();
    AddChild( fPositionInfo );
}

// destructor
MediaControlView::~MediaControlView()
{
}

// AttachedToWindow
void
MediaControlView::AttachedToWindow()
{
	// we are now a valid BHandler
	fRewind->SetTarget(this);
	fForward->SetTarget(this);
	fSkipBack->SetTarget(this);
	fSkipForward->SetTarget(this);
	fVolumeSlider->SetTarget(Window());

	BRect r(_MinFrame());
	if (BMenuBar* menuBar = Window()->KeyMenuBar()) {
		float width, height;
		menuBar->GetPreferredSize(&width, &height);
//		r.bottom += menuBar->Bounds().Height();
		r.bottom += height;
		// see that our calculated minimal width is not smaller than what
		// the menubar can be
		width -= r.Width();
		if (width > 0.0)
			r.right += width;
	}

	Window()->SetSizeLimits(r.Width(), r.Width() * 1.8, r.Height(), r.Height() * 1.3);
	if (!Window()->Bounds().Contains(r))
		Window()->ResizeTo(r.Width(), r.Height());
	else
		FrameResized(Bounds().Width(), Bounds().Height());

	// get pulse message every two frames
	Window()->SetPulseRate(80000);
}

// FrameResized
void
MediaControlView::FrameResized(float width, float height)
{
	BRect r(Bounds());
	// make sure we don't leave dirty pixels
	// (B_FULL_UPDATE_ON_RESIZE == annoying flicker -> this is smarter)
	if (fOldBounds.Width() < r.Width())
		Invalidate(BRect(fOldBounds.right, fOldBounds.top + 1.0,
						 fOldBounds.right, fOldBounds.bottom - 1.0));
	else
		Invalidate(BRect(r.right, r.top + 1.0,
						 r.right, r.bottom - 1.0));
	if (fOldBounds.Height() < r.Height())
		Invalidate(BRect(fOldBounds.left + 1.0, fOldBounds.bottom,
						 fOldBounds.right - 1.0, fOldBounds.bottom));
	else
		Invalidate(BRect(r.left + 1.0, r.bottom,
						 r.right - 1.0, r.bottom));
	// remember for next time
	fOldBounds = r;
	// layout controls
	r.InsetBy(BORDER_INSET, BORDER_INSET);
	_LayoutControls(r);
}

// GetPreferredSize
void
MediaControlView::GetPreferredSize(float* width, float* height)
{
	if (width && height)
	{
		BRect r(_MinFrame());
		*width = r.Width();
		*height = r.Height();
	}
}

// MessageReceived
void
MediaControlView::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case MSG_REWIND:
			break;
		case MSG_FORWARD:
			break;
		case MSG_SKIP_BACKWARDS:
			Window()->PostMessage(NAVIGATE_PREV);
			break;
		case MSG_SKIP_FORWARD:
			Window()->PostMessage(NAVIGATE_NEXT);
			break;
		default:
		    BBox::MessageReceived(message);
		    break;
	}
}

// Pulse
void
MediaControlView::Pulse()
{
	InterfaceWindow* window = dynamic_cast<InterfaceWindow*>(Window());
	if (window && window->IsStopped())
			fPlayPause->SetStopped();

    unsigned short i_volume;
    aout_VolumeGet( p_intf, (audio_volume_t*)&i_volume );
    fVolumeSlider->SetValue( i_volume );
}

// SetProgress
void
MediaControlView::SetProgress( float position )
{
	fSeekSlider->SetPosition( position );
}

// SetStatus
void
MediaControlView::SetStatus(int status, int rate)
{
	// we need to set the button status periodically
	// (even if it is the same) to get a blinking button
	fCurrentStatus = status;
    switch( status )
    {
        case PLAYING_S:
            fPlayPause->SetPlaying();
            break;
        case PAUSE_S:
            fPlayPause->SetPaused();
            break;
        default:
            fPlayPause->SetStopped();
            break;
    }
	if (rate != fCurrentRate)
	{
		fCurrentRate = rate;
	    if ( rate < INPUT_RATE_DEFAULT )
	    {
	    	// TODO: ...
	    }
	}
}

// SetEnabled
void
MediaControlView::SetEnabled(bool enabled)
{
    if( ( enabled && fIsEnabled ) ||
        ( !enabled && !fIsEnabled ) )
    {
        /* do not redraw if it is not necessary */
        return;
    }
    
	if( LockLooper() )
	{
		fSkipBack->SetEnabled( enabled );
		fPlayPause->SetEnabled( enabled );
		fSkipForward->SetEnabled( enabled );
		fStop->SetEnabled( enabled );
		fMute->SetEnabled( enabled );
		fVolumeSlider->SetEnabled( enabled );
		fSeekSlider->SetEnabled( enabled );
		fRewind->SetEnabled( enabled );
		fForward->SetEnabled( enabled );
		UnlockLooper();
		fIsEnabled = enabled;
	}
}

// SetAudioEnabled
void
MediaControlView::SetAudioEnabled(bool enabled)
{
	fMute->SetEnabled(enabled);
	fVolumeSlider->SetEnabled(enabled);
}

// GetVolume
uint32
MediaControlView::GetVolume() const
{
	return fVolumeSlider->Value();
}

// SetSkippable
void
MediaControlView::SetSkippable(bool backward, bool forward)
{
	fSkipBack->SetEnabled(backward);
	fSkipForward->SetEnabled(forward);
}

// SetMuted
void
MediaControlView::SetMuted(bool mute)
{
	fVolumeSlider->SetMuted(mute);
}

// _LayoutControls
void
MediaControlView::_LayoutControls(BRect frame) const
{
	// seek slider
	BRect r(frame);
	// calculate absolutly minimal width
	float minWidth = fSkipBack->Bounds().Width();
//	minWidth += fRewind->Bounds().Width();
	minWidth += fStop->Bounds().Width();
	minWidth += fPlayPause->Bounds().Width();
//	minWidth += fForward->Bounds().Width();
	minWidth += fSkipForward->Bounds().Width();
	minWidth += fMute->Bounds().Width();
	minWidth += VOLUME_MIN_WIDTH;
	
	// layout time slider and info view
    float width, height;
    fPositionInfo->GetBigPreferredSize( &width, &height );
    float ratio = width / height;
    width = r.Height() * ratio;
    if (frame.Width() - minWidth - MIN_SPACE >= width
              && frame.Height() >= height)
    {
        r.right = r.left + width;
        fPositionInfo->SetMode(PositionInfoView::MODE_BIG);
        _LayoutControl(fPositionInfo, r, true, true);
        frame.left = r.right + MIN_SPACE;
        r.left = frame.left;
        r.right = frame.right;
    //    r.bottom = r.top + r.Height() / 2.0 - MIN_SPACE / 2.0;
        r.bottom = r.top + fSeekSlider->Bounds().Height();
        _LayoutControl(fSeekSlider, r, true);
    }
    else
    {
        fPositionInfo->GetPreferredSize( &width, &height );
        fPositionInfo->SetMode(PositionInfoView::MODE_SMALL);
        fPositionInfo->ResizeTo(width, height);
        r.bottom = r.top + r.Height() / 2.0 - MIN_SPACE / 2.0;
        r.right = r.left + fPositionInfo->Bounds().Width();
        _LayoutControl(fPositionInfo, r, true );
        r.left = r.right + MIN_SPACE;
        r.right = frame.right;
        _LayoutControl(fSeekSlider, r, true);
    }
	float currentWidth = frame.Width();
	float space = (currentWidth - minWidth) / 6.0;//8.0;
	// apply weighting
	space = MIN_SPACE + (space - MIN_SPACE) / VOLUME_SLIDER_LAYOUT_WEIGHT;
	// layout controls with "space" inbetween
	r.left = frame.left;
	r.top = r.bottom + MIN_SPACE + 1.0;
	r.bottom = frame.bottom;
	// skip back
	r.right = r.left + fSkipBack->Bounds().Width();
	_LayoutControl(fSkipBack, r);
	// rewind
//	r.left = r.right + space;
//	r.right = r.left + fRewind->Bounds().Width();
//	_LayoutControl(fRewind, r);
	// stop
	r.left = r.right + space;
	r.right = r.left + fStop->Bounds().Width();
	_LayoutControl(fStop, r);
	// play/pause
	r.left = r.right + space;
	r.right = r.left + fPlayPause->Bounds().Width();
	_LayoutControl(fPlayPause, r);
	// forward
//	r.left = r.right + space;
//	r.right = r.left + fForward->Bounds().Width();
//	_LayoutControl(fForward, r);
	// skip forward
	r.left = r.right + space;
	r.right = r.left + fSkipForward->Bounds().Width();
	_LayoutControl(fSkipForward, r);
	// speaker icon
	r.left = r.right + space + space;
	r.right = r.left + fMute->Bounds().Width();
	_LayoutControl(fMute, r);
	// volume slider
	r.left = r.right + SPEAKER_SLIDER_DIST; // keep speaker icon and volume slider attached
	r.right = frame.right;
	_LayoutControl(fVolumeSlider, r, true);
}

// _MinFrame
BRect           
MediaControlView::_MinFrame() const
{
	// add up width of controls along bottom (seek slider will likely adopt)
	float minWidth = 2 * BORDER_INSET;
	minWidth += fSkipBack->Bounds().Width() + MIN_SPACE;
//	minWidth += fRewind->Bounds().Width() + MIN_SPACE;
	minWidth += fStop->Bounds().Width() + MIN_SPACE;
	minWidth += fPlayPause->Bounds().Width() + MIN_SPACE;
//	minWidth += fForward->Bounds().Width() + MIN_SPACE;
	minWidth += fSkipForward->Bounds().Width() + MIN_SPACE + MIN_SPACE;
	minWidth += fMute->Bounds().Width() + SPEAKER_SLIDER_DIST;
	minWidth += VOLUME_MIN_WIDTH;

	// add up height of seek slider and heighest control on bottom
	float minHeight = 2 * BORDER_INSET;
	minHeight += fSeekSlider->Bounds().Height() + MIN_SPACE + MIN_SPACE / 2.0;
	minHeight += fBottomControlHeight;
	return BRect(0.0, 0.0, minWidth - 1.0, minHeight - 1.0);
}

// _LayoutControl
void
MediaControlView::_LayoutControl(BView* view, BRect frame,
                                 bool resizeWidth, bool resizeHeight) const
{
    if (!resizeHeight)
	    // center vertically
	    frame.top = (frame.top + frame.bottom) / 2.0 - view->Bounds().Height() / 2.0;
	if (!resizeWidth)
	    //center horizontally
		frame.left = (frame.left + frame.right) / 2.0 - view->Bounds().Width() / 2.0;
	view->MoveTo(frame.LeftTop());
	float width = resizeWidth ? frame.Width() : view->Bounds().Width();
	float height = resizeHeight ? frame.Height() : view->Bounds().Height();
    if (resizeWidth || resizeHeight)
        view->ResizeTo(width, height);
}



/*****************************************************************************
 * SeekSlider
 *****************************************************************************/
SeekSlider::SeekSlider( intf_thread_t * _p_intf,
                        BRect frame, const char* name, MediaControlView *owner )
	: BControl(frame, name, NULL, NULL, B_FOLLOW_NONE,
			   B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	  p_intf(_p_intf),
	  fOwner(owner),
	  fTracking(false)
{
	BFont font(be_plain_font);
	font.SetSize(9.0);
	SetFont(&font);
}

SeekSlider::~SeekSlider()
{
}

/*****************************************************************************
 * VolumeSlider::AttachedToWindow
 *****************************************************************************/
void
SeekSlider::AttachedToWindow()
{
	BControl::AttachedToWindow();
	SetViewColor(B_TRANSPARENT_32_BIT);
}

/*****************************************************************************
 * VolumeSlider::Draw
 *****************************************************************************/
void
SeekSlider::Draw(BRect updateRect)
{
	BRect r(Bounds());
	float knobWidth2 = SEEK_SLIDER_KNOB_WIDTH / 2.0;
	float sliderStart = (r.left + knobWidth2);
	float sliderEnd = (r.right - knobWidth2);
	float knobPos = sliderStart
					+ floorf((sliderEnd - sliderStart - 1.0) * Value()
					/ SEEKSLIDER_RANGE);
	// draw both sides (the original from Be doesn't seem
	// to make a difference for enabled/disabled state)
//	DrawBitmapAsync(fLeftSideBits, r.LeftTop());
//	DrawBitmapAsync(fRightSideBits, BPoint(sliderEnd + 1.0, r.top));
	// colors for the slider area between the two bitmaps
	rgb_color background = kBackground;//ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color shadow = tint_color(background, B_DARKEN_2_TINT);
	rgb_color softShadow = tint_color(background, B_DARKEN_1_TINT);
	rgb_color darkShadow = tint_color(background, B_DARKEN_4_TINT);
	rgb_color midShadow = tint_color(background, B_DARKEN_3_TINT);
	rgb_color light = tint_color(background, B_LIGHTEN_MAX_TINT);
	rgb_color softLight = tint_color(background, B_LIGHTEN_1_TINT);
	rgb_color green = kSeekGreen;
	rgb_color greenShadow = kSeekGreenShadow;
	rgb_color black = kBlack;
	rgb_color dotGrey = midShadow;
	rgb_color dotGreen = greenShadow;
	// draw frame
	_StrokeFrame(r, softShadow, softShadow, softLight, softLight);
	r.InsetBy(1.0, 1.0);
	_StrokeFrame(r, black, black, light, light);
	if (IsEnabled())
	{
		r.InsetBy(1.0, 1.0);
		// inner shadow
		_StrokeFrame(r, greenShadow, greenShadow, green, green);
		r.top++;
		r.left++;
		_StrokeFrame(r, greenShadow, greenShadow, green, green);
		// inside area
		r.InsetBy(1.0, 1.0);
		SetHighColor(green);
		FillRect(r);
		// dots
		int32 dotCount = (int32)(r.Width() / 6.0);
		BPoint dotPos;
		dotPos.y = r.top + 2.0;
		SetHighColor(dotGreen);
		for (int32 i = 0; i < dotCount; i++)
		{
			dotPos.x = sliderStart + i * 6.0 + 5.0;
			StrokeLine(dotPos, BPoint(dotPos.x, dotPos.y + 6.0));
		}
		// slider handle
		r.top -= 4.0;
		r.bottom += 3.0;
		r.left = knobPos - knobWidth2;
		r.right = knobPos + knobWidth2;
		// black outline
		float handleBottomSize = 2.0;
		float handleArrowSize = 6.0;
		BeginLineArray(10);
			// upper handle
			AddLine(BPoint(r.left, r.top + handleBottomSize),
					BPoint(r.left, r.top), black);
			AddLine(BPoint(r.left + 1.0, r.top),
					BPoint(r.right, r.top), black);
			AddLine(BPoint(r.right, r.top + 1.0),
					BPoint(r.right, r.top + handleBottomSize), black);
			AddLine(BPoint(r.right - 1.0, r.top + handleBottomSize + 1.0),
					BPoint(knobPos, r.top + handleArrowSize), black);
			AddLine(BPoint(knobPos - 1.0, r.top + handleArrowSize - 1.0),
					BPoint(r.left + 1.0, r.top + handleBottomSize + 1.0), black);
			// lower handle
			AddLine(BPoint(r.left, r.bottom),
					BPoint(r.left, r.bottom - handleBottomSize), black);
			AddLine(BPoint(r.left + 1.0, r.bottom - handleBottomSize - 1.0),
					BPoint(knobPos, r.bottom - handleArrowSize), black);
			AddLine(BPoint(knobPos + 1.0, r.bottom - handleArrowSize + 1.0),
					BPoint(r.right, r.bottom - handleBottomSize), black);
			AddLine(BPoint(r.right, r.bottom - handleBottomSize + 1.0),
					BPoint(r.right, r.bottom), black);
			AddLine(BPoint(r.right - 1.0, r.bottom),
					BPoint(r.left + 1.0, r.bottom), black);
		EndLineArray();
		// inner red light and shadow lines
		r.InsetBy(1.0, 1.0);
		handleBottomSize--;
		handleArrowSize -= 2.0;
		BeginLineArray(10);
			// upper handle
			AddLine(BPoint(r.left, r.top + handleBottomSize),
					BPoint(r.left, r.top), kSeekRedLight);
			AddLine(BPoint(r.left + 1.0, r.top),
					BPoint(r.right, r.top), kSeekRedLight);
			AddLine(BPoint(r.right, r.top + 1.0),
					BPoint(r.right, r.top + handleBottomSize), kSeekRedShadow);
			AddLine(BPoint(r.right - 1.0, r.top + handleBottomSize + 1.0),
					BPoint(knobPos, r.top + handleArrowSize), kSeekRedShadow);
			AddLine(BPoint(knobPos - 1.0, r.top + handleArrowSize - 1.0),
					BPoint(r.left + 1.0, r.top + handleBottomSize + 1.0), kSeekRedLight);
			// lower handle
			AddLine(BPoint(r.left, r.bottom),
					BPoint(r.left, r.bottom - handleBottomSize), kSeekRedLight);
			AddLine(BPoint(r.left + 1.0, r.bottom - handleBottomSize - 1.0),
					BPoint(knobPos, r.bottom - handleArrowSize), kSeekRedLight);
			AddLine(BPoint(knobPos + 1.0, r.bottom - handleArrowSize + 1.0),
					BPoint(r.right, r.bottom - handleBottomSize), kSeekRedShadow);
			AddLine(BPoint(r.right, r.bottom - handleBottomSize + 1.0),
					BPoint(r.right, r.bottom), kSeekRedShadow);
			AddLine(BPoint(r.right - 1.0, r.bottom),
					BPoint(r.left + 1.0, r.bottom), kSeekRedShadow);
		EndLineArray();
		// fill rest of handles with red
		SetHighColor(kSeekRed);
		r.InsetBy(1.0, 1.0);
		handleArrowSize -= 2.0;
		BPoint arrow[3];
		// upper handle arrow
		arrow[0].x = r.left;
		arrow[0].y = r.top;
		arrow[1].x = r.right;
		arrow[1].y = r.top;
		arrow[2].x = knobPos;
		arrow[2].y = r.top + handleArrowSize;
		FillPolygon(arrow, 3);
		// lower handle arrow
		arrow[0].x = r.left;
		arrow[0].y = r.bottom;
		arrow[1].x = r.right;
		arrow[1].y = r.bottom;
		arrow[2].x = knobPos;
		arrow[2].y = r.bottom - handleArrowSize;
		FillPolygon(arrow, 3);
	}
	else
	{
		r.InsetBy(1.0, 1.0);
		_StrokeFrame(r, darkShadow, darkShadow, darkShadow, darkShadow);
		r.InsetBy(1.0, 1.0);
		_StrokeFrame(r, darkShadow, darkShadow, darkShadow, darkShadow);
		r.InsetBy(1.0, 1.0);
		SetHighColor(darkShadow);
		SetLowColor(shadow);
		// stripes
		float width = floorf(StringWidth(DISABLED_SEEK_MESSAGE));
		float textPos = r.left + r.Width() / 2.0 - width / 2.0;
		pattern stripes = {{ 0xc7, 0x8f, 0x1f, 0x3e, 0x7c, 0xf8, 0xf1, 0xe3 }};
		BRect stripesRect(r);
		stripesRect.right = textPos - 5.0;
		FillRect(stripesRect, stripes);
		stripesRect.left = textPos + width + 3.0;
		stripesRect.right = r.right;
		FillRect(stripesRect, stripes);
		// info text
		r.left = textPos - 4.0;
		r.right = textPos + width + 2.0;
		FillRect(r);
		SetHighColor(shadow);
		SetLowColor(darkShadow);
		font_height fh;
		GetFontHeight(&fh);
		DrawString(DISABLED_SEEK_MESSAGE, BPoint(textPos, r.top + ceilf(fh.ascent) - 1.0));
	}
}

/*****************************************************************************
 * SeekSlider::MouseDown
 *****************************************************************************/
void
SeekSlider::MouseDown(BPoint where)
{
	if (IsEnabled() && Bounds().Contains(where))
	{
		SetValue(_ValueFor(where.x));
		fTracking = true;
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	}
}

/*****************************************************************************
 * SeekSlider::MouseMoved
 *****************************************************************************/
void
SeekSlider::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	if (fTracking)
	{
		SetValue(_ValueFor(where.x));
	}
}

/*****************************************************************************
 * SeekSlider::MouseUp
 *****************************************************************************/
void
SeekSlider::MouseUp(BPoint where)
{
	if (fTracking)
	{
		fTracking = false;
		input_thread_t * p_input;
		p_input = (input_thread_t *)
            vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

        if( p_input )
        {
		    var_SetFloat( p_input, "position",
		                  (float) Value() / SEEKSLIDER_RANGE );
		    vlc_object_release( p_input );
		}
	}
}

/*****************************************************************************
 * SeekSlider::ResizeToPreferred
 *****************************************************************************/
void
SeekSlider::ResizeToPreferred()
{
	float width = 15.0 + StringWidth(DISABLED_SEEK_MESSAGE) + 15.0;
	ResizeTo(width, 17.0);
}

/*****************************************************************************
 * SeekSlider::SetPosition
 *****************************************************************************/
void
SeekSlider::SetPosition(float position)
{
	if ( LockLooper() )
	{
	    if( !fTracking )
	    {
		    SetValue( SEEKSLIDER_RANGE * position );
		}
		UnlockLooper();
	}
}

/*****************************************************************************
 * SeekSlider::_ValueFor
 *****************************************************************************/
int32
SeekSlider::_ValueFor(float xPos) const
{
	BRect r(Bounds());
	float knobWidth2 = SEEK_SLIDER_KNOB_WIDTH / 2.0;
	float sliderStart = (r.left + knobWidth2);
	float sliderEnd = (r.right - knobWidth2);
	int32 value =  (int32)(((xPos - sliderStart) * SEEKSLIDER_RANGE)
				  / (sliderEnd - sliderStart - 1.0));
	if (value < 0)
		value = 0;
	if (value > SEEKSLIDER_RANGE)
		value = SEEKSLIDER_RANGE;
	return value;
}

/*****************************************************************************
 * SeekSlider::_StrokeFrame
 *****************************************************************************/
void
SeekSlider::_StrokeFrame(BRect r, rgb_color left, rgb_color top,
						 rgb_color right, rgb_color bottom)
{
	BeginLineArray(4);
		AddLine(BPoint(r.left, r.bottom), BPoint(r.left, r.top), left);
		AddLine(BPoint(r.left + 1.0, r.top), BPoint(r.right, r.top), top);
		AddLine(BPoint(r.right, r.top + 1.0), BPoint(r.right, r.bottom), right);
		AddLine(BPoint(r.right - 1.0, r.bottom), BPoint(r.left + 1.0, r.bottom), bottom);
	EndLineArray();
}

/*****************************************************************************
 * VolumeSlider
 *****************************************************************************/
VolumeSlider::VolumeSlider(BRect frame, const char* name, int32 minValue, int32 maxValue,
						   BMessage* message, BHandler* target)
	: BControl(frame, name, NULL, message, B_FOLLOW_NONE,
			   B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	  fLeftSideBits(NULL),
	  fRightSideBits(NULL),
	  fKnobBits(NULL),
	  fTracking(false),
	  fMuted(false),
	  fMinValue(minValue),
	  fMaxValue(maxValue)
{
	SetTarget(target);

	// create bitmaps
	BRect r(BPoint(0.0, 0.0), kVolumeSliderBitmapSize);
	fLeftSideBits = new BBitmap(r, B_CMAP8);
	fRightSideBits = new BBitmap(r, B_CMAP8);
	r.Set(0.0, 0.0, kVolumeSliderKnobBitmapSize.x, kVolumeSliderKnobBitmapSize.y);
	fKnobBits = new BBitmap(r, B_CMAP8);

	_MakeBitmaps();
}

/*****************************************************************************
 * VolumeSlider destructor
 *****************************************************************************/
VolumeSlider::~VolumeSlider()
{
	delete fLeftSideBits;
	delete fRightSideBits;
	delete fKnobBits;
}

/*****************************************************************************
 * VolumeSlider::AttachedToWindow
 *****************************************************************************/
void
VolumeSlider::AttachedToWindow()
{
	BControl::AttachedToWindow();
	SetViewColor(B_TRANSPARENT_32_BIT);
}

/*****************************************************************************
 * VolumeSlider::SetValue
 *****************************************************************************/
void
VolumeSlider::SetValue(int32 value)
{
	if (value != Value())
	{
		BControl::SetValue(value);
		Invoke();
	}
}

/*****************************************************************************
 * VolumeSlider::SetEnabled
 *****************************************************************************/
void
VolumeSlider::SetEnabled(bool enable)
{
	if (enable != IsEnabled())
	{
		BControl::SetEnabled(enable);
		_MakeBitmaps();
		Invalidate();
	}
}

/*****************************************************************************
 * VolumeSlider::Draw
 *****************************************************************************/
void
VolumeSlider::Draw(BRect updateRect)
{
	if (IsValid())
	{
		BRect r(Bounds());
		float sliderSideWidth = kVolumeSliderBitmapWidth;
		float sliderStart = (r.left + sliderSideWidth);
		float sliderEnd = (r.right - sliderSideWidth);
		float knobPos = sliderStart
						+ (sliderEnd - sliderStart - 1.0) * (Value() - fMinValue)
						/ (fMaxValue - fMinValue);
		// draw both sides (the original from Be doesn't seem
		// to make a difference for enabled/disabled state)
		DrawBitmapAsync(fLeftSideBits, r.LeftTop());
		DrawBitmapAsync(fRightSideBits, BPoint(sliderEnd + 1.0, r.top));
		// colors for the slider area between the two bitmaps
		rgb_color background = kBackground;//ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color shadow = tint_color(background, B_DARKEN_2_TINT);
		rgb_color softShadow = tint_color(background, B_DARKEN_1_TINT);
		rgb_color darkShadow = tint_color(background, B_DARKEN_4_TINT);
		rgb_color midShadow = tint_color(background, B_DARKEN_3_TINT);
		rgb_color light = tint_color(background, B_LIGHTEN_MAX_TINT);
		rgb_color softLight = tint_color(background, B_LIGHTEN_1_TINT);
		rgb_color green = kGreen;
		rgb_color greenShadow = kGreenShadow;
		rgb_color black = kBlack;
		rgb_color dotGrey = midShadow;
		rgb_color dotGreen = greenShadow;
		// make dimmed version of colors if we're disabled
		if (!IsEnabled())
		{
			shadow = (rgb_color){ 200, 200, 200, 255 };
			softShadow = dimmed_color_cmap8(softShadow, background, DIM_LEVEL);
			darkShadow = dimmed_color_cmap8(darkShadow, background, DIM_LEVEL);
			midShadow = shadow;
			light = dimmed_color_cmap8(light, background, DIM_LEVEL);
			softLight = dimmed_color_cmap8(softLight, background, DIM_LEVEL);
			green = dimmed_color_cmap8(green, background, DIM_LEVEL);
			greenShadow = dimmed_color_cmap8(greenShadow, background, DIM_LEVEL);
			black = dimmed_color_cmap8(black, background, DIM_LEVEL);
			dotGreen = dotGrey;
		}
		else if (fMuted)
		{
			green = tint_color(kBackground, B_DARKEN_3_TINT);
			greenShadow = tint_color(kBackground, B_DARKEN_4_TINT);
			dotGreen = greenShadow;
		}
		// draw slider edges between bitmaps
		BeginLineArray(7);
			AddLine(BPoint(sliderStart, r.top),
					BPoint(sliderEnd, r.top), softShadow);
			AddLine(BPoint(sliderStart, r.bottom),
					BPoint(sliderEnd, r.bottom), softLight);
			r.InsetBy(0.0, 1.0);
			AddLine(BPoint(sliderStart, r.top),
					BPoint(sliderEnd, r.top), black);
			AddLine(BPoint(sliderStart, r.bottom),
					BPoint(sliderEnd, r.bottom), light);
			r.top++;
			AddLine(BPoint(sliderStart, r.top),
					BPoint(knobPos, r.top), greenShadow);
			AddLine(BPoint(knobPos, r.top),
					BPoint(sliderEnd, r.top), midShadow);
			r.top++;
			AddLine(BPoint(sliderStart, r.top),
					BPoint(knobPos, r.top), greenShadow);
		EndLineArray();
		// fill rest inside of slider
		r.InsetBy(0.0, 1.0);
		r.left = sliderStart;
		r.right = knobPos;
		SetHighColor(green);
		FillRect(r, B_SOLID_HIGH);
		r.left = knobPos + 1.0;
		r.right = sliderEnd;
		r.top -= 1.0;
		SetHighColor(shadow);
		FillRect(r, B_SOLID_HIGH);
		// draw little dots inside
		int32 dotCount = (int32)((sliderEnd - sliderStart) / 5.0);
		BPoint dotPos;
		dotPos.y = r.top + 4.0;
		for (int32 i = 0; i < dotCount; i++)
		{
			dotPos.x = sliderStart + i * 5.0 + 4.0;
			SetHighColor(dotPos.x < knobPos ? dotGreen : dotGrey);
			StrokeLine(dotPos, BPoint(dotPos.x, dotPos.y + 1.0));
		}
		// draw knob
		r.top -= 1.0;
		SetDrawingMode(B_OP_OVER); // part of knob is transparent
		DrawBitmapAsync(fKnobBits, BPoint(knobPos - kVolumeSliderKnobWidth / 2, r.top));
	}
}

/*****************************************************************************
 * VolumeSlider::MouseDown
 *****************************************************************************/
void
VolumeSlider::MouseDown(BPoint where)
{
	if (Bounds().Contains(where) && IsEnabled())
	{
		fTracking = true;
		SetValue(_ValueFor(where.x));
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	}
}

/*****************************************************************************
 * VolumeSlider::MouseMoved
 *****************************************************************************/
void
VolumeSlider::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
{
	if (fTracking)
		SetValue(_ValueFor(where.x));
}

/*****************************************************************************
 * VolumeSlider::MouseUp
 *****************************************************************************/
void
VolumeSlider::MouseUp(BPoint where)
{
	fTracking = false;
}


/*****************************************************************************
 * VolumeSlider::IsValid
 *****************************************************************************/
bool
VolumeSlider::IsValid() const
{
	return (fLeftSideBits && fLeftSideBits->IsValid()
			&& fRightSideBits && fRightSideBits->IsValid()
			&& fKnobBits && fKnobBits->IsValid());
}

/*****************************************************************************
 * VolumeSlider::SetMuted
 *****************************************************************************/
void
VolumeSlider::SetMuted(bool mute)
{
	if (mute != fMuted)
	{
		fMuted = mute;
		_MakeBitmaps();
		Invalidate();
	}
}

/*****************************************************************************
 * VolumeSlider::_MakeBitmaps
 *****************************************************************************/
void
VolumeSlider::_MakeBitmaps()
{
	if (IsValid())
	{
		// left side of slider
		memcpy(fLeftSideBits->Bits(), kVolumeSliderLeftBitmapBits,
			   fLeftSideBits->BitsLength());
		// right side of slider
		memcpy(fRightSideBits->Bits(), kVolumeSliderRightBits,
			   fRightSideBits->BitsLength());
		// slider knob
		int32 length = fKnobBits->BitsLength();
		memcpy(fKnobBits->Bits(), kVolumeSliderKnobBits, length);
		uint8* bits = (uint8*)fKnobBits->Bits();
		// black was used in the knob to represent transparency
		// use screen to get index for the "transarent" color used in the bitmap
		BScreen screen(B_MAIN_SCREEN_ID);
		uint8 blackIndex = screen.IndexForColor(kBlack);
		// replace black index with transparent index
		for (int32 i = 0; i < length; i++)
			if (bits[i] == blackIndex)
				bits[i] = B_TRANSPARENT_MAGIC_CMAP8;

		if (!IsEnabled())
		{
			// make ghosted versions of the bitmaps
			dim_bitmap(fLeftSideBits, kBackground, DIM_LEVEL);
			dim_bitmap(fRightSideBits, kBackground, DIM_LEVEL);
			dim_bitmap(fKnobBits, kBackground, DIM_LEVEL);
		}
		else if (fMuted)
		{
			// replace green color (and shadow) in left slider side
			bits = (uint8*)fLeftSideBits->Bits();
			length = fLeftSideBits->BitsLength();
			uint8 greenIndex = screen.IndexForColor(kGreen);
			uint8 greenShadowIndex = screen.IndexForColor(kGreenShadow);
			rgb_color shadow = tint_color(kBackground, B_DARKEN_3_TINT);
			rgb_color midShadow = tint_color(kBackground, B_DARKEN_4_TINT);
			uint8 replaceIndex = screen.IndexForColor(shadow);
			uint8 replaceShadowIndex = screen.IndexForColor(midShadow);
			for (int32 i = 0; i < length; i++)
			{
				if (bits[i] == greenIndex)
					bits[i] = replaceIndex;
				else if (bits[i] == greenShadowIndex)
					bits[i] = replaceShadowIndex;
			}
		}
	}
}

/*****************************************************************************
 * VolumeSlider::_ValueFor
 *****************************************************************************/
int32
VolumeSlider::_ValueFor(float xPos) const
{
	BRect r(Bounds());
	float sliderStart = (r.left + kVolumeSliderBitmapWidth);
	float sliderEnd = (r.right - kVolumeSliderBitmapWidth);
	int32 value =  fMinValue + (int32)(((xPos - sliderStart) * (fMaxValue - fMinValue))
				  / (sliderEnd - sliderStart - 1.0));
	if (value < fMinValue)
		value = fMinValue;
	if (value > fMaxValue)
		value = fMaxValue;
	return value;
}

/*****************************************************************************
 * PositionInfoView::PositionInfoView
 *****************************************************************************/
PositionInfoView::PositionInfoView( BRect frame, const char* name,
                                    intf_thread_t * p_interface )
	: BView( frame, name, B_FOLLOW_NONE,
			 B_WILL_DRAW | B_PULSE_NEEDED | B_FULL_UPDATE_ON_RESIZE ),
	  fMode( MODE_SMALL ),
	  fCurrentFileIndex( -1 ),
	  fCurrentFileSize( -1 ),
	  fCurrentTitleIndex( -1 ),
	  fCurrentTitleSize( -1 ),
	  fCurrentChapterIndex( -1 ),
	  fCurrentChapterSize( -1 ),
	  fSeconds( -1 ),
	  fTimeString( "-:--:--" ),
	  fLastPulseUpdate( system_time() ),
	  fStackedWidthCache( 0.0 ),
	  fStackedHeightCache( 0.0 )
{
    p_intf = p_interface;

	SetViewColor( B_TRANSPARENT_32_BIT );
	SetLowColor( kBlack );
	SetHighColor( 0, 255, 0, 255 );
	SetFontSize( 11.0 );
}

/*****************************************************************************
 * PositionInfoView::~PositionInfoView
 *****************************************************************************/
PositionInfoView::~PositionInfoView()
{
}

/*****************************************************************************
 * PositionInfoView::Draw
 *****************************************************************************/
void
PositionInfoView::Draw( BRect updateRect )
{
	rgb_color background = ui_color( B_PANEL_BACKGROUND_COLOR );
	rgb_color shadow = tint_color( background, B_DARKEN_1_TINT );
	rgb_color darkShadow = tint_color( background, B_DARKEN_4_TINT );
	rgb_color light = tint_color( background, B_LIGHTEN_MAX_TINT );
	rgb_color softLight = tint_color( background, B_LIGHTEN_1_TINT );
	// frame
	BRect r( Bounds() );
	BeginLineArray( 8 );
		AddLine( BPoint( r.left, r.bottom ),
				 BPoint( r.left, r.top ), shadow );
		AddLine( BPoint( r.left + 1.0, r.top ),
				 BPoint( r.right, r.top ), shadow );
		AddLine( BPoint( r.right, r.top + 1.0 ),
				 BPoint( r.right, r.bottom ), softLight );
		AddLine( BPoint( r.right - 1.0, r.bottom ),
				 BPoint( r.left + 1.0, r.bottom ), softLight );
		r.InsetBy( 1.0, 1.0 );
		AddLine( BPoint( r.left, r.bottom ),
				 BPoint( r.left, r.top ), darkShadow );
		AddLine( BPoint( r.left + 1.0, r.top ),
				 BPoint( r.right, r.top ), darkShadow );
		AddLine( BPoint( r.right, r.top + 1.0 ),
				 BPoint( r.right, r.bottom ), light );
		AddLine( BPoint( r.right - 1.0, r.bottom ),
				 BPoint( r.left + 1.0, r.bottom ), light );
	EndLineArray();
	// background
	r.InsetBy( 1.0, 1.0 );
	FillRect( r, B_SOLID_LOW );
	// contents
	font_height fh;
	GetFontHeight( &fh );
	switch ( fMode )
	{
		case MODE_SMALL:
		{
			float width = StringWidth( fTimeString.String() );
			DrawString( fTimeString.String(),
						BPoint( r.left + r.Width() / 2.0 - width / 2.0,
								r.top + r.Height() / 2.0 + fh.ascent / 2.0 - 1.0 ) );
			break;
		}
		case MODE_BIG:
		{
			BFont font;
			GetFont( &font );
			BFont smallFont = font;
			BFont bigFont = font;
			BFont tinyFont = font;
			smallFont.SetSize( r.Height() / 5.0 );
			bigFont.SetSize( r.Height() / 3.0 );
			tinyFont.SetSize( r.Height() / 7.0 );
			float timeHeight = r.Height() / 2.5;
			float height = ( r.Height() - timeHeight ) / 3.0;
			SetFont( &tinyFont );
			SetHighColor( 0, 180, 0, 255 );
			DrawString( _("File"), BPoint( r.left + 3.0, r.top + height ) );
			DrawString( _("Title"), BPoint( r.left + 3.0, r.top + 2.0 * height ) );
			DrawString( _("Chapter"), BPoint( r.left + 3.0, r.top + 3.0 * height ) );
			SetFont( &smallFont );
			BString helper;
			SetHighColor( 0, 255, 0, 255 );
			// file
			_MakeString( helper, fCurrentFileIndex, fCurrentFileSize );
			float width = StringWidth( helper.String() );
			DrawString( helper.String(), BPoint( r.right - 3.0 - width, r.top + height ) );
			// title
			_MakeString( helper, fCurrentTitleIndex, fCurrentTitleSize );
			width = StringWidth( helper.String() );
			DrawString( helper.String(), BPoint( r.right - 3.0 - width, r.top + 2.0 * height ) );
			// chapter
			_MakeString( helper, fCurrentChapterIndex, fCurrentChapterSize );
			width = StringWidth( helper.String() );
			DrawString( helper.String(), BPoint( r.right - 3.0 - width, r.top + 3.0 * height ) );
			// time
			SetFont( &bigFont );
			width = StringWidth( fTimeString.String() );
			DrawString( fTimeString.String(),
						BPoint( r.left + r.Width() / 2.0 - width / 2.0,
								r.bottom - 3.0 ) );
			break;
		}
	}
}

/*****************************************************************************
 * PositionInfoView::ResizeToPreferred
 *****************************************************************************/
void
PositionInfoView::ResizeToPreferred()
{
	float width, height;
	GetPreferredSize( &width, &height );
	ResizeTo( width, height );
}

/*****************************************************************************
 * PositionInfoView::GetPreferredSize
 *****************************************************************************/
void
PositionInfoView::GetPreferredSize( float* width, float* height )
{
	if ( width && height )
	{
		*width = 5.0 + ceilf( StringWidth( "0:00:00" ) ) + 5.0;
		font_height fh;
		GetFontHeight( &fh );
		*height = 3.0 + ceilf( fh.ascent ) + 3.0;
		fStackedWidthCache = *width * 1.2;
		fStackedHeightCache = *height * 2.7;
	}
}

/*****************************************************************************
 * PositionInfoView::Pulse
 *****************************************************************************/
void
PositionInfoView::Pulse()
{
	// allow for Pulse frequency to be higher, MediaControlView needs it
	bigtime_t now = system_time();
	if ( now - fLastPulseUpdate > 900000 )
	{
#if 0
		int32 index, size;
		p_intf->p_sys->p_wrapper->GetPlaylistInfo( index, size );
		SetFile( index + 1, size );
		p_intf->p_sys->p_wrapper->TitleInfo( index, size );
		SetTitle( index, size );
		p_intf->p_sys->p_wrapper->ChapterInfo( index, size );
		SetChapter( index, size );
		SetTime( p_intf->p_sys->p_wrapper->GetTimeAsString() );
		fLastPulseUpdate = now;
#endif
	}
}

/*****************************************************************************
 * PositionInfoView::GetBigPreferredSize
 *****************************************************************************/
void
PositionInfoView::GetBigPreferredSize( float* width, float* height )
{
	if ( width && height )
	{
		*width = fStackedWidthCache;
		*height = fStackedHeightCache;
	}
}

/*****************************************************************************
 * PositionInfoView::SetMode
 *****************************************************************************/
void
PositionInfoView::SetMode( uint32 mode )
{
	if ( fMode != mode )
	{
		fMode = mode;
		_InvalidateContents();
	}
}

/*****************************************************************************
 * PositionInfoView::SetFile
 *****************************************************************************/
void
PositionInfoView::SetFile( int32 index, int32 size )
{
	if ( fCurrentFileIndex != index || fCurrentFileSize != size )
	{
		fCurrentFileIndex = index;
		fCurrentFileSize = size;
		_InvalidateContents();
	}
}

/*****************************************************************************
 * PositionInfoView::SetTitle
 *****************************************************************************/
void
PositionInfoView::SetTitle( int32 index, int32 size )
{
	if ( fCurrentTitleIndex != index || fCurrentFileSize != size )
	{
		fCurrentTitleIndex = index;
		fCurrentTitleSize = size;
		_InvalidateContents();
	}
}

/*****************************************************************************
 * PositionInfoView::SetChapter
 *****************************************************************************/
void
PositionInfoView::SetChapter( int32 index, int32 size )
{
	if ( fCurrentChapterIndex != index || fCurrentFileSize != size )
	{
		fCurrentChapterIndex = index;
		fCurrentChapterSize = size;
		_InvalidateContents();
	}
}

/*****************************************************************************
 * PositionInfoView::SetTime
 *****************************************************************************/
void
PositionInfoView::SetTime( int32 seconds )
{
	if ( fSeconds != seconds )
	{
		if ( seconds >= 0 )
		{
			int32 minutes = seconds / 60;
			int32 hours = minutes / 60;
			seconds -= minutes * 60 - hours * 60 * 60;
			minutes -= hours * 60;
			fTimeString.SetTo( "" );
			fTimeString << hours << ":" << minutes << ":" << seconds;
		}
		else
			fTimeString.SetTo( "-:--:--" );

		fSeconds = seconds;
		_InvalidateContents();
	}
}

/*****************************************************************************
 * PositionInfoView::SetTime
 *****************************************************************************/
void
PositionInfoView::SetTime( const char* string )
{
	fTimeString.SetTo( string );
	_InvalidateContents();
}

/*****************************************************************************
 * PositionInfoView::_InvalidateContents
 *****************************************************************************/
void
PositionInfoView::_InvalidateContents( uint32 which )
{
	BRect r( Bounds() );
	r.InsetBy( 2.0, 2.0 );
	Invalidate( r );
}

/*****************************************************************************
 * PositionInfoView::_InvalidateContents
 *****************************************************************************/
void
PositionInfoView::_MakeString( BString& into, int32 index, int32 maxIndex ) const
{
	into = "";
	if ( index >= 0 && maxIndex >= 0 )
		into << index;
	else
		into << "-";
	into << "/";
	if ( maxIndex >= 0 )
		into << maxIndex;
	else
		into << "-";
}
