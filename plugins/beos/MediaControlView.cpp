/*****************************************************************************
 * MediaControlView.cpp: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: MediaControlView.cpp,v 1.8 2002/06/01 12:31:58 sam Exp $
 *
 * Authors: Tony Castley <tony@castley.net>
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

/* System headers */
#include <InterfaceKit.h>
#include <AppKit.h>
#include <string.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>

/* BeOS interface headers */
#include "MsgVals.h"
#include "Bitmaps.h"
#include "TransportButton.h"
#include "MediaControlView.h"


MediaControlView::MediaControlView( BRect frame )
    : BBox( frame, NULL, B_FOLLOW_ALL, B_WILL_DRAW, B_PLAIN_BORDER )
{
	float xStart = HORZ_SPACE;
	float yStart = VERT_SPACE;
	fScrubSem = B_ERROR;
	
	BRect controlRect = BRect(xStart,yStart, 
	                          frame.Width() - (HORZ_SPACE * 2), 15);
	
    /* Seek Status */
    rgb_color fill_color = {0,255,0};
    p_seek = new SeekSlider(controlRect, this, 0, 100, B_TRIANGLE_THUMB);
    p_seek->SetValue(0);
    p_seek->UseFillColor(true, &fill_color);
    AddChild( p_seek );
    yStart += 15 + VERT_SPACE;


    /* Buttons */
    /* Slow play */
    controlRect.SetLeftTop(BPoint(xStart, yStart));
    controlRect.SetRightBottom(controlRect.LeftTop() + kSkipButtonSize);
    xStart += kRewindBitmapWidth;
    p_slow = new TransportButton(controlRect, B_EMPTY_STRING,
                                            kSkipBackBitmapBits,
                                            kPressedSkipBackBitmapBits,
                                            kDisabledSkipBackBitmapBits,
                                            new BMessage(SLOWER_PLAY));
    AddChild( p_slow );

    /* Play Pause */
    controlRect.SetLeftTop(BPoint(xStart, yStart));
    controlRect.SetRightBottom(controlRect.LeftTop() + kPlayButtonSize);
    xStart += kPlayPauseBitmapWidth + 1.0;
    p_play = new PlayPauseButton(controlRect, B_EMPTY_STRING,
                                            kPlayButtonBitmapBits,
                                            kPressedPlayButtonBitmapBits,
                                            kDisabledPlayButtonBitmapBits,
                                            kPlayingPlayButtonBitmapBits,
                                            kPressedPlayingPlayButtonBitmapBits,
                                            kPausedPlayButtonBitmapBits,
                                            kPressedPausedPlayButtonBitmapBits,
                                            new BMessage(START_PLAYBACK));

    AddChild( p_play );

    /* Fast Foward */
    controlRect.SetLeftTop(BPoint(xStart, yStart));
    controlRect.SetRightBottom(controlRect.LeftTop() + kSkipButtonSize);
    xStart += kRewindBitmapWidth;
    p_fast = new TransportButton(controlRect, B_EMPTY_STRING,
                                            kSkipForwardBitmapBits,
                                            kPressedSkipForwardBitmapBits,
                                            kDisabledSkipForwardBitmapBits,
                                            new BMessage(FASTER_PLAY));
    AddChild( p_fast );

    /* Stop */
    controlRect.SetLeftTop(BPoint(xStart, yStart));
    controlRect.SetRightBottom(controlRect.LeftTop() + kStopButtonSize);
    xStart += kStopBitmapWidth;
    p_stop = new TransportButton(controlRect, B_EMPTY_STRING,
                                            kStopButtonBitmapBits,
                                            kPressedStopButtonBitmapBits,
                                            kDisabledStopButtonBitmapBits,
                                            new BMessage(STOP_PLAYBACK));
    AddChild( p_stop );

    controlRect.SetLeftTop(BPoint(xStart + 5, yStart + 6));
    controlRect.SetRightBottom(controlRect.LeftTop() + kSpeakerButtonSize);
    xStart += kSpeakerIconBitmapWidth;

    p_mute = new TransportButton(controlRect, B_EMPTY_STRING,
                                            kSpeakerIconBits,
                                            kPressedSpeakerIconBits,
                                            kSpeakerIconBits,
                                            new BMessage(VOLUME_MUTE));

    AddChild( p_mute );

    /* Volume Slider */
    p_vol = new MediaSlider(BRect(xStart,20,255,30), new BMessage(VOLUME_CHG),
                            0, VOLUME_MAX);
    p_vol->SetValue(VOLUME_DEFAULT);
    p_vol->UseFillColor(true, &fill_color);
    AddChild( p_vol );

}

