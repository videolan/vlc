/*****************************************************************************
 * MediaControlView.h: beos interface
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: MediaControlView.h,v 1.1 2001/06/15 09:07:10 tcastley Exp $
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
#define HORZ_SPACE 5.0
#define VERT_SPACE 5.0


class TransportButton;
class PlayPauseButton;
class MediaSlider;
class SeekSlider;

class MediaControlView : public BBox
{
public:
    MediaControlView( BRect frame );
    ~MediaControlView();

    virtual void    MessageReceived(BMessage *message);
    void            SetProgress(float position);

    void            SetStatus(int status, int rate); 
    void            SetEnabled(bool);
    int32           GetSeekTo();
    int32           GetVolume();
	sem_id	fScrubSem;
	bool	fSeeking;
    
private:
	MediaSlider * p_vol;
	SeekSlider * p_seek;
	TransportButton* p_slow;
	PlayPauseButton* p_play;
	TransportButton* p_fast;
	TransportButton* p_stop;
	TransportButton* p_mute;
	
	int current_rate;
	int current_status;
};

class MediaSlider : public BSlider
{
public:
	MediaSlider(BRect frame,
				BMessage *message,
				int32 minValue,
				int32 maxValue);
	~MediaSlider();
	virtual void DrawThumb(void);
};
				

class SeekSlider : public MediaSlider
{
public:
	SeekSlider(BRect frame,
				MediaControlView *owner,
				int32 minValue,
				int32 maxValue,
				thumb_style thumbType = B_TRIANGLE_THUMB);

	~SeekSlider();
	int32 seekTo;
	virtual void MouseDown(BPoint);
	virtual void MouseUp(BPoint pt);
	virtual void MouseMoved(BPoint pt, uint32 c, const BMessage *m);
private:
	MediaControlView*	fOwner;	
	bool fMouseDown;
};


