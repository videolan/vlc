/*****************************************************************************
 * sout.hpp : Stream output dialog (old-style, ala WX)
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

#ifndef _SOUT_DIALOG_H_
#define _SOUT_DIALOG_H_

#include <vlc/vlc.h>
#include <vlc_streaming.h>

#include "ui/sout.h"
#include "util/qvlcframe.hpp"

class QPushButton;
class QCheckBox;
class QGridLayout;
class QTextEdit;

class SoutDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    SoutDialog( QWidget* parent, intf_thread_t *,
                bool _transcode_only = false );
    virtual ~SoutDialog() {}

    QString mrl;
    //sout_gui_descr_t *sout;
private:
    Ui::Sout ui;
    
public slots:
    void ok();
    void cancel();
    void updateMRL();
    void fileBrowse();
    void toggleSout();
    void setOptions();
    void setVTranscodeOptions(bool);
    void setATranscodeOptions(bool);
    void setSTranscodeOptions(bool);
    void setRawOptions(bool);
};

#endif