MediaControlView::~MediaControlView()
{
}

void MediaControlView::MessageReceived(BMessage *message)
{
}

void MediaControlView::SetProgress(uint64 seek, uint64 size)
{
	p_seek->SetPosition((float)seek/size);
}

void MediaControlView::SetStatus(int status, int rate)
{
    switch( status )
    {
        case PLAYING_S:
        case FORWARD_S:
        case BACKWARD_S:
        case START_S:
            p_play->SetPlaying();
            break;
        case PAUSE_S:
            p_play->SetPaused();
            break;
        case UNDEF_S:
        case NOT_STARTED_S:
        default:
            p_play->SetStopped();
            break;
    }
    if ( rate < DEFAULT_RATE )
    {
    }
}

void MediaControlView::SetEnabled(bool enabled)
{
	p_slow->SetEnabled(enabled);
	p_play->SetEnabled(enabled);
	p_fast->SetEnabled(enabled);
	p_stop->SetEnabled(enabled);
	p_mute->SetEnabled(enabled);
	p_vol->SetEnabled(enabled);
	p_seek->SetEnabled(enabled);
}

uint32 MediaControlView::GetSeekTo()
{
	return p_seek->seekTo;
}

uint32 MediaControlView::GetVolume()
{
	return p_vol->Value();
}


/*****************************************************************************
 * MediaSlider
 *****************************************************************************/
MediaSlider::MediaSlider( BRect frame, BMessage *p_message,
                          int32 i_min, int32 i_max )
            :BSlider(frame, NULL, NULL, p_message, i_min, i_max )
{

}

MediaSlider::~MediaSlider()
{

}

void MediaSlider::DrawThumb(void)
{
    BRect r;
    BView *v;

    rgb_color black = {0,0,0};
    r = ThumbFrame();
    v = OffscreenView();

    if(IsEnabled())
    {
        v->SetHighColor(black);
    }
    else
    {
        v->SetHighColor(tint_color(black, B_LIGHTEN_2_TINT));
    }

    r.InsetBy(r.IntegerWidth()/4, r.IntegerHeight()/(4 * r.IntegerWidth() / r.IntegerHeight()));
    v->StrokeEllipse(r);

    if(IsEnabled())
    {
        v->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    }
    else
    {
        v->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_LIGHTEN_2_TINT));
    }

    r.InsetBy(1,1);
    v->FillEllipse(r);
}

/*****************************************************************************
 * SeekSlider
 *****************************************************************************/
SeekSlider::SeekSlider( BRect frame, MediaControlView *p_owner, int32 i_min,
                        int32 i_max, thumb_style thumbType = B_TRIANGLE_THUMB )
           :MediaSlider( frame, NULL, i_min, i_max )
{
    fOwner = p_owner;
    fMouseDown = false;
}

SeekSlider::~SeekSlider()
{
}

/*****************************************************************************
 * SeekSlider::MouseDown
 *****************************************************************************/
void SeekSlider::MouseDown(BPoint where)
{
    BSlider::MouseDown(where);
    seekTo = ValueForPoint(where);
    fOwner->fScrubSem = create_sem(0, "Vlc::fScrubSem");
    release_sem(fOwner->fScrubSem);
    fMouseDown = true;
}

/*****************************************************************************
 * SeekSlider::MouseUp
 *****************************************************************************/
void SeekSlider::MouseMoved(BPoint where, uint32 code, const BMessage *message)
{
    BSlider::MouseMoved(where, code, message);
    if (!fMouseDown)
        return;
    seekTo = ValueForPoint(where);
    release_sem(fOwner->fScrubSem);
}

/*****************************************************************************
 * SeekSlider::MouseUp
 *****************************************************************************/
void SeekSlider::MouseUp(BPoint where)
{
    BSlider::MouseUp(where);
    seekTo = ValueForPoint(where);
    delete_sem(fOwner->fScrubSem);
    fOwner->fScrubSem = B_ERROR;
    fMouseDown = false;
}
