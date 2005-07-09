/*****************************************************************************
 * MessagesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Eric Petit <titer@videolan.org>
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

#ifndef BEOS_MESSAGES_WINDOW_H
#define BEOS_MESSAGES_WINDOW_H

#include <Window.h>

class MessagesView : public BTextView
{
    public:
                             MessagesView( msg_subscription_t * _p_sub,
                                           BRect rect, char * name, BRect textRect,
                                           uint32 resizingMode, uint32 flags )
                                 : BTextView( rect, name, textRect,
                                              resizingMode, flags ),
                                 p_sub(_p_sub)
                             {
                             }
        virtual void         Pulse();

        msg_subscription_t * p_sub;
        BScrollBar         * fScrollBar;
};

class MessagesWindow : public BWindow
{
    public:
                             MessagesWindow( intf_thread_t * p_intf,
                                             BRect frame, const char * name );
        virtual              ~MessagesWindow();
        virtual void         FrameResized( float, float );
        virtual bool         QuitRequested();

        void                 ReallyQuit();

        intf_thread_t      * p_intf;
        msg_subscription_t * p_sub;

        BView *              fBackgroundView;
        MessagesView *       fMessagesView;
        BScrollView *        fScrollView;
};

#endif    // BEOS_PREFERENCES_WINDOW_H

