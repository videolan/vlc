/*****************************************************************************
 * MessagesWindow.h
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: MessagesWindow.h,v 1.2 2003/01/26 08:28:20 titer Exp $
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

class MessagesWindow : public BWindow
{
    public:
                             MessagesWindow( intf_thread_t * p_intf,
                                             BRect frame, const char * name );
        virtual              ~MessagesWindow();
        virtual void         FrameResized( float, float );
        virtual bool         QuitRequested();
        
        void                 ReallyQuit();
        void                 UpdateMessages();

    private:
        intf_thread_t *      p_intf;
        msg_subscription_t * p_sub;
        
        BView *              fBackgroundView;
        BTextView *          fMessagesView;
        BScrollView *        fScrollView;
        BScrollBar *         fScrollBar;
};

#endif	// BEOS_PREFERENCES_WINDOW_H

