/*****************************************************************************
 * extended.hpp : Extended controls - Undocked
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
 *****************************************************************************/

#ifndef QVLC_EXTENDED_DIALOG_H_
#define QVLC_EXTENDED_DIALOG_H_ 1

#include "util/qvlcframe.hpp"

#include "components/extended_panels.hpp"
#include "util/singleton.hpp"

class QTabWidget;

class ExtendedDialog : public QVLCDialog, public Singleton<ExtendedDialog>
{
    Q_OBJECT
public:
    enum
    {
        AUDIO_TAB = 0,
        VIDEO_TAB,
        SYNCHRO_TAB,
        V4L2_TAB
    };
    void showTab( int i );
    int currentTab();
private:
    ExtendedDialog( intf_thread_t * );
    virtual ~ExtendedDialog();

    SyncControls *syncW;
    ExtVideo *videoEffect;
    Equalizer *equal;
    QTabWidget *mainTabW;
    QCheckBox *writeChangesBox;
private slots:
    void changedItem( int );
    void currentTabChanged( int );

    friend class    Singleton<ExtendedDialog>;
};

#endif

