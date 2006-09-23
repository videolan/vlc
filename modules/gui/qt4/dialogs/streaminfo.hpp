/*****************************************************************************
 * streaminfo.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 ******************************************************************************/

#ifndef _STREAMINFO_DIALOG_H_
#define _STREAMINFO_DIALOG_H_

#include "util/qvlcframe.hpp"
#include <QTabWidget>
#include <QBoxLayout>

class InfoTab;

class StreamInfoDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static StreamInfoDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new StreamInfoDialog( p_intf);
        return instance;
    }
    static void killInstance()
    {
        if( instance ) delete instance;
        instance= NULL;
    }
    virtual ~StreamInfoDialog();
    bool need_update;
private:
    StreamInfoDialog( intf_thread_t * );
    input_thread_t *p_input;
    InfoTab *IT;
    static StreamInfoDialog *instance;
    int i_runs;
public slots:
    void update();
    void close();
};

#endif
