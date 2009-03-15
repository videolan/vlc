/*****************************************************************************
 * mediainfo.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef QVLC_MEDIAINFO_DIALOG_H_
#define QVLC_MEDIAINFO_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "components/info_panels.hpp"

class QTabWidget;

class MediaInfoDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    MediaInfoDialog( intf_thread_t *,
                     input_item_t * );

    static MediaInfoDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance) instance = new MediaInfoDialog( p_intf, NULL );
        return instance;
    }

    static void killInstance()
    {
        delete instance;
        instance = NULL;
    }

    void showTab( int );
#if 0
    void setInput( input_item_t * );
#endif

private:
    virtual ~MediaInfoDialog();

    static MediaInfoDialog *instance;
    bool isMainInputInfo;

    QTabWidget *infoTabW;

    InputStatsPanel *ISP;
    MetaPanel *MP;
    InfoPanel *IP;
    ExtraMetaPanel *EMP;

    QPushButton *saveMetaButton;

private slots:
    void updateAllTabs( input_item_t * );
    void clearAllTabs();

    virtual void close();

    void saveMeta();
    void updateButtons( int i_tab );
};

#endif
