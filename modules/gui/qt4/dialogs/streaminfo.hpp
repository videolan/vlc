/*****************************************************************************
 * streaminfo.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef _MEDIAINFO_DIALOG_H_
#define _MEDIAINFO_DIALOG_H_

#include "util/qvlcframe.hpp"
#include <QTabWidget>
#include <QBoxLayout>

class InfoTab;

class MediaInfoDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static MediaInfoDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new MediaInfoDialog( p_intf);
        return instance;
    }
    static void killInstance()
    {
        if( instance ) delete instance;
        instance= NULL;
    }
    virtual ~MediaInfoDialog();
    bool need_update;
private:
    MediaInfoDialog( intf_thread_t * );
    input_thread_t *p_input;
    InfoTab *IT;
    static MediaInfoDialog *instance;
    int i_runs;
public slots:
    void update();
    void close();
};

#endif
