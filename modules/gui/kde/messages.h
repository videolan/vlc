/*****************************************************************************
 * messages.h: the KMessagesWindow class
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * $Id$
 *
 * Author: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#include <kdialogbase.h>
#include <qtextview.h>
#include "common.h"

class KMessagesWindow : public KDialogBase
{
    Q_OBJECT
public:
    KMessagesWindow( intf_thread_t*,  msg_subscription_t * );
    ~KMessagesWindow();

public slots:
    void update();
private:
    intf_thread_t* p_intf;
    QTextView* text;
    msg_subscription_t *p_msg;
};

