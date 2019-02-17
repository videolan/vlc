/*****************************************************************************
 * fsc_window.hpp
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 *
 * Author: Erwan Tulou      <erwan10 At videolan Dot Org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef FSC_WINDOW_HPP
#define FSC_WINDOW_HPP

#include "../src/top_window.hpp"
#include "../commands/cmd_generic.hpp"

class OSTimer;

/// Class for the fullscreen controller
class FscWindow: public TopWindow
{
public:
    FscWindow( intf_thread_t *pIntf, int left, int top,
                              WindowManager &rWindowManager,
                              bool dragDrop, bool playOnDrop, bool visible );

    virtual ~FscWindow();

    virtual void processEvent( EvtLeave &rEvtLeave );
    virtual void processEvent( EvtMotion &rEvtMotion );

    /// Method called when fullscreen indicator changes
    virtual void onUpdate( Subject<VarBool> &rVariable , void* );

    /// Action when window is shown
    virtual void innerShow();

    /// Action when window is hidden
    virtual void innerHide();

    /// Action when mouse moved
    virtual void onMouseMoved();

    /// Action for each transition of fading out
    virtual void onTimerExpired();

    /// Relocate fsc into new area
    virtual void moveTo( int x, int y, int width, int height );

private:
    /// Timer for fsc fading-out
    OSTimer *m_pTimer;
    /// number of transitions when fading out
    int m_count;
    /// opacity set by user
    int m_opacity;
    /// delay set by user
    int m_delay;
    /// activation set by user
    bool m_enabled;

    /// Callback for the timer
    DEFINE_CALLBACK( FscWindow, FscHide )
};


#endif
